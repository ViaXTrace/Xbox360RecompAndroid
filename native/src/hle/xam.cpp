/**
 * xam.xex HLE dispatch — implements XAM (Xbox Application Manager) kernel exports.
 * Handles UI notifications, profile/account queries, marketplace, etc.
 * Most game-essential APIs: XContentCreate, XUserGetXUID, XShowMessageBox stubs.
 */
#include "../../include/hle/hle_kernel.h"
#include "../../include/jit/jit_engine.h"
#include <cstring>
#include <android/log.h>

#define LOG_TAG "X360:XAM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {

// Forward audio functions
void hleSubmitSourceBuffer(const uint8_t*, uint32_t, uint32_t, uint32_t, bool, float, bool);

static inline uint32_t guestRead32(const uint8_t* gm, uint64_t addr, uint64_t base) {
    uint32_t v; __builtin_memcpy(&v, gm + addr - base, 4); return __builtin_bswap32(v);
}
static inline void guestWrite32(uint8_t* gm, uint64_t addr, uint64_t base, uint32_t val) {
    val = __builtin_bswap32(val); __builtin_memcpy(gm + addr - base, &val, 4);
}

// XAM ordinal dispatch
// Called from dispatchKernelCall when module == "xam.xex"
void dispatchXamCall(HleKernel& kernel, uint32_t ordinal, jit::PPCContext& ctx,
                     uint8_t* gm, uint64_t gb) {
    auto& r = ctx.gpr;

    switch (ordinal) {
    // ── User/Profile ──────────────────────────────────────────────────────────
    case 395: // XUserGetXUID(UserIndex, Flags, XUID*)
    {
        uint64_t xuidPtr = r[5];
        if (xuidPtr > gb) {
            uint64_t fakeXuid = __builtin_bswap64(0x0009000000000001ULL);
            __builtin_memcpy(gm + xuidPtr - gb, &fakeXuid, 8);
        }
        r[3] = 0;
        break;
    }
    case 397: // XUserGetName(UserIndex, Buffer, BufferLen)
    {
        uint64_t bufPtr = r[4];
        uint32_t bufLen = (uint32_t)r[5];
        const char* name = "Player1";
        if (bufPtr > gb && bufLen > 0) {
            strncpy((char*)(gm + bufPtr - gb), name, bufLen - 1);
        }
        r[3] = 0;
        break;
    }
    case 398: // XUserGetSigninState → SignedInLocally
        r[3] = 1;
        break;
    case 371: // XUserGetGamerTag
        r[3] = 0;
        break;

    // ── Content / Saves ───────────────────────────────────────────────────────
    case 403: // XContentCreate
    case 415: // XContentOpenFile
        r[3] = 0;
        break;
    case 404: // XContentClose
        r[3] = 0;
        break;
    case 478: // XStorageDownloadToMemory
    case 479: // XStorageUploadFromMemory
        r[3] = 0x80004005; // E_FAIL (network stubs)
        break;

    // ── Marketplace ───────────────────────────────────────────────────────────
    case 522: // XShowMarketplaceUI → no-op
    case 523: // XShowAchievementsUI → no-op
    case 524: // XShowGamercardUI → no-op
        r[3] = 0;
        break;

    // ── Message boxes / UI overlays → dismiss immediately ────────────────────
    case 455: // XShowMessageBox → OK
        r[3] = 0; // XOVERLAPPED_SUCCESS, button index 0
        break;
    case 456: // XShowFriendRequestUI → nop
    case 457: // XShowSigninUI → nop
        r[3] = 0;
        break;

    // ── Audio (XAM audio helpers) ─────────────────────────────────────────────
    case 21: // XMPCreateTitlePlaylist (background music) → stub success
    case 22: // XMPDeleteTitlePlaylist
    case 28: // XMPPlayTitlePlaylist
    case 29: // XMPStopPlayback
        r[3] = 0;
        break;

    // ── Misc ─────────────────────────────────────────────────────────────────
    case 1: // XGetLanguage → English (0)
        r[3] = 0;
        break;
    case 2: // XGetVideoFlags
        r[3] = 0x00000004; // XVIDEO_FLAGS_HDTV_720p
        break;
    case 3: // XGetAudioFlags
        r[3] = 0;
        break;
    case 10: // XamLoaderGetLaunchData → no data
        r[3] = 0x80004005;
        break;
    case 47: // XamInputGetState → forward to XInput
        r[3] = kernel.getInputState((int)r[3], nullptr);
        break;
    case 49: // XamInputSetState → rumble stub
        r[3] = 0;
        break;

    default:
        LOGI("XAM: unimplemented ordinal %u", ordinal);
        r[3] = 0;
        break;
    }
}

} // namespace hle
} // namespace x360
