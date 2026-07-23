import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hive_flutter/hive_flutter.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:device_info_plus/device_info_plus.dart';

import 'app.dart';
import 'core/native/engine_bridge.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);

  SystemChrome.setSystemUIOverlayStyle(const SystemUiOverlayStyle(
    statusBarColor: Colors.transparent,
    statusBarIconBrightness: Brightness.light,
    systemNavigationBarColor: Color(0xFF0D0D0D),
    systemNavigationBarIconBrightness: Brightness.light,
  ));

  await Hive.initFlutter();
  await EngineBridge.initialize();
  await _requestPermissions();

  runApp(const ProviderScope(child: Xbox360RecompApp()));
}

Future<void> _requestPermissions() async {
  final deviceInfo = DeviceInfoPlugin();
  final androidInfo = await deviceInfo.androidInfo;
  final sdkInt = androidInfo.version.sdkInt;

  if (sdkInt >= 33) {
    // Android 13+ granular media permissions
    await [
      Permission.photos,
      Permission.videos,
      Permission.notification,
    ].request();
  } else if (sdkInt >= 30) {
    // Android 11-12: MANAGE_EXTERNAL_STORAGE for broad file access
    final status = await Permission.manageExternalStorage.status;
    if (!status.isGranted) {
      await Permission.manageExternalStorage.request();
    }
  } else {
    // Android 10 and below
    await [
      Permission.storage,
    ].request();
  }
}
