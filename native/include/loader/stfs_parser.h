#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace x360 {

// STFS container types
enum class StfsPackageType {
    CON,   // Homebrew signed
    PIRS,  // MS resigned
    LIVE,  // Xbox Live signed
};

struct StfsFileEntry {
    std::string name;
    uint32_t    size;
    uint32_t    flags;
    bool        isDirectory;
    std::string hostPath; // extracted host path (empty until extracted)
};

struct StfsPackage {
    StfsPackageType type;
    std::string     displayName;
    std::string     titleId;
    uint32_t        contentType;
    std::vector<StfsFileEntry> files;
};

class StfsParser {
public:
    StfsParser();
    ~StfsParser();

    // Open and parse an STFS container.
    bool open(const std::string& path, StfsPackage& outPkg);

    // Extract a specific file by internal name to a host directory.
    // Returns the host path of the extracted file.
    bool extractFile(const std::string& path, const std::string& internalName,
                     const std::string& destDir, std::string& outHostPath);

    // Extract all files to destDir. Returns count of extracted files.
    int extractAll(const std::string& path, const std::string& destDir,
                   StfsPackage& pkg);

    const std::string& lastError() const { return m_lastError; }

private:
    bool parseHeader(const std::vector<uint8_t>& data, StfsPackage& pkg);
    bool parseFatx(const std::vector<uint8_t>& data, StfsPackage& pkg);
    bool readBlock(const std::vector<uint8_t>& data, uint32_t blockIndex,
                   uint8_t* outBuf, size_t blockSize);

    std::string m_lastError;
    uint32_t m_blockSeparation = 0;
    uint32_t m_topLevel = 0;
};

} // namespace x360
