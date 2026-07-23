import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../library/providers/library_provider.dart';

class GameSettingsScreen extends ConsumerStatefulWidget {
  final String titleId;
  const GameSettingsScreen({super.key, required this.titleId});

  @override
  ConsumerState<GameSettingsScreen> createState() => _GameSettingsScreenState();
}

class _GameSettingsScreenState extends ConsumerState<GameSettingsScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;

  // Graphics settings
  int _internalWidth = 1280;
  int _internalHeight = 720;
  int _aa = 0; // 0=off 1=FXAA 2=TAA
  int _af = 4; // 1/2/4/8/16
  bool _vsync = true;
  int _fpsLimit = 60;
  int _presentMode = 0; // 0=fit 1=stretch 2=letterbox

  // Processor settings
  int _jitThreads = 4;
  int _jitMode = 0; // 0=lazy 1=eager
  int _irOpt = 1; // 0=none 1=basic 2=full

  // Audio settings
  double _masterVolume = 1.0;
  int _audioBackend = 0; // 0=AAudio 1=OpenSL
  int _bufferSize = 2048;

  // Driver settings
  int _driverMode = 0; // 0=system 1=turnip 2=custom
  bool _bcNative = true;
  bool _debugLayers = false;

  // Debug
  int _logLevel = 1;
  bool _fpsOverlay = true;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 5, vsync: this);
  }

  @override
  void dispose() {
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final game = ref.watch(libraryProvider).valueOrNull
        ?.firstWhere((g) => g.titleId == widget.titleId,
          orElse: () => throw Exception('Game not found'));

    return Scaffold(
      appBar: AppBar(
        title: Text(game?.title ?? 'Game Settings', overflow: TextOverflow.ellipsis),
        leading: IconButton(icon: const Icon(Icons.arrow_back), onPressed: () => context.pop()),
        actions: [
          TextButton(onPressed: _saveSettings, child: const Text('Save', style: TextStyle(color: Color(0xFF107C10)))),
        ],
        bottom: TabBar(
          controller: _tabs,
          isScrollable: true,
          indicatorColor: const Color(0xFF107C10),
          labelColor: const Color(0xFF107C10),
          unselectedLabelColor: Colors.white54,
          labelStyle: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600),
          tabs: const [
            Tab(text: 'Graphics'),
            Tab(text: 'Processor'),
            Tab(text: 'Audio'),
            Tab(text: 'Driver'),
            Tab(text: 'Debug'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          _graphicsTab(),
          _processorTab(),
          _audioTab(),
          _driverTab(),
          _debugTab(),
        ],
      ),
    );
  }

  Widget _graphicsTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _SectionHeader('Internal Resolution'),
      Row(children: [
        Expanded(child: _IntField('Width', _internalWidth, (v) => setState(() => _internalWidth = v), 320, 3840)),
        const SizedBox(width: 12),
        Expanded(child: _IntField('Height', _internalHeight, (v) => setState(() => _internalHeight = v), 240, 2160)),
      ]),
      const SizedBox(height: 8),
      Wrap(spacing: 8, children: [
        for (final p in [('720p', 1280, 720), ('1080p', 1920, 1080), ('1440p', 2560, 1440), ('4K', 3840, 2160)])
          ActionChip(label: Text(p.$1), onPressed: () => setState(() { _internalWidth = p.$2; _internalHeight = p.$3; })),
      ]),
      const SizedBox(height: 20),
      _SectionHeader('Anti-Aliasing'),
      _SegmentedControl(['Off', 'FXAA', 'TAA'], _aa, (v) => setState(() => _aa = v)),
      const SizedBox(height: 16),
      _SectionHeader('Anisotropic Filtering'),
      _SegmentedControl(['1x', '2x', '4x', '8x', '16x'], [1,2,4,8,16].indexOf(_af), (v) => setState(() => _af = [1,2,4,8,16][v])),
      const SizedBox(height: 16),
      _SectionHeader('Presentation'),
      _SegmentedControl(['Fit', 'Stretch', 'Letterbox'], _presentMode, (v) => setState(() => _presentMode = v)),
      const SizedBox(height: 16),
      _SwitchTile('VSync', 'Synchronize to display refresh', _vsync, (v) => setState(() => _vsync = v)),
      _SliderTile('FPS Limit', _fpsLimit.toDouble(), 0, 240, 30, (v) => setState(() => _fpsLimit = v.round()),
        labels: {0.0: 'Unlimited', 30.0: '30', 60.0: '60', 120.0: '120'}),
    ]);
  }

  Widget _processorTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _SectionHeader('JIT Threads (1–6)'),
      _SliderTile('Threads', _jitThreads.toDouble(), 1, 6, 1, (v) => setState(() => _jitThreads = v.round())),
      const SizedBox(height: 16),
      _SectionHeader('Compilation Mode'),
      _SegmentedControl(['Lazy (on demand)', 'Eager (pre-compile)'], _jitMode, (v) => setState(() => _jitMode = v)),
      const SizedBox(height: 16),
      _SectionHeader('IR Optimization Level'),
      _SegmentedControl(['None', 'Basic (DCE+Const)', 'Full'], _irOpt, (v) => setState(() => _irOpt = v)),
      const SizedBox(height: 20),
      Container(
        padding: const EdgeInsets.all(14),
        decoration: BoxDecoration(color: Colors.white.withOpacity(0.04), borderRadius: BorderRadius.circular(10)),
        child: const Text('More threads improve multi-threaded games but may cause issues with single-threaded titles. Start with 4 threads.',
          style: TextStyle(fontSize: 12, color: Colors.white38, height: 1.5)),
      ),
    ]);
  }

  Widget _audioTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _SectionHeader('Volume'),
      _SliderTile('Master', _masterVolume, 0, 1, null, (v) => setState(() => _masterVolume = v)),
      const SizedBox(height: 16),
      _SectionHeader('Audio Backend'),
      _SegmentedControl(['AAudio (API 26+)', 'OpenSL ES'], _audioBackend, (v) => setState(() => _audioBackend = v)),
      const SizedBox(height: 16),
      _SectionHeader('Buffer Size (samples)'),
      _SegmentedControl(['512', '1024', '2048', '4096'],
        [512, 1024, 2048, 4096].indexOf(_bufferSize),
        (v) => setState(() => _bufferSize = [512, 1024, 2048, 4096][v])),
      const SizedBox(height: 8),
      Text('Lower buffer = less latency but may cause crackling. 2048 is recommended.',
        style: const TextStyle(fontSize: 12, color: Colors.white38)),
    ]);
  }

  Widget _driverTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _SectionHeader('Vulkan Driver'),
      _SegmentedControl(['System', 'Turnip (bundled)', 'Custom .so'], _driverMode, (v) => setState(() => _driverMode = v)),
      if (_driverMode == 2) ...[
        const SizedBox(height: 12),
        OutlinedButton.icon(icon: const Icon(Icons.folder_open), label: const Text('Select driver .so / .zip'), onPressed: () {}),
      ],
      const SizedBox(height: 16),
      _SwitchTile('Native BC Textures', 'Use GPU native BC1/BC3 decompression when available', _bcNative, (v) => setState(() => _bcNative = v)),
      _SwitchTile('Vulkan Debug Layers', 'Enable validation layers (slow, for debugging)', _debugLayers, (v) => setState(() => _debugLayers = v)),
    ]);
  }

  Widget _debugTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _SectionHeader('Log Level'),
      _SegmentedControl(['Off', 'Info', 'Verbose'], _logLevel, (v) => setState(() => _logLevel = v)),
      const SizedBox(height: 16),
      _SwitchTile('FPS / Metrics Overlay', 'Show real-time FPS, frame time, RAM usage', _fpsOverlay, (v) => setState(() => _fpsOverlay = v)),
      const SizedBox(height: 16),
      OutlinedButton.icon(icon: const Icon(Icons.share), label: const Text('Export Session Log'), onPressed: () {}),
    ]);
  }

  void _saveSettings() {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Settings saved')),
    );
    context.pop();
  }
}

class _SectionHeader extends StatelessWidget {
  final String text;
  const _SectionHeader(this.text);
  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.only(bottom: 10),
    child: Text(text, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: Color(0xFF107C10))),
  );
}

class _SegmentedControl extends StatelessWidget {
  final List<String> labels;
  final int selected;
  final ValueChanged<int> onChanged;
  const _SegmentedControl(this.labels, this.selected, this.onChanged);

  @override
  Widget build(BuildContext context) {
    return Wrap(
      spacing: 6,
      children: List.generate(labels.length, (i) => GestureDetector(
        onTap: () => onChanged(i),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
          decoration: BoxDecoration(
            color: i == selected ? const Color(0xFF107C10).withOpacity(0.25) : Colors.white.withOpacity(0.05),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: i == selected ? const Color(0xFF107C10) : Colors.white12),
          ),
          child: Text(labels[i], style: TextStyle(fontSize: 13, color: i == selected ? const Color(0xFF52B043) : Colors.white54, fontWeight: FontWeight.w500)),
        ),
      )),
    );
  }
}

class _SwitchTile extends StatelessWidget {
  final String title, subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;
  const _SwitchTile(this.title, this.subtitle, this.value, this.onChanged);

  @override
  Widget build(BuildContext context) => SwitchListTile(
    title: Text(title, style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500, color: Colors.white)),
    subtitle: Text(subtitle, style: const TextStyle(fontSize: 12, color: Colors.white38)),
    value: value,
    onChanged: onChanged,
    contentPadding: EdgeInsets.zero,
  );
}

class _SliderTile extends StatelessWidget {
  final String label;
  final double value, min, max;
  final double? step;
  final ValueChanged<double> onChanged;
  final Map<double, String>? labels;
  const _SliderTile(this.label, this.value, this.min, this.max, this.step, this.onChanged, {this.labels});

  @override
  Widget build(BuildContext context) => Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
    Row(children: [
      Text(label, style: const TextStyle(fontSize: 13, color: Colors.white70)),
      const Spacer(),
      Text(labels?[value] ?? value.toStringAsFixed(value < 10 ? 2 : 0),
        style: const TextStyle(fontSize: 13, color: Colors.white38)),
    ]),
    Slider(value: value, min: min, max: max, divisions: step != null ? ((max - min) / step!).round() : null, onChanged: onChanged),
  ]);
}

class _IntField extends StatelessWidget {
  final String label;
  final int value;
  final ValueChanged<int> onChanged;
  final int min, max;
  const _IntField(this.label, this.value, this.onChanged, this.min, this.max);

  @override
  Widget build(BuildContext context) => Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
    Text(label, style: const TextStyle(fontSize: 12, color: Colors.white54)),
    const SizedBox(height: 6),
    TextFormField(
      initialValue: value.toString(),
      keyboardType: TextInputType.number,
      style: const TextStyle(color: Colors.white, fontSize: 14),
      decoration: InputDecoration(
        filled: true, fillColor: Colors.white.withOpacity(0.08),
        contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: BorderSide.none),
      ),
      onChanged: (v) { final i = int.tryParse(v); if (i != null && i >= min && i <= max) onChanged(i); },
    ),
  ]);
}
