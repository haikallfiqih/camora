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
            children: [CameraControlList(session: session, compact: true)],
          ),
        ),
      ],
    ),
  );
}
