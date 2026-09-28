import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../widgets/camera_control_widgets.dart';
import '../../widgets/ui_components.dart';

class StudioPage extends StatelessWidget {
  const StudioPage({required this.session, required this.appState, super.key});
  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final sideBySide = constraints.maxWidth >= 620;
        final controlsWidth = (constraints.maxWidth * 0.34).clamp(250.0, 340.0);
        return Column(
          children: [
            PageHeading(
              title: 'Studio',
              subtitle: 'Preview and configure your camera.',
              trailing: SizedBox(
                width: 250,
                child: CameraDeviceField(session: session),
              ),
            ),
            const SizedBox(height: 14),
            Expanded(
              child: sideBySide
                  ? Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Expanded(
                          flex: 5,
                          child: _PreviewWorkspace(session: session),
                        ),
                        const SizedBox(width: 12),
                        SizedBox(
                          width: controlsWidth,
                          child: _LiveControls(
                            session: session,
                            appState: appState,
                          ),
                        ),
                      ],
                    )
                  : Column(
                      children: [
                        Expanded(child: _PreviewWorkspace(session: session)),
                        const SizedBox(height: 16),
                        SizedBox(
                          height: 280,
                          child: _LiveControls(
                            session: session,
                            appState: appState,
                          ),
                        ),
                      ],
                    ),
            ),
          ],
        );
      },
    );
  }
}

class _PreviewWorkspace extends StatelessWidget {
  const _PreviewWorkspace({required this.session});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      Expanded(child: CameraPreview(session: session)),
      const SizedBox(height: 14),
      Row(
        children: [
          Expanded(child: CameraFormatField(session: session)),
          const SizedBox(width: 12),
          FilledButton.icon(
            onPressed: session.selectedFormat == null || session.previewLoading
                ? null
                : session.isPreviewing
                ? session.stopPreview
                : session.startPreview,
            icon: Icon(
              session.isPreviewing
                  ? Icons.stop_rounded
                  : Icons.play_arrow_rounded,
            ),
            label: Text(
              session.isPreviewing ? 'Stop preview' : 'Start preview',
            ),
          ),
        ],
      ),
    ],
  );
}

class _LiveControls extends StatefulWidget {
  const _LiveControls({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  @override
  State<_LiveControls> createState() => _LiveControlsState();
}

class _LiveControlsState extends State<_LiveControls> {
  bool cameraExpanded = true;
  bool effectsExpanded = true;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      if (cameraExpanded)
        Expanded(
          child: _ControlGroupPanel(
            title: 'Camera controls',
            subtitle: 'Adjust the selected camera and see changes live.',
            icon: Icons.tune_rounded,
            expanded: true,
            onToggle: () => setState(() => cameraExpanded = false),
            onOpenPage: () => widget.appState.navigate(CamoraPage.camera),
            child: ListView(
              children: [
                SwitchListTile(
                  contentPadding: EdgeInsets.zero,
                  dense: true,
                  secondary: const Icon(Icons.flip_rounded, size: 18),
                  title: const Text('Mirror camera'),
                  value: widget.appState.effects.cameraMirrored,
                  onChanged: widget.appState.effects.setCameraMirrored,
                ),
                const Divider(height: 12),
                CameraControlList(session: widget.session, compact: true),
              ],
            ),
          ),
        )
      else
        _ControlGroupPanel(
          title: 'Camera controls',
          subtitle: 'Adjust the selected camera and see changes live.',
          icon: Icons.tune_rounded,
          expanded: false,
          onToggle: () => setState(() => cameraExpanded = true),
          onOpenPage: () => widget.appState.navigate(CamoraPage.camera),
        ),
      const SizedBox(height: 10),
      if (effectsExpanded)
        Expanded(
          child: _ControlGroupPanel(
            title: 'Effects',
            subtitle: widget.session.nativeEffectsAvailable
                ? 'Apply enhancements and see the result live.'
                : 'Restart Camora to load the updated native runner.',
            icon: Icons.auto_awesome_outlined,
            expanded: true,
            onToggle: () => setState(() => effectsExpanded = false),
            onOpenPage: () => widget.appState.navigate(CamoraPage.effects),
            child: ListView(
              children: [
                _StudioEffects(
                  session: widget.session,
                  appState: widget.appState,
                ),
              ],
            ),
          ),
        )
      else
        _ControlGroupPanel(
          title: 'Effects',
          subtitle: 'Apply enhancements and see the result live.',
          icon: Icons.auto_awesome_outlined,
          expanded: false,
          onToggle: () => setState(() => effectsExpanded = true),
          onOpenPage: () => widget.appState.navigate(CamoraPage.effects),
        ),
      if (!cameraExpanded && !effectsExpanded) const Spacer(),
    ],
  );
}

class _ControlGroupPanel extends StatelessWidget {
  const _ControlGroupPanel({
    required this.title,
    required this.subtitle,
    required this.icon,
    required this.expanded,
    required this.onToggle,
    required this.onOpenPage,
    this.child,
  });

  final String title;
  final String subtitle;
  final IconData icon;
  final bool expanded;
  final VoidCallback onToggle;
  final VoidCallback onOpenPage;
  final Widget? child;

  @override
  Widget build(BuildContext context) => CamoraPanel(
    padding: EdgeInsets.fromLTRB(14, expanded ? 12 : 8, 10, expanded ? 14 : 8),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        InkWell(
          onTap: onToggle,
          borderRadius: BorderRadius.circular(8),
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 2),
            child: Row(
              children: [
                Icon(icon, size: 19, color: CamoraColors.purpleLight),
                const SizedBox(width: 9),
                Expanded(
                  child: Text(
                    title,
                    style: const TextStyle(
                      fontSize: 15,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
                IconButton(
                  onPressed: onOpenPage,
                  tooltip: 'Open $title page',
                  visualDensity: VisualDensity.compact,
                  iconSize: 18,
                  icon: const Icon(Icons.open_in_new_rounded),
                ),
                Icon(
                  expanded
                      ? Icons.keyboard_arrow_up_rounded
                      : Icons.keyboard_arrow_down_rounded,
                  color: CamoraColors.muted,
                ),
                const SizedBox(width: 4),
              ],
            ),
          ),
        ),
        if (expanded) ...[
          Padding(
            padding: const EdgeInsets.only(left: 28, right: 6),
            child: Text(
              subtitle,
              style: TextStyle(
                color: subtitle.startsWith('Restart')
                    ? Colors.orangeAccent
                    : CamoraColors.muted,
                fontSize: 11,
              ),
            ),
          ),
          const SizedBox(height: 10),
          Expanded(child: child!),
        ],
      ],
    ),
  );
}

class _StudioEffects extends StatelessWidget {
  const _StudioEffects({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) {
    final effects = appState.effects;
    final active = CameraEffect.values.where(effects.isEnabled).toList();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'Active Effects',
          style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
        ),
        const SizedBox(height: 8),
        if (active.isEmpty)
          const Text(
            'No processing effects are active.',
            style: TextStyle(color: CamoraColors.muted, fontSize: 11),
          )
        else
          Wrap(
            spacing: 6,
            runSpacing: 6,
            children: active
                .map(
                  (effect) => ActionChip(
                    avatar: Icon(_effectIcon(effect), size: 15),
                    label: Text(_activeLabel(effect, effects)),
                    onPressed: () => appState.selectEffect(effect),
                  ),
                )
                .toList(),
          ),
        const SizedBox(height: 10),
        const Divider(height: 1),
        const SizedBox(height: 6),
        ...CameraEffect.values.map(
          (effect) => _StudioEffectControl(
            row: _StudioEffectRow(
              icon: _effectIcon(effect),
              label: _effectName(effect),
              onTap: () => appState.selectEffect(effect),
              trailing: Switch(
                value: effects.isEnabled(effect),
                onChanged:
                    session.nativeEffectsAvailable &&
                        (effect != CameraEffect.backgroundImage ||
                            effects.backgroundImagePath != null)
                    ? (value) => effects.setEnabled(effect, value)
                    : null,
              ),
            ),
            additional: effect == CameraEffect.backgroundImage
                ? _StudioBackgroundImageControl(appState: appState)
                : effects.isEnabled(effect)
                ? _QuickEffectControl(effect: effect, appState: appState)
                : null,
          ),
        ),
      ],
    );
  }

  String _activeLabel(CameraEffect effect, CameraEffectsState effects) {
    final name = _effectName(effect);
    return switch (effect) {
      CameraEffect.backgroundBlur =>
        '$name · ${(effects.backgroundBlurStrength * 100).round()}%',
      CameraEffect.autoFraming =>
        '$name · ${(effects.autoFramingSensitivity * 100).round()}%',
      CameraEffect.lowLightEnhancement =>
        '$name · ${(effects.lowLightStrength * 100).round()}%',
      CameraEffect.backgroundImage =>
        effects.backgroundImagePath!.split('/').last,
      CameraEffect.backgroundRemoval => name,
    };
  }

  String _effectName(CameraEffect effect) => switch (effect) {
    CameraEffect.backgroundBlur => 'Background Blur',
    CameraEffect.backgroundRemoval => 'Background Removal',
    CameraEffect.backgroundImage => 'Virtual Background',
    CameraEffect.autoFraming => 'Auto Framing',
    CameraEffect.lowLightEnhancement => 'Low Light',
  };

  IconData _effectIcon(CameraEffect effect) => switch (effect) {
    CameraEffect.backgroundBlur => Icons.blur_on_outlined,
    CameraEffect.backgroundRemoval => Icons.content_cut_outlined,
    CameraEffect.backgroundImage => Icons.image_outlined,
    CameraEffect.autoFraming => Icons.center_focus_strong_outlined,
    CameraEffect.lowLightEnhancement => Icons.light_mode_outlined,
  };
}

class _StudioBackgroundImageControl extends StatelessWidget {
  const _StudioBackgroundImageControl({required this.appState});

  final AppState appState;

  Future<void> _chooseImage(BuildContext context) async {
    try {
      final result = await FilePicker.pickFile(
        type: FileType.custom,
        allowedExtensions: const [
          'jpg',
          'jpeg',
          'png',
          'webp',
          'gif',
          'svg',
          'mp4',
          'm4v',
          'mov',
          'webm',
        ],
        dialogTitle: 'Choose a virtual background',
      );
      if (!context.mounted || result == null) return;
      final path = result.path;
      if (path == null) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('The selected background has no local path.'),
          ),
        );
        return;
      }
      appState.effects.selectBackgroundImage(path);
    } catch (error) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Could not choose background: $error')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final path = appState.effects.backgroundImagePath;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          dense: true,
          secondary: const Icon(Icons.flip_rounded, size: 18),
          title: const Text('Mirror background'),
          value: appState.effects.backgroundMirrored,
          onChanged: appState.effects.setBackgroundMirrored,
        ),
        if (path != null)
          Text(
            path.split('/').last,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(color: CamoraColors.muted, fontSize: 11),
          ),
        Row(
          children: [
            TextButton.icon(
              onPressed: () => _chooseImage(context),
              icon: Icon(
                path == null
                    ? Icons.add_photo_alternate_outlined
                    : Icons.image_search_outlined,
                size: 16,
              ),
              label: Text(path == null ? 'Choose background' : 'Replace'),
            ),
            if (path != null)
              TextButton(
                onPressed: appState.effects.clearBackgroundImage,
                child: const Text('Remove'),
              ),
          ],
        ),
      ],
    );
  }
}

class _StudioEffectControl extends StatelessWidget {
  const _StudioEffectControl({required this.row, this.additional});

  final Widget row;
  final Widget? additional;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.only(bottom: 4),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        row,
        if (additional != null)
          Padding(
            padding: const EdgeInsets.only(left: 27, right: 2, bottom: 2),
            child: additional!,
          ),
      ],
    ),
  );
}

class _QuickEffectControl extends StatelessWidget {
  const _QuickEffectControl({required this.effect, required this.appState});

  final CameraEffect effect;
  final AppState appState;

  @override
  Widget build(BuildContext context) {
    final effects = appState.effects;
    return switch (effect) {
      CameraEffect.backgroundBlur => _QuickSlider(
        label: 'Blur strength',
        value: effects.backgroundBlurStrength,
        onChanged: effects.setBackgroundBlurStrength,
      ),
      CameraEffect.autoFraming => _QuickSlider(
        label: 'Tracking sensitivity',
        value: effects.autoFramingSensitivity,
        onChanged: effects.setAutoFramingSensitivity,
      ),
      CameraEffect.lowLightEnhancement => _QuickSlider(
        label: 'Enhancement strength',
        value: effects.lowLightStrength,
        onChanged: effects.setLowLightStrength,
      ),
      CameraEffect.backgroundImage => Text(
        effects.backgroundImagePath!.split('/').last,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: const TextStyle(color: CamoraColors.muted, fontSize: 11),
      ),
      CameraEffect.backgroundRemoval => const Text(
        'Subject isolation is active.',
        style: TextStyle(color: CamoraColors.muted, fontSize: 11),
      ),
    };
  }
}

class _QuickSlider extends StatelessWidget {
  const _QuickSlider({
    required this.label,
    required this.value,
    required this.onChanged,
  });

  final String label;
  final double value;
  final ValueChanged<double> onChanged;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      Row(
        children: [
          Expanded(child: Text(label, style: const TextStyle(fontSize: 11))),
          Text(
            '${(value * 100).round()}%',
            style: const TextStyle(color: CamoraColors.muted, fontSize: 11),
          ),
        ],
      ),
      Slider(value: value, onChanged: onChanged),
    ],
  );
}

class _StudioEffectRow extends StatelessWidget {
  const _StudioEffectRow({
    required this.icon,
    required this.label,
    required this.trailing,
    this.onTap,
  });

  final IconData icon;
  final String label;
  final Widget trailing;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) => ConstrainedBox(
    constraints: const BoxConstraints(minHeight: 38),
    child: InkWell(
      onTap: onTap,
      child: Row(
        children: [
          Icon(icon, size: 18, color: CamoraColors.purpleLight),
          const SizedBox(width: 9),
          Expanded(
            child: Text(
              label,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 12),
            ),
          ),
          trailing,
        ],
      ),
    ),
  );
}
