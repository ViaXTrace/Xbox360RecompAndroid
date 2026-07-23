#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace x360 {

// XEX2 header magic
static constexpr uint32_t XEX2_MAGIC = 0x58455832; // 'XEX2'

// Module flags
enum class XexModuleFlag : uint32_t {
    TitleModule     = 0x00000001,
    ExportsToKernel = 0x00000002,
    SystemDebugger  = 0x00000004,
    Dll             = 0x00000008,
    Module          = 0x00000010,
    PatchFull       = 0x00000020,
    PatchDelta      = 0x00000040,
    UserMode        = 0x00000080,
};

// Compression type
enum class XexCompressionType : uint32_t {
    None        = 0,
    Basic       = 1,
    Normal      = 2,
    Delta       = 3,
};

// Encryption type
enum class XexEncryptionType : uint32_t {
    None   = 0,
    Normal = 1,
};

struct XexSection {
    uint32_t virtualAddress;
    uint32_t virtualSize;
    std::vector<uint8_t> data;
    bool isCode;
    bool isData;
};

struct XexImport {
    std::string moduleName;
    uint32_t ordinal;
    uint32_t address;  // address in virtual memory where thunk lives
};

struct XexHeader {
    uint32_t magic;
    uint32_t moduleFlags;
    uint32_t sizeOfHeaders;
    uint32_t sizeOfDiscardableHeaders;
    uint32_t securityOffset;
    uint32_t headerDirectoryEntryCount;
    uint32_t baseAddress;       // typically 0x82000000
    uint32_t entryPoint;
    uint32_t defaultStackSize;
    uint32_t defaultFibersSize;
    uint32_t defaultHeapSize;
    char     titleId[4];
    uint32_t executableTableOffset;
    uint32_t importTableOffset;
    uint32_t resourceTableOffset;
    XexCompressionType compressionType;
    XexEncryptionType  encryptionType;
};

struct XexImage {
    XexHeader header;
    std::string titleId;
    std::string titleName;
    std::string region;
    uint32_t baseAddress;
    uint32_t entryPoint;
    std::vector<XexSection> sections;
    std::vector<XexImport> imports;
    std::vector<uint8_t> rawMemory;  // full mapped image
};

class XexLoader {
public:
    XexLoader();
    ~XexLoader();

    // Load XEX from file path. Returns true on success.
    bool load(const std::string& path, XexImage& outImage);

    const std::string& lastError() const { return m_lastError; }

private:
    bool parseHeader(const std::vector<uint8_t>& raw, XexImage& img);
    bool decryptSections(std::vector<uint8_t>& data, XexImage& img);
    bool decompressSections(std::vector<uint8_t>& data, XexImage& img);
    bool resolveImports(XexImage& img);
    bool mapToVirtualMemory(XexImage& img);

    // AES-128-CBC decryption (retail key baked in per Xbox 360 spec)
    void aesDecrypt(const uint8_t* key, const uint8_t* iv,
                    const uint8_t* cipher, uint8_t* plain, size_t len);

    // LZX decompression (used for XEX normal compression)
    bool lzxDecompress(const uint8_t* src, size_t srcLen,
                       uint8_t* dst, size_t dstLen, int windowBits);

    std::string m_lastError;
};

} // namespace x360
