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

  bool startPreviewOnLaunch = false;
  bool minimizeToTray = false;
  bool hardwareAcceleration = true;

  bool effectEnabled(CameraEffect effect) => _enabledEffects.contains(effect);

  void navigate(CamoraPage page) {
    if (currentPage == page) return;
    currentPage = page;
    notifyListeners();
  }

  void setEffect(CameraEffect effect, bool enabled) {
    if (enabled) {
      _enabledEffects.add(effect);
    } else {
      _enabledEffects.remove(effect);
    }
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
