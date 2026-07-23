/**
 * HLE Filesystem — NtCreateFile, NtReadFile, NtWriteFile, etc.
 * Maps Xbox 360 guest paths to Android host paths.
 *
 * Path mappings:
 *   game:\   → gameDir
 *   d:\      → gameDir (disc)
 *   hdd1:\   → saveDir/hdd1
 *   usb:\    → saveDir/usb
 *   cache:\  → saveDir/cache
 */
#include "../../include/hle/hle_kernel.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "X360:FS"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {
namespace fs = std::filesystem;

std::string HleKernel::resolveGuestPath(const std::string& guestPath) const {
    std::string lower = guestPath;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Replace backslashes
    std::replace(lower.begin(), lower.end(), '\\', '/');

    for (const auto& [prefix, hostDir] : m_pathMappings) {
        if (lower.rfind(prefix, 0) == 0) {
            std::string rel = guestPath.substr(prefix.size());
            std::replace(rel.begin(), rel.end(), '\\', '/');
            return hostDir + "/" + rel;
        }
    }
    // Fallback: treat as relative to game dir
    std::string rel = guestPath;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    return m_gameDir + "/" + rel;
}

uint32_t HleKernel::createFile(const std::string& guestPath, uint32_t access,
                                uint32_t shareMode, uint32_t createDisposition) {
    std::string hostPath = resolveGuestPath(guestPath);
    LOGI("FS: createFile '%s' → '%s' access=0x%X disp=0x%X",
         guestPath.c_str(), hostPath.c_str(), access, createDisposition);

    const char* mode = "rb";
    bool write = (access & 0x40000000) != 0; // GENERIC_WRITE
    bool create = (createDisposition == 2 || createDisposition == 3 || createDisposition == 5);

    if (write || create) {
        if (createDisposition == 2) mode = "wb";       // CREATE_ALWAYS
        else if (createDisposition == 5) mode = "ab";  // OPEN_ALWAYS append
        else mode = "r+b";
    }

    // Ensure parent directory exists for write mode
    if (write || create) {
        try { fs::create_directories(fs::path(hostPath).parent_path()); }
        catch (...) {}
    }

    FILE* f = fopen(hostPath.c_str(), mode);
    if (!f && create) {
        // Try creating the file
        f = fopen(hostPath.c_str(), "wb");
        if (f) { fclose(f); f = fopen(hostPath.c_str(), mode); }
    }

    if (!f) {
        LOGE("FS: cannot open '%s': %s", hostPath.c_str(), strerror(errno));
        return 0xFFFFFFFF; // INVALID_HANDLE_VALUE
    }

    uint32_t handle = allocHandle();
    auto obj = std::make_unique<KernelFile>();
    obj->handle    = handle;
    obj->type      = KernelObjectType::File;
    obj->hostFile  = f;
    obj->guestPath = guestPath;
    obj->hostPath  = hostPath;

    std::lock_guard<std::mutex> lk(m_objectMutex);
    m_objects[handle] = std::move(obj);
    return handle;
}

bool HleKernel::readFile(uint32_t handle, void* buffer, uint32_t size, uint32_t* bytesRead) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::File) { if (bytesRead) *bytesRead = 0; return false; }
    auto* f = static_cast<KernelFile*>(obj);
    size_t n = fread(buffer, 1, size, f->hostFile);
    if (bytesRead) *bytesRead = (uint32_t)n;
    return n > 0 || size == 0;
}

bool HleKernel::writeFile(uint32_t handle, const void* buffer, uint32_t size, uint32_t* bytesWritten) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::File) { if (bytesWritten) *bytesWritten = 0; return false; }
    auto* f = static_cast<KernelFile*>(obj);
    size_t n = fwrite(buffer, 1, size, f->hostFile);
    if (bytesWritten) *bytesWritten = (uint32_t)n;
    return n == size;
}

void HleKernel::closeHandle(uint32_t handle) {
    auto* obj = getObject(handle);
    if (obj && obj->type == KernelObjectType::File) {
        auto* f = static_cast<KernelFile*>(obj);
        if (f->hostFile) { fclose(f->hostFile); f->hostFile = nullptr; }
    }
    destroyObject(handle);
}

uint64_t HleKernel::getFileSize(uint32_t handle) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::File) return 0;
    auto* f = static_cast<KernelFile*>(obj);
    long cur = ftell(f->hostFile);
    fseek(f->hostFile, 0, SEEK_END);
    long size = ftell(f->hostFile);
    fseek(f->hostFile, cur, SEEK_SET);
    return (uint64_t)size;
}

bool HleKernel::setFilePointer(uint32_t handle, int64_t distanceToMove, uint32_t moveMethod) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::File) return false;
    auto* f = static_cast<KernelFile*>(obj);
    int whence = (moveMethod == 0) ? SEEK_SET : (moveMethod == 1) ? SEEK_CUR : SEEK_END;
    return fseek(f->hostFile, (long)distanceToMove, whence) == 0;
}

} // namespace hle
} // namespace x360
