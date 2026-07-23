import 'package:freezed_annotation/freezed_annotation.dart';

part 'game_entry.freezed.dart';
part 'game_entry.g.dart';

enum GameFormat { xex, iso, stfs, unknown }
enum CompatStatus { playable, ingame, boots, nothing, untested }

@freezed
class GameEntry with _$GameEntry {
  const factory GameEntry({
    required String titleId,
    required String title,
    required String region,
    required String execPath,
    required String installDir,
    @Default(GameFormat.unknown) GameFormat format,
    @Default(CompatStatus.untested) CompatStatus compatStatus,
    String? artworkPath,
    String? artworkUrl,
    @Default(0) int installSizeBytes,
    String? installedAt,
    String? lastPlayedAt,
    @Default(0) int totalPlaytimeSeconds,
    String? version,
    @Default([]) List<String> compatIssues,
  }) = _GameEntry;

  factory GameEntry.fromJson(Map<String, dynamic> json) =>
      _$GameEntryFromJson(json);
}

extension GameEntryX on GameEntry {
  String get displaySize {
    if (installSizeBytes <= 0) return 'Unknown';
    if (installSizeBytes < 1024 * 1024) {
      return '${(installSizeBytes / 1024).toStringAsFixed(1)} KB';
    } else if (installSizeBytes < 1024 * 1024 * 1024) {
      return '${(installSizeBytes / (1024 * 1024)).toStringAsFixed(1)} MB';
    } else {
      return '${(installSizeBytes / (1024 * 1024 * 1024)).toStringAsFixed(2)} GB';
    }
  }

  String get playtimeDisplay {
    if (totalPlaytimeSeconds <= 0) return 'Never played';
    final h = totalPlaytimeSeconds ~/ 3600;
    final m = (totalPlaytimeSeconds % 3600) ~/ 60;
    if (h > 0) return '${h}h ${m}m';
    return '${m}m';
  }

  bool get isCompatible =>
      compatStatus == CompatStatus.playable ||
      compatStatus == CompatStatus.ingame;
}
