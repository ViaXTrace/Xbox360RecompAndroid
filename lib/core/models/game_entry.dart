import 'package:freezed_annotation/freezed_annotation.dart';
import 'package:hive/hive.dart';

part 'game_entry.freezed.dart';
part 'game_entry.g.dart';

enum GameFormat { xex, iso, stfs, unknown }
enum CompatStatus { playable, ingame, boots, nothing, untested }

@freezed
@HiveType(typeId: 0)
class GameEntry with _$GameEntry {
  const factory GameEntry({
    @HiveField(0) required String titleId,
    @HiveField(1) required String title,
    @HiveField(2) required String region,
    @HiveField(3) required String execPath,
    @HiveField(4) required String installDir,
    @HiveField(5) @Default(GameFormat.unknown) GameFormat format,
    @HiveField(6) @Default(CompatStatus.untested) CompatStatus compatStatus,
    @HiveField(7) String? artworkPath,
    @HiveField(8) String? artworkUrl,
    @HiveField(9) @Default(0) int installSizeBytes,
    @HiveField(10) String? installedAt,
    @HiveField(11) String? lastPlayedAt,
    @HiveField(12) @Default(0) int totalPlaytimeSeconds,
    @HiveField(13) String? version,
    @HiveField(14) @Default([]) List<String> compatIssues,
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

  bool get isCompatible => compatStatus == CompatStatus.playable ||
      compatStatus == CompatStatus.ingame;
}
