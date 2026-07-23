import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import '../../../core/theme/app_theme.dart';

class EmptyLibrary extends StatefulWidget {
  const EmptyLibrary({super.key});
  @override
  State<EmptyLibrary> createState() => _EmptyLibraryState();
}

class _EmptyLibraryState extends State<EmptyLibrary> with SingleTickerProviderStateMixin {
  late AnimationController _ctrl;
  late Animation<double> _pulse;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(vsync: this, duration: const Duration(seconds: 2))..repeat(reverse: true);
    _pulse = Tween(begin: 0.85, end: 1.0).animate(CurvedAnimation(parent: _ctrl, curve: Curves.easeInOut));
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ScaleTransition(
              scale: _pulse,
              child: Container(
                width: 120, height: 120,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: AppTheme.xboxGreen.withAlpha(15),
                  border: Border.all(color: AppTheme.xboxGreen.withAlpha(60), width: 2),
                ),
                child: const Icon(
                  Icons.sports_esports_outlined,
                  size: 52,
                  color: AppTheme.xboxGreen,
                ),
              ),
            ),
            const SizedBox(height: 28),
            const Text(
              'Your library is empty',
              style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700, color: Colors.white),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 10),
            const Text(
              'Import your Xbox 360 game files to get started.\nSupported formats: XEX, ISO, STFS.',
              style: TextStyle(fontSize: 14, color: Colors.white54, height: 1.6),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 32),
            ElevatedButton.icon(
              onPressed: () => context.go('/import'),
              icon: const Icon(Icons.add_rounded, size: 20),
              label: const Text('Import a Game'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 14),
              ),
            ),
            const SizedBox(height: 20),
            OutlinedButton(
              onPressed: () => showDialog(
                context: context,
                builder: (_) => const _SupportedFormatsDialog(),
              ),
              child: const Text('Supported formats'),
            ),
          ],
        ),
      ),
    );
  }
}

class _SupportedFormatsDialog extends StatelessWidget {
  const _SupportedFormatsDialog();

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Supported Formats'),
      content: const Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _FormatRow(ext: '.xex', desc: 'Xbox Executable — raw game executable'),
          SizedBox(height: 12),
          _FormatRow(ext: '.iso', desc: 'Disc Image — ISO 9660 or XDVDFS format'),
          SizedBox(height: 12),
          _FormatRow(ext: 'STFS', desc: 'Xbox Live Package — CON, PIRS, or LIVE'),
        ],
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: const Text('Close')),
      ],
    );
  }
}

class _FormatRow extends StatelessWidget {
  final String ext, desc;
  const _FormatRow({required this.ext, required this.desc});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
          decoration: BoxDecoration(
            color: AppTheme.xboxGreen.withAlpha(30),
            borderRadius: BorderRadius.circular(6),
            border: Border.all(color: AppTheme.xboxGreen.withAlpha(80)),
          ),
          child: Text(ext, style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: AppTheme.xboxGreenLight, fontFamily: 'monospace')),
        ),
        const SizedBox(width: 12),
        Expanded(child: Text(desc, style: const TextStyle(fontSize: 13, color: Colors.white70))),
      ],
    );
  }
}
