import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../widgets/ui_components.dart';

class EffectsPage extends StatelessWidget {
  const EffectsPage({required this.session, required this.appState, super.key});

  final CameraSession session;

  final AppState appState;

  static const effects = <EffectDefinition>[
    EffectDefinition(
      CameraEffect.backgroundBlur,
      'Background Blur',
      'Soften the area behind you while keeping the subject clear.',
      Icons.blur_on_outlined,
      available: true,
    ),
    EffectDefinition(
      CameraEffect.backgroundRemoval,
      'Background Removal',
      'Isolate the subject and remove the scene behind them.',
      Icons.content_cut_outlined,
      available: true,
    ),
    EffectDefinition(
      CameraEffect.backgroundImage,
      'Background Image',
      'Place the subject over an image selected from your computer.',
      Icons.image_outlined,
      available: true,
    ),
    EffectDefinition(
      CameraEffect.autoFraming,
      'Auto Framing',
      'Keep the subject centered as they move around the frame.',
      Icons.center_focus_strong_outlined,
      available: true,
    ),
    EffectDefinition(
      CameraEffect.lowLightEnhancement,
      'Low Light Enhancement',
      'Improve subject visibility when the room is dim.',
      Icons.light_mode_outlined,
      available: true,
    ),
  ];

  @override
  Widget build(BuildContext context) => Column(
    children: [
      PageHeading(
        title: 'Effects',
        subtitle: 'Prepare enhancements for the Camora processing pipeline.',
        trailing: StatusPill(
          session.nativeEffectsAvailable
              ? 'Native effects available'
              : 'Restart required',
          available: session.nativeEffectsAvailable,
        ),
      ),
      const SizedBox(height: 14),
      Expanded(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final splitView = constraints.maxWidth >= 780;
            final effectList = _EffectList(
              session: session,
              appState: appState,
            );
            final inspector = _EffectInspector(
              session: session,
              appState: appState,
            );

            if (!splitView) {
              return ListView(
                children: [
                  const _AvailabilityNotice(),
                  const SizedBox(height: 12),
                  effectList,
                  const SizedBox(height: 12),
                  SizedBox(height: 330, child: inspector),
                ],
              );
            }

            return Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(
                  child: ListView(
                    children: [
                      const _AvailabilityNotice(),
                      const SizedBox(height: 12),
                      effectList,
                    ],
                  ),
                ),
                const SizedBox(width: 14),
                SizedBox(
                  width: (constraints.maxWidth * 0.36).clamp(280.0, 390.0),
                  child: inspector,
                ),
              ],
            );
          },
        ),
      ),
    ],
  );
}

class _AvailabilityNotice extends StatelessWidget {
  const _AvailabilityNotice();

  @override
  Widget build(BuildContext context) => CamoraPanel(
    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 13),
    child: const Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Icon(Icons.info_outline_rounded, color: CamoraColors.purpleLight),
        SizedBox(width: 12),
        Expanded(
          child: Text(
            'Low Light Enhancement is processed live in the native camera pipeline. '
            'Background Blur, Background Removal, Background Image, and Low Light Enhancement are processed live in the native camera pipeline.',
            style: TextStyle(color: Color(0xFFD6DAE3), height: 1.4),
          ),
        ),
      ],
    ),
  );
}

class _EffectList extends StatelessWidget {
  const _EffectList({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) => Column(
    children: EffectsPage.effects
        .map(
          (effect) => Padding(
            padding: const EdgeInsets.only(bottom: 10),
            child: _EffectCard(
              definition: effect,
              selected: appState.selectedEffect == effect.effect,
              configured: appState.effects.isEnabled(effect.effect),
              available:
                  effect.available &&
                  session.nativeEffectsAvailable &&
                  (effect.effect != CameraEffect.backgroundImage ||
                      appState.effects.backgroundImagePath != null),
              onSelected: () => appState.selectEffect(effect.effect),
              onChanged: (value) {
                appState.selectEffect(effect.effect);
                appState.effects.setEnabled(effect.effect, value);
              },
            ),
          ),
        )
        .toList(),
  );
}

class _EffectCard extends StatelessWidget {
  const _EffectCard({
    required this.definition,
    required this.selected,
    required this.configured,
    required this.available,
    required this.onSelected,
    required this.onChanged,
  });

  final EffectDefinition definition;
  final bool selected;
  final bool configured;
  final bool available;
  final VoidCallback onSelected;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => Material(
    color: selected
        ? CamoraColors.purple.withValues(alpha: 0.1)
        : CamoraColors.surface,
    shape: RoundedRectangleBorder(
      borderRadius: BorderRadius.circular(14),
      side: BorderSide(
        color: selected ? CamoraColors.purple : CamoraColors.border,
      ),
    ),
    clipBehavior: Clip.antiAlias,
    child: InkWell(
      onTap: onSelected,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 13),
        child: Row(
          children: [
            Container(
              width: 42,
              height: 42,
              decoration: BoxDecoration(
                color: CamoraColors.purple.withValues(alpha: 0.16),
                borderRadius: BorderRadius.circular(11),
              ),
              child: Icon(definition.icon, color: CamoraColors.purpleLight),
            ),
            const SizedBox(width: 13),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    definition.title,
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 3),
                  Text(
                    definition.description,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ],
              ),
            ),
            const SizedBox(width: 10),
            Semantics(
              label: 'Configure ${definition.title}',
              child: Switch(
                value: configured,
                onChanged: available ? onChanged : null,
              ),
            ),
            const SizedBox(width: 2),
            Icon(
              Icons.chevron_right_rounded,
              color: selected ? Colors.white : CamoraColors.muted,
            ),
          ],
        ),
      ),
    ),
  );
}

class _EffectInspector extends StatelessWidget {
  const _EffectInspector({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) {
    final definition = EffectsPage.effects.firstWhere(
      (item) => item.effect == appState.selectedEffect,
    );
    final configured = appState.effects.isEnabled(definition.effect);

    return CamoraPanel(
      padding: const EdgeInsets.fromLTRB(18, 16, 18, 18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  definition.title,
                  style: Theme.of(context).textTheme.titleLarge,
                ),
              ),
              Switch(
                value: configured,
                onChanged:
                    definition.available &&
                        session.nativeEffectsAvailable &&
                        (definition.effect != CameraEffect.backgroundImage ||
                            appState.effects.backgroundImagePath != null)
                    ? (value) {
                        appState.effects.setEnabled(definition.effect, value);
                      }
                    : null,
              ),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            definition.description,
            style: Theme.of(context).textTheme.bodySmall,
          ),
          const SizedBox(height: 16),
          const Divider(height: 1),
          const SizedBox(height: 18),
          Expanded(
            child: SingleChildScrollView(
              child:
                  (definition.available && session.nativeEffectsAvailable) ||
                      definition.effect == CameraEffect.backgroundImage
                  ? _EffectSettings(
                      effect: definition.effect,
                      appState: appState,
                    )
                  : const _SettingMessage(
                      icon: Icons.lock_outline_rounded,
                      title: 'Processor unavailable',
                      message: 'This effect requires subject segmentation, which is not installed yet.',
                    ),
            ),
          ),
          const Divider(height: 1),
          const SizedBox(height: 12),
          Row(
            children: [
              Icon(
                definition.available && session.nativeEffectsAvailable
                    ? Icons.bolt_rounded
                    : Icons.schedule_rounded,
                size: 17,
                color: definition.available && session.nativeEffectsAvailable
                    ? CamoraColors.success
                    : CamoraColors.muted,
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  definition.available && session.nativeEffectsAvailable
                      ? 'Applied live to the native camera preview'
                      : 'Processing backend required',
                  style: const TextStyle(
                    color: CamoraColors.muted,
                    fontSize: 11,
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _EffectSettings extends StatelessWidget {
  const _EffectSettings({required this.effect, required this.appState});

  final CameraEffect effect;
  final AppState appState;

  @override
  Widget build(BuildContext context) => switch (effect) {
    CameraEffect.backgroundBlur => _EffectSlider(
      label: 'Blur strength',
      value: appState.effects.backgroundBlurStrength,
      onChanged: (value) {
        appState.effects.setBackgroundBlurStrength(value);
      },
    ),
    CameraEffect.backgroundRemoval => const _SettingMessage(
      icon: Icons.layers_clear_outlined,
      title: 'Transparent background',
      message: 'The processed output will use transparency where supported.',
    ),
    CameraEffect.backgroundImage => _BackgroundImageSetting(appState: appState),
    CameraEffect.autoFraming => _EffectSlider(
      label: 'Tracking sensitivity',
      value: appState.effects.autoFramingSensitivity,
      onChanged: (value) {
        appState.effects.setAutoFramingSensitivity(value);
      },
    ),
    CameraEffect.lowLightEnhancement => _EffectSlider(
      label: 'Enhancement strength',
      value: appState.effects.lowLightStrength,
      onChanged: (value) {
        appState.effects.setLowLightStrength(value);
      },
    ),
  };
}

class _EffectSlider extends StatelessWidget {
  const _EffectSlider({
    required this.label,
    required this.value,
    required this.onChanged,
  });

  final String label;
  final double value;
  final ValueChanged<double> onChanged;

  @override
  Widget build(BuildContext context) => Column(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Row(
        children: [
          Expanded(child: Text(label)),
          Text(
            '${(value * 100).round()}%',
            style: const TextStyle(color: CamoraColors.muted, fontSize: 12),
          ),
        ],
      ),
      Slider(value: value, onChanged: onChanged),
    ],
  );
}

class _BackgroundImageSetting extends StatelessWidget {
  const _BackgroundImageSetting({required this.appState});

  final AppState appState;

  Future<void> _chooseImage(BuildContext context) async {
    try {
      final result = await FilePicker.pickFile(
        type: FileType.custom,
        allowedExtensions: const ['jpg', 'jpeg', 'png', 'webp'],
        dialogTitle: 'Choose a background image',
      );
      if (!context.mounted || result == null) return;
      final path = result.path;
      if (path == null) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('The selected image has no local path.'),
          ),
        );
        return;
      }
      appState.effects.selectBackgroundImage(path);
    } catch (error) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Could not choose image: $error')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final path = appState.effects.backgroundImagePath;
    final fileName = path?.split(Platform.pathSeparator).last;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text('Background source'),
        const SizedBox(height: 10),
        if (path != null) ...[
          ClipRRect(
            borderRadius: BorderRadius.circular(10),
            child: AspectRatio(
              aspectRatio: 16 / 9,
              child: Image.file(
                File(path),
                fit: BoxFit.cover,
                errorBuilder: (context, error, stackTrace) => Container(
                  color: CamoraColors.surfaceRaised,
                  alignment: Alignment.center,
                  child: const Icon(
                    Icons.broken_image_outlined,
                    color: CamoraColors.muted,
                  ),
                ),
              ),
            ),
          ),
          const SizedBox(height: 9),
          Text(
            fileName!,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(fontSize: 12),
          ),
          const SizedBox(height: 9),
        ],
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            OutlinedButton.icon(
              onPressed: () => _chooseImage(context),
              icon: Icon(
                path == null
                    ? Icons.add_photo_alternate_outlined
                    : Icons.swap_horiz_rounded,
              ),
              label: Text(path == null ? 'Choose image' : 'Replace'),
            ),
            if (path != null)
              TextButton.icon(
                onPressed: () {
                  appState.effects.clearBackgroundImage();
                },
                icon: const Icon(Icons.delete_outline_rounded),
                label: const Text('Remove'),
              ),
          ],
        ),
        const SizedBox(height: 10),
        const Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.bolt_rounded, size: 15, color: CamoraColors.success),
            SizedBox(width: 7),
            Expanded(
              child: Text(
                'The selected image is applied live while Background Image is enabled.',
                style: TextStyle(
                  color: CamoraColors.muted,
                  fontSize: 11,
                  height: 1.35,
                ),
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class _SettingMessage extends StatelessWidget {
  const _SettingMessage({
    required this.icon,
    required this.title,
    required this.message,
  });

  final IconData icon;
  final String title;
  final String message;

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.all(14),
    decoration: BoxDecoration(
      color: CamoraColors.surfaceRaised,
      borderRadius: BorderRadius.circular(10),
      border: Border.all(color: CamoraColors.border),
    ),
    child: Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Icon(icon, size: 20, color: CamoraColors.purpleLight),
        const SizedBox(width: 10),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: const TextStyle(fontWeight: FontWeight.w600)),
              const SizedBox(height: 4),
              Text(message, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
        ),
      ],
    ),
  );
}

class EffectDefinition {
  const EffectDefinition(
    this.effect,
    this.title,
    this.description,
    this.icon, {
    this.available = false,
  });

  final CameraEffect effect;
  final String title;
  final String description;
  final IconData icon;
  final bool available;
}
