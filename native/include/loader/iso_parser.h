#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace x360 {

struct IsoFileEntry {
    std::string name;
    uint32_t    offset;   // byte offset in ISO
    uint32_t    size;
    bool        isDirectory;
};

class IsoParser {
public:
    IsoParser();
    ~IsoParser();

    // Open an ISO image (ISO 9660 or XDVDFS).
    bool open(const std::string& path);
    void close();

    // List all files recursively.
    std::vector<IsoFileEntry> listFiles() const { return m_files; }

    // Find a file by name (case-insensitive). Returns nullptr if not found.
    const IsoFileEntry* findFile(const std::string& name) const;

    // Read file contents into buffer. Returns bytes read or -1 on error.
    int readFile(const IsoFileEntry& entry, std::vector<uint8_t>& out);

    // Extract a file to disk.
    bool extractFile(const IsoFileEntry& entry, const std::string& destPath);

    const std::string& lastError() const { return m_lastError; }

private:
    bool parseIso9660();
    bool parseXdvdfs();
    void walkIso9660Directory(uint32_t lba, uint32_t size, const std::string& prefix);
    void walkXdvdfsDirectory(uint32_t offset, uint32_t size, const std::string& prefix);

    std::string m_path;
    FILE*       m_file = nullptr;
    bool        m_isXdvdfs = false;
    std::vector<IsoFileEntry> m_files;
    std::string m_lastError;
};

} // namespace x360
