import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../features/library/screens/library_screen.dart';
import '../../features/import/screens/import_screen.dart';
import '../../features/settings/screens/settings_screen.dart';
import '../../features/game_settings/screens/game_settings_screen.dart';
import '../../features/emulation/screens/emulation_screen.dart';
import '../../features/controls/screens/controls_editor_screen.dart';
import '../../features/about/screens/about_screen.dart';

final appRouterProvider = Provider<GoRouter>((ref) {
  return GoRouter(
    initialLocation: '/',
    routes: [
      GoRoute(
        path: '/',
        name: 'library',
        builder: (context, state) => const LibraryScreen(),
      ),
      GoRoute(
        path: '/import',
        name: 'import',
        builder: (context, state) => const ImportScreen(),
      ),
      GoRoute(
        path: '/settings',
        name: 'settings',
        builder: (context, state) => const SettingsScreen(),
      ),
      GoRoute(
        path: '/game/:titleId/settings',
        name: 'game-settings',
        builder: (context, state) {
          final titleId = state.pathParameters['titleId']!;
          return GameSettingsScreen(titleId: titleId);
        },
      ),
      GoRoute(
        path: '/game/:titleId/run',
        name: 'emulation',
        builder: (context, state) {
          final titleId = state.pathParameters['titleId']!;
          return EmulationScreen(titleId: titleId);
        },
      ),
      GoRoute(
        path: '/game/:titleId/controls',
        name: 'controls-editor',
        builder: (context, state) {
          final titleId = state.pathParameters['titleId']!;
          return ControlsEditorScreen(titleId: titleId);
        },
      ),
      GoRoute(
        path: '/about',
        name: 'about',
        builder: (context, state) => const AboutScreen(),
      ),
    ],
    errorBuilder: (context, state) => Scaffold(
      body: Center(
        child: Text('Route not found: ${state.uri}'),
      ),
    ),
  );
});
