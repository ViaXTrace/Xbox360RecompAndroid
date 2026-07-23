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
      ShellRoute(
        builder: (context, state, child) => _AppShell(child: child),
        routes: [
          GoRoute(
            path: '/',
            name: 'library',
            pageBuilder: (context, state) => const NoTransitionPage(child: LibraryScreen()),
          ),
          GoRoute(
            path: '/import',
            name: 'import',
            pageBuilder: (context, state) => const NoTransitionPage(child: ImportScreen()),
          ),
          GoRoute(
            path: '/settings',
            name: 'settings',
            pageBuilder: (context, state) => const NoTransitionPage(child: SettingsScreen()),
          ),
        ],
      ),
      GoRoute(
        path: '/emulation/:titleId',
        name: 'emulation',
        builder: (context, state) => EmulationScreen(
          titleId: state.pathParameters['titleId']!,
        ),
      ),
      GoRoute(
        path: '/game-settings/:titleId',
        name: 'game-settings',
        builder: (context, state) => GameSettingsScreen(
          titleId: state.pathParameters['titleId']!,
        ),
      ),
      GoRoute(
        path: '/controls-editor',
        name: 'controls-editor',
        builder: (context, state) => const ControlsEditorScreen(),
      ),
      GoRoute(
        path: '/about',
        name: 'about',
        builder: (context, state) => const AboutScreen(),
      ),
    ],
  );
});

class _AppShell extends StatefulWidget {
  final Widget child;
  const _AppShell({required this.child});

  @override
  State<_AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<_AppShell> {
  int _selectedIndex = 0;

  static const _tabs = [
    (path: '/', icon: Icons.sports_esports_outlined, activeIcon: Icons.sports_esports, label: 'Library'),
    (path: '/import', icon: Icons.add_circle_outline, activeIcon: Icons.add_circle, label: 'Import'),
    (path: '/settings', icon: Icons.settings_outlined, activeIcon: Icons.settings, label: 'Settings'),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: widget.child,
      bottomNavigationBar: NavigationBar(
        selectedIndex: _selectedIndex,
        onDestinationSelected: (i) {
          setState(() => _selectedIndex = i);
          context.go(_tabs[i].path);
        },
        destinations: _tabs.map((t) => NavigationDestination(
          icon: Icon(t.icon),
          selectedIcon: Icon(t.activeIcon),
          label: t.label,
        )).toList(),
      ),
    );
  }
}
