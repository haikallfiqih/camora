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

  static Future<void> setEffects({
    required bool lowLightEnabled,
    required double lowLightStrength,
  }) async {
    await _channel.invokeMethod<void>('setEffects', {
      'lowLightEnabled': lowLightEnabled,
      'lowLightStrength': (lowLightStrength * 100).round(),
    });
  }

  static Future<void> stop() async {
    await _channel.invokeMethod<void>('stop');
  }
}
