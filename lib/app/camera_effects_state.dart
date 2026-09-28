import 'package:flutter/foundation.dart';

enum CameraEffect {
  backgroundBlur,
  backgroundRemoval,
  backgroundImage,
  autoFraming,
  lowLightEnhancement,
}

enum BackgroundMode { none, blur, removal, image }

@immutable
class CameraEffectsSnapshot {
  const CameraEffectsSnapshot({
    required this.backgroundMode,
    required this.backgroundBlurStrength,
    required this.backgroundImagePath,
    required this.autoFramingEnabled,
    required this.autoFramingSensitivity,
    required this.lowLightEnabled,
    required this.lowLightStrength,
  });

  final BackgroundMode backgroundMode;
  final double backgroundBlurStrength;
  final String? backgroundImagePath;
  final bool autoFramingEnabled;
  final double autoFramingSensitivity;
  final bool lowLightEnabled;
  final double lowLightStrength;
}

/// The single UI source of truth for Camora's processing configuration.
class CameraEffectsState extends ChangeNotifier {
  BackgroundMode _backgroundMode = BackgroundMode.none;
  double _backgroundBlurStrength = 0.7;
  String? _backgroundImagePath;
  bool _autoFramingEnabled = false;
  double _autoFramingSensitivity = 0.5;
  bool _lowLightEnabled = false;
  double _lowLightStrength = 0.5;

  BackgroundMode get backgroundMode => _backgroundMode;
  double get backgroundBlurStrength => _backgroundBlurStrength;
  String? get backgroundImagePath => _backgroundImagePath;
  bool get autoFramingEnabled => _autoFramingEnabled;
  double get autoFramingSensitivity => _autoFramingSensitivity;
  bool get lowLightEnabled => _lowLightEnabled;
  double get lowLightStrength => _lowLightStrength;

  bool isEnabled(CameraEffect effect) => switch (effect) {
    CameraEffect.backgroundBlur => _backgroundMode == BackgroundMode.blur,
    CameraEffect.backgroundRemoval => _backgroundMode == BackgroundMode.removal,
    CameraEffect.backgroundImage => _backgroundMode == BackgroundMode.image,
    CameraEffect.autoFraming => _autoFramingEnabled,
    CameraEffect.lowLightEnhancement => _lowLightEnabled,
  };

  void setEnabled(CameraEffect effect, bool enabled) {
    switch (effect) {
      case CameraEffect.backgroundBlur:
        _setBackgroundMode(
          enabled ? BackgroundMode.blur : BackgroundMode.none,
          disabling: BackgroundMode.blur,
        );
      case CameraEffect.backgroundRemoval:
        _setBackgroundMode(
          enabled ? BackgroundMode.removal : BackgroundMode.none,
          disabling: BackgroundMode.removal,
        );
      case CameraEffect.backgroundImage:
        if (enabled && _backgroundImagePath == null) return;
        _setBackgroundMode(
          enabled ? BackgroundMode.image : BackgroundMode.none,
          disabling: BackgroundMode.image,
        );
      case CameraEffect.autoFraming:
        if (_autoFramingEnabled == enabled) return;
        _autoFramingEnabled = enabled;
        notifyListeners();
      case CameraEffect.lowLightEnhancement:
        if (_lowLightEnabled == enabled) return;
        _lowLightEnabled = enabled;
        notifyListeners();
    }
  }

  void _setBackgroundMode(
    BackgroundMode mode, {
    required BackgroundMode disabling,
  }) {
    if (mode == BackgroundMode.none && _backgroundMode != disabling) return;
    if (_backgroundMode == mode) return;
    _backgroundMode = mode;
    notifyListeners();
  }

  void setBackgroundBlurStrength(double value) {
    final next = value.clamp(0.0, 1.0);
    if (_backgroundBlurStrength == next) return;
    _backgroundBlurStrength = next;
    notifyListeners();
  }

  void setAutoFramingSensitivity(double value) {
    final next = value.clamp(0.0, 1.0);
    if (_autoFramingSensitivity == next) return;
    _autoFramingSensitivity = next;
    notifyListeners();
  }

  void setLowLightStrength(double value) {
    final next = value.clamp(0.0, 1.0);
    if (_lowLightStrength == next) return;
    _lowLightStrength = next;
    notifyListeners();
  }

  void selectBackgroundImage(String path) {
    if (_backgroundImagePath == path &&
        _backgroundMode == BackgroundMode.image) {
      return;
    }
    _backgroundImagePath = path;
    _backgroundMode = BackgroundMode.image;
    notifyListeners();
  }

  void clearBackgroundImage() {
    if (_backgroundImagePath == null) return;
    _backgroundImagePath = null;
    if (_backgroundMode == BackgroundMode.image) {
      _backgroundMode = BackgroundMode.none;
    }
    notifyListeners();
  }

  CameraEffectsSnapshot get snapshot => CameraEffectsSnapshot(
    backgroundMode: _backgroundMode,
    backgroundBlurStrength: _backgroundBlurStrength,
    backgroundImagePath: _backgroundImagePath,
    autoFramingEnabled: _autoFramingEnabled,
    autoFramingSensitivity: _autoFramingSensitivity,
    lowLightEnabled: _lowLightEnabled,
    lowLightStrength: _lowLightStrength,
  );
}
