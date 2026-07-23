import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:url_launcher/url_launcher.dart';
import '../../../core/theme/app_theme.dart';

class AboutScreen extends StatefulWidget {
  const AboutScreen({super.key});
  @override
  State<AboutScreen> createState() => _AboutScreenState();
}

class _AboutScreenState extends State<AboutScreen> {
  String _version = '';

  @override
  void initState() {
    super.initState();
    PackageInfo.fromPlatform().then((i) => setState(() => _version = '${i.version}+${i.buildNumber}'));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bgBase,
      appBar: AppBar(
        title: const Text('About'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back_rounded),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            const SizedBox(height: 8),
            // App icon area
            Container(
              width: 96, height: 96,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: AppTheme.xboxGreen.withAlpha(20),
                border: Border.all(color: AppTheme.xboxGreen.withAlpha(80), width: 2),
              ),
              child: const Icon(Icons.sports_esports, size: 48, color: AppTheme.xboxGreen),
            ),
            const SizedBox(height: 16),
            const Text(
              'Xbox360Recomp',
              style: TextStyle(fontSize: 24, fontWeight: FontWeight.w800, color: Colors.white),
            ),
            const SizedBox(height: 4),
            Text(
              'Version $_version',
              style: const TextStyle(fontSize: 13, color: Colors.white38),
            ),
            const SizedBox(height: 6),
            const Text(
              'Universal Xbox 360 → Android recompiler',
              style: TextStyle(fontSize: 13, color: Colors.white54),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 32),
            // Description
            _InfoCard(children: [
              const _InfoText(
                'Xbox360RecompAndroid is an open-source project that dynamically recompiles PowerPC Xbox 360 instructions to ARM64 for execution on Android devices.\n\n'
                'It uses a custom JIT engine, HLE (High-Level Emulation) kernel stubs for Xbox OS calls, and a Xenos → Vulkan GPU layer for rendering.',
              ),
            ]),
            const SizedBox(height: 16),
            // Architecture
            _SectionHeader('Architecture'),
            _InfoCard(children: [
              _TechRow(icon: Icons.memory, label: 'JIT Engine', desc: 'PowerPC → ARM64 dynamic recompiler'),
              const _ItemDivider(),
              _TechRow(icon: Icons.settings_input_component, label: 'HLE Kernel', desc: 'Xbox 360 kernel call stubs'),
              const _ItemDivider(),
              _TechRow(icon: Icons.view_in_ar, label: 'GPU Layer', desc: 'Xenos PM4 → Vulkan renderer'),
              const _ItemDivider(),
              _TechRow(icon: Icons.folder_zip_outlined, label: 'Loader', desc: 'XEX, ISO (XDVDFS), STFS parser'),
            ]),
            const SizedBox(height: 16),
            // Inspired by
            _SectionHeader('Standing on the Shoulders of Giants'),
            _InfoCard(children: [
              _LinkRow(label: 'Xenia', desc: 'Xbox 360 emulator (reference)', url: 'https://xenia.jp'),
              const _ItemDivider(),
              _LinkRow(label: 'N64Recomp', desc: 'Static recompilation approach', url: 'https://github.com/N64Recomp/N64Recomp'),
              const _ItemDivider(),
              _LinkRow(label: 'Flutter', desc: 'Cross-platform UI framework', url: 'https://flutter.dev'),
            ]),
            const SizedBox(height: 16),
            // Links
            _SectionHeader('Links'),
            _InfoCard(children: [
              _LinkRow(label: 'Source Code', desc: 'github.com/ViaXTrace/Xbox360RecompAndroid', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid'),
              const _ItemDivider(),
              _LinkRow(label: 'Compatibility Database', desc: 'Report or browse game compatibility', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid/blob/main/compat/games.json'),
              const _ItemDivider(),
              _LinkRow(label: 'Report a Bug', desc: 'Open an issue on GitHub', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid/issues/new'),
            ]),
            const SizedBox(height: 16),
            // Legal
            _SectionHeader('Legal'),
            _InfoCard(children: [
              const _InfoText(
                'Xbox360RecompAndroid is distributed under the GNU General Public License v2.0. '
                'This software does not include any Microsoft proprietary code, Xbox 360 firmware, or game files. '
                'You must provide your own legally obtained game files.\n\n'
                '"Xbox 360" is a trademark of Microsoft Corporation. This project is not affiliated with or endorsed by Microsoft.',
              ),
              const _ItemDivider(),
              ListTile(
                leading: const Icon(Icons.description_outlined, color: Colors.white54, size: 20),
                title: const Text('Open Source Licenses', style: TextStyle(fontSize: 14, color: Colors.white)),
                trailing: const Icon(Icons.chevron_right_rounded, color: Colors.white24, size: 20),
                onTap: () => showLicensePage(context: context, applicationName: 'Xbox360RecompAndroid', applicationVersion: _version),
              ),
            ]),
            const SizedBox(height: 40),
            const Text(
              '© 2024 ViaXTrace • GPL-2.0',
              style: TextStyle(fontSize: 11, color: Colors.white24),
            ),
            const SizedBox(height: 24),
          ],
        ),
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  final String text;
  const _SectionHeader(this.text);
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, bottom: 8),
      child: Align(
        alignment: Alignment.centerLeft,
        child: Text(text.toUpperCase(),
          style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w700, color: AppTheme.xboxGreen, letterSpacing: 1.2)),
      ),
    );
  }
}

class _InfoCard extends StatelessWidget {
  final List<Widget> children;
  const _InfoCard({required this.children});
  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      decoration: BoxDecoration(
        color: AppTheme.surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppTheme.border),
      ),
      child: Column(children: children),
    );
  }
}

class _InfoText extends StatelessWidget {
  final String text;
  const _InfoText(this.text);
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Text(text, style: const TextStyle(fontSize: 13, color: Colors.white60, height: 1.6)),
    );
  }
}

class _ItemDivider extends StatelessWidget {
  const _ItemDivider();
  @override
  Widget build(BuildContext context) => const Divider(height: 1, indent: 16);
}

class _TechRow extends StatelessWidget {
  final IconData icon;
  final String label, desc;
  const _TechRow({required this.icon, required this.label, required this.desc});
  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, color: AppTheme.xboxGreen, size: 20),
      title: Text(label, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600, color: Colors.white)),
      subtitle: Text(desc, style: const TextStyle(fontSize: 12, color: Colors.white54)),
    );
  }
}

class _LinkRow extends StatelessWidget {
  final String label, desc, url;
  const _LinkRow({required this.label, required this.desc, required this.url});
  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: const Icon(Icons.open_in_new_rounded, color: Colors.white54, size: 18),
      title: Text(label, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600, color: Colors.white)),
      subtitle: Text(desc, style: const TextStyle(fontSize: 11, color: Colors.white38)),
      onTap: () => launchUrl(Uri.parse(url), mode: LaunchMode.externalApplication),
    );
  }
}
