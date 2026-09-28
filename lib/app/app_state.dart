import 'package:flutter/foundation.dart';

enum CamoraPage { studio, camera, effects, virtualCamera, settings }

enum CameraEffect {
  backgroundBlur,
  backgroundRemoval,
  backgroundImage,
  autoFraming,
  lowLightEnhancement,
}

class AppState extends ChangeNotifier {
  CamoraPage currentPage = CamoraPage.studio;
  final Set<CameraEffect> _enabledEffects = {};

  CameraEffect selectedEffect = CameraEffect.backgroundBlur;
  double backgroundBlurStrength = 0.7;
  double autoFramingSensitivity = 0.5;
  double lowLightStrength = 0.5;
  String? backgroundImagePath;

  bool startPreviewOnLaunch = false;
  bool minimizeToTray = false;
  bool hardwareAcceleration = true;

  bool effectEnabled(CameraEffect effect) => _enabledEffects.contains(effect);

  void navigate(CamoraPage page) {
    if (currentPage == page) return;
    currentPage = page;
    notifyListeners();
  }

  void selectEffect(CameraEffect effect) {
    if (selectedEffect == effect) return;
    selectedEffect = effect;
    notifyListeners();
  }

  void setEffect(CameraEffect effect, bool enabled) {
    if (enabled) {
      if (effect == CameraEffect.backgroundBlur ||
          effect == CameraEffect.backgroundRemoval ||
          effect == CameraEffect.backgroundImage) {
        _enabledEffects
          ..remove(CameraEffect.backgroundBlur)
          ..remove(CameraEffect.backgroundRemoval)
          ..remove(CameraEffect.backgroundImage);
      }
      _enabledEffects.add(effect);
    } else {
      _enabledEffects.remove(effect);
    }
    notifyListeners();
  }

  void setBackgroundBlurStrength(double value) {
    backgroundBlurStrength = value;
    notifyListeners();
  }

  void setAutoFramingSensitivity(double value) {
    autoFramingSensitivity = value;
    notifyListeners();
  }

  void setLowLightStrength(double value) {
    lowLightStrength = value;
    notifyListeners();
  }

  void setBackgroundImage(String? path) {
    if (backgroundImagePath == path) return;
    backgroundImagePath = path;
    notifyListeners();
  }

  void setStartPreviewOnLaunch(bool value) {
    startPreviewOnLaunch = value;
    notifyListeners();
  }

  void setMinimizeToTray(bool value) {
    minimizeToTray = value;
    notifyListeners();
  }

  void setHardwareAcceleration(bool value) {
    hardwareAcceleration = value;
    notifyListeners();
  }
}
