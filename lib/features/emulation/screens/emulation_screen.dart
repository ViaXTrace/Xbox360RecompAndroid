import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../core/native/engine_bridge.dart';
import '../../../core/theme/app_theme.dart';
import '../../library/providers/library_provider.dart';
import '../widgets/metrics_overlay.dart';
import '../widgets/touch_controls_overlay.dart';

class EmulationScreen extends ConsumerStatefulWidget {
  final String titleId;
  const EmulationScreen({super.key, required this.titleId});

  @override
  ConsumerState<EmulationScreen> createState() => _EmulationScreenState();
}

class _EmulationScreenState extends ConsumerState<EmulationScreen>
    with WidgetsBindingObserver {
  bool _showControls = true;
  bool _showMetrics = true;
  bool _showMenu = false;
  bool _isRunning = false;
  bool _isPaused = false;
  String _statusMessage = 'Initializing…';
  Timer? _playtimeTimer;
  int _sessionSeconds = 0;
  Timer? _menuHideTimer;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _enterFullscreen();
    _startEmulation();
  }

  @override
  void dispose() {
    _playtimeTimer?.cancel();
    _menuHideTimer?.cancel();
    _stopEmulation();
    _exitFullscreen();
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused && _isRunning) {
      _pauseEmulation();
    } else if (state == AppLifecycleState.resumed && _isPaused) {
      _resumeEmulation();
    }
  }

  void _enterFullscreen() {
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  }

  void _exitFullscreen() {
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
  }

  Future<void> _startEmulation() async {
    setState(() => _statusMessage = 'Loading game…');
    final game = ref.read(libraryProvider).valueOrNull
        ?.firstWhere((g) => g.titleId == widget.titleId, orElse: () => throw Exception('Game not found'));

    if (game == null) {
      if (mounted) context.pop();
      return;
    }

    try {
      setState(() => _statusMessage = 'Parsing executable…');
      final bridge = EngineBridge.instance;
      int result = await compute((_) => bridge.loadGame(game.execPath, game.format), null);

      if (result != 0) {
        if (mounted) _showError('Failed to load game (error $result)');
        return;
      }

      setState(() => _statusMessage = 'Starting JIT engine…');
      result = await compute((_) => bridge.startJit(4), null);
      if (result != 0) {
        if (mounted) _showError('JIT engine failed to start (error $result)');
        return;
      }

      setState(() {
        _statusMessage = 'Running';
        _isRunning = true;
      });

      _playtimeTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        _sessionSeconds++;
      });
    } catch (e) {
      if (mounted) _showError('$e');
    }
  }

  void _pauseEmulation() {
    if (!_isRunning || _isPaused) return;
    EngineBridge.instance.pauseJit();
    setState(() => _isPaused = true);
    _playtimeTimer?.cancel();
  }

  void _resumeEmulation() {
    if (!_isPaused) return;
    EngineBridge.instance.resumeJit();
    setState(() => _isPaused = false);
    _playtimeTimer = Timer.periodic(const Duration(seconds: 1), (_) => _sessionSeconds++);
  }

  void _stopEmulation() {
    if (_isRunning) {
      EngineBridge.instance.stopJit();
      EngineBridge.instance.unloadGame();
      _savePlaytime();
    }
  }

  void _savePlaytime() {
    if (_sessionSeconds > 0) {
      ref.read(libraryProvider.notifier).addPlaytime(widget.titleId, _sessionSeconds);
    }
  }

  void _showError(String message) {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => AlertDialog(
        title: const Text('Emulation Error'),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () { Navigator.pop(context); context.pop(); },
            child: const Text('Back to Library'),
          ),
        ],
      ),
    );
  }

  void _toggleMenu() {
    setState(() => _showMenu = !_showMenu);
    if (_showMenu) {
      _menuHideTimer?.cancel();
      _menuHideTimer = Timer(const Duration(seconds: 5), () {
        if (mounted) setState(() => _showMenu = false);
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: GestureDetector(
        onTap: _toggleMenu,
        child: Stack(
          children: [
            // Native rendering surface
            const _GameSurface(),

            // Loading overlay
            if (!_isRunning)
              Container(
                color: Colors.black,
                child: Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const CircularProgressIndicator(color: AppTheme.xboxGreen, strokeWidth: 3),
                      const SizedBox(height: 20),
                      Text(_statusMessage,
                          style: const TextStyle(color: Colors.white70, fontSize: 15, fontWeight: FontWeight.w500)),
                    ],
                  ),
                ),
              ),

            // Paused overlay
            if (_isPaused)
              Container(
                color: Colors.black54,
                child: Center(
                  child: Container(
                    padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 20),
                    decoration: BoxDecoration(
                      color: AppTheme.surface,
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(color: AppTheme.border),
                    ),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const Icon(Icons.pause_circle_filled, size: 56, color: AppTheme.xboxGreen),
                        const SizedBox(height: 12),
                        const Text('Paused', style: TextStyle(color: Colors.white, fontSize: 20, fontWeight: FontWeight.w700)),
                        const SizedBox(height: 20),
                        ElevatedButton.icon(
                          onPressed: _resumeEmulation,
                          icon: const Icon(Icons.play_arrow_rounded),
                          label: const Text('Resume'),
                        ),
                      ],
                    ),
                  ),
                ),
              ),

            // Touch controls
            if (_isRunning && _showControls && !_isPaused)
              const TouchControlsOverlay(),

            // Metrics overlay
            if (_isRunning && _showMetrics)
              const Positioned(top: 12, left: 12, child: MetricsOverlay()),

            // In-game menu
            if (_showMenu)
              _InGameMenu(
                isPaused: _isPaused,
                showControls: _showControls,
                showMetrics: _showMetrics,
                onPause: () { _toggleMenu(); _pauseEmulation(); },
                onResume: () { _toggleMenu(); _resumeEmulation(); },
                onToggleControls: () => setState(() => _showControls = !_showControls),
                onToggleMetrics: () => setState(() => _showMetrics = !_showMetrics),
                onGameSettings: () {
                  _pauseEmulation();
                  context.push('/game-settings/${widget.titleId}');
                },
                onControlsEditor: () {
                  _pauseEmulation();
                  context.push('/controls-editor');
                },
                onQuit: () {
                  showDialog(
                    context: context,
                    builder: (_) => AlertDialog(
                      title: const Text('Quit Game?'),
                      content: const Text('Unsaved progress will be lost.'),
                      actions: [
                        TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
                        TextButton(
                          onPressed: () { Navigator.pop(context); context.pop(); },
                          style: TextButton.styleFrom(foregroundColor: Colors.red),
                          child: const Text('Quit'),
                        ),
                      ],
                    ),
                  );
                },
              ),
          ],
        ),
      ),
    );
  }
}

class _GameSurface extends StatelessWidget {
  const _GameSurface();
  @override
  Widget build(BuildContext context) {
    return const AndroidView(
      viewType: 'xbox360recomp/game_surface',
      layoutDirection: TextDirection.ltr,
    );
  }
}

class _InGameMenu extends StatelessWidget {
  final bool isPaused, showControls, showMetrics;
  final VoidCallback onPause, onResume, onToggleControls, onToggleMetrics;
  final VoidCallback onGameSettings, onControlsEditor, onQuit;

  const _InGameMenu({
    required this.isPaused, required this.showControls, required this.showMetrics,
    required this.onPause, required this.onResume, required this.onToggleControls,
    required this.onToggleMetrics, required this.onGameSettings,
    required this.onControlsEditor, required this.onQuit,
  });

  @override
  Widget build(BuildContext context) {
    return Positioned(
      top: 0, right: 0, bottom: 0,
      child: Container(
        width: 220,
        color: Colors.black87,
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('Menu', style: TextStyle(color: Colors.white70, fontSize: 11, fontWeight: FontWeight.w700, letterSpacing: 1.2)),
                const SizedBox(height: 16),
                if (!isPaused)
                  _MenuButton(icon: Icons.pause_rounded, label: 'Pause', onTap: onPause)
                else
                  _MenuButton(icon: Icons.play_arrow_rounded, label: 'Resume', color: AppTheme.xboxGreen, onTap: onResume),
                const SizedBox(height: 8),
                _MenuToggle(icon: Icons.gamepad_outlined, label: 'Touch Controls', value: showControls, onChanged: (_) => onToggleControls()),
                _MenuToggle(icon: Icons.speed_outlined, label: 'FPS Overlay', value: showMetrics, onChanged: (_) => onToggleMetrics()),
                const SizedBox(height: 8),
                _MenuButton(icon: Icons.tune_rounded, label: 'Game Settings', onTap: onGameSettings),
                _MenuButton(icon: Icons.gamepad_rounded, label: 'Edit Controls', onTap: onControlsEditor),
                const Spacer(),
                _MenuButton(icon: Icons.exit_to_app_rounded, label: 'Quit Game', color: Colors.red, onTap: onQuit),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _MenuButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color? color;
  final VoidCallback onTap;
  const _MenuButton({required this.icon, required this.label, this.color, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      dense: true,
      leading: Icon(icon, color: color ?? Colors.white70, size: 20),
      title: Text(label, style: TextStyle(color: color ?? Colors.white, fontSize: 13, fontWeight: FontWeight.w500)),
      onTap: onTap,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    );
  }
}

class _MenuToggle extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool value;
  final ValueChanged<bool> onChanged;
  const _MenuToggle({required this.icon, required this.label, required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      dense: true,
      leading: Icon(icon, color: Colors.white70, size: 20),
      title: Text(label, style: const TextStyle(color: Colors.white, fontSize: 13, fontWeight: FontWeight.w500)),
      trailing: Switch(value: value, onChanged: onChanged),
      onTap: () => onChanged(!value),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    );
  }
}

// Stub compute for bridge calls
Future<R> compute<Q, R>(R Function(Q) callback, Q message) async {
  return callback(message);
}
