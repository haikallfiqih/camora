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

class _LiveControls extends StatelessWidget {
  const _LiveControls({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) => CamoraPanel(
    padding: const EdgeInsets.fromLTRB(16, 14, 16, 16),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Expanded(
              child: Text(
                'Camera controls',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
              ),
            ),
            IconButton(
              onPressed: () => appState.navigate(CamoraPage.camera),
              tooltip: 'Open camera settings',
              visualDensity: VisualDensity.compact,
              icon: const Icon(Icons.chevron_right_rounded),
            ),
          ],
        ),
        const Text(
          'Adjust the selected camera and see changes live.',
          style: TextStyle(color: CamoraColors.muted, fontSize: 12),
        ),
        const SizedBox(height: 12),
        Expanded(
          child: ListView(
            children: [
              CameraControlList(session: session, compact: true),
              const SizedBox(height: 16),
              const Divider(height: 1),
              const SizedBox(height: 10),
              _StudioEffects(session: session, appState: appState),
            ],
          ),
        ),
      ],
    ),
  );
}

class _StudioEffects extends StatelessWidget {
  const _StudioEffects({required this.session, required this.appState});

  final CameraSession session;
  final AppState appState;

  static const _unavailableEffects = [
    ('Background Blur', Icons.blur_on_outlined),
    ('Background Removal', Icons.content_cut_outlined),
    ('Background Image', Icons.image_outlined),
    ('Auto Framing', Icons.center_focus_strong_outlined),
  ];

  @override
  Widget build(BuildContext context) {
    final enabled = appState.effectEnabled(CameraEffect.lowLightEnhancement);
    final nativeAvailable = session.nativeEffectsAvailable;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Expanded(
              child: Text(
                'Effects',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
              ),
            ),
            IconButton(
              onPressed: () => appState.navigate(CamoraPage.effects),
              tooltip: 'Open effects settings',
              visualDensity: VisualDensity.compact,
              icon: const Icon(Icons.chevron_right_rounded),
            ),
          ],
        ),
        Text(
          nativeAvailable
              ? 'Apply enhancements and see the result live.'
              : 'Restart Camora to load the updated native effects runner.',
          style: TextStyle(
            color: nativeAvailable ? CamoraColors.muted : Colors.orangeAccent,
            fontSize: 12,
          ),
        ),
        const SizedBox(height: 10),
        _StudioEffectRow(
          icon: Icons.light_mode_outlined,
          label: 'Low Light Enhancement',
          trailing: Switch(
            value: enabled,
            onChanged: nativeAvailable
                ? (value) {
                    appState.setEffect(CameraEffect.lowLightEnhancement, value);
                    session.configureLowLight(
                      enabled: value,
                      strength: appState.lowLightStrength,
                    );
                  }
                : null,
          ),
        ),
        if (enabled && nativeAvailable) ...[
          Padding(
            padding: const EdgeInsets.only(left: 34, right: 2),
            child: Row(
              children: [
                const Expanded(
                  child: Text(
                    'Strength',
                    style: TextStyle(color: CamoraColors.muted, fontSize: 11),
                  ),
                ),
                Text(
                  '${(appState.lowLightStrength * 100).round()}%',
                  style: const TextStyle(
                    color: CamoraColors.muted,
                    fontSize: 11,
                  ),
                ),
              ],
            ),
          ),
          Slider(
            value: appState.lowLightStrength,
            onChanged: (value) {
              appState.setLowLightStrength(value);
              session.configureLowLight(enabled: true, strength: value);
            },
          ),
        ],
        ..._unavailableEffects.map(
          (effect) => _StudioEffectRow(
            icon: effect.$2,
            label: effect.$1,
            trailing: const Tooltip(
              message: 'Requires subject segmentation',
              child: Icon(
                Icons.lock_outline_rounded,
                size: 16,
                color: CamoraColors.muted,
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _StudioEffectRow extends StatelessWidget {
  const _StudioEffectRow({
    required this.icon,
    required this.label,
    required this.trailing,
  });

  final IconData icon;
  final String label;
  final Widget trailing;

  @override
  Widget build(BuildContext context) => ConstrainedBox(
    constraints: const BoxConstraints(minHeight: 38),
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
  );
}
