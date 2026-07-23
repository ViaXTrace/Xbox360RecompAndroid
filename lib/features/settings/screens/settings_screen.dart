import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:url_launcher/url_launcher.dart';

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _Section(title: 'General', children: [
            _SettingTile(icon: Icons.language, title: 'Language', subtitle: 'System default', onTap: () {}),
            _SettingTile(icon: Icons.cloud_download, title: 'Check for Updates', subtitle: 'Current: v0.1.0', onTap: () {}),
            _SettingTile(icon: Icons.database, title: 'Compatibility Database', subtitle: 'Sync from GitHub', onTap: () {}),
          ]),
          const SizedBox(height: 16),
          _Section(title: 'Default Game Settings', children: [
            _SettingTile(icon: Icons.display_settings, title: 'Graphics', subtitle: 'Resolution, AA, AF, VSync', onTap: () {}),
            _SettingTile(icon: Icons.memory, title: 'Processor', subtitle: 'JIT threads, optimization', onTap: () {}),
            _SettingTile(icon: Icons.volume_up, title: 'Audio', subtitle: 'Backend, buffer size', onTap: () {}),
            _SettingTile(icon: Icons.sports_esports, title: 'Default Controls', subtitle: 'Touch layout template', onTap: () {}),
          ]),
          const SizedBox(height: 16),
          _Section(title: 'Storage', children: [
            _SettingTile(icon: Icons.folder, title: 'Games Directory', subtitle: 'Internal storage / games', onTap: () {}),
            _SettingTile(icon: Icons.save, title: 'Saves Directory', subtitle: 'Internal storage / saves', onTap: () {}),
            _SettingTile(icon: Icons.delete_sweep, title: 'Clear Cache', subtitle: 'Compiled shader cache', onTap: () {}),
          ]),
          const SizedBox(height: 16),
          _Section(title: 'Debug', children: [
            _SettingTile(icon: Icons.bug_report, title: 'Log Level', subtitle: 'Info', onTap: () {}),
            _SettingTile(icon: Icons.share, title: 'Export Session Log', subtitle: 'Share last session log', onTap: () {}),
          ]),
          const SizedBox(height: 16),
          _Section(title: 'About', children: [
            _SettingTile(icon: Icons.info_outline, title: 'Version', subtitle: 'Xbox360RecompAndroid v0.1.0', onTap: () {}),
            _SettingTile(icon: Icons.code, title: 'Source Code', subtitle: 'github.com/ViaXTrace/Xbox360RecompAndroid',
              onTap: () => launchUrl(Uri.parse('https://github.com/ViaXTrace/Xbox360RecompAndroid'))),
            _SettingTile(icon: Icons.list_alt, title: 'Compatibility Database', subtitle: 'Report or browse game compatibility',
              onTap: () => launchUrl(Uri.parse('https://github.com/ViaXTrace/Xbox360RecompAndroid/blob/main/compat/games.json'))),
            _SettingTile(icon: Icons.gavel, title: 'License', subtitle: 'GPL-2.0', onTap: () => context.push('/about')),
          ]),
        ],
      ),
    );
  }
}

class _Section extends StatelessWidget {
  final String title;
  final List<Widget> children;
  const _Section({required this.title, required this.children});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 4, bottom: 8),
          child: Text(title.toUpperCase(),
            style: const TextStyle(fontSize: 11, letterSpacing: 1.2, color: Color(0xFF107C10), fontWeight: FontWeight.w700)),
        ),
        Container(
          decoration: BoxDecoration(
            color: const Color(0xFF252525),
            borderRadius: BorderRadius.circular(12),
          ),
          child: Column(
            children: [
              for (int i = 0; i < children.length; i++) ...[
                children[i],
                if (i < children.length - 1)
                  const Divider(height: 1, indent: 56, endIndent: 0, color: Colors.white08),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _SettingTile extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;
  final Widget? trailing;

  const _SettingTile({required this.icon, required this.title, required this.subtitle, required this.onTap, this.trailing});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Container(
        width: 36, height: 36,
        decoration: BoxDecoration(color: Colors.white08, borderRadius: BorderRadius.circular(8)),
        child: Icon(icon, size: 18, color: Colors.white70),
      ),
      title: Text(title, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500, color: Colors.white)),
      subtitle: Text(subtitle, style: const TextStyle(fontSize: 12, color: Colors.white38)),
      trailing: trailing ?? const Icon(Icons.chevron_right, color: Colors.white24, size: 18),
      onTap: onTap,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
    );
  }
}
