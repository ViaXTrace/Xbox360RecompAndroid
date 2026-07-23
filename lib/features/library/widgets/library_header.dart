import 'package:flutter/material.dart';
import '../../../core/models/game_entry.dart';

class LibraryHeader extends StatefulWidget {
  final VoidCallback onImport;
  final VoidCallback onSettings;
  final VoidCallback onAbout;
  final String searchQuery;
  final ValueChanged<String> onSearchChanged;
  final bool gridView;
  final VoidCallback onToggleView;
  final CompatStatus? filterStatus;
  final ValueChanged<CompatStatus?> onFilterChanged;

  const LibraryHeader({
    super.key,
    required this.onImport,
    required this.onSettings,
    required this.onAbout,
    required this.searchQuery,
    required this.onSearchChanged,
    required this.gridView,
    required this.onToggleView,
    required this.filterStatus,
    required this.onFilterChanged,
  });

  @override
  State<LibraryHeader> createState() => _LibraryHeaderState();
}

class _LibraryHeaderState extends State<LibraryHeader> {
  final _search = TextEditingController();

  @override
  void dispose() {
    _search.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(16, 56, 16, 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              // Xbox circle logo
              Container(
                width: 36, height: 36,
                decoration: const BoxDecoration(
                  color: Color(0xFF107C10),
                  shape: BoxShape.circle,
                ),
                child: const Icon(Icons.sports_esports, size: 20, color: Colors.white),
              ),
              const SizedBox(width: 10),
              const Text(
                'Xbox360Recomp',
                style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700, color: Colors.white),
              ),
              const Spacer(),
              IconButton(
                onPressed: widget.onAbout,
                icon: const Icon(Icons.info_outline, size: 20),
                color: Colors.white54,
              ),
              IconButton(
                onPressed: widget.onSettings,
                icon: const Icon(Icons.settings, size: 20),
                color: Colors.white54,
              ),
            ],
          ),
          const SizedBox(height: 16),
          // Search bar
          TextField(
            controller: _search,
            onChanged: widget.onSearchChanged,
            style: const TextStyle(color: Colors.white, fontSize: 14),
            decoration: InputDecoration(
              hintText: 'Search games...',
              hintStyle: const TextStyle(color: Colors.white38),
              prefixIcon: const Icon(Icons.search, color: Colors.white38, size: 20),
              suffixIcon: _search.text.isNotEmpty
                  ? IconButton(
                      icon: const Icon(Icons.clear, size: 18, color: Colors.white38),
                      onPressed: () { _search.clear(); widget.onSearchChanged(''); },
                    )
                  : null,
              filled: true,
              fillColor: const Color(0xFF252525),
              contentPadding: const EdgeInsets.symmetric(vertical: 12),
              border: OutlineInputBorder(
                borderRadius: BorderRadius.circular(10),
                borderSide: BorderSide.none,
              ),
            ),
          ),
          const SizedBox(height: 10),
          // Filter chips + view toggle
          Row(
            children: [
              _FilterChip(label: 'All', selected: widget.filterStatus == null,
                onTap: () => widget.onFilterChanged(null)),
              const SizedBox(width: 6),
              _FilterChip(label: 'Playable', selected: widget.filterStatus == CompatStatus.playable,
                onTap: () => widget.onFilterChanged(CompatStatus.playable), color: const Color(0xFF107C10)),
              const SizedBox(width: 6),
              _FilterChip(label: 'In-game', selected: widget.filterStatus == CompatStatus.ingame,
                onTap: () => widget.onFilterChanged(CompatStatus.ingame), color: const Color(0xFFF59E0B)),
              const Spacer(),
              IconButton(
                onPressed: widget.onToggleView,
                icon: Icon(widget.gridView ? Icons.view_list : Icons.grid_view, size: 20),
                color: Colors.white54,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints(minWidth: 32, minHeight: 32),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _FilterChip extends StatelessWidget {
  final String label;
  final bool selected;
  final VoidCallback onTap;
  final Color? color;

  const _FilterChip({required this.label, required this.selected, required this.onTap, this.color});

  @override
  Widget build(BuildContext context) {
    final c = color ?? Colors.white;
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        decoration: BoxDecoration(
          color: selected ? c.withOpacity(0.2) : Colors.white.withOpacity(0.05),
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: selected ? c.withOpacity(0.6) : Colors.white12),
        ),
        child: Text(label, style: TextStyle(fontSize: 12, color: selected ? c : Colors.white54, fontWeight: FontWeight.w500)),
      ),
    );
  }
}
