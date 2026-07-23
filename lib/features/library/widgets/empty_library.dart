import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

class EmptyLibrary extends StatelessWidget {
  const EmptyLibrary({super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(40),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Container(
              width: 120, height: 120,
              decoration: BoxDecoration(
                color: Colors.white.withOpacity(0.05),
                shape: BoxShape.circle,
              ),
              child: const Icon(Icons.sports_esports, size: 60, color: Color(0xFF107C10)),
            ),
            const SizedBox(height: 24),
            const Text(
              'No games yet',
              style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700, color: Colors.white),
            ),
            const SizedBox(height: 12),
            const Text(
              'Import your Xbox 360 game dumps (XEX, ISO, or STFS packages) to get started.',
              textAlign: TextAlign.center,
              style: TextStyle(fontSize: 14, color: Colors.white54, height: 1.5),
            ),
            const SizedBox(height: 32),
            ElevatedButton.icon(
              onPressed: () => context.push('/import'),
              icon: const Icon(Icons.folder_open),
              label: const Text('Import Game'),
            ),
          ],
        ),
      ),
    );
  }
}
