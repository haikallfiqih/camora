import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../widgets/ui_components.dart';

class EffectsPage extends StatelessWidget {
  const EffectsPage({required this.appState, super.key});
  final AppState appState;

  static const effects = [
    (
      CameraEffect.backgroundBlur,
      'Background Blur',
      'Soften the area behind you.',
      Icons.blur_on_outlined,
    ),
    (
      CameraEffect.backgroundRemoval,
      'Background Removal',
      'Remove the background from the frame.',
      Icons.content_cut_outlined,
    ),
    (
      CameraEffect.backgroundImage,
      'Background Image',
      'Replace the background with an image.',
      Icons.image_outlined,
    ),
    (
      CameraEffect.autoFraming,
      'Auto Framing',
      'Keep the subject centered automatically.',
      Icons.center_focus_strong_outlined,
    ),
    (
      CameraEffect.lowLightEnhancement,
      'Low Light Enhancement',
      'Improve visibility in dim environments.',
      Icons.light_mode_outlined,
    ),
  ];

  @override
  Widget build(BuildContext context) => Column(
    children: [
      const PageHeading(
        title: 'Effects',
        subtitle: 'Video enhancement tools for your camera feed.',
      ),
      const SizedBox(height: 20),
      Expanded(
        child: ListView(
          children: [
            CamoraPanel(
              child: Row(
                children: [
                  const Icon(
                    Icons.info_outline_rounded,
                    color: CamoraColors.purpleLight,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      'Effects are planned but are not connected to the native preview pipeline yet.',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 14),
            ...effects.map(
              (effect) => Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: CamoraPanel(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 18,
                    vertical: 14,
                  ),
                  child: Row(
                    children: [
                      Container(
                        width: 42,
                        height: 42,
                        decoration: BoxDecoration(
                          color: CamoraColors.purple.withValues(alpha: 0.14),
                          borderRadius: BorderRadius.circular(10),
                        ),
                        child: Icon(effect.$4, color: CamoraColors.purpleLight),
                      ),
                      const SizedBox(width: 14),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              effect.$2,
                              style: Theme.of(context).textTheme.titleMedium,
                            ),
                            const SizedBox(height: 3),
                            Text(
                              effect.$3,
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ],
                        ),
                      ),
                      const StatusPill('Coming soon'),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    ],
  );
}
