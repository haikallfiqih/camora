import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../widgets/ui_components.dart';

class StudioPage extends StatelessWidget {
  const StudioPage({required this.session, required this.appState, super.key});
  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final wide = constraints.maxWidth >= 1050;
        return Column(
          children: [
            PageHeading(
              title: 'Studio',
              subtitle: 'Preview and configure your camera.',
              trailing: SizedBox(
                width: 290,
                child: CameraDeviceField(session: session),
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: wide
                  ? Row(
                      children: [
                        Expanded(child: _PreviewWorkspace(session: session)),
                        const SizedBox(width: 18),
                        SizedBox(
                          width: 320,
                          child: _QuickEffects(appState: appState),
                        ),
                      ],
                    )
                  : Column(
                      children: [
                        Expanded(child: _PreviewWorkspace(session: session)),
                        const SizedBox(height: 16),
                        SizedBox(
                          height: 210,
                          child: _QuickEffects(appState: appState),
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

class _QuickEffects extends StatelessWidget {
  const _QuickEffects({required this.appState});
  final AppState appState;

  static const effects = [
    (CameraEffect.backgroundBlur, 'Background blur', Icons.blur_on_outlined),
    (
      CameraEffect.backgroundRemoval,
      'Background removal',
      Icons.content_cut_outlined,
    ),
    (CameraEffect.backgroundImage, 'Background image', Icons.image_outlined),
    (
      CameraEffect.autoFraming,
      'Auto framing',
      Icons.center_focus_strong_outlined,
    ),
    (
      CameraEffect.lowLightEnhancement,
      'Low light enhancement',
      Icons.light_mode_outlined,
    ),
  ];

  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            const Expanded(
              child: Text(
                'Quick effects',
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
              ),
            ),
            TextButton(
              onPressed: () => appState.navigate(CamoraPage.effects),
              child: const Text('View all'),
            ),
          ],
        ),
        const Text(
          'Effect processing is not available yet.',
          style: TextStyle(color: CamoraColors.muted, fontSize: 12),
        ),
        const SizedBox(height: 12),
        Expanded(
          child: ListView(
            children: effects
                .map(
                  (item) => ListTile(
                    dense: true,
                    contentPadding: EdgeInsets.zero,
                    leading: Icon(item.$3, size: 20, color: CamoraColors.muted),
                    title: Text(item.$2),
                    trailing: const StatusPill('Coming soon'),
                  ),
                )
                .toList(),
          ),
        ),
      ],
    ),
  );
}
