import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'dart:io';
import 'dart:isolate';
import 'package:flutter/foundation.dart';

/// dart:ffi bridge to the native Xbox360 recompiler engine (C++20 NDK).
/// Loads libxbox360recomp.so and exposes JNI entry points for:
///   - XEX/STFS/ISO loading
///   - JIT engine lifecycle
///   - HLE kernel state
///   - GPU/Vulkan surface management
///   - Per-frame metrics (FPS, frame time, memory)
class EngineBridge {
  static EngineBridge? _instance;
  static late DynamicLibrary _lib;
  static bool _initialized = false;

  // ─── Native function typedefs ───────────────────────────────────────────────

  // Engine lifecycle
  static late int Function(int logLevel) _engineInit;
  static late void Function() _engineShutdown;

  // XEX / game loading
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _xexLoad;
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _stfsLoad;
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _isoLoad;
  static late void Function() _gameUnload;

  // JIT control
  static late int Function(int threadCount) _jitStart;
  static late void Function() _jitStop;
  static late int Function() _jitGetState;

  // GPU surface
  static late int Function(Pointer<Void> nativeWindow) _gpuSetSurface;
  static late void Function() _gpuClearSurface;

  // Metrics
  static late double Function() _metricsGetFps;
  static late double Function() _metricsGetFrameTime;
  static late int Function() _metricsGetRamUsage;
  static late double Function() _metricsGetCpuTemp;
  static late double Function() _metricsGetGpuTemp;

  // Settings
  static late void Function(int width, int height) _settingsSetResolution;
  static late void Function(int af) _settingsSetAf;
  static late void Function(int aa) _settingsSetAa;
  static late void Function(int limit) _settingsSetFpsLimit;
  static late void Function(int threads) _settingsSetJitThreads;

  // Input
  static late void Function(int pad, int buttons, int lx, int ly, int rx, int ry, int lt, int rt) _inputSetState;
  static late void Function(int pad, int lowFreq, int highFreq) _inputSetRumble;

  // Saves
  static late int Function(Pointer<Utf8> outPath, int outLen) _savesGetDir;
  static late int Function(Pointer<Utf8> titleId) _savesBackup;

  // Logs
  static late int Function(Pointer<Utf8> outBuf, int outLen) _logsExport;

  static Future<void> initialize() async {
    if (_initialized) return;

    if (Platform.isAndroid) {
      try {
        _lib = DynamicLibrary.open('libxbox360recomp.so');
        _bindFunctions();
        _initialized = true;
        debugPrint('[EngineBridge] Native engine loaded successfully');
      } catch (e) {
        debugPrint('[EngineBridge] WARNING: Native engine not available: $e');
        // Running in stub mode (e.g. during UI development)
        _initialized = false;
      }
    } else {
      debugPrint('[EngineBridge] Non-Android platform — stub mode');
    }
  }

  static void _bindFunctions() {
    _engineInit = _lib.lookupFunction<Int32 Function(Int32), int Function(int)>('x360_engine_init');
    _engineShutdown = _lib.lookupFunction<Void Function(), void Function()>('x360_engine_shutdown');

    _xexLoad = _lib.lookupFunction<
      Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32),
      int Function(Pointer<Utf8>, Pointer<Utf8>, int)
    >('x360_xex_load');

    _stfsLoad = _lib.lookupFunction<
      Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32),
      int Function(Pointer<Utf8>, Pointer<Utf8>, int)
    >('x360_stfs_load');

    _isoLoad = _lib.lookupFunction<
      Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32),
      int Function(Pointer<Utf8>, Pointer<Utf8>, int)
    >('x360_iso_load');

    _gameUnload = _lib.lookupFunction<Void Function(), void Function()>('x360_game_unload');

    _jitStart = _lib.lookupFunction<Int32 Function(Int32), int Function(int)>('x360_jit_start');
    _jitStop = _lib.lookupFunction<Void Function(), void Function()>('x360_jit_stop');
    _jitGetState = _lib.lookupFunction<Int32 Function(), int Function()>('x360_jit_get_state');

    _gpuSetSurface = _lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)
    >('x360_gpu_set_surface');

    _gpuClearSurface = _lib.lookupFunction<Void Function(), void Function()>('x360_gpu_clear_surface');

    _metricsGetFps = _lib.lookupFunction<Float Function(), double Function()>('x360_metrics_fps');
    _metricsGetFrameTime = _lib.lookupFunction<Float Function(), double Function()>('x360_metrics_frame_time');
    _metricsGetRamUsage = _lib.lookupFunction<Int64 Function(), int Function()>('x360_metrics_ram');
    _metricsGetCpuTemp = _lib.lookupFunction<Float Function(), double Function()>('x360_metrics_cpu_temp');
    _metricsGetGpuTemp = _lib.lookupFunction<Float Function(), double Function()>('x360_metrics_gpu_temp');

    _inputSetState = _lib.lookupFunction<
      Void Function(Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32),
      void Function(int, int, int, int, int, int, int, int)
    >('x360_input_set_state');

    _inputSetRumble = _lib.lookupFunction<
      Void Function(Int32, Int32, Int32),
      void Function(int, int, int)
    >('x360_input_set_rumble');

    _logsExport = _lib.lookupFunction<
      Int32 Function(Pointer<Utf8>, Int32),
      int Function(Pointer<Utf8>, int)
    >('x360_logs_export');
  }

  // ─── Public API ────────────────────────────────────────────────────────────

  static bool get isAvailable => _initialized;

  static int engineInit({int logLevel = 1}) {
    if (!_initialized) return -1;
    return _engineInit(logLevel);
  }

  static void engineShutdown() {
    if (!_initialized) return;
    _engineShutdown();
  }

  static int loadXex(String path) {
    if (!_initialized) return -1;
    final pathPtr = path.toNativeUtf8();
    final titleBuf = calloc<Utf8>(64);
    try {
      return _xexLoad(pathPtr, titleBuf, 64);
    } finally {
      calloc.free(pathPtr);
      calloc.free(titleBuf);
    }
  }

  static int loadStfs(String path) {
    if (!_initialized) return -1;
    final pathPtr = path.toNativeUtf8();
    final titleBuf = calloc<Utf8>(64);
    try {
      return _stfsLoad(pathPtr, titleBuf, 64);
    } finally {
      calloc.free(pathPtr);
      calloc.free(titleBuf);
    }
  }

  static int loadIso(String path) {
    if (!_initialized) return -1;
    final pathPtr = path.toNativeUtf8();
    final titleBuf = calloc<Utf8>(64);
    try {
      return _isoLoad(pathPtr, titleBuf, 64);
    } finally {
      calloc.free(pathPtr);
      calloc.free(titleBuf);
    }
  }

  static void unloadGame() {
    if (!_initialized) return;
    _gameUnload();
  }

  static int startJit({int threadCount = 4}) {
    if (!_initialized) return -1;
    return _jitStart(threadCount);
  }

  static void stopJit() {
    if (!_initialized) return;
    _jitStop();
  }

  static void setInputState({
    required int pad,
    required int buttons,
    int lx = 0, int ly = 0,
    int rx = 0, int ry = 0,
    int lt = 0, int rt = 0,
  }) {
    if (!_initialized) return;
    _inputSetState(pad, buttons, lx, ly, rx, ry, lt, rt);
  }

  static void setRumble(int pad, int lowFreq, int highFreq) {
    if (!_initialized) return;
    _inputSetRumble(pad, lowFreq, highFreq);
  }

  static double getFps() => _initialized ? _metricsGetFps() : 0.0;
  static double getFrameTime() => _initialized ? _metricsGetFrameTime() : 0.0;
  static int getRamUsage() => _initialized ? _metricsGetRamUsage() : 0;
  static double getCpuTemp() => _initialized ? _metricsGetCpuTemp() : 0.0;
  static double getGpuTemp() => _initialized ? _metricsGetGpuTemp() : 0.0;
}

/// Allocate native UTF-8 string
extension StringExtNative on String {
  Pointer<Utf8> toNativeUtf8() {
    final units = codeUnits;
    final buf = calloc<Uint8>(units.length + 1);
    for (var i = 0; i < units.length; i++) {
      buf[i] = units[i];
    }
    buf[units.length] = 0;
    return buf.cast<Utf8>();
  }
}
