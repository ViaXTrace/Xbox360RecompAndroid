import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:file_picker/file_picker.dart';
import 'package:go_router/go_router.dart';

import '../../../core/models/game_entry.dart';
import '../../library/providers/library_provider.dart';
import '../providers/import_provider.dart';

class ImportScreen extends ConsumerStatefulWidget {
  const ImportScreen({super.key});

  @override
  ConsumerState<ImportScreen> createState() => _ImportScreenState();
}

class _ImportScreenState extends ConsumerState<ImportScreen> {
  @override
  Widget build(BuildContext context) {
    final importState = ref.watch(importProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Import Game'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => context.pop(),
        ),
      ),
      body: importState.isImporting
          ? _buildProgress(importState)
          : _buildOptions(context),
    );
  }

  Widget _buildOptions(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        const Text(
          'Select a game to import',
          style: TextStyle(fontSize: 18, fontWeight: FontWeight.w600, color: Colors.white),
        ),
        const SizedBox(height: 8),
        const Text(
          'Provide your legally obtained Xbox 360 game dump. No game files are included with this app.',
          style: TextStyle(fontSize: 13, color: Colors.white54, height: 1.5),
        ),
        const SizedBox(height: 28),
        _FormatCard(
          icon: Icons.description_outlined,
          title: 'XEX Executable',
          subtitle: 'Default.xex — the game executable',
          formats: const ['*.xex', '*.xex2'],
          onImport: () => _pickFile(['xex', 'xex2'], GameFormat.xex),
        ),
        const SizedBox(height: 12),
        _FormatCard(
          icon: Icons.disc_full_outlined,
          title: 'ISO / Disc Image',
          subtitle: 'ISO 9660 or XDVDFS disc dump',
          formats: const ['*.iso'],
          onImport: () => _pickFile(['iso'], GameFormat.iso),
        ),
        const SizedBox(height: 12),
        _FormatCard(
          icon: Icons.inventory_2_outlined,
          title: 'STFS Package',
          subtitle: 'CON (homebrew), PIRS, or LIVE container',
          formats: const ['CON', 'PIRS', 'LIVE', 'no extension'],
          onImport: () => _pickFile(null, GameFormat.stfs),
        ),
        const SizedBox(height: 32),
        Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            color: Colors.amber.withOpacity(0.08),
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: Colors.amber.withOpacity(0.3)),
          ),
          child: const Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Icon(Icons.warning_amber_rounded, color: Colors.amber, size: 20),
              SizedBox(width: 12),
              Expanded(
                child: Text(
                  'This app does not include game files. You must supply a legally obtained copy of any Xbox 360 game you wish to run.',
                  style: TextStyle(fontSize: 13, color: Colors.amber, height: 1.5),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildProgress(ImportState importState) {
    return Padding(
      padding: const EdgeInsets.all(32),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.downloading, size: 64, color: Color(0xFF107C10)),
          const SizedBox(height: 24),
          Text(
            importState.currentStep,
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: Colors.white),
          ),
          const SizedBox(height: 16),
          LinearProgressIndicator(value: importState.progress),
          const SizedBox(height: 12),
          Text(
            '${(importState.progress * 100).toStringAsFixed(0)}%',
            style: const TextStyle(fontSize: 13, color: Colors.white54),
          ),
          if (importState.details.isNotEmpty) ...[
            const SizedBox(height: 16),
            Text(
              importState.details,
              style: const TextStyle(fontSize: 12, color: Colors.white38),
              textAlign: TextAlign.center,
            ),
          ],
        ],
      ),
    );
  }

  Future<void> _pickFile(List<String>? extensions, GameFormat format) async {
    final result = await FilePicker.platform.pickFiles(
      type: extensions != null ? FileType.custom : FileType.any,
      allowedExtensions: extensions,
    );
    if (result == null || result.files.isEmpty) return;
    final path = result.files.first.path;
    if (path == null) return;

    final game = await ref.read(importProvider.notifier).importGame(path, format);
    if (game != null && mounted) {
      await ref.read(libraryProvider.notifier).addGame(game);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('${game.title} imported successfully')),
        );
        context.pop();
      }
    }
  }
}

class _FormatCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final List<String> formats;
  final VoidCallback onImport;

  const _FormatCard({
    required this.icon, required this.title, required this.subtitle,
    required this.formats, required this.onImport,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onImport,
      child: Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: const Color(0xFF252525),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: Colors.white10),
        ),
        child: Row(
          children: [
            Container(
              width: 48, height: 48,
              decoration: BoxDecoration(
                color: const Color(0xFF107C10).withOpacity(0.15),
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(icon, color: const Color(0xFF107C10), size: 24),
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title, style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: Colors.white)),
                  const SizedBox(height: 2),
                  Text(subtitle, style: const TextStyle(fontSize: 12, color: Colors.white54)),
                  const SizedBox(height: 6),
                  Wrap(
                    spacing: 6,
                    children: formats.map((f) => Container(
                      padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 2),
                      decoration: BoxDecoration(
                        color: Colors.white.withOpacity(0.08),
                        borderRadius: BorderRadius.circular(4),
                      ),
                      child: Text(f, style: const TextStyle(fontSize: 10, color: Colors.white54, fontFamily: 'monospace')),
                    )).toList(),
                  ),
                ],
              ),
            ),
            const Icon(Icons.chevron_right, color: Colors.white24),
          ],
        ),
      ),
    );
  }
}
