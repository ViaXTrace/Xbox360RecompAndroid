import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../core/models/game_entry.dart';
import '../../../core/native/engine_bridge.dart';
import '../../library/providers/library_provider.dart';
import '../widgets/touch_controls_overlay.dart';
import '../widgets/metrics_overlay.dart';

class EmulationScreen extends ConsumerStatefulWidget {
  final String titleId;
  const EmulationScreen({super.key, required this.titleId});

  @override
  ConsumerState<EmulationScreen> createState() => _EmulationScreenState();
}

class _EmulationScreenState extends ConsumerState<EmulationScreen>
    with WidgetsBindingObserver {
  bool _running = false;
  bool _paused = false;
  bool _showOverlay = false;
  bool _showMetrics = false;
  Timer? _metricsTimer;
  double _fps = 0;
  double _frameTime = 0;
  int _ramMb = 0;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _enterImmersive();
    _startEmulation();
  }

  @override
  void dispose() {
    _metricsTimer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    _stopEmulation();
    _exitImmersive();
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused && _running) _pauseEmulation();
    if (state == AppLifecycleState.resumed && _paused) _resumeEmulation();
  }

  void _enterImmersive() {
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
  }

  void _exitImmersive() {
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
  }

  Future<void> _startEmulation() async {
    final game = ref.read(libraryProvider.notifier).getGame(widget.titleId);
    if (game == null) return;

    if (EngineBridge.isAvailable) {
      EngineBridge.engineInit(logLevel: 1);
      final rc = switch (game.format) {
        GameFormat.xex => EngineBridge.loadXex(game.execPath),
        GameFormat.iso => EngineBridge.loadIso(game.execPath),
        GameFormat.stfs => EngineBridge.loadStfs(game.execPath),
        _ => -1,
      };
      if (rc != 0 && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Failed to load game executable')),
        );
        context.pop();
        return;
      }
      EngineBridge.startJit(threadCount: 4);
    }

    setState(() => _running = true);
    ref.read(libraryProvider.notifier).updateLastPlayed(widget.titleId);

    // Start metrics polling
    _metricsTimer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      if (!mounted || !_showMetrics) return;
      setState(() {
        _fps = EngineBridge.getFps();
        _frameTime = EngineBridge.getFrameTime();
        _ramMb = EngineBridge.getRamUsage() ~/ (1024 * 1024);
      });
    });
  }

  void _pauseEmulation() {
    setState(() => _paused = true);
    // TODO: call native pause
  }

  void _resumeEmulation() {
    setState(() => _paused = false);
    // TODO: call native resume
  }

  void _stopEmulation() {
    _metricsTimer?.cancel();
    if (EngineBridge.isAvailable) {
      EngineBridge.stopJit();
      EngineBridge.unloadGame();
      EngineBridge.engineShutdown();
    }
    setState(() { _running = false; _paused = false; });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        children: [
          // Game surface — Vulkan renders here via AndroidView
          if (EngineBridge.isAvailable)
            const AndroidView(
              viewType: 'com.xbox360recomp/game_surface',
              creationParams: {},
              creationParamsCodec: StandardMessageCodec(),
            )
          else
            _buildStubSurface(),

          // Pause overlay
          if (_paused)
            _buildPauseOverlay(),

          // Touch controls
          TouchControlsOverlay(
            onButtonPressed: _onButton,
            onAxisChanged: _onAxis,
          ),

          // Metrics overlay
          if (_showMetrics)
            MetricsOverlay(fps: _fps, frameTime: _frameTime, ramMb: _ramMb),

          // Menu button (top-right, small)
          Positioned(
            top: 8, right: 8,
            child: GestureDetector(
              onTap: () => setState(() => _showOverlay = !_showOverlay),
              child: Container(
                width: 36, height: 36,
                decoration: BoxDecoration(
                  color: Colors.black38,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Icon(Icons.menu, size: 18, color: Colors.white54),
              ),
            ),
          ),

          // Dropdown menu
          if (_showOverlay)
            Positioned(
              top: 48, right: 8,
              child: _buildMenu(),
            ),
        ],
      ),
    );
  }

  Widget _buildStubSurface() {
    return Container(
      color: const Color(0xFF0D0D0D),
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Container(
              width: 80, height: 80,
              decoration: BoxDecoration(
                color: const Color(0xFF107C10).withOpacity(0.15),
                shape: BoxShape.circle,
              ),
              child: const Icon(Icons.sports_esports, size: 40, color: Color(0xFF107C10)),
            ),
            const SizedBox(height: 16),
            const Text('Native engine not available', style: TextStyle(color: Colors.white38)),
            const SizedBox(height: 8),
            const Text('(Build the NDK library to enable emulation)',
              style: TextStyle(color: Colors.white24, fontSize: 12)),
          ],
        ),
      ),
    );
  }

  Widget _buildPauseOverlay() {
    return Container(
      color: Colors.black54,
      child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.pause_circle, size: 72, color: Colors.white54),
            const SizedBox(height: 16),
            const Text('Paused', style: TextStyle(color: Colors.white, fontSize: 24, fontWeight: FontWeight.w700)),
            const SizedBox(height: 24),
            ElevatedButton(onPressed: _resumeEmulation, child: const Text('Resume')),
          ],
        ),
      ),
    );
  }

  Widget _buildMenu() {
    return Container(
      width: 200,
      decoration: BoxDecoration(
        color: const Color(0xFF1A1A1A),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.white12),
        boxShadow: const [BoxShadow(color: Colors.black54, blurRadius: 16)],
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _MenuItem(icon: Icons.pause, label: _paused ? 'Resume' : 'Pause',
            onTap: () { setState(() => _showOverlay = false); _paused ? _resumeEmulation() : _pauseEmulation(); }),
          _MenuItem(icon: Icons.bar_chart, label: _showMetrics ? 'Hide Metrics' : 'Show Metrics',
            onTap: () => setState(() { _showMetrics = !_showMetrics; _showOverlay = false; })),
          _MenuItem(icon: Icons.gamepad, label: 'Controls', onTap: () {
            setState(() => _showOverlay = false);
            context.push('/game/${widget.titleId}/controls');
          }),
          _MenuItem(icon: Icons.settings, label: 'Settings', onTap: () {
            setState(() => _showOverlay = false);
            context.push('/game/${widget.titleId}/settings');
          }),
          const Divider(height: 1, color: Colors.white10),
          _MenuItem(
            icon: Icons.stop_circle_outlined,
            label: 'Stop & Exit',
            color: Colors.redAccent,
            onTap: () { _stopEmulation(); context.pop(); },
          ),
        ],
      ),
    );
  }

  void _onButton(int pad, int buttons) {
    EngineBridge.setInputState(pad: pad, buttons: buttons);
  }

  void _onAxis(int pad, int lx, int ly, int rx, int ry) {
    EngineBridge.setInputState(pad: pad, buttons: 0, lx: lx, ly: ly, rx: rx, ry: ry);
  }
}

class _MenuItem extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback onTap;
  final Color? color;

  const _MenuItem({required this.icon, required this.label, required this.onTap, this.color});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      dense: true,
      leading: Icon(icon, size: 18, color: color ?? Colors.white70),
      title: Text(label, style: TextStyle(fontSize: 14, color: color ?? Colors.white)),
      onTap: onTap,
    );
  }
}
