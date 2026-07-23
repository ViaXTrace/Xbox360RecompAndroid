import 'package:flutter/material.dart';

class MetricsOverlay extends StatelessWidget {
  final double fps;
  final double frameTime;
  final int ramMb;

  const MetricsOverlay({
    super.key,
    required this.fps,
    required this.frameTime,
    required this.ramMb,
  });

  @override
  Widget build(BuildContext context) {
    return Positioned(
      top: 60, left: 8,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: Colors.black.withOpacity(0.65),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _MetricRow('FPS', fps.toStringAsFixed(1), _fpsColor(fps)),
            _MetricRow('Frame', '${frameTime.toStringAsFixed(1)}ms', Colors.white70),
            _MetricRow('RAM', '${ramMb}MB', Colors.white70),
          ],
        ),
      ),
    );
  }

  Color _fpsColor(double fps) {
    if (fps >= 55) return const Color(0xFF52B043);
    if (fps >= 30) return const Color(0xFFF59E0B);
    return const Color(0xFFEF4444);
  }
}

class _MetricRow extends StatelessWidget {
  final String label;
  final String value;
  final Color color;
  const _MetricRow(this.label, this.value, this.color);

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        SizedBox(
          width: 42,
          child: Text(label, style: const TextStyle(fontSize: 11, color: Colors.white54, fontFamily: 'monospace')),
        ),
        Text(value, style: TextStyle(fontSize: 11, color: color, fontFamily: 'monospace', fontWeight: FontWeight.w700)),
      ],
    );
  }
}
