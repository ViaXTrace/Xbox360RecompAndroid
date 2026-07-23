import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../../core/models/game_entry.dart';
import '../../../core/theme/app_theme.dart';
import '../providers/library_provider.dart';
import '../widgets/game_card.dart';
import '../widgets/empty_library.dart';

class LibraryScreen extends ConsumerStatefulWidget {
  const LibraryScreen({super.key});

  @override
  ConsumerState<LibraryScreen> createState() => _LibraryScreenState();
}

class _LibraryScreenState extends ConsumerState<LibraryScreen>
    with SingleTickerProviderStateMixin {
  final _searchController = TextEditingController();
  String _searchQuery = '';
  CompatStatus? _filterStatus;
  bool _gridView = true;
  _SortMode _sortMode = _SortMode.lastPlayed;

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final gamesAsync = ref.watch(libraryProvider);

    return Scaffold(
      backgroundColor: AppTheme.bgBase,
      body: SafeArea(
        child: Column(
          children: [
            _buildTopBar(context),
            _buildSearchAndFilter(),
            Expanded(
              child: gamesAsync.when(
                data: (games) {
                  final filtered = _applyFilter(games);
                  if (filtered.isEmpty && games.isEmpty) {
                    return const EmptyLibrary();
                  }
                  if (filtered.isEmpty) {
                    return _buildNoResults();
                  }
                  return RefreshIndicator(
                    onRefresh: () => ref.refresh(libraryProvider.future),
                    color: AppTheme.xboxGreen,
                    backgroundColor: AppTheme.surface,
                    child: _gridView
                        ? _buildGrid(filtered)
                        : _buildList(filtered),
                  );
                },
                loading: () => const Center(
                  child: CircularProgressIndicator(color: AppTheme.xboxGreen),
                ),
                error: (e, _) => Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const Icon(Icons.error_outline, color: Colors.red, size: 48),
                      const SizedBox(height: 12),
                      Text('Failed to load library', style: Theme.of(context).textTheme.titleMedium),
                      const SizedBox(height: 8),
                      TextButton(
                        onPressed: () => ref.refresh(libraryProvider),
                        child: const Text('Retry'),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTopBar(BuildContext context) {
    final gamesAsync = ref.watch(libraryProvider);
    final count = gamesAsync.valueOrNull?.length ?? 0;

    return Padding(
      padding: const EdgeInsets.fromLTRB(20, 16, 16, 8),
      child: Row(
        children: [
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'My Library',
                style: Theme.of(context).textTheme.displayMedium!.copyWith(
                  letterSpacing: -0.5,
                ),
              ),
              if (count > 0)
                Text(
                  '$count game${count == 1 ? '' : 's'}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
            ],
          ),
          const Spacer(),
          _SortButton(
            current: _sortMode,
            onChanged: (m) => setState(() => _sortMode = m),
          ),
          const SizedBox(width: 4),
          IconButton(
            icon: Icon(
              _gridView ? Icons.view_list_rounded : Icons.grid_view_rounded,
              color: Colors.white54,
            ),
            onPressed: () => setState(() => _gridView = !_gridView),
            tooltip: _gridView ? 'List view' : 'Grid view',
          ),
        ],
      ),
    );
  }

  Widget _buildSearchAndFilter() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
      child: Column(
        children: [
          TextField(
            controller: _searchController,
            onChanged: (q) => setState(() => _searchQuery = q.toLowerCase()),
            decoration: InputDecoration(
              hintText: 'Search games…',
              prefixIcon: const Icon(Icons.search_rounded, size: 20),
              suffixIcon: _searchQuery.isNotEmpty
                  ? IconButton(
                      icon: const Icon(Icons.close_rounded, size: 18),
                      onPressed: () {
                        _searchController.clear();
                        setState(() => _searchQuery = '');
                      },
                    )
                  : null,
            ),
          ),
          const SizedBox(height: 10),
          SizedBox(
            height: 34,
            child: ListView(
              scrollDirection: Axis.horizontal,
              children: [
                _FilterChip(
                  label: 'All',
                  selected: _filterStatus == null,
                  onTap: () => setState(() => _filterStatus = null),
                ),
                const SizedBox(width: 6),
                for (final status in CompatStatus.values)
                  Padding(
                    padding: const EdgeInsets.only(right: 6),
                    child: _FilterChip(
                      label: _statusLabel(status),
                      selected: _filterStatus == status,
                      color: _statusColor(status),
                      onTap: () => setState(() =>
                          _filterStatus = _filterStatus == status ? null : status),
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildGrid(List<GameEntry> games) {
    return GridView.builder(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 100),
      gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
        maxCrossAxisExtent: 180,
        childAspectRatio: 0.65,
        crossAxisSpacing: 10,
        mainAxisSpacing: 10,
      ),
      itemCount: games.length,
      itemBuilder: (ctx, i) => GameCard(
        game: games[i],
        onTap: () => _showGameActions(context, games[i]),
        onLongPress: () => _confirmDelete(context, games[i]),
      ),
    );
  }

  Widget _buildList(List<GameEntry> games) {
    return ListView.separated(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 100),
      itemCount: games.length,
      separatorBuilder: (_, __) => const SizedBox(height: 6),
      itemBuilder: (ctx, i) => GameCard(
        game: games[i],
        listMode: true,
        onTap: () => _showGameActions(context, games[i]),
        onLongPress: () => _confirmDelete(context, games[i]),
      ),
    );
  }

  Widget _buildNoResults() {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.search_off_rounded, size: 56, color: Colors.white12),
          const SizedBox(height: 16),
          Text('No games match your search', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          TextButton(
            onPressed: () {
              _searchController.clear();
              setState(() {
                _searchQuery = '';
                _filterStatus = null;
              });
            },
            child: const Text('Clear filters'),
          ),
        ],
      ),
    );
  }

  List<GameEntry> _applyFilter(List<GameEntry> games) {
    var result = games;
    if (_searchQuery.isNotEmpty) {
      result = result.where((g) =>
          g.title.toLowerCase().contains(_searchQuery) ||
          g.titleId.toLowerCase().contains(_searchQuery)).toList();
    }
    if (_filterStatus != null) {
      result = result.where((g) => g.compatStatus == _filterStatus).toList();
    }
    switch (_sortMode) {
      case _SortMode.lastPlayed:
        result.sort((a, b) => (b.lastPlayedAt ?? '').compareTo(a.lastPlayedAt ?? ''));
      case _SortMode.title:
        result.sort((a, b) => a.title.compareTo(b.title));
      case _SortMode.playtime:
        result.sort((a, b) => b.totalPlaytimeSeconds.compareTo(a.totalPlaytimeSeconds));
      case _SortMode.added:
        result.sort((a, b) => (b.installedAt ?? '').compareTo(a.installedAt ?? ''));
    }
    return result;
  }

  void _showGameActions(BuildContext context, GameEntry game) {
    showModalBottomSheet(
      context: context,
      builder: (ctx) => _GameActionsSheet(
        game: game,
        onPlay: () {
          Navigator.pop(ctx);
          ref.read(libraryProvider.notifier).updateLastPlayed(game.titleId);
          context.push('/emulation/${game.titleId}');
        },
        onSettings: () {
          Navigator.pop(ctx);
          context.push('/game-settings/${game.titleId}');
        },
        onDelete: () {
          Navigator.pop(ctx);
          _confirmDelete(context, game);
        },
      ),
    );
  }

  void _confirmDelete(BuildContext context, GameEntry game) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Remove Game'),
        content: Text('Remove "${game.title}" from your library? Game files will not be deleted.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          TextButton(
            onPressed: () {
              Navigator.pop(ctx);
              ref.read(libraryProvider.notifier).removeGame(game.titleId);
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text('${game.title} removed')),
              );
            },
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Remove'),
          ),
        ],
      ),
    );
  }

  String _statusLabel(CompatStatus s) => switch (s) {
    CompatStatus.playable => 'Playable',
    CompatStatus.ingame => 'In-Game',
    CompatStatus.boots => 'Boots',
    CompatStatus.nothing => 'Nothing',
    CompatStatus.untested => 'Untested',
  };

  Color _statusColor(CompatStatus s) => switch (s) {
    CompatStatus.playable => AppTheme.statusPlayable,
    CompatStatus.ingame => AppTheme.statusIngame,
    CompatStatus.boots => AppTheme.statusBoots,
    CompatStatus.nothing => AppTheme.statusNothing,
    CompatStatus.untested => AppTheme.statusUntested,
  };
}

enum _SortMode { lastPlayed, title, playtime, added }

class _SortButton extends StatelessWidget {
  final _SortMode current;
  final ValueChanged<_SortMode> onChanged;
  const _SortButton({required this.current, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return PopupMenuButton<_SortMode>(
      initialValue: current,
      onSelected: onChanged,
      color: AppTheme.surface,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12), side: const BorderSide(color: AppTheme.border)),
      tooltip: 'Sort',
      icon: const Icon(Icons.sort_rounded, color: Colors.white54),
      itemBuilder: (_) => [
        const PopupMenuItem(value: _SortMode.lastPlayed, child: Text('Last Played')),
        const PopupMenuItem(value: _SortMode.title, child: Text('Title')),
        const PopupMenuItem(value: _SortMode.playtime, child: Text('Playtime')),
        const PopupMenuItem(value: _SortMode.added, child: Text('Date Added')),
      ],
    );
  }
}

class _FilterChip extends StatelessWidget {
  final String label;
  final bool selected;
  final Color? color;
  final VoidCallback onTap;
  const _FilterChip({required this.label, required this.selected, this.color, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final c = color ?? AppTheme.xboxGreen;
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 7),
        decoration: BoxDecoration(
          color: selected ? c.withAlpha(40) : AppTheme.surfaceVariant,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: selected ? c : AppTheme.border),
        ),
        child: Text(
          label,
          style: TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w500,
            color: selected ? c : Colors.white54,
          ),
        ),
      ),
    );
  }
}

class _GameActionsSheet extends StatelessWidget {
  final GameEntry game;
  final VoidCallback onPlay, onSettings, onDelete;
  const _GameActionsSheet({required this.game, required this.onPlay, required this.onSettings, required this.onDelete});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(20, 8, 20, 32),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Container(
                width: 48, height: 48,
                decoration: BoxDecoration(
                  color: AppTheme.xboxGreen.withAlpha(30),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: AppTheme.xboxGreen.withAlpha(60)),
                ),
                child: const Icon(Icons.sports_esports, color: AppTheme.xboxGreenLight),
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(game.title, style: Theme.of(context).textTheme.titleLarge, maxLines: 1, overflow: TextOverflow.ellipsis),
                    Text(game.titleId, style: Theme.of(context).textTheme.bodySmall),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 20),
          _ActionTile(icon: Icons.play_arrow_rounded, label: 'Play', color: AppTheme.xboxGreen, onTap: onPlay),
          _ActionTile(icon: Icons.tune_rounded, label: 'Game Settings', onTap: onSettings),
          _ActionTile(icon: Icons.delete_outline_rounded, label: 'Remove from Library', color: Colors.red, onTap: onDelete),
        ],
      ),
    );
  }
}

class _ActionTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color? color;
  final VoidCallback onTap;
  const _ActionTile({required this.icon, required this.label, this.color, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, color: color ?? Colors.white70, size: 22),
      title: Text(label, style: TextStyle(color: color ?? Colors.white, fontSize: 15, fontWeight: FontWeight.w500)),
      onTap: onTap,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
    );
  }
}
