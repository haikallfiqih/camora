import 'package:camora/app/camera_effects_state.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('CameraEffectsState', () {
    test('background modes are mutually exclusive', () {
      final effects = CameraEffectsState();
      addTearDown(effects.dispose);

      effects.setEnabled(CameraEffect.backgroundBlur, true);
      expect(effects.backgroundMode, BackgroundMode.blur);

      effects.setEnabled(CameraEffect.backgroundRemoval, true);
      expect(effects.backgroundMode, BackgroundMode.removal);
      expect(effects.isEnabled(CameraEffect.backgroundBlur), isFalse);
      expect(effects.isEnabled(CameraEffect.backgroundRemoval), isTrue);

      effects.selectBackgroundImage('/tmp/background.png');
      expect(effects.backgroundMode, BackgroundMode.image);
      expect(effects.isEnabled(CameraEffect.backgroundRemoval), isFalse);
      expect(effects.isEnabled(CameraEffect.backgroundImage), isTrue);
    });

    test('auto framing and low light remain independent', () {
      final effects = CameraEffectsState();
      addTearDown(effects.dispose);

      effects.setEnabled(CameraEffect.backgroundRemoval, true);
      effects.setEnabled(CameraEffect.autoFraming, true);
      effects.setEnabled(CameraEffect.lowLightEnhancement, true);

      expect(effects.isEnabled(CameraEffect.backgroundRemoval), isTrue);
      expect(effects.autoFramingEnabled, isTrue);
      expect(effects.lowLightEnabled, isTrue);
    });

    test('configured strengths survive disable and re-enable', () {
      final effects = CameraEffectsState();
      addTearDown(effects.dispose);

      effects.setBackgroundBlurStrength(0.73);
      effects.setAutoFramingSensitivity(0.42);
      effects.setLowLightStrength(0.61);
      effects.setEnabled(CameraEffect.backgroundBlur, true);
      effects.setEnabled(CameraEffect.backgroundBlur, false);
      effects.setEnabled(CameraEffect.autoFraming, true);
      effects.setEnabled(CameraEffect.autoFraming, false);
      effects.setEnabled(CameraEffect.lowLightEnhancement, true);
      effects.setEnabled(CameraEffect.lowLightEnhancement, false);

      expect(effects.backgroundBlurStrength, 0.73);
      expect(effects.autoFramingSensitivity, 0.42);
      expect(effects.lowLightStrength, 0.61);
    });

    test(
      'background image cannot enable without a path and clears atomically',
      () {
        final effects = CameraEffectsState();
        addTearDown(effects.dispose);

        effects.setEnabled(CameraEffect.backgroundImage, true);
        expect(effects.backgroundMode, BackgroundMode.none);

        effects.selectBackgroundImage('/tmp/background.png');
        expect(effects.backgroundImagePath, '/tmp/background.png');
        expect(effects.backgroundMode, BackgroundMode.image);

        effects.clearBackgroundImage();
        expect(effects.backgroundImagePath, isNull);
        expect(effects.backgroundMode, BackgroundMode.none);
      },
    );
  });
}
