import 'package:flutter/material.dart';

import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../widgets/ui_components.dart';

class VirtualCameraPage extends StatelessWidget {
  const VirtualCameraPage({required this.session, super.key});

  final CameraSession session;

  @override
  Widget build(BuildContext context) {
    final running = session.isVirtualCameraRunning;
    final format = session.selectedFormat;
    final cameraReady =
        session.selectedCamera != null && session.selectedFormat != null;

    return Column(
      children: [
        const PageHeading(
          title: 'Camora Virtual Camera',
          subtitle: 'Use your Camora video in calls, meetings, and recordings.',
        ),
        const SizedBox(height: 20),
        Expanded(
          child: ListView(
            children: [
              CamoraPanel(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Container(
                          width: 10,
                          height: 10,
                          decoration: BoxDecoration(
                            color: running
                                ? CamoraColors.success
                                : CamoraColors.muted,
                            shape: BoxShape.circle,
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            running
                                ? 'Camora Virtual Camera is on'
                                : 'Camora Virtual Camera is off',
                            style: const TextStyle(
                              fontSize: 17,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                        StatusPill(running ? 'Live' : 'Off'),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Text(
                      running
                          ? 'Select “Camora Virtual Camera” as the camera in your other app.'
                          : 'Start it when you are ready to share Camora’s processed video.',
                      style: const TextStyle(color: CamoraColors.muted),
                    ),
                    if (session.virtualCameraError != null) ...[
                      const SizedBox(height: 12),
                      Text(
                        session.virtualCameraError!,
                        style: TextStyle(
                          color: Theme.of(context).colorScheme.error,
                        ),
                      ),
                    ],
                    const SizedBox(height: 22),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        onPressed: !cameraReady || session.virtualCameraLoading
                            ? null
                            : running
                            ? session.stopVirtualCamera
                            : session.startVirtualCamera,
                        icon: session.virtualCameraLoading
                            ? const SizedBox.square(
                                dimension: 18,
                                child: CircularProgressIndicator(
                                  strokeWidth: 2,
                                ),
                              )
                            : Icon(
                                running
                                    ? Icons.stop_rounded
                                    : Icons.play_arrow_rounded,
                              ),
                        label: Text(
                          session.virtualCameraLoading
                              ? 'Please wait…'
                              : running
                              ? 'Stop'
                              : 'Start',
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 16),
              CamoraPanel(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Output',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    const SizedBox(height: 12),
                    const _OutputRow(
                      label: 'Camera name',
                      value: 'Camora Virtual Camera',
                    ),
                    const SizedBox(height: 8),
                    _OutputRow(
                      label: 'Video',
                      value: format == null
                          ? 'No camera selected'
                          : '${format.resolution} · ${format.fps} FPS',
                    ),
                    const SizedBox(height: 8),
                    const _OutputRow(label: 'Effects', value: 'Updates live'),
                  ],
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _OutputRow extends StatelessWidget {
  const _OutputRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) => Row(
    children: [
      SizedBox(
        width: 110,
        child: Text(label, style: const TextStyle(color: CamoraColors.muted)),
      ),
      Expanded(
        child: Text(
          value,
          textAlign: TextAlign.end,
          style: const TextStyle(fontWeight: FontWeight.w500),
        ),
      ),
    ],
  );
}
