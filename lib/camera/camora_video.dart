import 'package:flutter/services.dart';

class CamoraVideo {
  static const _channel = MethodChannel('dev.tapticlabs.camora/video');

  static Future<int> start({
    required String device,
    int width = 1920,
    int height = 1080,
    int fps = 30,
  }) async {
    final textureId = await _channel.invokeMethod<int>('start', {
      'device': device,
      'width': width,
      'height': height,
      'fps': fps,
    });

    if (textureId == null) {
      throw StateError('Native Camora returned no texture.');
    }

    return textureId;
  }

  static Future<bool> setEffects({
    required bool lowLightEnabled,
    required double lowLightStrength,
    required bool backgroundImageEnabled,
    String? backgroundImagePath,
    required bool backgroundBlurEnabled,
    required double backgroundBlurStrength,
    required bool backgroundRemovalEnabled,
    required bool autoFramingEnabled,
    required double autoFramingSensitivity,
  }) async {
    try {
      final available = await _channel.invokeMethod<bool>('setEffects', {
        'lowLightEnabled': lowLightEnabled,
        'lowLightStrength': (lowLightStrength * 100).round(),
        'backgroundImageEnabled': backgroundImageEnabled,
        'backgroundImagePath': backgroundImagePath,
        'backgroundBlurEnabled': backgroundBlurEnabled,
        'backgroundBlurStrength': (backgroundBlurStrength * 100).round(),
        'backgroundRemovalEnabled': backgroundRemovalEnabled,
        'autoFramingEnabled': autoFramingEnabled,
        'autoFramingSensitivity': (autoFramingSensitivity * 100).round(),
      });
      return available ?? true;
    } on MissingPluginException {
      return false;
    }
  }

  static Future<void> stop() async {
    await _channel.invokeMethod<void>('stop');
  }

  static Future<void> startVirtualCamera() async {
    await _channel.invokeMethod<void>('startVirtualCamera');
  }

  static Future<void> stopVirtualCamera() async {
    await _channel.invokeMethod<void>('stopVirtualCamera');
  }

  static Future<Map<String, Object?>> virtualCameraStatus() async {
    final result = await _channel.invokeMapMethod<String, Object?>(
      'virtualCameraStatus',
    );
    return result ?? const {};
  }
}
