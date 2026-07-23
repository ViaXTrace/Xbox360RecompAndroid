package com.viaxecomp.xbox360recomp

import android.os.Bundle
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {

    private val CHANNEL = "com.viaxecomp.xbox360recomp/engine"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Keep screen on during gameplay
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL).setMethodCallHandler { call, result ->
            when (call.method) {
                "getSupportedAbis" -> result.success(android.os.Build.SUPPORTED_ABIS.toList())
                "getVulkanVersion" -> result.success(getVulkanVersion())
                else -> result.notImplemented()
            }
        }
    }

    private fun getVulkanVersion(): String {
        return try {
            val activityManager = getSystemService(ACTIVITY_SERVICE) as android.app.ActivityManager
            val info = android.app.ActivityManager.MemoryInfo()
            activityManager.getMemoryInfo(info)
            // Vulkan version detection via Android API
            if (android.os.Build.VERSION.SDK_INT >= 28) "1.1+" else "1.0"
        } catch (e: Exception) {
            "unknown"
        }
    }
}