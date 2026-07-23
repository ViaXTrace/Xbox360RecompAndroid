/**
 * HLE Input — XInput emulation.
 * Receives state from dart:ffi (Android touch/gamepad layer) via setInputState().
 * Provides XInputGetState-compatible response to guest game code.
 */
#include "../../include/hle/hle_kernel.h"
#include <cstring>
#include <android/log.h>

#define LOG_TAG "X360:INPUT"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {

void HleKernel::setInputState(int pad, uint32_t buttons,
                               int16_t lx, int16_t ly,
                               int16_t rx, int16_t ry,
                               uint8_t lt, uint8_t rt) {
    if (pad < 0 || pad >= 4) return;
    std::lock_guard<std::mutex> lk(m_inputState[pad].m);
    m_inputState[pad].buttons = buttons;
    m_inputState[pad].lx = lx;
    m_inputState[pad].ly = ly;
    m_inputState[pad].rx = rx;
    m_inputState[pad].ry = ry;
    m_inputState[pad].lt = lt;
    m_inputState[pad].rt = rt;
}

// Returns 0 (ERROR_SUCCESS) if pad is connected, fills XINPUT_STATE at outState
uint32_t HleKernel::getInputState(int pad, void* outState) {
    if (pad < 0 || pad >= 4 || !outState) return 0x48F; // ERROR_DEVICE_NOT_CONNECTED

    // XINPUT_STATE layout on Xbox 360 (big-endian in guest memory):
    // DWORD  dwPacketNumber
    // struct XINPUT_GAMEPAD {
    //   WORD  wButtons;        (big-endian)
    //   BYTE  bLeftTrigger;
    //   BYTE  bRightTrigger;
    //   SHORT sThumbLX;        (big-endian)
    //   SHORT sThumbLY;
    //   SHORT sThumbRX;
    //   SHORT sThumbRY;
    // }
    static uint32_t packetNum = 0;
    uint8_t* s = (uint8_t*)outState;

    std::lock_guard<std::mutex> lk(m_inputState[pad].m);
    const auto& in = m_inputState[pad];

    // Write big-endian packet number
    uint32_t pn = __builtin_bswap32(++packetNum);
    memcpy(s, &pn, 4);

    // wButtons (big-endian)
    uint16_t btns = __builtin_bswap16((uint16_t)(in.buttons & 0xFFFF));
    memcpy(s + 4, &btns, 2);

    s[6] = in.lt;
    s[7] = in.rt;

    // Thumbstick axes (big-endian int16)
    int16_t lx = __builtin_bswap16(in.lx);
    int16_t ly = __builtin_bswap16(in.ly);
    int16_t rx = __builtin_bswap16(in.rx);
    int16_t ry = __builtin_bswap16(in.ry);
    memcpy(s + 8,  &lx, 2);
    memcpy(s + 10, &ly, 2);
    memcpy(s + 12, &rx, 2);
    memcpy(s + 14, &ry, 2);

    return 0; // ERROR_SUCCESS
}

} // namespace hle
} // namespace x360
