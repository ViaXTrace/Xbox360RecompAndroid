#pragma once
#include <cstdint>
#include <cstddef>

namespace x360 {

// AES-128-CBC software implementation (no hardware dependency)
class Aes128 {
public:
    // Encrypt/decrypt in-place, CBC mode. iv is 16 bytes, modified in-place.
    static void cbcDecrypt(const uint8_t* key16, uint8_t* iv16,
                           uint8_t* data, size_t len);
    static void cbcEncrypt(const uint8_t* key16, uint8_t* iv16,
                           uint8_t* data, size_t len);

    // ECB single-block (16 bytes)
    static void ecbDecrypt(const uint8_t* key16, const uint8_t* in, uint8_t* out);
    static void ecbEncrypt(const uint8_t* key16, const uint8_t* in, uint8_t* out);
};

} // namespace x360
