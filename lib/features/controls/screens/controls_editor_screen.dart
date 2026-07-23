import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class ControlsEditorScreen extends StatefulWidget {
  final String titleId;
  const ControlsEditorScreen({super.key, required this.titleId});

  @override
  State<ControlsEditorScreen> createState() => _ControlsEditorScreenState();
}

class _ControlsEditorScreenState extends State<ControlsEditorScreen> {
  bool _portrait = false;
  double _opacity = 0.7;
  bool _snapToGrid = true;
  double _buttonScale = 1.0;

  // Each control element with its position
  final Map<String, Offset> _positions = {
    'left_stick': const Offset(80, 400),
    'right_stick': const Offset(520, 420),
    'dpad': const Offset(60, 260),
    'abxy': const Offset(510, 240),
    'lb': const Offset(20, 60),
    'lt': const Offset(100, 50),
    'rb': const Offset(520, 50),
    'rt': const Offset(600, 60),
    'start': const Offset(340, 380),
    'back': const Offset(260, 380),
  };

  String? _dragging;
  String? _selected;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        children: [
          // Game background placeholder
          Positioned.fill(
            child: Container(
              color: const Color(0xFF0A0A0A),
              child: const Center(
                child: Text('Layout Editor\n(Game runs behind controls)',
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Colors.white12, fontSize: 14)),
              ),
            ),
          ),

          // Grid overlay when snap enabled
          if (_snapToGrid)
            Positioned.fill(
              child: CustomPaint(painter: _GridPainter()),
            ),

          // Draggable control elements
          for (final entry in _positions.entries)
            _buildDraggable(entry.key, entry.value),

          // Top toolbar
          Positioned(
            top: 0, left: 0, right: 0,
            child: Container(
              color: Colors.black87,
              padding: EdgeInsets.only(
                top: MediaQuery.of(context).padding.top + 8,
                bottom: 8, left: 12, right: 12,
              ),
              child: Row(
                children: [
                  IconButton(
                    icon: const Icon(Icons.arrow_back, color: Colors.white),
                    onPressed: () => context.pop(),
                  ),
                  const Text('Controls Editor', style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600, fontSize: 16)),
                  const Spacer(),
                  IconButton(
                    icon: Icon(_snapToGrid ? Icons.grid_on : Icons.grid_off, color: Colors.white54),
                    tooltip: 'Snap to grid',
                    onPressed: () => setState(() => _snapToGrid = !_snapToGrid),
                  ),
                  IconButton(
                    icon: const Icon(Icons.restore, color: Colors.white54),
                    tooltip: 'Reset layout',
                    onPressed: _resetLayout,
                  ),
                  TextButton(
                    onPressed: _saveLayout,
                    child: const Text('Save', style: TextStyle(color: Color(0xFF107C10), fontWeight: FontWeight.w700)),
                  ),
                ],
              ),
            ),
          ),

          // Bottom panel — selected element controls
          Positioned(
            bottom: 0, left: 0, right: 0,
            child: Container(
              color: Colors.black87,
              padding: const EdgeInsets.all(16),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Row(children: [
                    const Text('Opacity', style: TextStyle(color: Colors.white54, fontSize: 13)),
                    Expanded(child: Slider(
                      value: _opacity,
                      min: 0.1, max: 1.0,
                      onChanged: (v) => setState(() => _opacity = v),
                    )),
                    Text('${(_opacity * 100).round()}%', style: const TextStyle(color: Colors.white38, fontSize: 12)),
                  ]),
                  Row(children: [
                    const Text('Size', style: TextStyle(color: Colors.white54, fontSize: 13)),
                    Expanded(child: Slider(
                      value: _buttonScale,
                      min: 0.5, max: 2.0,
                      onChanged: (v) => setState(() => _buttonScale = v),
                    )),
                    Text('${(_buttonScale * 100).round()}%', style: const TextStyle(color: Colors.white38, fontSize: 12)),
                  ]),
                  Row(mainAxisAlignment: MainAxisAlignment.spaceEvenly, children: [
                    OutlinedButton(onPressed: () => setState(() => _portrait = !_portrait),
                      child: Text(_portrait ? 'Portrait' : 'Landscape')),
                    OutlinedButton(onPressed: () {}, child: const Text('Import Profile')),
                    OutlinedButton(onPressed: () {}, child: const Text('Export Profile')),
                  ]),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildDraggable(String key, Offset position) {
    final label = _labelFor(key);
    final isSelected = _selected == key;

    return Positioned(
      left: position.dx,
      top: position.dy,
      child: GestureDetector(
        onTap: () => setState(() => _selected = key),
        onPanUpdate: (d) {
          setState(() {
            var newPos = _positions[key]! + d.delta;
            if (_snapToGrid) {
              const grid = 16.0;
              newPos = Offset(
                (newPos.dx / grid).round() * grid,
                (newPos.dy / grid).round() * grid,
              );
            }
            _positions[key] = newPos;
          });
        },
        child: Opacity(
          opacity: _opacity,
          child: Transform.scale(
            scale: _buttonScale,
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              decoration: BoxDecoration(
                color: isSelected ? const Color(0xFF107C10).withOpacity(0.4) : Colors.white.withOpacity(0.15),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                  color: isSelected ? const Color(0xFF107C10) : Colors.white24,
                  width: isSelected ? 2 : 1,
                ),
              ),
              child: Text(label,
                style: TextStyle(color: isSelected ? Colors.white : Colors.white70,
                  fontSize: 12, fontWeight: FontWeight.w600)),
            ),
          ),
        ),
      ),
    );
  }

  String _labelFor(String key) => switch (key) {
    'left_stick' => '⬤ L',
    'right_stick' => '⬤ R',
    'dpad' => '✛',
    'abxy' => 'ABXY',
    'lb' => 'LB',
    'lt' => 'LT',
    'rb' => 'RB',
    'rt' => 'RT',
    'start' => '≡',
    'back' => '⧉',
    _ => key,
  };

  void _resetLayout() {
    setState(() {
      _positions.addAll({
        'left_stick': const Offset(80, 400),
        'right_stick': const Offset(520, 420),
        'dpad': const Offset(60, 260),
        'abxy': const Offset(510, 240),
        'lb': const Offset(20, 60),
        'lt': const Offset(100, 50),
        'rb': const Offset(520, 50),
        'rt': const Offset(600, 60),
        'start': const Offset(340, 380),
        'back': const Offset(260, 380),
      });
    });
  }

  void _saveLayout() {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Controls layout saved')),
    );
    context.pop();
  }
}

class _GridPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()..color = Colors.white.withOpacity(0.04)..strokeWidth = 0.5;
    const step = 16.0;
    for (double x = 0; x < size.width; x += step) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), paint);
    }
    for (double y = 0; y < size.height; y += step) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
    }
  }
  @override
  bool shouldRepaint(_) => false;
}
