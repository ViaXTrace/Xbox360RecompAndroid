# Xbox360RecompAndroid

<p align="center">
  <img src="docs/banner.png" alt="Xbox360RecompAndroid" width="600"/>
</p>

<p align="center">
  <a href="https://github.com/ViaXTrace/Xbox360RecompAndroid/releases"><img src="https://img.shields.io/github/v/release/ViaXTrace/Xbox360RecompAndroid?style=flat-square&label=latest%20release" alt="Latest Release"/></a>
  <a href="https://github.com/ViaXTrace/Xbox360RecompAndroid/actions"><img src="https://img.shields.io/github/actions/workflow/status/ViaXTrace/Xbox360RecompAndroid/build.yml?style=flat-square" alt="Build Status"/></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0-blue?style=flat-square" alt="License"/></a>
  <a href="https://github.com/ViaXTrace/Xbox360RecompAndroid/blob/main/compat/games.json"><img src="https://img.shields.io/badge/compatibility-database-green?style=flat-square" alt="Compat DB"/></a>
</p>

> **Universal Xbox 360 → Android recompiler** — dynamic PowerPC Xenon JIT + HLE kernel + Xenos→Vulkan GPU layer.  
> Flutter UI · C++20 NDK engine · arm64-v8a · Android 9+

---

## ⚠️ Disclaimer

This project does **not** include any game files, BIOS, dashboard, or proprietary Microsoft content. You must provide a legally obtained copy of any game you wish to run. Developed for preservation and reverse engineering research purposes. All JIT, HLE, and GPU layer code is written from scratch without using any proprietary Microsoft source code.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                Flutter UI (Dart)                    │
│  Game Library · Import · Settings · Touch Controls │
└────────────────────┬────────────────────────────────┘
                     │ dart:ffi / JNI
┌────────────────────▼────────────────────────────────┐
│              Native Engine (C++20 NDK)              │
├──────────┬──────────┬──────────┬────────────────────┤
│XEX/STFS  │ JIT Core │  Kernel  │    GPU Layer       │
│  Loader  │ PPC→ARM64│  HLE     │  Xenos→Vulkan      │
└──────────┴──────────┴──────────┴────────────────────┘
```

| Layer | Technology |
|---|---|
| UI | Flutter + Dart |
| JNI Bridge | C++20 via dart:ffi |
| JIT Engine | C++20, inline ARM64 codegen |
| GPU Layer | Vulkan 1.1+ |
| Adreno Driver | AdrenoTools (libadrenotools) |
| Exynos Driver | ExynosTools |
| Audio | AAudio (API ≥ 26) + OpenSL ES fallback |
| XMA2 Decode | FFmpeg xma2 codec |
| Build | CMake 3.22+ + Gradle 8+ + Flutter |

---

## Features

- 📦 **Universal XEX/ISO/STFS loader** — CON, PIRS, LIVE, ISO 9660/XDVDFS
- ⚡ **Dynamic PowerPC Xenon → ARM64 JIT** — SSA IR with DCE, constant folding, VMX128 → NEON
- 🖥️ **Kernel HLE** — memory, filesystem, threads, synchronization, XInput, XAudio2
- 🎮 **Xenos → Vulkan GPU** — PM4 parser, shader microcode → SPIR-V recompiler, texture detile
- 🔧 **Custom driver support** — Turnip (Adreno), ExynosTools (Samsung Xclipse)
- 📱 **Modern Flutter UI** — game library with artwork, per-game settings, touch controller editor
- 🕹️ **Gamepad + touch controls** — HID gamepads, dual-rumble, drag-and-drop touch layout editor
- 📡 **In-app updates** — auto-update via GitHub Releases API
- 🗃️ **Compatibility database** — community-maintained `compat/games.json`

---

## Requirements

- Android **9.0+** (API 28) — minimum
- Android **14+** (API 34) — recommended
- **ARM64** device (arm64-v8a ABI only)
- Vulkan **1.1+** support
- ~4GB RAM recommended

---

## Building

### Prerequisites

- Flutter SDK ≥ 3.19
- Android NDK r26+
- CMake 3.22+
- Android SDK with API 34

### Build APK

```bash
flutter build apk --release --target-platform android-arm64
```

### Build with GitHub Actions

Every push to `main` and every tag `v*` triggers the CI pipeline. Tags automatically create a GitHub Release with the signed APK attached.

---

## Compatibility Database

See [`compat/games.json`](compat/games.json) for the community compatibility list.

Status levels:
- ✅ **Playable** — completes the game with minor issues
- 🟡 **In-game** — reaches gameplay but has significant issues
- 🟠 **Boots** — shows title screen or menu
- ❌ **Nothing** — crashes on launch

---

## Roadmap

- [x] Phase 1 — XEX/STFS loader, basic JIT, HLE skeleton, Flutter UI
- [ ] Phase 2 — XMA2 audio, PM4/Vulkan pipeline, shader recompiler
- [ ] Phase 3 — Touch controls editor, gamepad API, per-game settings
- [ ] Phase 4 — AdrenoTools/ExynosTools, BC texture transcode
- [ ] Phase 5 — In-app updates, compat DB, community contributions

---

## References

| Project | Relevance |
|---|---|
| [SansNope/UnleashedRecomp-Android](https://github.com/SansNope/UnleashedRecomp-Android) | Direct reference: Android HLE + GPU layer |
| [xenia-project/xenia](https://github.com/xenia-project/xenia) | XEX loader, HLE, PM4 parser, shader recompiler |
| [Mr-Wiseguy/N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) | Universal recompilation pipeline architecture |
| [libadrenotools](https://github.com/K11MCH1/AdrenoToolsDrivers) | Custom Vulkan driver loading for Adreno |
| [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) | SPIR-V generation and optimization |
| [FFmpeg xma2 codec](https://ffmpeg.org/doxygen/trunk/xma2dec_8c.html) | XMA2 audio decoding |

---

## License

GPL-2.0 — see [LICENSE](LICENSE).
