import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'dart:io';
import 'package:flutter/foundation.dart';

import '../models/game_entry.dart';

/// Per-frame engine metrics snapshot.
class EngineMetrics {
  final double fps;
  final double frameTimeMs;
  final double cpuUsagePercent;
  final int ramUsageBytes;
  final double gpuTempCelsius;
  final int jitBlocksCompiled;

  const EngineMetrics({
    this.fps = 0,
    this.frameTimeMs = 0,
    this.cpuUsagePercent = 0,
    this.ramUsageBytes = 0,
    this.gpuTempCelsius = 0,
    this.jitBlocksCompiled = 0,
  });
}

/// dart:ffi bridge to the native Xbox360 recompiler engine (C++20 NDK).
/// Loads libxbox360recomp.so and exposes JNI entry points for:
///   - XEX/STFS/ISO loading, JIT lifecycle, HLE kernel state,
///   - GPU/Vulkan surface management, per-frame metrics.
class EngineBridge {
  static EngineBridge? _instance;
  static late DynamicLibrary _lib;
  static bool _initialized = false;

  /// Singleton accessor.
  static EngineBridge get instance {
    _instance ??= EngineBridge._();
    return _instance!;
  }
  EngineBridge._();

  // ─── Native function pointers ─────────────────────────────────────────────
  static late int Function(int logLevel) _engineInit;
  static late void Function() _engineShutdown;
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _xexLoad;
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _stfsLoad;
  static late int Function(Pointer<Utf8> path, Pointer<Utf8> outTitleId, int outLen) _isoLoad;
  static late void Function() _gameUnload;
  static late int Function(int threadCount) _jitStart;
  static late void Function() _jitStop;
  static late int Function() _jitGetState;
  static late void Function() _jitPause;
  static late void Function() _jitResume;
  static late int Function(Pointer<Void> nativeWindow) _gpuSetSurface;
  static late void Function() _gpuClearSurface;
  static late double Function() _metricsGetFps;
  static late double Function() _metricsGetFrameTime;
  static late int Function() _metricsGetRamUsage;
  static late double Function() _metricsGetCpuUsage;
  static late double Function() _metricsGetGpuTemp;
  static late int Function() _metricsGetJitBlocks;
  static late void Function(int width, int height) _settingsSetResolution;
  static late void Function(int af) _settingsSetAf;
  static late void Function(int aa) _settingsSetAa;
  static late void Function(int limit) _settingsSetFpsLimit;
  static late void Function(int threads) _settingsSetJitThreads;
  static late void Function(int pad, int buttons, int lx, int ly, int rx, int ry, int lt, int rt) _inputSetState;
  static late void Function(int pad, int lowFreq, int highFreq) _inputSetRumble;

  // ─── Input state ──────────────────────────────────────────────────────────
  int _buttonMask = 0;
  int _lx = 0, _ly = 0, _rx = 0, _ry = 0;
  int _lt = 0, _rt = 0;

  // ─── Initialization ───────────────────────────────────────────────────────
  static Future<void> initialize() async {
    if (_initialized) return;
    _instance = EngineBridge._();
    if (Platform.isAndroid) {
      try {
        _lib = DynamicLibrary.open('libxbox360recomp.so');
        _bindFunctions();
        _engineInit(1);
        _initialized = true;
        debugPrint('[EngineBridge] Native engine loaded');
      } catch (e) {
        debugPrint('[EngineBridge] Stub mode: $e');
      }
    } else {
      debugPrint('[EngineBridge] Non-Android — stub mode');
    }
  }

  static void _bindFunctions() {
    _engineInit = _lib.lookupFunction<Int32 Function(Int32), int Function(int)>('x360_engine_init');
    _engineShutdown = _lib.lookupFunction<Void Function(), void Function()>('x360_engine_shutdown');
    _xexLoad = _lib.lookupFunction<Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32), int Function(Pointer<Utf8>, Pointer<Utf8>, int)>('x360_xex_load');
    _stfsLoad = _lib.lookupFunction<Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32), int Function(Pointer<Utf8>, Pointer<Utf8>, int)>('x360_stfs_load');
    _isoLoad = _lib.lookupFunction<Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32), int Function(Pointer<Utf8>, Pointer<Utf8>, int)>('x360_iso_load');
    _gameUnload = _lib.lookupFunction<Void Function(), void Function()>('x360_game_unload');
    _jitStart = _lib.lookupFunction<Int32 Function(Int32), int Function(int)>('x360_jit_start');
    _jitStop = _lib.lookupFunction<Void Function(), void Function()>('x360_jit_stop');
    _jitGetState = _lib.lookupFunction<Int32 Function(), int Function()>('x360_jit_get_state');
    _jitPause = _lib.lookupFunction<Void Function(), void Function()>('x360_jit_pause');
    _jitResume = _lib.lookupFunction<Void Function(), void Function()>('x360_jit_resume');
    _gpuSetSurface = _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('x360_gpu_set_surface');
    _gpuClearSurface = _lib.lookupFunction<Void Function(), void Function()>('x360_gpu_clear_surface');
    _metricsGetFps = _lib.lookupFunction<Double Function(), double Function()>('x360_metrics_fps');
    _metricsGetFrameTime = _lib.lookupFunction<Double Function(), double Function()>('x360_metrics_frame_time');
    _metricsGetRamUsage = _lib.lookupFunction<Int64 Function(), int Function()>('x360_metrics_ram_bytes');
    _metricsGetCpuUsage = _lib.lookupFunction<Double Function(), double Function()>('x360_metrics_cpu_usage');
    _metricsGetGpuTemp = _lib.lookupFunction<Double Function(), double Function()>('x360_metrics_gpu_temp');
    _metricsGetJitBlocks = _lib.lookupFunction<Int32 Function(), int Function()>('x360_metrics_jit_blocks');
    _settingsSetResolution = _lib.lookupFunction<Void Function(Int32, Int32), void Function(int, int)>('x360_settings_resolution');
    _settingsSetAf = _lib.lookupFunction<Void Function(Int32), void Function(int)>('x360_settings_af');
    _settingsSetAa = _lib.lookupFunction<Void Function(Int32), void Function(int)>('x360_settings_aa');
    _settingsSetFpsLimit = _lib.lookupFunction<Void Function(Int32), void Function(int)>('x360_settings_fps_limit');
    _settingsSetJitThreads = _lib.lookupFunction<Void Function(Int32), void Function(int)>('x360_settings_jit_threads');
    _inputSetState = _lib.lookupFunction<Void Function(Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32), void Function(int, int, int, int, int, int, int, int)>('x360_input_set_state');
    _inputSetRumble = _lib.lookupFunction<Void Function(Int32, Int32, Int32), void Function(int, int, int)>('x360_input_rumble');
  }

  // ─── Instance API ─────────────────────────────────────────────────────────
  int loadGame(String path, GameFormat format) {
    if (!_initialized) return -1;
    final pathPtr = path.toNativeUtf8();
    final titleBuf = calloc<Uint8>(64).cast<Utf8>();
    try {
      return switch (format) {
        GameFormat.xex  => _xexLoad(pathPtr, titleBuf, 64),
        GameFormat.stfs => _stfsLoad(pathPtr, titleBuf, 64),
        GameFormat.iso  => _isoLoad(pathPtr, titleBuf, 64),
        _               => _xexLoad(pathPtr, titleBuf, 64),
      };
    } finally {
      calloc.free(pathPtr);
      calloc.free(titleBuf);
    }
  }

  int startJit([int threadCount = 4]) {
    if (!_initialized) return -1;
    return _jitStart(threadCount);
  }

  void pauseJit() {
    if (!_initialized) return;
    try { _jitPause(); } catch (_) {}
  }

  void resumeJit() {
    if (!_initialized) return;
    try { _jitResume(); } catch (_) {}
  }

  void stopJit() {
    if (!_initialized) return;
    _jitStop();
  }

  void unloadGame() {
    if (!_initialized) return;
    _gameUnload();
  }

  void buttonDown(int btn) { _buttonMask |= btn; _flushInput(); }
  void buttonUp(int btn)   { _buttonMask &= ~btn; _flushInput(); }

  void setAxis(int axis, double x, double y) {
    final ix = (x * 32767).round().clamp(-32768, 32767);
    final iy = (y * 32767).round().clamp(-32768, 32767);
    if (axis == 0) { _lx = ix; _ly = iy; } else { _rx = ix; _ry = iy; }
    _flushInput();
  }

  void setTrigger(int trigger, double value) {
    final iv = (value * 255).round().clamp(0, 255);
    if (trigger == 0) { _lt = iv; } else { _rt = iv; }
    _flushInput();
  }

  void _flushInput() {
    if (!_initialized) return;
    _inputSetState(0, _buttonMask, _lx, _ly, _rx, _ry, _lt, _rt);
  }

  EngineMetrics getMetrics() {
    if (!_initialized) return const EngineMetrics();
    return EngineMetrics(
      fps: _metricsGetFps(),
      frameTimeMs: _metricsGetFrameTime() * 1000,
      cpuUsagePercent: _metricsGetCpuUsage() * 100,
      ramUsageBytes: _metricsGetRamUsage(),
      gpuTempCelsius: _metricsGetGpuTemp(),
      jitBlocksCompiled: _metricsGetJitBlocks(),
    );
  }

  // ─── Static convenience shims ─────────────────────────────────────────────
  static double getFps()       => _initialized ? _metricsGetFps() : 0.0;
  static double getFrameTime() => _initialized ? _metricsGetFrameTime() * 1000 : 0.0;
  static int getRamUsage()     => _initialized ? _metricsGetRamUsage() : 0;
  static double getGpuTemp()   => _initialized ? _metricsGetGpuTemp() : 0.0;

  static void shutdown() { if (_initialized) _engineShutdown(); }
}
