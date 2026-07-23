import 'dart:io';
import 'package:flutter/material.dart';
import '../../../core/models/game_entry.dart';

class GameCard extends StatelessWidget {
  final GameEntry game;
  final bool listMode;
  final VoidCallback onTap;

  const GameCard({
    super.key,
    required this.game,
    this.listMode = false,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return listMode ? _buildListTile(context) : _buildGridCard(context);
  }

  Widget _buildGridCard(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0xFF252525),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: Colors.white.withOpacity(0.08)),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              child: ClipRRect(
                borderRadius: const BorderRadius.vertical(top: Radius.circular(12)),
                child: _buildArtwork(),
              ),
            ),
            Padding(
              padding: const EdgeInsets.all(10),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    game.title,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: Colors.white),
                  ),
                  const SizedBox(height: 4),
                  Row(
                    children: [
                      _CompatBadge(status: game.compatStatus),
                      const Spacer(),
                      Text(
                        game.region,
                        style: const TextStyle(fontSize: 10, color: Colors.white38),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildListTile(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        margin: const EdgeInsets.only(bottom: 8),
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: const Color(0xFF252525),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: Colors.white.withOpacity(0.08)),
        ),
        child: Row(
          children: [
            ClipRRect(
              borderRadius: BorderRadius.circular(8),
              child: SizedBox(width: 56, height: 72, child: _buildArtwork()),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    game.title,
                    style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600, color: Colors.white),
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                  const SizedBox(height: 4),
                  Text(
                    '${game.titleId} · ${game.region} · ${game.displaySize}',
                    style: const TextStyle(fontSize: 11, color: Colors.white38),
                  ),
                  const SizedBox(height: 6),
                  Row(
                    children: [
                      _CompatBadge(status: game.compatStatus),
                      const SizedBox(width: 8),
                      Text(
                        game.playtimeDisplay,
                        style: const TextStyle(fontSize: 11, color: Colors.white38),
                      ),
                    ],
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

  Widget _buildArtwork() {
    if (game.artworkPath != null) {
      final f = File(game.artworkPath!);
      if (f.existsSync()) {
        return Image.file(f, fit: BoxFit.cover);
      }
    }
    // Placeholder artwork
    return Container(
      color: const Color(0xFF1A1A1A),
      child: Stack(
        alignment: Alignment.center,
        children: [
          Icon(
            Icons.sports_esports,
            size: 40,
            color: Colors.white.withOpacity(0.08),
          ),
          Positioned(
            bottom: 8,
            left: 8,
            right: 8,
            child: Text(
              game.title,
              textAlign: TextAlign.center,
              maxLines: 3,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 10, color: Colors.white38),
            ),
          ),
        ],
      ),
    );
  }
}

class _CompatBadge extends StatelessWidget {
  final CompatStatus status;
  const _CompatBadge({required this.status});

  @override
  Widget build(BuildContext context) {
    final (label, color) = switch (status) {
      CompatStatus.playable => ('Playable', const Color(0xFF107C10)),
      CompatStatus.ingame => ('In-game', const Color(0xFFF59E0B)),
      CompatStatus.boots => ('Boots', const Color(0xFFEF4444)),
      CompatStatus.nothing => ('Broken', const Color(0xFF6B7280)),
      CompatStatus.untested => ('Untested', const Color(0xFF374151)),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      decoration: BoxDecoration(
        color: color.withOpacity(0.2),
        borderRadius: BorderRadius.circular(4),
        border: Border.all(color: color.withOpacity(0.5)),
      ),
      child: Text(label, style: TextStyle(fontSize: 9, color: color, fontWeight: FontWeight.w600)),
    );
  }
}
