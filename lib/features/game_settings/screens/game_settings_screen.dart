import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../../core/theme/app_theme.dart';
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

  // Graphics
  int _internalWidth = 1280, _internalHeight = 720;
  int _aa = 0; // 0=Off 1=FXAA 2=TAA
  int _af = 4; // 1/2/4/8/16
  int _presentMode = 0; // 0=Fit 1=Stretch 2=Letterbox
  bool _vsync = true;
  int _fpsLimit = 0;

  // CPU
  int _jitThreads = 4;
  int _jitMode = 0; // 0=Lazy 1=Eager
  bool _blockCache = true;
  bool _fastMath = true;

  // Audio
  int _audioBackend = 0; // 0=OpenSL 1=AAudio
  int _bufferFrames = 1024;
  int _sampleRate = 48000;
  bool _audioStretch = false;

  // Driver
  int _driverMode = 0; // 0=System 1=Turnip
  bool _bcNative = true;
  bool _debugLayers = false;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 4, vsync: this);
  }

  @override
  void dispose() {
    _tabs.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final game = ref.watch(libraryProvider).valueOrNull
        ?.where((g) => g.titleId == widget.titleId)
        .firstOrNull;

    return Scaffold(
      backgroundColor: AppTheme.bgBase,
      appBar: AppBar(
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(game?.title ?? 'Game Settings', style: const TextStyle(fontSize: 17), overflow: TextOverflow.ellipsis),
            if (game != null)
              Text(game.titleId, style: const TextStyle(fontSize: 11, color: Colors.white38, fontFamily: 'monospace')),
          ],
        ),
        leading: IconButton(icon: const Icon(Icons.arrow_back_rounded), onPressed: () => context.pop()),
        actions: [
          TextButton.icon(
            onPressed: _saveSettings,
            icon: const Icon(Icons.save_rounded, size: 16),
            label: const Text('Save'),
          ),
        ],
        bottom: TabBar(
          controller: _tabs,
          isScrollable: true,
          tabAlignment: TabAlignment.start,
          tabs: const [
            Tab(text: 'Graphics'),
            Tab(text: 'CPU'),
            Tab(text: 'Audio'),
            Tab(text: 'Driver'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          _graphicsTab(),
          _cpuTab(),
          _audioTab(),
          _driverTab(),
        ],
      ),
    );
  }

  void _saveSettings() {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Settings saved')),
    );
    context.pop();
  }

  Widget _graphicsTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _Section('Internal Resolution'),
      _Card(children: [
        Padding(
          padding: const EdgeInsets.all(16),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              Expanded(child: _IntField('Width', _internalWidth, (v) => setState(() => _internalWidth = v), 320, 3840)),
              const SizedBox(width: 12),
              Expanded(child: _IntField('Height', _internalHeight, (v) => setState(() => _internalHeight = v), 240, 2160)),
            ]),
            const SizedBox(height: 12),
            Wrap(spacing: 8, runSpacing: 6, children: [
              for (final p in [('Native', 1280, 720), ('1080p', 1920, 1080), ('1440p', 2560, 1440), ('4K', 3840, 2160)])
                ActionChip(
                  label: Text(p.$1),
                  onPressed: () => setState(() { _internalWidth = p.$2; _internalHeight = p.$3; }),
                  backgroundColor: (_internalWidth == p.$2 && _internalHeight == p.$3)
                      ? AppTheme.xboxGreen.withAlpha(50) : null,
                ),
            ]),
          ]),
        ),
      ]),
      const SizedBox(height: 14),
      _Section('Anti-Aliasing'),
      _SegmentedCard(['Off', 'FXAA', 'TAA'], _aa, (v) => setState(() => _aa = v)),
      const SizedBox(height: 14),
      _Section('Anisotropic Filtering'),
      _SegmentedCard(['1×', '2×', '4×', '8×', '16×'], [1,2,4,8,16].indexOf(_af), (v) => setState(() => _af = [1,2,4,8,16][v])),
      const SizedBox(height: 14),
      _Section('Presentation'),
      _SegmentedCard(['Fit', 'Stretch', 'Letterbox'], _presentMode, (v) => setState(() => _presentMode = v)),
      const SizedBox(height: 14),
      _Section('Frame Control'),
      _Card(children: [
        _SwitchRow('VSync', 'Sync to display refresh rate', _vsync, (v) => setState(() => _vsync = v)),
        const Divider(height: 1, indent: 16),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(mainAxisAlignment: MainAxisAlignment.spaceBetween, children: [
              const Text('FPS Limit', style: TextStyle(color: Colors.white, fontSize: 14)),
              Text(_fpsLimit == 0 ? 'Unlimited' : '$_fpsLimit fps',
                  style: const TextStyle(color: AppTheme.xboxGreenLight, fontSize: 13, fontWeight: FontWeight.w600)),
            ]),
            Slider(
              value: _fpsLimit.toDouble(),
              min: 0, max: 120, divisions: 8,
              label: _fpsLimit == 0 ? 'Unlimited' : '$_fpsLimit',
              onChanged: (v) => setState(() => _fpsLimit = v.round()),
            ),
          ]),
        ),
      ]),
      const SizedBox(height: 80),
    ]);
  }

  Widget _cpuTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _Section('JIT Threads (1–6)'),
      _Card(children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(mainAxisAlignment: MainAxisAlignment.spaceBetween, children: [
              const Text('Threads', style: TextStyle(color: Colors.white, fontSize: 14)),
              Text('$_jitThreads', style: const TextStyle(color: AppTheme.xboxGreenLight, fontSize: 13, fontWeight: FontWeight.w600)),
            ]),
            Slider(
              value: _jitThreads.toDouble(),
              min: 1, max: 6, divisions: 5,
              label: '$_jitThreads',
              onChanged: (v) => setState(() => _jitThreads = v.round()),
            ),
            const Text('Higher values improve performance on multi-core devices.',
              style: TextStyle(fontSize: 11, color: Colors.white38)),
          ]),
        ),
      ]),
      const SizedBox(height: 14),
      _Section('Compilation Mode'),
      _SegmentedCard(['Lazy (on demand)', 'Eager (pre-compile)'], _jitMode, (v) => setState(() => _jitMode = v)),
      const SizedBox(height: 14),
      _Section('Optimizations'),
      _Card(children: [
        _SwitchRow('Block Cache', 'Cache compiled JIT blocks to disk', _blockCache, (v) => setState(() => _blockCache = v)),
        const Divider(height: 1, indent: 16),
        _SwitchRow('Fast Math', 'Relaxed floating-point (may affect accuracy)', _fastMath, (v) => setState(() => _fastMath = v)),
      ]),
      const SizedBox(height: 80),
    ]);
  }

  Widget _audioTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _Section('Audio Backend'),
      _SegmentedCard(['OpenSL ES', 'AAudio'], _audioBackend, (v) => setState(() => _audioBackend = v)),
      const SizedBox(height: 14),
      _Section('Buffer Size'),
      _SegmentedCard(['256', '512', '1024', '2048'], [256,512,1024,2048].indexOf(_bufferFrames), (v) => setState(() => _bufferFrames = [256,512,1024,2048][v])),
      const SizedBox(height: 14),
      _Section('Sample Rate'),
      _SegmentedCard(['44100 Hz', '48000 Hz'], _sampleRate == 44100 ? 0 : 1, (v) => setState(() => _sampleRate = v == 0 ? 44100 : 48000)),
      const SizedBox(height: 14),
      _Section('Timing'),
      _Card(children: [
        _SwitchRow('Audio Stretch', 'Stretch audio to match video speed', _audioStretch, (v) => setState(() => _audioStretch = v)),
      ]),
      const SizedBox(height: 80),
    ]);
  }

  Widget _driverTab() {
    return ListView(padding: const EdgeInsets.all(16), children: [
      _Section('Vulkan Driver'),
      _SegmentedCard(['System', 'Turnip (Mesa)'], _driverMode, (v) => setState(() => _driverMode = v)),
      if (_driverMode == 1)
        Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: Colors.amber.withAlpha(15),
              borderRadius: BorderRadius.circular(10),
              border: Border.all(color: Colors.amber.withAlpha(50)),
            ),
            child: const Row(
              children: [
                Icon(Icons.info_outline_rounded, color: Colors.amber, size: 16),
                SizedBox(width: 10),
                Expanded(
                  child: Text('Turnip driver requires Adreno GPU. Install the Turnip Vulkan Driver app first.',
                    style: TextStyle(fontSize: 11, color: Colors.amber)),
                ),
              ],
            ),
          ),
        ),
      const SizedBox(height: 14),
      _Section('Rendering'),
      _Card(children: [
        _SwitchRow('Native BC Textures', 'Use hardware block compression decoding', _bcNative, (v) => setState(() => _bcNative = v)),
        const Divider(height: 1, indent: 16),
        _SwitchRow('Debug Layers', 'Enable Vulkan validation (debug only)', _debugLayers, (v) => setState(() => _debugLayers = v)),
      ]),
      if (_debugLayers)
        Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: Colors.red.withAlpha(15),
              borderRadius: BorderRadius.circular(10),
              border: Border.all(color: Colors.red.withAlpha(50)),
            ),
            child: const Text('Debug layers significantly reduce performance. Disable for normal use.',
              style: TextStyle(fontSize: 11, color: Colors.red)),
          ),
        ),
      const SizedBox(height: 80),
    ]);
  }

  Widget _Section(String title) => Padding(
    padding: const EdgeInsets.only(left: 4, bottom: 8),
    child: Text(title.toUpperCase(),
      style: const TextStyle(fontSize: 10, fontWeight: FontWeight.w700, color: AppTheme.xboxGreen, letterSpacing: 1.2)),
  );

  Widget _Card({required List<Widget> children}) => Container(
    margin: const EdgeInsets.only(bottom: 0),
    decoration: BoxDecoration(
      color: AppTheme.surface,
      borderRadius: BorderRadius.circular(12),
      border: Border.all(color: AppTheme.border),
    ),
    child: Column(children: children),
  );

  Widget _SegmentedCard(List<String> labels, int selected, ValueChanged<int> onChanged) {
    return _Card(children: [
      Padding(
        padding: const EdgeInsets.all(14),
        child: SegmentedButton<int>(
          segments: labels.asMap().entries.map((e) =>
            ButtonSegment(value: e.key, label: Text(e.value))).toList(),
          selected: {selected},
          onSelectionChanged: (s) => onChanged(s.first),
          showSelectedIcon: false,
          style: ButtonStyle(
            visualDensity: VisualDensity.compact,
          ),
        ),
      ),
    ]);
  }

  Widget _SwitchRow(String title, String subtitle, bool value, ValueChanged<bool> onChanged) {
    return ListTile(
      title: Text(title, style: const TextStyle(fontSize: 14, color: Colors.white)),
      subtitle: Text(subtitle, style: const TextStyle(fontSize: 11, color: Colors.white38)),
      trailing: Switch(value: value, onChanged: onChanged),
      onTap: () => onChanged(!value),
    );
  }
}

Widget _IntField(String label, int value, ValueChanged<int> onChanged, int min, int max) {
  final ctrl = TextEditingController(text: '$value');
  return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
    Text(label, style: const TextStyle(fontSize: 12, color: Colors.white54)),
    const SizedBox(height: 4),
    TextField(
      controller: ctrl,
      keyboardType: TextInputType.number,
      style: const TextStyle(fontSize: 14, color: Colors.white),
      decoration: const InputDecoration(isDense: true),
      onChanged: (s) {
        final v = int.tryParse(s);
        if (v != null && v >= min && v <= max) onChanged(v);
      },
    ),
  ]);
}
