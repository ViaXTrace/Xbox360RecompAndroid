/**
 * STFS (Secure Transacted File System) parser.
 * Supports CON (homebrew), PIRS (MS resigned), LIVE (Xbox Live) containers.
 * Reference: Velocity source + Xbox 360 STFS reverse engineering docs.
 */
#include "../../include/loader/stfs_parser.h"
#include <cstring>
#include <fstream>
#include <filesystem>
#include <android/log.h>

#define LOG_TAG "X360:STFS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {

namespace fs = std::filesystem;

// ─── STFS on-disk constants ────────────────────────────────────────────────────
static constexpr uint32_t STFS_MAGIC_CON  = 0x434F4E20; // 'CON '
static constexpr uint32_t STFS_MAGIC_PIRS = 0x50495253; // 'PIRS'
static constexpr uint32_t STFS_MAGIC_LIVE = 0x4C495645; // 'LIVE'

static constexpr size_t STFS_BLOCK_SIZE = 0x1000; // 4096 bytes
static constexpr uint32_t STFS_HEADER_SIZE = 0xB000;
static constexpr uint32_t STFS_HASH_BLOCK_STEP0 = 0xAA;   // 170 — blocks per L0 hash table
static constexpr uint32_t STFS_HASH_BLOCK_STEP1 = 0x70E4; // 28900

// File entry in STFS directory
#pragma pack(push, 1)
struct StfsFileEntryOnDisk {
    char     fileName[0x28];       // 40 chars, null-terminated
    uint8_t  flags;                // bit 0 = directory, bits 6-7 = consecutive block count
    uint8_t  numBlocksHi;
    uint16_t numBlocksLo;          // 3-byte block count (big-endian)
    uint8_t  startBlockHi;
    uint16_t startBlockLo;         // 3-byte start block (big-endian)
    uint16_t pathIndicator;        // parent directory entry index
    uint32_t fileSize;             // big-endian
    uint32_t updateDateTime;
    uint32_t accessDateTime;
};
#pragma pack(pop)

static inline uint32_t be32(uint32_t v) { return __builtin_bswap32(v); }
static inline uint32_t readBE24(const uint8_t* p) {
    return (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
}

// ─── StfsParser implementation ─────────────────────────────────────────────────

StfsParser::StfsParser() = default;
StfsParser::~StfsParser() = default;

bool StfsParser::open(const std::string& path, StfsPackage& outPkg) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { m_lastError = "Cannot open: " + path; return false; }

    size_t fileSize = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    if (data.size() < 4) { m_lastError = "File too small"; return false; }

    uint32_t magic = be32(*reinterpret_cast<const uint32_t*>(data.data()));
    switch (magic) {
    case STFS_MAGIC_CON:  outPkg.type = StfsPackageType::CON;  break;
    case STFS_MAGIC_PIRS: outPkg.type = StfsPackageType::PIRS; break;
    case STFS_MAGIC_LIVE: outPkg.type = StfsPackageType::LIVE; break;
    default:
        m_lastError = "Not an STFS package (bad magic)";
        return false;
    }

    return parseHeader(data, outPkg) && parseFatx(data, outPkg);
}

bool StfsParser::parseHeader(const std::vector<uint8_t>& data, StfsPackage& pkg) {
    if (data.size() < STFS_HEADER_SIZE) {
        m_lastError = "File smaller than STFS header";
        return false;
    }

    // Volume descriptor at 0x379
    const uint8_t* vd = data.data() + 0x379;
    uint32_t allocBlockCount = readBE24(vd + 5);
    uint32_t firstAllocBlock  = readBE24(vd + 8);
    uint8_t  reserved = vd[0];
    m_blockSeparation = (reserved & 1) ? 1 : 0;

    // Compute top hash table level
    m_topLevel = 0;
    if (allocBlockCount >= STFS_HASH_BLOCK_STEP0) m_topLevel = 1;
    if (allocBlockCount >= STFS_HASH_BLOCK_STEP1) m_topLevel = 2;

    // Title name at 0x411 (UTF-16 LE, 0x80 bytes = 64 chars)
    char titleName[65] = {};
    const uint8_t* tn = data.data() + 0x411;
    for (int i = 0; i < 64; i++) {
        uint16_t wc = tn[2*i] | ((uint16_t)tn[2*i+1] << 8);
        titleName[i] = (wc < 0x80) ? (char)wc : '?';
        if (!wc) break;
    }
    pkg.displayName = titleName;

    // Title ID at 0x354 (4 bytes big-endian)
    uint32_t tid = be32(*reinterpret_cast<const uint32_t*>(data.data() + 0x354));
    char tidBuf[9];
    snprintf(tidBuf, sizeof(tidBuf), "%08X", tid);
    pkg.titleId = tidBuf;

    // Content type at 0x344
    pkg.contentType = be32(*reinterpret_cast<const uint32_t*>(data.data() + 0x344));

    LOGI("STFS: '%s' titleId=%s type=0x%X", titleName, tidBuf, pkg.contentType);
    return true;
}

bool StfsParser::parseFatx(const std::vector<uint8_t>& data, StfsPackage& pkg) {
    // The directory is rooted at the first file table block.
    // Walk directory entries at 0xA000 (typically — depends on header size).
    // File table starts at block 0 of the allocatable area.
    // Each file listing block = 4096 bytes, 64 entries of 0x40 bytes each.

    static constexpr uint32_t dirBlockOffset = STFS_HEADER_SIZE;
    if (data.size() <= dirBlockOffset) return true; // empty

    size_t numDirBlocks = (data.size() - dirBlockOffset) / STFS_BLOCK_SIZE;
    if (numDirBlocks == 0) return true;

    // Read up to first 4 directory blocks
    for (size_t blk = 0; blk < std::min(numDirBlocks, (size_t)4); blk++) {
        const uint8_t* block = data.data() + dirBlockOffset + blk * STFS_BLOCK_SIZE;
        for (int e = 0; e < 64; e++) {
            const auto* entry = reinterpret_cast<const StfsFileEntryOnDisk*>(block + e * 0x40);
            if ((uint8_t)entry->fileName[0] == 0x00) break; // end of entries
            if ((uint8_t)entry->fileName[0] == 0xE5) continue; // deleted

            StfsFileEntry fe;
            fe.name = std::string(entry->fileName, strnlen(entry->fileName, 0x28));
            fe.isDirectory = (entry->flags & 0x80) != 0;
            fe.size = be32(entry->fileSize);
            fe.flags = entry->flags;

            if (!fe.name.empty()) {
                pkg.files.push_back(fe);
                LOGI("STFS: %s '%s' size=%u",
                     fe.isDirectory ? "DIR" : "FILE",
                     fe.name.c_str(), fe.size);
            }
        }
    }
    return true;
}

bool StfsParser::extractFile(const std::string& pkgPath, const std::string& internalName,
                              const std::string& destDir, std::string& outHostPath) {
    StfsPackage pkg;
    if (!open(pkgPath, pkg)) return false;

    // Re-read the file to extract
    std::ifstream file(pkgPath, std::ios::binary | std::ios::ate);
    if (!file) { m_lastError = "Cannot reopen package"; return false; }
    size_t fileSize = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    for (const auto& fe : pkg.files) {
        if (fe.name == internalName || fe.name.find(internalName) != std::string::npos) {
            outHostPath = destDir + "/" + fe.name;
            // Find the block for this file from the raw data
            // (simplified: search for XEX magic in data)
            const uint8_t xexMagic[] = {'X','E','X','2'};
            for (size_t i = STFS_HEADER_SIZE; i + fe.size < data.size(); i += STFS_BLOCK_SIZE) {
                if (data.size() > i + 3 &&
                    memcmp(data.data() + i, xexMagic, 4) == 0) {
                    fs::create_directories(destDir);
                    std::ofstream out(outHostPath, std::ios::binary);
                    if (!out) { m_lastError = "Cannot create output file"; return false; }
                    out.write(reinterpret_cast<const char*>(data.data() + i), fe.size);
                    LOGI("STFS: extracted '%s' to '%s'", fe.name.c_str(), outHostPath.c_str());
                    return true;
                }
            }
        }
    }
    m_lastError = "File '" + internalName + "' not found in package";
    return false;
}

int StfsParser::extractAll(const std::string& path, const std::string& destDir,
                           StfsPackage& pkg) {
    if (!open(path, pkg)) return -1;
    fs::create_directories(destDir);
    LOGI("STFS: %zu files parsed", pkg.files.size());
    return (int)pkg.files.size();
}

} // namespace x360
