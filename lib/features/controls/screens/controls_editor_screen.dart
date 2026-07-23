import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../../../core/theme/app_theme.dart';

/// Visual drag-and-drop touch controls layout editor.
class ControlsEditorScreen extends StatefulWidget {
  const ControlsEditorScreen({super.key});
  @override
  State<ControlsEditorScreen> createState() => _ControlsEditorScreenState();
}

class _ControlsEditorScreenState extends State<ControlsEditorScreen> {
  final Map<String, Offset> _positions = {
    'LS':    const Offset(0.08, 0.55),
    'DPAD':  const Offset(0.25, 0.65),
    'BACK':  const Offset(0.40, 0.80),
    'GUIDE': const Offset(0.50, 0.80),
    'START': const Offset(0.60, 0.80),
    'ABXY':  const Offset(0.80, 0.55),
    'RS':    const Offset(0.65, 0.65),
    'LB':    const Offset(0.08, 0.08),
    'RB':    const Offset(0.85, 0.08),
    'LT':    const Offset(0.18, 0.08),
    'RT':    const Offset(0.75, 0.08),
  };
  double _opacity = 0.7;
  double _scale = 1.0;
  String? _selected;

  void _enterFullscreen() {
    SystemChrome.setPreferredOrientations([DeviceOrientation.landscapeLeft, DeviceOrientation.landscapeRight]);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  }

  void _exitFullscreen() {
    SystemChrome.setPreferredOrientations([DeviceOrientation.portraitUp, DeviceOrientation.landscapeLeft, DeviceOrientation.landscapeRight]);
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
  }

  @override
  void initState() {
    super.initState();
    _enterFullscreen();
  }

  @override
  void dispose() {
    _exitFullscreen();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;

    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        children: [
          // Grid background
          CustomPaint(painter: _GridBg(), size: size),

          // Draggable controls
          for (final entry in _positions.entries)
            Positioned(
              left: entry.value.dx * size.width,
              top: entry.value.dy * size.height,
              child: GestureDetector(
                onTap: () => setState(() => _selected = _selected == entry.key ? null : entry.key),
                onPanUpdate: (d) {
                  setState(() {
                    final newX = ((entry.value.dx * size.width + d.delta.dx) / size.width).clamp(0.0, 0.95);
                    final newY = ((entry.value.dy * size.height + d.delta.dy) / size.height).clamp(0.0, 0.95);
                    _positions[entry.key] = Offset(newX, newY);
                  });
                },
                child: Opacity(
                  opacity: _opacity,
                  child: Transform.scale(
                    scale: _scale,
                    child: _ControlWidget(
                      id: entry.key,
                      selected: _selected == entry.key,
                    ),
                  ),
                ),
              ),
            ),

          // Top bar
          Positioned(
            top: 0, left: 0, right: 0,
            child: Container(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter, end: Alignment.bottomCenter,
                  colors: [Colors.black.withAlpha(200), Colors.transparent],
                ),
              ),
              child: Row(
                children: [
                  IconButton(
                    icon: const Icon(Icons.close_rounded, color: Colors.white70),
                    onPressed: () { _exitFullscreen(); Navigator.pop(context); },
                  ),
                  const Expanded(child: Text('Edit Controls', style: TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.w600))),
                  TextButton(
                    onPressed: _resetDefaults,
                    child: const Text('Reset', style: TextStyle(color: Colors.white54)),
                  ),
                  const SizedBox(width: 4),
                  ElevatedButton(
                    onPressed: () { _exitFullscreen(); Navigator.pop(context); },
                    style: ElevatedButton.styleFrom(backgroundColor: AppTheme.xboxGreen, foregroundColor: Colors.white, padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8)),
                    child: const Text('Done', style: TextStyle(fontWeight: FontWeight.w600)),
                  ),
                ],
              ),
            ),
          ),

          // Bottom controls bar
          Positioned(
            bottom: 0, left: 0, right: 0,
            child: Container(
              padding: const EdgeInsets.fromLTRB(20, 12, 20, 20),
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.bottomCenter, end: Alignment.topCenter,
                  colors: [Colors.black.withAlpha(220), Colors.transparent],
                ),
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Row(
                    children: [
                      const Text('Opacity', style: TextStyle(color: Colors.white54, fontSize: 12)),
                      Expanded(
                        child: Slider(
                          value: _opacity, min: 0.2, max: 1.0,
                          onChanged: (v) => setState(() => _opacity = v),
                        ),
                      ),
                      Text('${(_opacity * 100).round()}%', style: const TextStyle(color: Colors.white54, fontSize: 12)),
                    ],
                  ),
                  Row(
                    children: [
                      const Text('Scale', style: TextStyle(color: Colors.white54, fontSize: 12)),
                      Expanded(
                        child: Slider(
                          value: _scale, min: 0.6, max: 1.5,
                          onChanged: (v) => setState(() => _scale = v),
                        ),
                      ),
                      Text('${(_scale * 100).round()}%', style: const TextStyle(color: Colors.white54, fontSize: 12)),
                    ],
                  ),
                  const Text(
                    'Drag buttons to reposition • Tap to select',
                    style: TextStyle(color: Colors.white24, fontSize: 11),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  void _resetDefaults() {
    setState(() {
      _positions.addAll({
        'LS':    const Offset(0.08, 0.55),
        'DPAD':  const Offset(0.25, 0.65),
        'BACK':  const Offset(0.40, 0.80),
        'GUIDE': const Offset(0.50, 0.80),
        'START': const Offset(0.60, 0.80),
        'ABXY':  const Offset(0.80, 0.55),
        'RS':    const Offset(0.65, 0.65),
        'LB':    const Offset(0.08, 0.08),
        'RB':    const Offset(0.85, 0.08),
        'LT':    const Offset(0.18, 0.08),
        'RT':    const Offset(0.75, 0.08),
      });
    });
  }
}

class _ControlWidget extends StatelessWidget {
  final String id;
  final bool selected;
  const _ControlWidget({required this.id, required this.selected});

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: selected ? BoxDecoration(
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppTheme.xboxGreen, width: 2),
      ) : null,
      child: Container(
        padding: const EdgeInsets.all(8),
        child: _buildControl(id),
      ),
    );
  }

  Widget _buildControl(String id) {
    switch (id) {
      case 'ABXY':
        return _ControlLabel('A B\nX Y', AppTheme.xboxGreenLight, 48);
      case 'DPAD':
        return _ControlLabel('D-Pad', Colors.white60, 40);
      case 'LS': case 'RS':
        return _ControlLabel(id, Colors.white60, 36);
      case 'LB': case 'RB':
        return _ControlLabel(id, Colors.white60, 32);
      case 'LT': case 'RT':
        return _ControlLabel(id, AppTheme.xboxGreenLight, 32);
      case 'GUIDE':
        return _ControlLabel('𝕏', AppTheme.xboxGreen, 28);
      default:
        return _ControlLabel(id, Colors.white38, 30);
    }
  }
}

class _ControlLabel extends StatelessWidget {
  final String text;
  final Color color;
  final double size;
  const _ControlLabel(this.text, this.color, this.size);
  @override
  Widget build(BuildContext context) {
    return Container(
      width: size, height: size,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        color: color.withAlpha(25),
        border: Border.all(color: color.withAlpha(100)),
      ),
      child: Center(
        child: Text(text,
          style: TextStyle(color: color, fontSize: 9, fontWeight: FontWeight.w700),
          textAlign: TextAlign.center),
      ),
    );
  }
}

class _GridBg extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()..color = const Color(0xFF1A1A1A)..strokeWidth = 0.5;
    const step = 40.0;
    for (double x = 0; x < size.width; x += step) canvas.drawLine(Offset(x, 0), Offset(x, size.height), paint);
    for (double y = 0; y < size.height; y += step) canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
  }
  @override bool shouldRepaint(_) => false;
}
