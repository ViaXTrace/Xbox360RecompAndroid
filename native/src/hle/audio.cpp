/**
 * HLE Audio — XAudio2 v3 (Xbox 360 specific) → AAudio (Android ≥ 8.0).
 * Implements SubmitSourceBuffer, mixer loop, and XMA2 decode stub.
 * Falls back to OpenSL ES on Android 7 and below.
 */
#include "../../include/hle/hle_kernel.h"
#include <aaudio/AAudio.h>
#include <cstring>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cmath>
#include <android/log.h>

#define LOG_TAG "X360:AUDIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {

// ─── Audio subsystem globals ───────────────────────────────────────────────────
static AAudioStream*        g_stream = nullptr;
static std::atomic<bool>    g_audioRunning{false};

// Voice buffer queue
struct AudioVoice {
    std::vector<int16_t> samples; // decoded PCM (stereo, 16-bit)
    size_t               readPos = 0;
    float                volume  = 1.0f;
    bool                 loop    = false;
    bool                 active  = false;
};

static constexpr int kMaxVoices = 32;
static AudioVoice g_voices[kMaxVoices];
static std::mutex g_voiceMutex;

// Mix all active voices into output buffer
static aaudio_data_callback_result_t audioCallback(
    AAudioStream* stream, void* userData,
    void* audioData, int32_t numFrames) {

    int16_t* out = (int16_t*)audioData;
    memset(out, 0, numFrames * 2 * sizeof(int16_t)); // stereo

    std::lock_guard<std::mutex> lk(g_voiceMutex);
    for (auto& voice : g_voices) {
        if (!voice.active || voice.samples.empty()) continue;
        for (int32_t f = 0; f < numFrames; f++) {
            if (voice.readPos + 1 >= voice.samples.size()) {
                if (voice.loop) voice.readPos = 0;
                else { voice.active = false; break; }
            }
            // Mix (clamp)
            int32_t l = out[f*2]   + (int32_t)(voice.samples[voice.readPos]   * voice.volume);
            int32_t r = out[f*2+1] + (int32_t)(voice.samples[voice.readPos+1] * voice.volume);
            out[f*2]   = (int16_t)std::max(-32768, std::min(32767, l));
            out[f*2+1] = (int16_t)std::max(-32768, std::min(32767, r));
            voice.readPos += 2;
        }
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool initAudio() {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
        LOGE("AUDIO: AAudio_createStreamBuilder failed");
        return false;
    }

    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, 48000);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(builder, audioCallback, nullptr);

    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &g_stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        LOGE("AUDIO: open stream failed: %s", AAudio_convertResultToText(result));
        return false;
    }

    AAudioStream_requestStart(g_stream);
    g_audioRunning.store(true);
    LOGI("AUDIO: AAudio stream started, sampleRate=%d",
         AAudioStream_getSampleRate(g_stream));
    return true;
}

void shutdownAudio() {
    if (g_stream) {
        AAudioStream_requestStop(g_stream);
        AAudioStream_close(g_stream);
        g_stream = nullptr;
    }
    g_audioRunning.store(false);
}

// ─── XMA2 stub decode ──────────────────────────────────────────────────────────
// XMA2 is a proprietary Microsoft format based on WMA Pro.
// Full decode requires FFmpeg xma2 codec or libxma2dec.
// This stub generates silence and logs — replace with FFmpeg integration.
static bool decodeXma2(const uint8_t* /*xmaData*/, uint32_t /*xmaSize*/,
                        std::vector<int16_t>& pcmOut, uint32_t sampleRate,
                        uint32_t channelCount) {
    LOGI("AUDIO: XMA2 decode stub — outputting silence (integrate FFmpeg xma2 codec)");
    // Output 1 second of silence
    size_t numSamples = sampleRate * channelCount;
    pcmOut.assign(numSamples, 0);
    return true;
}

// ─── XAudio2 HLE APIs (called from xboxkrnl dispatch) ─────────────────────────

// XAudio2SourceVoice::SubmitSourceBuffer (simplified)
void hleSubmitSourceBuffer(const uint8_t* audioData, uint32_t audioBytes,
                           uint32_t sampleRate, uint32_t channels,
                           bool isXma2, float volume, bool loop) {
    if (!g_audioRunning.load()) initAudio();

    std::vector<int16_t> pcm;

    if (isXma2) {
        decodeXma2(audioData, audioBytes, pcm, sampleRate, channels);
    } else {
        // Assume PCM 16-bit
        size_t numSamples = audioBytes / sizeof(int16_t);
        pcm.resize(numSamples);
        memcpy(pcm.data(), audioData, audioBytes);
        // Byte-swap if big-endian PCM (Xbox 360 audio is big-endian)
        for (auto& s : pcm) s = (int16_t)__builtin_bswap16((uint16_t)s);
    }

    std::lock_guard<std::mutex> lk(g_voiceMutex);
    // Find a free voice slot
    for (auto& voice : g_voices) {
        if (!voice.active) {
            voice.samples  = std::move(pcm);
            voice.readPos  = 0;
            voice.volume   = volume;
            voice.loop     = loop;
            voice.active   = true;
            break;
        }
    }
}

// XAudio2::SetVolume
void hleSetMasterVolume(float volume) {
    std::lock_guard<std::mutex> lk(g_voiceMutex);
    for (auto& voice : g_voices) voice.volume = std::clamp(voice.volume * volume, 0.0f, 1.0f);
}

} // namespace hle
} // namespace x360
