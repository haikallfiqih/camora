import 'package:flutter/foundation.dart';

import 'camera_control.dart';
import 'camera_device.dart';
import 'camera_format.dart';
import 'camora_video.dart';
import 'v4l2_camera_repository.dart';

class CameraSession extends ChangeNotifier {
  CameraSession({V4l2CameraRepository? repository})
    : _repository = repository ?? V4l2CameraRepository();

  final V4l2CameraRepository _repository;

  List<CameraDevice> cameras = const [];
  List<CameraControl> controls = const [];
  List<CameraFormat> formats = const [];
  CameraDevice? selectedCamera;
  CameraFormat? selectedFormat;
  int? textureId;
  bool isLoading = false;
  bool previewLoading = false;
  bool lowLightEnabled = false;
  bool nativeEffectsAvailable = true;
  double lowLightStrength = 0.5;
  bool backgroundImageEnabled = false;
  String? backgroundImagePath;
  bool backgroundBlurEnabled = false;
  double backgroundBlurStrength = 0.7;
  bool backgroundRemovalEnabled = false;
  String? error;
  String? previewError;

  bool get isPreviewing => textureId != null;

  Future<void> initialize() => refreshCameras();

  Future<void> refreshCameras() async {
    final restartPreview = isPreviewing;
    if (restartPreview) await stopPreview();
    isLoading = true;
    error = null;
    notifyListeners();

    try {
      final result = await _repository.listCameras();
      CameraDevice? preferred;
      if (result.isNotEmpty) {
        preferred = result.firstWhere(
          (camera) => camera.name.toLowerCase().contains('emeet'),
          orElse: () => result.first,
        );
      }

      cameras = result;
      selectedCamera = preferred;
      controls = const [];
      formats = const [];
      selectedFormat = null;
      notifyListeners();

      if (preferred != null) {
        await _loadCameraDetails(preferred);
        if (restartPreview) {
          await startPreview();
        }
      }
    } catch (exception) {
      error = exception.toString();
    } finally {
      isLoading = false;
      notifyListeners();
    }
  }

  Future<void> selectCamera(CameraDevice camera) async {
    if (camera.path == selectedCamera?.path) return;
    final restartPreview = isPreviewing;
    if (restartPreview) await stopPreview();

    selectedCamera = camera;
    controls = const [];
    formats = const [];
    selectedFormat = null;
    error = null;
    isLoading = true;
    notifyListeners();

    try {
      await _loadCameraDetails(camera);
      if (restartPreview) await startPreview();
    } finally {
      isLoading = false;
      notifyListeners();
    }
  }

  Future<void> _loadCameraDetails(CameraDevice camera) async {
    try {
      final results = await Future.wait<Object>([
        _repository.listControls(camera.path),
        _repository.listFormats(camera.path),
      ]);
      if (selectedCamera?.path != camera.path) return;

      controls = results[0] as List<CameraControl>;
      formats = results[1] as List<CameraFormat>;
      selectedFormat = _preferredFormat(formats);
      error = null;
    } catch (exception) {
      error = exception.toString();
    }
    notifyListeners();
  }

  CameraFormat? _preferredFormat(List<CameraFormat> availableFormats) {
    if (availableFormats.isEmpty) return null;
    for (final format in availableFormats) {
      if (format.width == 1920 && format.height == 1080 && format.fps == 60) {
        return format;
      }
    }
    return availableFormats.first;
  }

  Future<void> selectFormat(CameraFormat format) async {
    if (format == selectedFormat) return;
    final restartPreview = isPreviewing;
    if (restartPreview) await stopPreview();
    selectedFormat = format;
    notifyListeners();
    if (restartPreview) await startPreview();
  }

  Future<void> setControl(CameraControl control, int value) async {
    final camera = selectedCamera;
    if (camera == null) return;

    final success = await _repository.setControl(
      camera.path,
      control.id,
      value,
    );
    if (!success || selectedCamera?.path != camera.path) return;
    controls = await _repository.listControls(camera.path);
    notifyListeners();
  }

  Future<void> configureLowLight({
    required bool enabled,
    required double strength,
  }) async {
    lowLightEnabled = enabled;
    lowLightStrength = strength.clamp(0.0, 1.0);
    await _pushEffects();
  }

  Future<void> configureBackgroundRemoval({
    required bool enabled,
  }) async {
    backgroundRemovalEnabled = enabled;

    if (enabled) {
      backgroundBlurEnabled = false;
      backgroundImageEnabled = false;
    }

    await _pushEffects();
  }

  Future<void> configureBackgroundBlur({
    required bool enabled,
    required double strength,
  }) async {
    backgroundBlurEnabled = enabled;
    backgroundBlurStrength = strength.clamp(0.0, 1.0);

    if (enabled) {
      backgroundRemovalEnabled = false;
      backgroundImageEnabled = false;
    }

    await _pushEffects();
  }

  Future<void> configureBackgroundImage({
    required bool enabled,
    String? path,
  }) async {
    backgroundImageEnabled = enabled && path != null;
    backgroundImagePath = path;

    if (backgroundImageEnabled) {
      backgroundBlurEnabled = false;
      backgroundRemovalEnabled = false;
    }

    await _pushEffects();
  }

  Future<void> _pushEffects() async {
    final wasAvailable = nativeEffectsAvailable;
    nativeEffectsAvailable = await CamoraVideo.setEffects(
      lowLightEnabled: lowLightEnabled,
      lowLightStrength: lowLightStrength,
      backgroundImageEnabled: backgroundImageEnabled,
      backgroundImagePath: backgroundImagePath,
      backgroundBlurEnabled: backgroundBlurEnabled,
      backgroundBlurStrength: backgroundBlurStrength,
      backgroundRemovalEnabled: backgroundRemovalEnabled,
    );
    if (nativeEffectsAvailable != wasAvailable) notifyListeners();
  }

  Future<void> startPreview() async {
    final camera = selectedCamera;
    final format = selectedFormat;
    if (camera == null || format == null || previewLoading) return;

    previewLoading = true;
    previewError = null;
    notifyListeners();
    try {
      await _pushEffects();
      textureId = await CamoraVideo.start(
        device: camera.path,
        width: format.width,
        height: format.height,
        fps: format.fps,
      );
    } catch (exception) {
      previewError = exception.toString();
    } finally {
      previewLoading = false;
      notifyListeners();
    }
  }

  Future<void> stopPreview() async {
    if (!isPreviewing && !previewLoading) return;
    await CamoraVideo.stop();
    textureId = null;
    previewLoading = false;
    notifyListeners();
  }

  @override
  void dispose() {
    if (isPreviewing) {
      CamoraVideo.stop();
    }
    super.dispose();
  }
}
