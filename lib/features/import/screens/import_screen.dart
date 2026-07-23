import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:file_picker/file_picker.dart';
import 'package:go_router/go_router.dart';

import '../../../core/models/game_entry.dart';
import '../../../core/theme/app_theme.dart';
import '../../library/providers/library_provider.dart';
import '../providers/import_provider.dart';

class ImportScreen extends ConsumerStatefulWidget {
  const ImportScreen({super.key});

  @override
  ConsumerState<ImportScreen> createState() => _ImportScreenState();
}

class _ImportScreenState extends ConsumerState<ImportScreen> {
  bool _importing = false;

  @override
  Widget build(BuildContext context) {
    final importState = ref.watch(importProvider);

    return Scaffold(
      backgroundColor: AppTheme.bgBase,
      body: SafeArea(
        child: CustomScrollView(
          slivers: [
            SliverToBoxAdapter(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(20, 20, 20, 0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Import Game', style: Theme.of(context).textTheme.displayMedium),
                    const SizedBox(height: 4),
                    Text(
                      'Select your Xbox 360 game file',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                    const SizedBox(height: 24),
                    if (!_importing) _buildFormatCards() else _buildProgress(importState),
                    const SizedBox(height: 24),
                    _buildLegalNote(),
                    const SizedBox(height: 100),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildFormatCards() {
    return Column(
      children: [
        _FormatCard(
          icon: Icons.insert_drive_file_outlined,
          color: AppTheme.xboxGreenLight,
          title: 'XEX Executable',
          subtitle: 'Direct Xbox 360 executable file',
          badges: const ['default.xex', 'game.xex'],
          onImport: () => _pickFile(['xex'], GameFormat.xex),
        ),
        const SizedBox(height: 12),
        _FormatCard(
          icon: Icons.album_outlined,
          color: const Color(0xFF4A9DE8),
          title: 'ISO Disc Image',
          subtitle: 'Full disc dump in ISO 9660 or XDVDFS format',
          badges: const ['*.iso'],
          onImport: () => _pickFile(['iso'], GameFormat.iso),
        ),
        const SizedBox(height: 12),
        _FormatCard(
          icon: Icons.inventory_2_outlined,
          color: const Color(0xFFD4AC0A),
          title: 'STFS Package',
          subtitle: 'Xbox Live container: CON, PIRS, or LIVE',
          badges: const ['CON', 'PIRS', 'LIVE'],
          onImport: () => _pickFile(null, GameFormat.stfs),
        ),
      ],
    );
  }

  Widget _buildProgress(ImportState importState) {
    return Container(
      padding: const EdgeInsets.all(28),
      decoration: BoxDecoration(
        color: AppTheme.surface,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AppTheme.border),
      ),
      child: Column(
        children: [
          Container(
            width: 72, height: 72,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: AppTheme.xboxGreen.withAlpha(20),
              border: Border.all(color: AppTheme.xboxGreen.withAlpha(60), width: 2),
            ),
            child: const Padding(
              padding: EdgeInsets.all(16),
              child: CircularProgressIndicator(
                color: AppTheme.xboxGreen,
                strokeWidth: 2.5,
              ),
            ),
          ),
          const SizedBox(height: 20),
          Text(
            importState.currentStep,
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: Colors.white),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 16),
          ClipRRect(
            borderRadius: BorderRadius.circular(6),
            child: LinearProgressIndicator(
              value: importState.progress,
              minHeight: 6,
            ),
          ),
          const SizedBox(height: 10),
          Text(
            '${(importState.progress * 100).toStringAsFixed(0)}%',
            style: const TextStyle(fontSize: 13, color: Colors.white38, fontWeight: FontWeight.w600),
          ),
          if (importState.details.isNotEmpty) ...[
            const SizedBox(height: 12),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: AppTheme.surfaceVariant,
                borderRadius: BorderRadius.circular(8),
              ),
              child: Text(
                importState.details,
                style: const TextStyle(fontSize: 11, color: Colors.white38, fontFamily: 'monospace'),
                textAlign: TextAlign.center,
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _buildLegalNote() {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.amber.withAlpha(15),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.amber.withAlpha(50)),
      ),
      child: const Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.gavel_rounded, color: Colors.amber, size: 18),
          SizedBox(width: 12),
          Expanded(
            child: Text(
              'Xbox360Recomp does not include any game files. You must supply a legally obtained copy of any game you wish to run.',
              style: TextStyle(fontSize: 12, color: Colors.amber, height: 1.5),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _pickFile(List<String>? extensions, GameFormat format) async {
    final result = await FilePicker.platform.pickFiles(
      type: extensions != null ? FileType.custom : FileType.any,
      allowedExtensions: extensions,
      dialogTitle: 'Select Xbox 360 game file',
    );
    if (result == null || result.files.isEmpty) return;
    final path = result.files.first.path;
    if (path == null) return;

    setState(() => _importing = true);
    try {
      final game = await ref.read(importProvider.notifier).importGame(path, format);
      if (game != null && mounted) {
        await ref.read(libraryProvider.notifier).addGame(game);
        if (mounted) {
          setState(() => _importing = false);
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('${game.title} imported successfully'),
              action: SnackBarAction(
                label: 'Play',
                onPressed: () => context.push('/emulation/${game.titleId}'),
              ),
            ),
          );
          context.go('/');
        }
      } else if (mounted) {
        setState(() => _importing = false);
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Import failed. Check that the file is a valid Xbox 360 game.')),
        );
      }
    } catch (e) {
      if (mounted) {
        setState(() => _importing = false);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Import error: $e')),
        );
      }
    }
  }
}

class _FormatCard extends StatelessWidget {
  final IconData icon;
  final Color color;
  final String title, subtitle;
  final List<String> badges;
  final VoidCallback onImport;

  const _FormatCard({
    required this.icon, required this.color,
    required this.title, required this.subtitle,
    required this.badges, required this.onImport,
  });

  @override
  Widget build(BuildContext context) {
    return Material(
      color: AppTheme.card,
      borderRadius: BorderRadius.circular(14),
      child: InkWell(
        onTap: onImport,
        borderRadius: BorderRadius.circular(14),
        child: Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: AppTheme.border),
          ),
          child: Row(
            children: [
              Container(
                width: 52, height: 52,
                decoration: BoxDecoration(
                  color: color.withAlpha(20),
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: color.withAlpha(60)),
                ),
                child: Icon(icon, color: color, size: 24),
              ),
              const SizedBox(width: 16),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title, style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: Colors.white)),
                    const SizedBox(height: 3),
                    Text(subtitle, style: const TextStyle(fontSize: 12, color: Colors.white54)),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 6,
                      children: badges.map((b) => Container(
                        padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 2),
                        decoration: BoxDecoration(
                          color: color.withAlpha(15),
                          borderRadius: BorderRadius.circular(4),
                          border: Border.all(color: color.withAlpha(50), width: 0.5),
                        ),
                        child: Text(b, style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: color, fontFamily: 'monospace')),
                      )).toList(),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 8),
              const Icon(Icons.chevron_right_rounded, color: Colors.white24),
            ],
          ),
        ),
      ),
    );
  }
}
