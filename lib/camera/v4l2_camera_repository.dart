import 'dart:convert';

import 'package:flutter/services.dart';

import 'camera_control.dart';
import 'camera_device.dart';
import 'camera_format.dart';

class V4l2CameraRepository {
  static const _channel = MethodChannel('dev.tapticlabs.camora/camera');

  Future<List<CameraDevice>> listCameras() async {
    final raw = await _channel.invokeMethod<String>('listCameras');

    if (raw == null) {
      return [];
    }

    final data = jsonDecode(raw) as List<dynamic>;

    return data
        .map((item) => CameraDevice.fromJson(item as Map<String, dynamic>))
        .toList();
  }

  Future<List<CameraControl>> listControls(String device) async {
    final raw = await _channel.invokeMethod<String>('listControls', {
      'device': device,
    });

    if (raw == null) {
      return [];
    }

    final data = jsonDecode(raw) as List<dynamic>;

    return data
        .map((item) => CameraControl.fromJson(item as Map<String, dynamic>))
        .toList();
  }

  Future<List<CameraFormat>> listFormats(String device) async {
    final raw = await _channel.invokeMethod<String>('listFormats', {
      'device': device,
    });

    if (raw == null) {
      return [];
    }

    final data = jsonDecode(raw) as List<dynamic>;

    final formats = data
        .map((item) => CameraFormat.fromJson(item as Map<String, dynamic>))
        .where((format) => format.pixelFormat == 'MJPG')
        .toList();

    formats.sort((a, b) {
      final pixelsA = a.width * a.height;

      final pixelsB = b.width * b.height;

      final resolution = pixelsB.compareTo(pixelsA);

      if (resolution != 0) {
        return resolution;
      }

      return b.fps.compareTo(a.fps);
    });

    return formats;
  }

  Future<bool> setControl(String device, int id, int value) async {
    final result = await _channel.invokeMethod<bool>('setControl', {
      'device': device,
      'id': id,
      'value': value,
    });

    return result ?? false;
  }
}
