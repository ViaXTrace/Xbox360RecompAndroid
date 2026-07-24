import 'package:flutter/material.dart';
import '../../../core/native/engine_bridge.dart';
import '../../../core/theme/app_theme.dart';

/// Full Xbox 360 touch controller overlay.
/// Button IDs map to XInput bitmask values.
class TouchControlsOverlay extends StatefulWidget {
  const TouchControlsOverlay({super.key});
  @override
  State<TouchControlsOverlay> createState() => _TouchControlsOverlayState();
}

class _TouchControlsOverlayState extends State<TouchControlsOverlay> {
  double _lsX = 0, _lsY = 0;
  double _rsX = 0, _rsY = 0;
  double _ltValue = 0, _rtValue = 0;

  void _press(int btn) => EngineBridge.instance.buttonDown(btn);
  void _release(int btn) => EngineBridge.instance.buttonUp(btn);
  void _setLeftStick(double x, double y) {
    setState(() { _lsX = x; _lsY = y; });
    EngineBridge.instance.setAxis(0, x, y);
  }
  void _setRightStick(double x, double y) {
    setState(() { _rsX = x; _rsY = y; });
    EngineBridge.instance.setAxis(1, x, y);
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    return Stack(
      children: [
        // ─ Left thumbstick ──────────────────────────
        Positioned(
          left: 30, bottom: 90,
          child: _Thumbstick(x: _lsX, y: _lsY, onMove: _setLeftStick, label: 'LS'),
        ),
        // ─ D-Pad ─────────────────────────────────────
        Positioned(
          left: 155, bottom: 50,
          child: _DPad(onPress: _press, onRelease: _release),
        ),
        // ─ Left shoulder / trigger ───────────────────
        Positioned(
          left: 24, top: 14,
          child: Column(children: [
            _TriggerButton(
              label: 'LT',
              value: _ltValue,
              onDrag: (v) {
                setState(() => _ltValue = v);
                EngineBridge.instance.setTrigger(0, v);
              },
            ),
            _ShoulderButton(label: 'LB', onPress: () => _press(0x0100), onRelease: () => _release(0x0100)),
          ]),
        ),
        // ─ Right shoulder / trigger ──────────────────
        Positioned(
          right: 24, top: 14,
          child: Column(children: [
            _TriggerButton(
              label: 'RT',
              value: _rtValue,
              onDrag: (v) {
                setState(() => _rtValue = v);
                EngineBridge.instance.setTrigger(1, v);
              },
            ),
            _ShoulderButton(label: 'RB', onPress: () => _press(0x0200), onRelease: () => _release(0x0200)),
          ]),
        ),
        // ─ ABXY buttons ──────────────────────────────
        Positioned(
          right: 30, bottom: 60,
          child: _ABXYCluster(onPress: _press, onRelease: _release),
        ),
        // ─ Right thumbstick ──────────────────────────
        Positioned(
          right: 160, bottom: 90,
          child: _Thumbstick(x: _rsX, y: _rsY, onMove: _setRightStick, label: 'RS'),
        ),
        // ─ Start / Back / Guide ──────────────────────
        Positioned(
          left: size.width / 2 - 90, bottom: 24,
          child: Row(
            children: [
              _SmallButton(label: 'BACK', onPress: () => _press(0x0020), onRelease: () => _release(0x0020)),
              const SizedBox(width: 16),
              _GuideButton(onPress: () => _press(0x0400), onRelease: () => _release(0x0400)),
              const SizedBox(width: 16),
              _SmallButton(label: 'START', onPress: () => _press(0x0010), onRelease: () => _release(0x0010)),
            ],
          ),
        ),
      ],
    );
  }
}

// ─── Thumbstick ────────────────────────────────────────────────────────────

class _Thumbstick extends StatefulWidget {
  final double x, y;
  final void Function(double x, double y) onMove;
  final String label;
  const _Thumbstick({required this.x, required this.y, required this.onMove, required this.label});
  @override
  State<_Thumbstick> createState() => _ThumbstickState();
}

class _ThumbstickState extends State<_Thumbstick> {
  static const _radius = 52.0;
  static const _thumbRadius = 20.0;
  Offset _thumbPos = Offset.zero;
  bool _active = false;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: (d) {
        setState(() => _active = true);
        _updatePosition(d.localPosition, const Offset(_radius, _radius));
      },
      onPanUpdate: (d) => _updatePosition(d.localPosition, const Offset(_radius, _radius)),
      onPanEnd: (_) {
        setState(() { _active = false; _thumbPos = Offset.zero; });
        widget.onMove(0, 0);
      },
      child: Container(
        width: _radius * 2, height: _radius * 2,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: Colors.white.withAlpha(_active ? 20 : 12),
          border: Border.all(color: Colors.white.withAlpha(_active ? 60 : 30), width: 1.5),
        ),
        child: Stack(
          alignment: Alignment.center,
          children: [
            Transform.translate(
              offset: _thumbPos,
              child: Container(
                width: _thumbRadius * 2, height: _thumbRadius * 2,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: Colors.white.withAlpha(_active ? 80 : 40),
                  border: Border.all(color: Colors.white.withAlpha(100), width: 1),
                ),
              ),
            ),
            if (!_active)
              Text(widget.label, style: const TextStyle(color: Colors.white24, fontSize: 9, fontWeight: FontWeight.w700, letterSpacing: 0.5)),
          ],
        ),
      ),
    );
  }

  void _updatePosition(Offset pos, Offset center) {
    final delta = pos - center;
    final dist = delta.distance;
    final maxDist = _radius - _thumbRadius;
    final clamped = dist <= maxDist ? delta : delta / dist * maxDist;
    setState(() => _thumbPos = clamped);
    final nx = (clamped.dx / maxDist).clamp(-1.0, 1.0);
    final ny = (clamped.dy / maxDist).clamp(-1.0, 1.0);
    widget.onMove(nx, -ny);
  }
}

// ─── D-Pad ─────────────────────────────────────────────────────────────────

class _DPad extends StatelessWidget {
  final void Function(int) onPress;
  final void Function(int) onRelease;
  const _DPad({required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    const size = 36.0;
    const btn = Color(0x33FFFFFF);
    const border = Color(0x44FFFFFF);
    return SizedBox(
      width: size * 3, height: size * 3,
      child: Stack(
        children: [
          Positioned(left: size, top: 0,    child: _DPadBtn(w: size, h: size, icon: Icons.keyboard_arrow_up_rounded,    color: btn, border: border, onPress: () => onPress(0x0001),  onRelease: () => onRelease(0x0001))),
          Positioned(left: size, bottom: 0, child: _DPadBtn(w: size, h: size, icon: Icons.keyboard_arrow_down_rounded,  color: btn, border: border, onPress: () => onPress(0x0002),  onRelease: () => onRelease(0x0002))),
          Positioned(left: 0,    top: size, child: _DPadBtn(w: size, h: size, icon: Icons.keyboard_arrow_left_rounded,  color: btn, border: border, onPress: () => onPress(0x0004),  onRelease: () => onRelease(0x0004))),
          Positioned(right: 0,   top: size, child: _DPadBtn(w: size, h: size, icon: Icons.keyboard_arrow_right_rounded, color: btn, border: border, onPress: () => onPress(0x0008),  onRelease: () => onRelease(0x0008))),
          Positioned(left: size, top: size, child: Container(width: size, height: size, color: const Color(0x22FFFFFF))),
        ],
      ),
    );
  }
}

class _DPadBtn extends StatefulWidget {
  final double w, h;
  final IconData icon;
  final Color color, border;
  final VoidCallback onPress, onRelease;
  const _DPadBtn({required this.w, required this.h, required this.icon, required this.color, required this.border, required this.onPress, required this.onRelease});
  @override
  State<_DPadBtn> createState() => _DPadBtnState();
}

class _DPadBtnState extends State<_DPadBtn> {
  bool _pressed = false;
  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) { setState(() => _pressed = true); widget.onPress(); },
      onTapUp: (_) { setState(() => _pressed = false); widget.onRelease(); },
      onTapCancel: () { setState(() => _pressed = false); widget.onRelease(); },
      child: Container(
        width: widget.w, height: widget.h,
        decoration: BoxDecoration(
          color: _pressed ? widget.color.withAlpha(80) : widget.color,
          border: Border.all(color: widget.border, width: 0.5),
        ),
        child: Icon(widget.icon, color: Colors.white.withAlpha(_pressed ? 220 : 140), size: 20),
      ),
    );
  }
}

// ─── ABXY Cluster ──────────────────────────────────────────────────────────

class _ABXYCluster extends StatelessWidget {
  final void Function(int) onPress;
  final void Function(int) onRelease;
  const _ABXYCluster({required this.onPress, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    const s = 44.0;
    return SizedBox(
      width: s * 3, height: s * 3,
      child: Stack(
        alignment: Alignment.center,
        children: [
          Positioned(left: s, top: 0,     child: _FaceBtn(label: 'Y', color: AppTheme.buttonY,  btnId: 0x8000, size: s, onPress: onPress, onRelease: onRelease)),
          Positioned(left: s, bottom: 0,  child: _FaceBtn(label: 'A', color: AppTheme.buttonA,  btnId: 0x1000, size: s, onPress: onPress, onRelease: onRelease)),
          Positioned(left: 0, top: s,     child: _FaceBtn(label: 'X', color: AppTheme.buttonX,  btnId: 0x4000, size: s, onPress: onPress, onRelease: onRelease)),
          Positioned(right: 0, top: s,    child: _FaceBtn(label: 'B', color: AppTheme.buttonB,  btnId: 0x2000, size: s, onPress: onPress, onRelease: onRelease)),
        ],
      ),
    );
  }
}

class _FaceBtn extends StatefulWidget {
  final String label;
  final Color color;
  final int btnId;
  final double size;
  final void Function(int) onPress, onRelease;
  const _FaceBtn({required this.label, required this.color, required this.btnId, required this.size, required this.onPress, required this.onRelease});
  @override
  State<_FaceBtn> createState() => _FaceBtnState();
}

class _FaceBtnState extends State<_FaceBtn> {
  bool _pressed = false;
  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) { setState(() => _pressed = true); widget.onPress(widget.btnId); },
      onTapUp: (_) { setState(() => _pressed = false); widget.onRelease(widget.btnId); },
      onTapCancel: () { setState(() => _pressed = false); widget.onRelease(widget.btnId); },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 60),
        width: widget.size, height: widget.size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: _pressed ? widget.color.withAlpha(200) : widget.color.withAlpha(60),
          border: Border.all(color: widget.color.withAlpha(_pressed ? 255 : 160), width: 1.5),
          boxShadow: _pressed ? [BoxShadow(color: widget.color.withAlpha(100), blurRadius: 8)] : null,
        ),
        child: Center(
          child: Text(widget.label,
              style: TextStyle(
                color: widget.color.withAlpha(_pressed ? 255 : 200),
                fontSize: 14, fontWeight: FontWeight.w800,
              )),
        ),
      ),
    );
  }
}

// ─── Shoulder / Trigger buttons ────────────────────────────────────────────

class _ShoulderButton extends StatefulWidget {
  final String label;
  final VoidCallback onPress, onRelease;
  const _ShoulderButton({required this.label, required this.onPress, required this.onRelease});
  @override
  State<_ShoulderButton> createState() => _ShoulderButtonState();
}

class _ShoulderButtonState extends State<_ShoulderButton> {
  bool _pressed = false;
  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) { setState(() => _pressed = true); widget.onPress(); },
      onTapUp: (_) { setState(() => _pressed = false); widget.onRelease(); },
      onTapCancel: () { setState(() => _pressed = false); widget.onRelease(); },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 60),
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 10),
        decoration: BoxDecoration(
          color: _pressed ? Colors.white.withAlpha(50) : Colors.white.withAlpha(20),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: Colors.white.withAlpha(_pressed ? 120 : 50)),
        ),
        child: Text(widget.label, style: TextStyle(color: Colors.white.withAlpha(_pressed ? 230 : 160), fontSize: 12, fontWeight: FontWeight.w700)),
      ),
    );
  }
}

class _TriggerButton extends StatelessWidget {
  final String label;
  final double value;
  final ValueChanged<double> onDrag;
  const _TriggerButton({required this.label, required this.value, required this.onDrag});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onVerticalDragUpdate: (d) {
        final delta = -d.delta.dy / 80;
        final newVal = (value + delta).clamp(0.0, 1.0);
        onDrag(newVal);
      },
      onVerticalDragEnd: (_) => onDrag(0),
      child: Container(
        width: 44, height: 20,
        decoration: BoxDecoration(
          color: Colors.white.withAlpha((20 + (value * 50)).round()),
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: Colors.white.withAlpha(60)),
        ),
        child: Stack(
          children: [
            FractionallySizedBox(
              widthFactor: value,
              child: Container(
                decoration: BoxDecoration(
                  color: AppTheme.xboxGreen.withAlpha(140),
                  borderRadius: BorderRadius.circular(5),
                ),
              ),
            ),
            Center(
              child: Text(label, style: const TextStyle(color: Colors.white70, fontSize: 9, fontWeight: FontWeight.w700)),
            ),
          ],
        ),
      ),
    );
  }
}

class _GuideButton extends StatefulWidget {
  final VoidCallback onPress, onRelease;
  const _GuideButton({required this.onPress, required this.onRelease});
  @override
  State<_GuideButton> createState() => _GuideButtonState();
}

class _GuideButtonState extends State<_GuideButton> {
  bool _pressed = false;
  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) { setState(() => _pressed = true); widget.onPress(); },
      onTapUp: (_) { setState(() => _pressed = false); widget.onRelease(); },
      onTapCancel: () { setState(() => _pressed = false); widget.onRelease(); },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 60),
        width: 36, height: 36,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: _pressed ? AppTheme.xboxGreen.withAlpha(180) : AppTheme.xboxGreen.withAlpha(50),
          border: Border.all(color: AppTheme.xboxGreen.withAlpha(180), width: 2),
        ),
        child: const Center(
          child: Text('𝕏', style: TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.w900)),
        ),
      ),
    );
  }
}

class _SmallButton extends StatefulWidget {
  final String label;
  final VoidCallback onPress, onRelease;
  const _SmallButton({required this.label, required this.onPress, required this.onRelease});
  @override
  State<_SmallButton> createState() => _SmallButtonState();
}

class _SmallButtonState extends State<_SmallButton> {
  bool _pressed = false;
  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) { setState(() => _pressed = true); widget.onPress(); },
      onTapUp: (_) { setState(() => _pressed = false); widget.onRelease(); },
      onTapCancel: () { setState(() => _pressed = false); widget.onRelease(); },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 60),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        decoration: BoxDecoration(
          color: _pressed ? Colors.white.withAlpha(40) : Colors.white.withAlpha(12),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: Colors.white.withAlpha(_pressed ? 80 : 30)),
        ),
        child: Text(widget.label, style: TextStyle(color: Colors.white.withAlpha(_pressed ? 200 : 120), fontSize: 9, fontWeight: FontWeight.w700, letterSpacing: 0.5)),
      ),
    );
  }
}
