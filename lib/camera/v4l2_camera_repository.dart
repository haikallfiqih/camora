import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

import 'camera_control.dart';
import 'camera_device.dart';

typedef _NativeListCameras =
    Pointer<Utf8> Function();

typedef _DartListCameras =
    Pointer<Utf8> Function();

typedef _NativeListControls =
    Pointer<Utf8> Function(Pointer<Utf8>);

typedef _DartListControls =
    Pointer<Utf8> Function(Pointer<Utf8>);

typedef _NativeSetControl =
    Int32 Function(
      Pointer<Utf8>,
      Uint32,
      Int32,
    );

typedef _DartSetControl =
    int Function(
      Pointer<Utf8>,
      int,
      int,
    );

class V4l2CameraRepository {
  late final DynamicLibrary _library;

  late final _DartListCameras _nativeListCameras;
  late final _DartListControls _nativeListControls;
  late final _DartSetControl _nativeSetControl;

  V4l2CameraRepository() {
    _library = _openLibrary();

    _nativeListCameras =
        _library.lookupFunction<
            _NativeListCameras,
            _DartListCameras>(
          'camora_list_cameras',
        );

    _nativeListControls =
        _library.lookupFunction<
            _NativeListControls,
            _DartListControls>(
          'camora_list_controls',
        );

    _nativeSetControl =
        _library.lookupFunction<
            _NativeSetControl,
            _DartSetControl>(
          'camora_set_control',
        );
  }

  DynamicLibrary _openLibrary() {
    final current =
        Directory.current.path;

    final candidates = [
      '$current/build/native/libcamora_v4l2.so',
      'libcamora_v4l2.so',
    ];

    for (final path in candidates) {
      try {
        return DynamicLibrary.open(path);
      } catch (_) {}
    }

    throw StateError(
      'Could not load libcamora_v4l2.so.',
    );
  }

  List<CameraDevice> listCameras() {
    final raw =
        _nativeListCameras().toDartString();

    final data =
        jsonDecode(raw) as List<dynamic>;

    return data
        .map(
          (item) =>
              CameraDevice.fromJson(
            item as Map<String, dynamic>,
          ),
        )
        .toList();
  }

  List<CameraControl> listControls(
    String device,
  ) {
    final pointer =
        device.toNativeUtf8();

    try {
      final raw =
          _nativeListControls(pointer)
              .toDartString();

      final data =
          jsonDecode(raw) as List<dynamic>;

      return data
          .map(
            (item) =>
                CameraControl.fromJson(
              item as Map<String, dynamic>,
            ),
          )
          .toList();
    } finally {
      malloc.free(pointer);
    }
  }

  bool setControl(
    String device,
    int id,
    int value,
  ) {
    final pointer =
        device.toNativeUtf8();

    try {
      return _nativeSetControl(
            pointer,
            id,
            value,
          ) ==
          0;
    } finally {
      malloc.free(pointer);
    }
  }
}
