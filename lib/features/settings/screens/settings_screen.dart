import 'dart:io';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:path_provider/path_provider.dart';
import 'package:url_launcher/url_launcher.dart';
import '../../../core/theme/app_theme.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});
  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  String _version = '';
  String _cacheSize = '...';
  bool _fpsOverlay = true;
  bool _darkStatusBar = true;
  int _logLevel = 1;

  @override
  void initState() {
    super.initState();
    _loadInfo();
  }

  Future<void> _loadInfo() async {
    final info = await PackageInfo.fromPlatform();
    setState(() => _version = '${info.version}+${info.buildNumber}');
    _computeCacheSize();
  }

  Future<void> _computeCacheSize() async {
    try {
      final dir = await getTemporaryDirectory();
      int size = 0;
      await for (final f in dir.list(recursive: true)) {
        if (f is File) size += await f.length();
      }
      final mb = (size / (1024 * 1024)).toStringAsFixed(1);
      setState(() => _cacheSize = '$mb MB');
    } catch (_) {
      setState(() => _cacheSize = 'Unknown');
    }
  }

  Future<void> _clearCache() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Clear Cache'),
        content: const Text('This will delete compiled shader caches. Games may take longer to load after clearing.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancel')),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Clear'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    try {
      final dir = await getTemporaryDirectory();
      await dir.delete(recursive: true);
      await dir.create();
      setState(() => _cacheSize = '0.0 MB');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Cache cleared')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to clear cache: $e')),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bgBase,
      body: SafeArea(
        child: CustomScrollView(
          slivers: [
            SliverToBoxAdapter(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(20, 20, 20, 0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Settings', style: Theme.of(context).textTheme.displayMedium),
                    const SizedBox(height: 24),

                    // ─── DISPLAY ────────────────────────────────────────
                    _SectionLabel('Display'),
                    _SettingsCard(children: [
                      _SwitchTile(
                        icon: Icons.speed_rounded,
                        title: 'FPS Overlay',
                        subtitle: 'Show frame rate during gameplay',
                        value: _fpsOverlay,
                        onChanged: (v) => setState(() => _fpsOverlay = v),
                      ),
                      const _Divider(),
                      _SwitchTile(
                        icon: Icons.brightness_2_outlined,
                        title: 'Dark Status Bar',
                        subtitle: 'Dark icons in the status bar',
                        value: _darkStatusBar,
                        onChanged: (v) => setState(() => _darkStatusBar = v),
                      ),
                    ]),
                    const SizedBox(height: 16),

                    // ─── PROCESSOR ────────────────────────────────────
                    _SectionLabel('Processor'),
                    _SettingsCard(children: [
                      _SelectTile(
                        icon: Icons.tune_rounded,
                        title: 'Log Level',
                        subtitle: _logLevelName(_logLevel),
                        onTap: () => _showLogLevelDialog(),
                      ),
                    ]),
                    const SizedBox(height: 16),

                    // ─── STORAGE ──────────────────────────────────────
                    _SectionLabel('Storage'),
                    _SettingsCard(children: [
                      _SelectTile(
                        icon: Icons.folder_outlined,
                        title: 'Games Directory',
                        subtitle: 'Internal storage / Xbox360Recomp / games',
                        onTap: () {},
                      ),
                      const _Divider(),
                      _SelectTile(
                        icon: Icons.save_outlined,
                        title: 'Saves Directory',
                        subtitle: 'Internal storage / Xbox360Recomp / saves',
                        onTap: () {},
                      ),
                      const _Divider(),
                      _SelectTile(
                        icon: Icons.delete_sweep_outlined,
                        title: 'Clear Shader Cache',
                        subtitle: _cacheSize,
                        trailing: TextButton(
                          onPressed: _clearCache,
                          style: TextButton.styleFrom(foregroundColor: Colors.red),
                          child: const Text('Clear'),
                        ),
                        onTap: _clearCache,
                      ),
                    ]),
                    const SizedBox(height: 16),

                    // ─── COMPATIBILITY DB ─────────────────────────────
                    _SectionLabel('Compatibility'),
                    _SettingsCard(children: [
                      _SelectTile(
                        icon: Icons.cloud_sync_outlined,
                        title: 'Sync Compatibility Database',
                        subtitle: 'Update from GitHub',
                        onTap: _syncCompatDb,
                      ),
                      const _Divider(),
                      _SelectTile(
                        icon: Icons.open_in_new_rounded,
                        title: 'Browse Compatibility List',
                        subtitle: 'github.com/ViaXTrace/Xbox360RecompAndroid',
                        onTap: () => launchUrl(Uri.parse('https://github.com/ViaXTrace/Xbox360RecompAndroid/blob/main/compat/games.json')),
                      ),
                    ]),
                    const SizedBox(height: 16),

                    // ─── ABOUT ────────────────────────────────────────
                    _SectionLabel('About'),
                    _SettingsCard(children: [
                      _SelectTile(
                        icon: Icons.info_outline_rounded,
                        title: 'Xbox360RecompAndroid',
                        subtitle: 'Version $_version',
                        onTap: () => context.push('/about'),
                      ),
                      const _Divider(),
                      _SelectTile(
                        icon: Icons.code_rounded,
                        title: 'Source Code',
                        subtitle: 'Open on GitHub',
                        onTap: () => launchUrl(Uri.parse('https://github.com/ViaXTrace/Xbox360RecompAndroid')),
                      ),
                      const _Divider(),
                      _SelectTile(
                        icon: Icons.description_outlined,
                        title: 'Licenses',
                        subtitle: 'Open source components',
                        onTap: () => showLicensePage(context: context, applicationName: 'Xbox360RecompAndroid', applicationVersion: _version),
                      ),
                    ]),
                    const SizedBox(height: 100),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  String _logLevelName(int l) => ['Quiet', 'Error', 'Warning', 'Info', 'Debug', 'Trace'][l.clamp(0, 5)];

  Future<void> _showLogLevelDialog() async {
    final result = await showDialog<int>(
      context: context,
      builder: (_) => SimpleDialog(
        title: const Text('Log Level'),
        children: List.generate(6, (i) => SimpleDialogOption(
          onPressed: () => Navigator.pop(context, i),
          child: Text(_logLevelName(i)),
        )),
      ),
    );
    if (result != null) setState(() => _logLevel = result);
  }

  Future<void> _syncCompatDb() async {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Syncing compatibility database…')),
    );
  }
}

class _SectionLabel extends StatelessWidget {
  final String text;
  const _SectionLabel(this.text);
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, bottom: 8),
      child: Text(
        text.toUpperCase(),
        style: const TextStyle(
          fontSize: 11, fontWeight: FontWeight.w700,
          color: AppTheme.xboxGreen, letterSpacing: 1.2,
        ),
      ),
    );
  }
}

class _SettingsCard extends StatelessWidget {
  final List<Widget> children;
  const _SettingsCard({required this.children});
  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: AppTheme.surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppTheme.border),
      ),
      child: Column(children: children),
    );
  }
}

class _Divider extends StatelessWidget {
  const _Divider();
  @override
  Widget build(BuildContext context) {
    return const Divider(height: 1, indent: 16, endIndent: 0);
  }
}

class _SwitchTile extends StatelessWidget {
  final IconData icon;
  final String title, subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;
  const _SwitchTile({required this.icon, required this.title, required this.subtitle, required this.value, required this.onChanged});
  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, color: Colors.white54, size: 22),
      title: Text(title),
      subtitle: Text(subtitle),
      trailing: Switch(value: value, onChanged: onChanged),
      onTap: () => onChanged(!value),
    );
  }
}

class _SelectTile extends StatelessWidget {
  final IconData icon;
  final String title, subtitle;
  final VoidCallback onTap;
  final Widget? trailing;
  const _SelectTile({required this.icon, required this.title, required this.subtitle, required this.onTap, this.trailing});
  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, color: Colors.white54, size: 22),
      title: Text(title),
      subtitle: Text(subtitle),
      trailing: trailing ?? const Icon(Icons.chevron_right_rounded, color: Colors.white24, size: 20),
      onTap: onTap,
    );
  }
}
