import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

class AboutScreen extends StatelessWidget {
  const AboutScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('About')),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          Center(
            child: Column(
              children: [
                Container(
                  width: 96, height: 96,
                  decoration: const BoxDecoration(color: Color(0xFF107C10), shape: BoxShape.circle),
                  child: const Icon(Icons.sports_esports, size: 52, color: Colors.white),
                ),
                const SizedBox(height: 16),
                const Text('Xbox360RecompAndroid', style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700, color: Colors.white)),
                const SizedBox(height: 4),
                const Text('v0.1.0 — Phase 1 MVP', style: TextStyle(fontSize: 13, color: Colors.white38)),
                const SizedBox(height: 8),
                const Text(
                  'Universal Xbox 360 → Android recompiler.\nDynamic PowerPC JIT + HLE + Xenos→Vulkan.',
                  textAlign: TextAlign.center,
                  style: TextStyle(fontSize: 13, color: Colors.white54, height: 1.5),
                ),
              ],
            ),
          ),
          const SizedBox(height: 32),
          const _InfoCard(
            title: 'License',
            content: 'GPL-2.0 — This project is free and open-source. All engine code (JIT, HLE, GPU layer) is written from scratch without any proprietary Microsoft source code.',
          ),
          const SizedBox(height: 12),
          const _InfoCard(
            title: 'Legal Notice',
            content: 'This app does NOT include any game files, BIOS, Xbox 360 dashboard, or proprietary Microsoft content. You must supply a legally obtained copy of any game you wish to run. Developed for preservation and reverse engineering research.',
          ),
          const SizedBox(height: 12),
          _LinkCard(title: 'Source Code', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid',
            subtitle: 'github.com/ViaXTrace/Xbox360RecompAndroid'),
          const SizedBox(height: 12),
          _LinkCard(title: 'Compatibility Database', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid/blob/main/compat/games.json',
            subtitle: 'Report or browse game compatibility'),
          const SizedBox(height: 12),
          _LinkCard(title: 'Submit a Bug Report', url: 'https://github.com/ViaXTrace/Xbox360RecompAndroid/issues',
            subtitle: 'Open an issue on GitHub'),
          const SizedBox(height: 24),
          const Text('Built on the shoulders of giants:', style: TextStyle(fontSize: 13, color: Colors.white38, fontWeight: FontWeight.w600)),
          const SizedBox(height: 8),
          for (final ref in _refs)
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: GestureDetector(
                onTap: () => launchUrl(Uri.parse(ref.url)),
                child: Row(children: [
                  const Icon(Icons.open_in_new, size: 14, color: Colors.white24),
                  const SizedBox(width: 8),
                  Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                    Text(ref.name, style: const TextStyle(fontSize: 13, color: Color(0xFF52B043))),
                    Text(ref.desc, style: const TextStyle(fontSize: 11, color: Colors.white38)),
                  ])),
                ]),
              ),
            ),
        ],
      ),
    );
  }
}

class _InfoCard extends StatelessWidget {
  final String title, content;
  const _InfoCard({required this.title, required this.content});

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(16),
    decoration: BoxDecoration(color: const Color(0xFF252525), borderRadius: BorderRadius.circular(12)),
    child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Text(title, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w700, color: Color(0xFF107C10))),
      const SizedBox(height: 8),
      Text(content, style: const TextStyle(fontSize: 13, color: Colors.white54, height: 1.5)),
    ]),
  );
}

class _LinkCard extends StatelessWidget {
  final String title, subtitle, url;
  const _LinkCard({required this.title, required this.subtitle, required this.url});

  @override
  Widget build(BuildContext context) => GestureDetector(
    onTap: () => launchUrl(Uri.parse(url)),
    child: Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(color: const Color(0xFF252525), borderRadius: BorderRadius.circular(12)),
      child: Row(children: [
        const Icon(Icons.open_in_new, size: 18, color: Color(0xFF107C10)),
        const SizedBox(width: 12),
        Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(title, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600, color: Colors.white)),
          Text(subtitle, style: const TextStyle(fontSize: 12, color: Colors.white38)),
        ])),
      ]),
    ),
  );
}

class _Ref { final String name, desc, url; const _Ref(this.name, this.desc, this.url); }
const _refs = [
  _Ref('SansNope/UnleashedRecomp-Android', 'Direct Android HLE + GPU reference', 'https://github.com/SansNope/UnleashedRecomp-Android'),
  _Ref('xenia-project/xenia', 'XEX loader, HLE, PM4, shader recompiler reference', 'https://github.com/xenia-project/xenia'),
  _Ref('Mr-Wiseguy/N64Recomp', 'Universal recompilation pipeline architecture', 'https://github.com/Mr-Wiseguy/N64Recomp'),
  _Ref('libadrenotools', 'Custom Vulkan driver loading for Adreno', 'https://github.com/K11MCH1/AdrenoToolsDrivers'),
  _Ref('KhronosGroup/SPIRV-Tools', 'SPIR-V generation and optimization', 'https://github.com/KhronosGroup/SPIRV-Tools'),
];
