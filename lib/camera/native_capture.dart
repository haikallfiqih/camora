import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

typedef _StartNative = Int32 Function(Pointer<Utf8>, Int32, Int32, Int32);

typedef _StartDart = int Function(Pointer<Utf8>, int, int, int);

typedef _StopNative = Void Function();
typedef _StopDart = void Function();

typedef _IntNative = Int32 Function();
typedef _IntDart = int Function();

typedef _CopyNative = Int32 Function(Pointer<Uint8>, Int32);

typedef _CopyDart = int Function(Pointer<Uint8>, int);

class NativeCapture {
  late final DynamicLibrary _library;

  late final _StartDart _start;
  late final _StopDart _stop;

  late final _IntDart _width;
  late final _IntDart _height;
  late final _IntDart _frameSize;

  late final _CopyDart _copy;

  NativeCapture() {
    _library = _open();

    _start = _library.lookupFunction<_StartNative, _StartDart>(
      'camora_capture_start',
    );

    _stop = _library.lookupFunction<_StopNative, _StopDart>(
      'camora_capture_stop',
    );

    _width = _library.lookupFunction<_IntNative, _IntDart>(
      'camora_capture_width',
    );

    _height = _library.lookupFunction<_IntNative, _IntDart>(
      'camora_capture_height',
    );

    _frameSize = _library.lookupFunction<_IntNative, _IntDart>(
      'camora_capture_frame_size',
    );

    _copy = _library.lookupFunction<_CopyNative, _CopyDart>(
      'camora_capture_copy_frame',
    );
  }

  DynamicLibrary _open() {
    final root = Directory.current.path;

    return DynamicLibrary.open('$root/build/native/libcamora_v4l2.so');
  }

  bool start(
    String device, {
    int width = 1920,
    int height = 1080,
    int fps = 30,
  }) {
    final path = device.toNativeUtf8();

    try {
      return _start(path, width, height, fps) == 0;
    } finally {
      malloc.free(path);
    }
  }

  void stop() => _stop();

  int get width => _width();

  int get height => _height();

  Uint8List? getFrame() {
    final size = _frameSize();

    if (size <= 0) return null;

    final buffer = malloc.allocate<Uint8>(size);

    try {
      final result = _copy(buffer, size);

      if (result != 1) {
        return null;
      }

      return Uint8List.fromList(buffer.asTypedList(size));
    } finally {
      malloc.free(buffer);
    }
  }
}
