/**
 * ISO 9660 / XDVDFS (Xbox DVD File System) parser.
 * Supports standard ISO images and Xbox 360 XDVDFS discs.
 * Reference: XDVDFS spec + Xenia xdvdfs.cc
 */
#include "../../include/loader/iso_parser.h"
#include <cstring>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <android/log.h>

#define LOG_TAG "X360:ISO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace fs = std::filesystem;

// ─── XDVDFS constants ──────────────────────────────────────────────────────────
static constexpr uint32_t XDVDFS_MAGIC0 = 0x20584446; // ' XDF'
static constexpr uint32_t XDVDFS_SECTOR = 0x800;      // 2048 bytes
static constexpr uint32_t XDVDFS_HEADER_SECTOR = 0x20; // sector 32 (0x10000 bytes in)

// ISO 9660 constants
static constexpr size_t ISO_SECTOR_SIZE = 2048;
static constexpr size_t ISO_PVD_SECTOR  = 16;

#pragma pack(push, 1)
struct XdvdfsHeader {
    char     magic[20];     // "MICROSOFT*XBOX*MEDIA"
    uint32_t rootDirSector;
    uint32_t rootDirSize;
    uint64_t fileTime;
    uint8_t  padding[0x7C8];
    char     magic2[20];    // "MICROSOFT*XBOX*MEDIA" (repeated at 0x7EC)
};

struct XdvdfsEntry {
    uint16_t subtreeLeft;   // offset to left subtree entry (in 4-byte units from dir start)
    uint16_t subtreeRight;  // offset to right subtree entry
    uint32_t sector;        // first sector of file
    uint32_t fileSize;
    uint8_t  attributes;    // 0x10 = directory
    uint8_t  nameLen;
    char     name[1];       // variable length
};

struct Iso9660DirRecord {
    uint8_t  length;
    uint8_t  extAttrLen;
    uint32_t lbaLE; uint32_t lbaBE;
    uint32_t sizeLE; uint32_t sizeBE;
    uint8_t  date[7];
    uint8_t  flags;
    uint8_t  interleaveUnitSize;
    uint8_t  interleaveGapSize;
    uint16_t volSeqNumLE; uint16_t volSeqNumBE;
    uint8_t  nameLen;
    char     name[1];
};
#pragma pack(pop)

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

IsoParser::IsoParser() = default;
IsoParser::~IsoParser() { close(); }

bool IsoParser::open(const std::string& path) {
    close();
    m_path = path;
    m_file = fopen(path.c_str(), "rb");
    if (!m_file) { m_lastError = "Cannot open: " + path; return false; }

    // Try XDVDFS first
    uint8_t hdrBuf[sizeof(XdvdfsHeader)];
    fseek(m_file, XDVDFS_HEADER_SECTOR * XDVDFS_SECTOR, SEEK_SET);
    if (fread(hdrBuf, 1, sizeof(hdrBuf), m_file) == sizeof(hdrBuf)) {
        if (memcmp(hdrBuf, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
            m_isXdvdfs = true;
            LOGI("ISO: XDVDFS format detected");
            parseXdvdfs();
            return !m_files.empty() || true;
        }
    }

    // Fall back to ISO 9660
    m_isXdvdfs = false;
    LOGI("ISO: trying ISO 9660");
    return parseIso9660();
}

void IsoParser::close() {
    if (m_file) { fclose(m_file); m_file = nullptr; }
    m_files.clear();
}

bool IsoParser::parseXdvdfs() {
    uint8_t hdrBuf[sizeof(XdvdfsHeader)];
    fseek(m_file, XDVDFS_HEADER_SECTOR * XDVDFS_SECTOR, SEEK_SET);
    if (fread(hdrBuf, 1, sizeof(hdrBuf), m_file) != sizeof(hdrBuf)) return false;

    const auto* hdr = reinterpret_cast<const XdvdfsHeader*>(hdrBuf);
    uint32_t rootSector = hdr->rootDirSector;
    uint32_t rootSize   = hdr->rootDirSize;

    walkXdvdfsDirectory(rootSector * XDVDFS_SECTOR, rootSize, "");
    LOGI("ISO: XDVDFS parsed %zu files", m_files.size());
    return !m_files.empty();
}

void IsoParser::walkXdvdfsDirectory(uint32_t offset, uint32_t size, const std::string& prefix) {
    if (size == 0 || size > 4 * 1024 * 1024) return; // sanity
    std::vector<uint8_t> buf(size);
    fseek(m_file, offset, SEEK_SET);
    if (fread(buf.data(), 1, size, m_file) != size) return;

    // Walk the binary tree of entries
    std::vector<uint32_t> toVisit = {0};
    while (!toVisit.empty()) {
        uint32_t entryOffset = toVisit.back(); toVisit.pop_back();
        if (entryOffset * 4 + sizeof(XdvdfsEntry) > size) continue;

        const auto* e = reinterpret_cast<const XdvdfsEntry*>(buf.data() + entryOffset * 4);
        if (e->nameLen == 0 || e->nameLen > 0xFE) continue;

        std::string name(e->name, e->nameLen);
        std::string fullPath = prefix.empty() ? name : (prefix + "/" + name);

        IsoFileEntry fe;
        fe.name   = fullPath;
        fe.offset = e->sector * XDVDFS_SECTOR;
        fe.size   = e->fileSize;
        fe.isDirectory = (e->attributes & 0x10) != 0;

        m_files.push_back(fe);

        if (fe.isDirectory && e->fileSize > 0) {
            walkXdvdfsDirectory(e->sector * XDVDFS_SECTOR, e->fileSize, fullPath);
        }
        if (e->subtreeLeft  != 0xFFFF) toVisit.push_back(e->subtreeLeft);
        if (e->subtreeRight != 0xFFFF) toVisit.push_back(e->subtreeRight);
    }
}

bool IsoParser::parseIso9660() {
    // Read PVD at sector 16
    uint8_t pvd[ISO_SECTOR_SIZE];
    fseek(m_file, ISO_PVD_SECTOR * ISO_SECTOR_SIZE, SEEK_SET);
    if (fread(pvd, 1, ISO_SECTOR_SIZE, m_file) != ISO_SECTOR_SIZE) {
        m_lastError = "Cannot read ISO PVD";
        return false;
    }
    if (pvd[0] != 0x01 || memcmp(pvd + 1, "CD001", 5) != 0) {
        m_lastError = "Not a valid ISO 9660 image";
        return false;
    }

    // Root directory record at offset 156
    const auto* root = reinterpret_cast<const Iso9660DirRecord*>(pvd + 156);
    uint32_t rootLba  = root->lbaLE;
    uint32_t rootSize = root->sizeLE;

    walkIso9660Directory(rootLba, rootSize, "");
    LOGI("ISO: ISO9660 parsed %zu files", m_files.size());
    return true;
}

void IsoParser::walkIso9660Directory(uint32_t lba, uint32_t size, const std::string& prefix) {
    if (size > 256 * 1024 * 1024) return;
    std::vector<uint8_t> buf(size);
    fseek(m_file, lba * (long)ISO_SECTOR_SIZE, SEEK_SET);
    if (fread(buf.data(), 1, size, m_file) != size) return;

    size_t pos = 0;
    while (pos < size) {
        const auto* dr = reinterpret_cast<const Iso9660DirRecord*>(buf.data() + pos);
        if (dr->length == 0) { pos = (pos + ISO_SECTOR_SIZE) & ~(ISO_SECTOR_SIZE - 1); continue; }
        if (dr->nameLen == 1 && (dr->name[0] == '\0' || dr->name[0] == '\1')) {
            pos += dr->length; continue;
        }

        std::string name(dr->name, dr->nameLen);
        // Strip version number ";1"
        auto semi = name.find(';');
        if (semi != std::string::npos) name = name.substr(0, semi);

        std::string fullPath = prefix.empty() ? name : (prefix + "/" + name);

        IsoFileEntry fe;
        fe.name   = fullPath;
        fe.offset = dr->lbaLE * (uint32_t)ISO_SECTOR_SIZE;
        fe.size   = dr->sizeLE;
        fe.isDirectory = (dr->flags & 0x02) != 0;

        if (!name.empty()) {
            m_files.push_back(fe);
            if (fe.isDirectory && dr->lbaLE != lba) {
                walkIso9660Directory(dr->lbaLE, dr->sizeLE, fullPath);
            }
        }
        pos += dr->length;
    }
}

const IsoFileEntry* IsoParser::findFile(const std::string& name) const {
    std::string lower = toLower(name);
    for (const auto& f : m_files) {
        if (toLower(f.name) == lower || toLower(f.name).find(lower) != std::string::npos)
            return &f;
    }
    return nullptr;
}

int IsoParser::readFile(const IsoFileEntry& entry, std::vector<uint8_t>& out) {
    if (!m_file) return -1;
    out.resize(entry.size);
    fseek(m_file, entry.offset, SEEK_SET);
    return (int)fread(out.data(), 1, entry.size, m_file);
}

bool IsoParser::extractFile(const IsoFileEntry& entry, const std::string& destPath) {
    std::vector<uint8_t> buf;
    int n = readFile(entry, buf);
    if (n < 0) return false;
    std::ofstream out(destPath, std::ios::binary);
    if (!out) { m_lastError = "Cannot create: " + destPath; return false; }
    out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    return true;
}

} // namespace x360
