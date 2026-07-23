import 'package:flutter/material.dart';

/// XInput button bitmask constants (matches Xbox 360 XInput spec)
class XButton {
  static const dpadUp = 0x0001;
  static const dpadDown = 0x0002;
  static const dpadLeft = 0x0004;
  static const dpadRight = 0x0008;
  static const start = 0x0010;
  static const back = 0x0020;
  static const ls = 0x0040;
  static const rs = 0x0080;
  static const lb = 0x0100;
  static const rb = 0x0200;
  static const a = 0x1000;
  static const b = 0x2000;
  static const x = 0x4000;
  static const y = 0x8000;
}

typedef ButtonCallback = void Function(int pad, int buttons);
typedef AxisCallback = void Function(int pad, int lx, int ly, int rx, int ry);

class TouchControlsOverlay extends StatefulWidget {
  final ButtonCallback onButtonPressed;
  final AxisCallback onAxisChanged;

  const TouchControlsOverlay({
    super.key,
    required this.onButtonPressed,
    required this.onAxisChanged,
  });

  @override
  State<TouchControlsOverlay> createState() => _TouchControlsOverlayState();
}

class _TouchControlsOverlayState extends State<TouchControlsOverlay> {
  int _buttons = 0;
  Offset _leftStick = Offset.zero;
  Offset _rightStick = Offset.zero;

  void _pressButton(int btn) {
    _buttons |= btn;
    widget.onButtonPressed(0, _buttons);
  }

  void _releaseButton(int btn) {
    _buttons &= ~btn;
    widget.onButtonPressed(0, _buttons);
  }

  void _updateLeftStick(Offset o) {
    _leftStick = o;
    widget.onAxisChanged(0,
      (_leftStick.dx * 32767).toInt(), (_leftStick.dy * 32767).toInt(),
      (_rightStick.dx * 32767).toInt(), (_rightStick.dy * 32767).toInt());
  }

  void _updateRightStick(Offset o) {
    _rightStick = o;
    widget.onAxisChanged(0,
      (_leftStick.dx * 32767).toInt(), (_leftStick.dy * 32767).toInt(),
      (_rightStick.dx * 32767).toInt(), (_rightStick.dy * 32767).toInt());
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    return Stack(
      children: [
        // Left analog stick
        Positioned(
          left: 40, bottom: 60,
          child: _AnalogStick(
            radius: 50,
            onChanged: _updateLeftStick,
          ),
        ),
        // Right analog stick
        Positioned(
          right: 140, bottom: 40,
          child: _AnalogStick(
            radius: 50,
            onChanged: _updateRightStick,
          ),
        ),
        // D-Pad
        Positioned(
          left: 16, bottom: 160,
          child: _DPad(onPress: _pressButton, onRelease: _releaseButton),
        ),
        // ABXY buttons
        Positioned(
          right: 16, bottom: 80,
          child: _ABXYCluster(onPress: _pressButton, onRelease: _releaseButton),
        ),
        // LB / LT
        Positioned(
          left: 16, top: 16,
          child: Row(
            children: [
              _ShoulderButton(label: 'LB', onPress: () => _pressButton(XButton.lb), onRelease: () => _releaseButton(XButton.lb)),
              const SizedBox(width: 8),
              _ShoulderButton(label: 'LT', onPress: () => _pressButton(0x01000000), onRelease: () => _releaseButton(0x01000000)),
            ],
          ),
        ),
        // RB / RT
        Positioned(
          right: 16, top: 16,
          child: Row(
            children: [
              _ShoulderButton(label: 'RT', onPress: () => _pressButton(0x02000000), onRelease: () => _releaseButton(0x02000000)),
              const SizedBox(width: 8),
              _ShoulderButton(label: 'RB', onPress: () => _pressButton(XButton.rb), onRelease: () => _releaseButton(XButton.rb)),
            ],
          ),
        ),
        // Start / Back
        Positioned(
          left: size.width / 2 - 50, bottom: 16,
          child: Row(
            children: [
              _SmallButton(label: 'BACK', onPress: () => _pressButton(XButton.back), onRelease: () => _releaseButton(XButton.back)),
              const SizedBox(width: 12),
              _SmallButton(label: 'START', onPress: () => _pressButton(XButton.start), onRelease: () => _releaseButton(XButton.start)),
            ],
          ),
        ),
      ],
    );
  }
}

class _AnalogStick extends StatefulWidget {
  final double radius;
  final ValueChanged<Offset> onChanged;
  const _AnalogStick({required this.radius, required this.onChanged});

  @override
  State<_AnalogStick> createState() => _AnalogStickState();
}

class _AnalogStickState extends State<_AnalogStick> {
  Offset _pos = Offset.zero;
  Offset? _origin;

  void _onPanStart(DragStartDetails d) => _origin = d.localPosition;

  void _onPanUpdate(DragUpdateDetails d) {
    if (_origin == null) return;
    final delta = d.localPosition - _origin!;
    final maxR = widget.radius * 0.6;
    final clamped = delta.distance > maxR
        ? Offset.fromDirection(delta.direction, maxR)
        : delta;
    final normalized = Offset(clamped.dx / maxR, clamped.dy / maxR);
    setState(() => _pos = clamped);
    widget.onChanged(normalized);
  }

  void _onPanEnd(DragEndDetails _) {
    setState(() => _pos = Offset.zero);
    _origin = null;
    widget.onChanged(Offset.zero);
  }

  @override
  Widget build(BuildContext context) {
    final r = widget.radius;
    return GestureDetector(
      onPanStart: _onPanStart,
      onPanUpdate: _onPanUpdate,
      onPanEnd: _onPanEnd,
      child: SizedBox(
        width: r * 2, height: r * 2,
        child: CustomPaint(painter: _StickPainter(_pos, r)),
      ),
    );
  }
}

class _StickPainter extends CustomPainter {
  final Offset pos;
  final double r;
  _StickPainter(this.pos, this.r);

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(r, r);
    canvas.drawCircle(center, r, Paint()..color = Colors.white.withOpacity(0.12));
    canvas.drawCircle(center, r, Paint()..color = Colors.white.withOpacity(0.25)..style = PaintingStyle.stroke..strokeWidth = 1.5);
    canvas.drawCircle(center + pos, r * 0.38, Paint()..color = Colors.white.withOpacity(0.55));
  }

  @override
  bool shouldRepaint(_StickPainter old) => old.pos != pos;
}

class _DPad extends StatelessWidget {
  final void Function(int) onPress;
  final void Function(int) onRelease;
  const _DPad({required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 100, height: 100,
      child: Stack(
        children: [
          Positioned(top: 0, left: 30, child: _DPadBtn('↑', () => onPress(XButton.dpadUp), () => onRelease(XButton.dpadUp))),
          Positioned(bottom: 0, left: 30, child: _DPadBtn('↓', () => onPress(XButton.dpadDown), () => onRelease(XButton.dpadDown))),
          Positioned(top: 30, left: 0, child: _DPadBtn('←', () => onPress(XButton.dpadLeft), () => onRelease(XButton.dpadLeft))),
          Positioned(top: 30, right: 0, child: _DPadBtn('→', () => onPress(XButton.dpadRight), () => onRelease(XButton.dpadRight))),
        ],
      ),
    );
  }
}

class _DPadBtn extends StatelessWidget {
  final String label;
  final VoidCallback onPress, onRelease;
  const _DPadBtn(this.label, this.onPress, this.onRelease);

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => onPress(),
      onTapUp: (_) => onRelease(),
      onTapCancel: onRelease,
      child: Container(
        width: 36, height: 36,
        decoration: BoxDecoration(
          color: Colors.white.withOpacity(0.15),
          borderRadius: BorderRadius.circular(6),
        ),
        child: Center(child: Text(label, style: const TextStyle(color: Colors.white70, fontSize: 16))),
      ),
    );
  }
}

class _ABXYCluster extends StatelessWidget {
  final void Function(int) onPress;
  final void Function(int) onRelease;
  const _ABXYCluster({required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 110, height: 110,
      child: Stack(
        children: [
          Positioned(top: 0, left: 36, child: _XBtn('Y', const Color(0xFFFFD700), XButton.y, onPress, onRelease)),
          Positioned(bottom: 0, left: 36, child: _XBtn('A', const Color(0xFF107C10), XButton.a, onPress, onRelease)),
          Positioned(top: 36, left: 0, child: _XBtn('X', const Color(0xFF0078D7), XButton.x, onPress, onRelease)),
          Positioned(top: 36, right: 0, child: _XBtn('B', const Color(0xFFD32F2F), XButton.b, onPress, onRelease)),
        ],
      ),
    );
  }
}

class _XBtn extends StatelessWidget {
  final String label;
  final Color color;
  final int btn;
  final void Function(int) onPress;
  final void Function(int) onRelease;
  const _XBtn(this.label, this.color, this.btn, this.onPress, this.onRelease);

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => onPress(btn),
      onTapUp: (_) => onRelease(btn),
      onTapCancel: () => onRelease(btn),
      child: Container(
        width: 36, height: 36,
        decoration: BoxDecoration(shape: BoxShape.circle, color: color.withOpacity(0.75)),
        child: Center(child: Text(label, style: const TextStyle(color: Colors.white, fontWeight: FontWeight.w700, fontSize: 14))),
      ),
    );
  }
}

class _ShoulderButton extends StatelessWidget {
  final String label;
  final VoidCallback onPress, onRelease;
  const _ShoulderButton({required this.label, required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => onPress(),
      onTapUp: (_) => onRelease(),
      onTapCancel: onRelease,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
        decoration: BoxDecoration(
          color: Colors.white.withOpacity(0.15),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: Colors.white24),
        ),
        child: Text(label, style: const TextStyle(color: Colors.white70, fontSize: 12, fontWeight: FontWeight.w600)),
      ),
    );
  }
}

class _SmallButton extends StatelessWidget {
  final String label;
  final VoidCallback onPress, onRelease;
  const _SmallButton({required this.label, required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => onPress(),
      onTapUp: (_) => onRelease(),
      onTapCancel: onRelease,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: Colors.white.withOpacity(0.10),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: Colors.white12),
        ),
        child: Text(label, style: const TextStyle(color: Colors.white54, fontSize: 10, fontWeight: FontWeight.w600)),
      ),
    );
  }
}
