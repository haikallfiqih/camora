import 'package:flutter/foundation.dart';

import 'camera_effects_state.dart';
export 'camera_effects_state.dart';

enum CamoraPage { studio, camera, effects, virtualCamera, settings }

class AppState extends ChangeNotifier {
  AppState() {
    effects.addListener(notifyListeners);
  }

  CamoraPage currentPage = CamoraPage.studio;
  final CameraEffectsState effects = CameraEffectsState();
  CameraEffect selectedEffect = CameraEffect.backgroundBlur;

  bool startPreviewOnLaunch = false;
  bool minimizeToTray = false;
  bool hardwareAcceleration = true;

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

  @override
  void dispose() {
    effects.removeListener(notifyListeners);
    effects.dispose();
    super.dispose();
  }
}
