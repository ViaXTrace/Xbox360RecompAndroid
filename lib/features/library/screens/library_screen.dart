import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../core/models/game_entry.dart';
import '../providers/library_provider.dart';
import '../widgets/game_card.dart';
import '../widgets/empty_library.dart';
import '../widgets/library_header.dart';

class LibraryScreen extends ConsumerStatefulWidget {
  const LibraryScreen({super.key});

  @override
  ConsumerState<LibraryScreen> createState() => _LibraryScreenState();
}

class _LibraryScreenState extends ConsumerState<LibraryScreen> {
  String _searchQuery = '';
  CompatStatus? _filterStatus;
  bool _gridView = true;

  @override
  Widget build(BuildContext context) {
    final gamesAsync = ref.watch(libraryProvider);

    return Scaffold(
      backgroundColor: Theme.of(context).colorScheme.background,
      body: CustomScrollView(
        slivers: [
          SliverToBoxAdapter(
            child: LibraryHeader(
              onImport: () => context.push('/import'),
              onSettings: () => context.push('/settings'),
              onAbout: () => context.push('/about'),
              searchQuery: _searchQuery,
              onSearchChanged: (q) => setState(() => _searchQuery = q),
              gridView: _gridView,
              onToggleView: () => setState(() => _gridView = !_gridView),
              filterStatus: _filterStatus,
              onFilterChanged: (s) => setState(() => _filterStatus = s),
            ),
          ),
          gamesAsync.when(
            data: (games) {
              final filtered = _applyFilter(games);
              if (filtered.isEmpty) {
                return const SliverFillRemaining(child: EmptyLibrary());
              }
              if (_gridView) {
                return SliverPadding(
                  padding: const EdgeInsets.all(16),
                  sliver: SliverGrid(
                    delegate: SliverChildBuilderDelegate(
                      (ctx, i) => GameCard(
                        game: filtered[i],
                        onTap: () => _showGameMenu(context, filtered[i]),
                      ),
                      childCount: filtered.length,
                    ),
                    gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
                      maxCrossAxisExtent: 200,
                      childAspectRatio: 0.68,
                      crossAxisSpacing: 12,
                      mainAxisSpacing: 12,
                    ),
                  ),
                );
              } else {
                return SliverPadding(
                  padding: const EdgeInsets.symmetric(horizontal: 16),
                  sliver: SliverList(
                    delegate: SliverChildBuilderDelegate(
                      (ctx, i) => GameCard(
                        game: filtered[i],
                        listMode: true,
                        onTap: () => _showGameMenu(context, filtered[i]),
                      ),
                      childCount: filtered.length,
                    ),
                  ),
                );
              }
            },
            loading: () => const SliverFillRemaining(
              child: Center(child: CircularProgressIndicator()),
            ),
            error: (e, _) => SliverFillRemaining(
              child: Center(child: Text('Error: $e')),
            ),
          ),
          const SliverPadding(padding: EdgeInsets.only(bottom: 80)),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => context.push('/import'),
        icon: const Icon(Icons.add),
        label: const Text('Import Game'),
        backgroundColor: Theme.of(context).colorScheme.primary,
      ),
    );
  }

  List<GameEntry> _applyFilter(List<GameEntry> games) {
    var list = games;
    if (_searchQuery.isNotEmpty) {
      final q = _searchQuery.toLowerCase();
      list = list.where((g) =>
        g.title.toLowerCase().contains(q) ||
        g.titleId.toLowerCase().contains(q)
      ).toList();
    }
    if (_filterStatus != null) {
      list = list.where((g) => g.compatStatus == _filterStatus).toList();
    }
    return list;
  }

  void _showGameMenu(BuildContext context, GameEntry game) {
    showModalBottomSheet(
      context: context,
      builder: (_) => _GameMenu(game: game),
    );
  }
}

class _GameMenu extends ConsumerWidget {
  final GameEntry game;
  const _GameMenu({required this.game});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 40, height: 4,
            margin: const EdgeInsets.only(top: 12, bottom: 8),
            decoration: BoxDecoration(
              color: Colors.white24,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
            child: Row(
              children: [
                if (game.artworkPath != null)
                  ClipRRect(
                    borderRadius: BorderRadius.circular(8),
                    child: Image.asset(game.artworkPath!, width: 60, height: 80, fit: BoxFit.cover),
                  )
                else
                  Container(
                    width: 60, height: 80,
                    decoration: BoxDecoration(
                      color: Colors.white12,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: const Icon(Icons.sports_esports, color: Colors.white38),
                  ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(game.title, style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                      const SizedBox(height: 4),
                      Text(game.titleId, style: const TextStyle(fontSize: 12, color: Colors.white54)),
                      const SizedBox(height: 4),
                      Text(game.displaySize, style: const TextStyle(fontSize: 12, color: Colors.white38)),
                    ],
                  ),
                ),
              ],
            ),
          ),
          const Divider(),
          ListTile(
            leading: const Icon(Icons.play_arrow, color: Color(0xFF107C10)),
            title: const Text('Launch'),
            onTap: () {
              Navigator.pop(context);
              context.push('/game/${game.titleId}/run');
            },
          ),
          ListTile(
            leading: const Icon(Icons.settings),
            title: const Text('Settings'),
            onTap: () {
              Navigator.pop(context);
              context.push('/game/${game.titleId}/settings');
            },
          ),
          ListTile(
            leading: const Icon(Icons.gamepad),
            title: const Text('Controls'),
            onTap: () {
              Navigator.pop(context);
              context.push('/game/${game.titleId}/controls');
            },
          ),
          ListTile(
            leading: const Icon(Icons.delete_outline, color: Colors.redAccent),
            title: const Text('Remove', style: TextStyle(color: Colors.redAccent)),
            onTap: () {
              Navigator.pop(context);
              ref.read(libraryProvider.notifier).removeGame(game.titleId);
            },
          ),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
