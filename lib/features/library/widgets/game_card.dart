import 'dart:io';
import 'package:flutter/material.dart';
import '../../../core/models/game_entry.dart';
import '../../../core/theme/app_theme.dart';

class GameCard extends StatelessWidget {
  final GameEntry game;
  final bool listMode;
  final VoidCallback onTap;
  final VoidCallback? onLongPress;

  const GameCard({
    super.key,
    required this.game,
    this.listMode = false,
    required this.onTap,
    this.onLongPress,
  });

  @override
  Widget build(BuildContext context) {
    return listMode ? _buildListCard(context) : _buildGridCard(context);
  }

  Widget _buildGridCard(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      onLongPress: onLongPress,
      child: Container(
        decoration: BoxDecoration(
          color: AppTheme.card,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: AppTheme.border),
        ),
        clipBehavior: Clip.antiAlias,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              flex: 5,
              child: Stack(
                fit: StackFit.expand,
                children: [
                  _ArtworkWidget(artworkPath: game.artworkPath),
                  Positioned(
                    top: 8, right: 8,
                    child: _CompatBadge(status: game.compatStatus),
                  ),
                  if (game.lastPlayedAt != null)
                    Positioned(
                      bottom: 0, left: 0, right: 0,
                      child: Container(
                        height: 36,
                        decoration: const BoxDecoration(
                          gradient: LinearGradient(
                            begin: Alignment.bottomCenter,
                            end: Alignment.topCenter,
                            colors: [Color(0xDD000000), Colors.transparent],
                          ),
                        ),
                      ),
                    ),
                ],
              ),
            ),
            Expanded(
              flex: 3,
              child: Padding(
                padding: const EdgeInsets.all(10),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      game.title,
                      style: const TextStyle(
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                        color: Colors.white,
                      ),
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                    ),
                    const Spacer(),
                    Row(
                      children: [
                        Icon(Icons.timer_outlined, size: 11, color: Colors.white38),
                        const SizedBox(width: 3),
                        Text(
                          game.playtimeDisplay,
                          style: const TextStyle(fontSize: 10, color: Colors.white38),
                        ),
                        const Spacer(),
                        Text(
                          game.format.name.toUpperCase(),
                          style: const TextStyle(
                            fontSize: 9, fontWeight: FontWeight.w700,
                            color: AppTheme.xboxGreen, letterSpacing: 0.5,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildListCard(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      onLongPress: onLongPress,
      child: Container(
        decoration: BoxDecoration(
          color: AppTheme.card,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: AppTheme.border),
        ),
        clipBehavior: Clip.antiAlias,
        child: Row(
          children: [
            SizedBox(
              width: 64, height: 76,
              child: _ArtworkWidget(artworkPath: game.artworkPath),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(vertical: 12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      game.title,
                      style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600, color: Colors.white),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        _CompatBadge(status: game.compatStatus, compact: true),
                        const SizedBox(width: 8),
                        Text(
                          game.format.name.toUpperCase(),
                          style: const TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: AppTheme.xboxGreen, letterSpacing: 0.5),
                        ),
                      ],
                    ),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Icon(Icons.timer_outlined, size: 12, color: Colors.white38),
                        const SizedBox(width: 4),
                        Text(game.playtimeDisplay, style: const TextStyle(fontSize: 11, color: Colors.white38)),
                        if (game.displaySize != 'Unknown') ...[
                          const SizedBox(width: 10),
                          Icon(Icons.storage_outlined, size: 12, color: Colors.white38),
                          const SizedBox(width: 4),
                          Text(game.displaySize, style: const TextStyle(fontSize: 11, color: Colors.white38)),
                        ],
                      ],
                    ),
                  ],
                ),
              ),
            ),
            const Padding(
              padding: EdgeInsets.only(right: 12),
              child: Icon(Icons.chevron_right_rounded, color: Colors.white24, size: 20),
            ),
          ],
        ),
      ),
    );
  }
}

class _ArtworkWidget extends StatelessWidget {
  final String? artworkPath;
  const _ArtworkWidget({this.artworkPath});

  @override
  Widget build(BuildContext context) {
    if (artworkPath != null) {
      final file = File(artworkPath!);
      if (file.existsSync()) {
        return Image.file(file, fit: BoxFit.cover);
      }
    }
    return Container(
      color: const Color(0xFF111111),
      child: Stack(
        alignment: Alignment.center,
        children: [
          Positioned.fill(
            child: CustomPaint(painter: _GridPainter()),
          ),
          Container(
            width: 40, height: 40,
            decoration: BoxDecoration(
              color: AppTheme.xboxGreen.withAlpha(20),
              shape: BoxShape.circle,
              border: Border.all(color: AppTheme.xboxGreen.withAlpha(60), width: 1.5),
            ),
            child: const Icon(Icons.sports_esports, color: AppTheme.xboxGreen, size: 20),
          ),
        ],
      ),
    );
  }
}

class _GridPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = const Color(0xFF1A1A1A)
      ..strokeWidth = 0.5;
    const step = 20.0;
    for (double x = 0; x < size.width; x += step) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), paint);
    }
    for (double y = 0; y < size.height; y += step) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
    }
  }
  @override
  bool shouldRepaint(_) => false;
}

class _CompatBadge extends StatelessWidget {
  final CompatStatus status;
  final bool compact;
  const _CompatBadge({required this.status, this.compact = false});

  @override
  Widget build(BuildContext context) {
    final (label, color) = switch (status) {
      CompatStatus.playable  => ('Playable', AppTheme.statusPlayable),
      CompatStatus.ingame    => ('In-Game', AppTheme.statusIngame),
      CompatStatus.boots     => ('Boots', AppTheme.statusBoots),
      CompatStatus.nothing   => ('Nothing', AppTheme.statusNothing),
      CompatStatus.untested  => ('Untested', AppTheme.statusUntested),
    };

    if (compact) {
      return Container(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
        decoration: BoxDecoration(
          color: color.withAlpha(30),
          borderRadius: BorderRadius.circular(4),
          border: Border.all(color: color.withAlpha(80), width: 0.5),
        ),
        child: Text(label, style: TextStyle(fontSize: 9, fontWeight: FontWeight.w700, color: color, letterSpacing: 0.3)),
      );
    }

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
      decoration: BoxDecoration(
        color: Colors.black.withAlpha(160),
        borderRadius: BorderRadius.circular(5),
        border: Border.all(color: color.withAlpha(120), width: 0.5),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(width: 5, height: 5, decoration: BoxDecoration(color: color, shape: BoxShape.circle)),
          const SizedBox(width: 4),
          Text(label, style: TextStyle(fontSize: 9, fontWeight: FontWeight.w700, color: color, letterSpacing: 0.3)),
        ],
      ),
    );
  }
}
