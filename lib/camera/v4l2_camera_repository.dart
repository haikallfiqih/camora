import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'camera_device.dart';

typedef _NativeListCameras = Pointer<Utf8> Function();
typedef _DartListCameras = Pointer<Utf8> Function();

class V4l2CameraRepository {
  DynamicLibrary _openLibrary() {
    final candidates = <String>[
      'libcamora_v4l2.so',
      '${Directory.current.path}/build/native/libcamora_v4l2.so',
    ];

    for (final path in candidates) {
      try {
        return DynamicLibrary.open(path);
      } catch (_) {
        // Try next candidate.
      }
    }

    throw StateError(
      'Could not load libcamora_v4l2.so. '
      'Run ./tool/build_native.sh first.',
    );
  }

  List<CameraDevice> listCameras() {
    final library = _openLibrary();

    final nativeList = library.lookupFunction<
        _NativeListCameras,
        _DartListCameras>('camora_list_cameras');

    final raw = nativeList().toDartString();
    final decoded = jsonDecode(raw) as List<dynamic>;

    return decoded
        .map(
          (item) => CameraDevice.fromJson(
            item as Map<String, dynamic>,
          ),
        )
        .toList();
  }
}
