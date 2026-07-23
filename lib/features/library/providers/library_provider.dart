import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hive_flutter/hive_flutter.dart';
import '../../../core/models/game_entry.dart';

const _boxName = 'games';

final libraryProvider =
    AsyncNotifierProvider<LibraryNotifier, List<GameEntry>>(LibraryNotifier.new);

class LibraryNotifier extends AsyncNotifier<List<GameEntry>> {
  late Box<Map> _box;

  @override
  Future<List<GameEntry>> build() async {
    _box = await Hive.openBox<Map>(_boxName);
    return _loadAll();
  }

  List<GameEntry> _loadAll() {
    return _box.values
        .map((m) => GameEntry.fromJson(Map<String, dynamic>.from(m)))
        .toList()
      ..sort((a, b) => (b.lastPlayedAt ?? '').compareTo(a.lastPlayedAt ?? ''));
  }

  Future<void> addGame(GameEntry game) async {
    await _box.put(game.titleId, game.toJson());
    state = AsyncData(_loadAll());
  }

  Future<void> updateGame(GameEntry game) async {
    await _box.put(game.titleId, game.toJson());
    state = AsyncData(_loadAll());
  }

  Future<void> removeGame(String titleId) async {
    await _box.delete(titleId);
    state = AsyncData(_loadAll());
  }

  GameEntry? getGame(String titleId) {
    final m = _box.get(titleId);
    if (m == null) return null;
    return GameEntry.fromJson(Map<String, dynamic>.from(m));
  }

  Future<void> updateLastPlayed(String titleId) async {
    final game = getGame(titleId);
    if (game == null) return;
    await updateGame(game.copyWith(lastPlayedAt: DateTime.now().toIso8601String()));
  }
}
