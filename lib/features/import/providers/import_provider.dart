import 'dart:io';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';
import 'package:crypto/crypto.dart';
import 'package:uuid/uuid.dart';

import '../../../core/models/game_entry.dart';
import '../../../core/native/engine_bridge.dart';

class ImportState {
  final bool isImporting;
  final String currentStep;
  final double progress;
  final String details;
  final String? error;

  const ImportState({
    this.isImporting = false,
    this.currentStep = '',
    this.progress = 0,
    this.details = '',
    this.error,
  });

  ImportState copyWith({
    bool? isImporting, String? currentStep, double? progress,
    String? details, String? error,
  }) => ImportState(
    isImporting: isImporting ?? this.isImporting,
    currentStep: currentStep ?? this.currentStep,
    progress: progress ?? this.progress,
    details: details ?? this.details,
    error: error ?? this.error,
  );
}

final importProvider = NotifierProvider<ImportNotifier, ImportState>(ImportNotifier.new);

class ImportNotifier extends Notifier<ImportState> {
  @override
  ImportState build() => const ImportState();

  Future<GameEntry?> importGame(String sourcePath, GameFormat format) async {
    state = const ImportState(isImporting: true, currentStep: 'Analyzing file...', progress: 0.05);

    try {
      final appDir = await getApplicationDocumentsDirectory();
      final gamesDir = Directory('${appDir.path}/games');
      await gamesDir.create(recursive: true);

      state = state.copyWith(currentStep: 'Validating integrity...', progress: 0.15);
      await Future.delayed(const Duration(milliseconds: 200));

      // Compute file hash for dedup
      final file = File(sourcePath);
      final bytes = await file.readAsBytes();
      final hash = sha256.convert(bytes).toString().substring(0, 16);

      state = state.copyWith(currentStep: 'Parsing executable...', progress: 0.30);
      await Future.delayed(const Duration(milliseconds: 300));

      // Parse game metadata via native engine (stub if unavailable)
      String titleId = 'UNKNOWN_${hash.toUpperCase()}';
      String title = 'Unknown Game';
      String region = 'NTSC-U';

      if (EngineBridge.isAvailable) {
        final rc = switch (format) {
          GameFormat.xex => EngineBridge.loadXex(sourcePath),
          GameFormat.iso => EngineBridge.loadIso(sourcePath),
          GameFormat.stfs => EngineBridge.loadStfs(sourcePath),
          _ => -1,
        };
        if (rc != 0) throw Exception('Native loader returned error code $rc');
      } else {
        // Stub: parse title from filename
        title = file.uri.pathSegments.last.replaceAll(RegExp(r'\.[^.]+$'), '');
        titleId = 'STUB_${hash.toUpperCase().substring(0, 8)}';
      }

      state = state.copyWith(currentStep: 'Copying files...', progress: 0.55);

      final installDir = '${gamesDir.path}/$titleId';
      await Directory(installDir).create(recursive: true);

      // Copy source file to install dir
      final destPath = '$installDir/${file.uri.pathSegments.last}';
      await file.copy(destPath);

      state = state.copyWith(currentStep: 'Finalizing installation...', progress: 0.90);
      await Future.delayed(const Duration(milliseconds: 200));

      final entry = GameEntry(
        titleId: titleId,
        title: title,
        region: region,
        execPath: destPath,
        installDir: installDir,
        format: format,
        compatStatus: CompatStatus.untested,
        installSizeBytes: bytes.length,
        installedAt: DateTime.now().toIso8601String(),
      );

      state = const ImportState(isImporting: false, progress: 1.0);
      return entry;
    } catch (e) {
      state = ImportState(isImporting: false, error: e.toString());
      return null;
    }
  }
}
