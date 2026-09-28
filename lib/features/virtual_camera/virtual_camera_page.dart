import 'package:flutter/material.dart';

import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../widgets/ui_components.dart';

class VirtualCameraPage extends StatelessWidget {
  const VirtualCameraPage({required this.session, super.key});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      const PageHeading(
        title: 'Virtual Camera',
        subtitle: 'Configure an output for video conferencing applications.',
      ),
      const SizedBox(height: 20),
      Expanded(
        child: ListView(
          children: [
            CamoraPanel(
              child: Row(
                children: [
                  Container(
                    width: 10,
                    height: 10,
                    decoration: const BoxDecoration(
                      color: CamoraColors.muted,
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 12),
                  const Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Virtual camera is unavailable',
                          style: TextStyle(
                            fontSize: 16,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                        SizedBox(height: 4),
                        Text(
                          'The output device and processing pipeline have not been implemented.',
                          style: TextStyle(color: CamoraColors.muted),
                        ),
                      ],
                    ),
                  ),
                  const StatusPill('Not implemented'),
                ],
              ),
            ),
            const SizedBox(height: 16),
            CamoraPanel(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Output configuration',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 16),
                  DropdownButtonFormField<String>(
                    initialValue: session.selectedFormat?.resolution,
                    decoration: const InputDecoration(
                      labelText: 'Output resolution',
                    ),
                    items: session.selectedFormat == null
                        ? const []
                        : [
                            DropdownMenuItem(
                              value: session.selectedFormat!.resolution,
                              child: Text(session.selectedFormat!.resolution),
                            ),
                          ],
                    onChanged: null,
                  ),
                  const SizedBox(height: 12),
                  DropdownButtonFormField<int>(
                    initialValue: session.selectedFormat?.fps,
                    decoration: const InputDecoration(
                      labelText: 'Output frame rate',
                    ),
                    items: session.selectedFormat == null
                        ? const []
                        : [
                            DropdownMenuItem(
                              value: session.selectedFormat!.fps,
                              child: Text('${session.selectedFormat!.fps} FPS'),
                            ),
                          ],
                    onChanged: null,
                  ),
                  const SizedBox(height: 18),
                  SizedBox(
                    width: double.infinity,
                    child: FilledButton.icon(
                      onPressed: null,
                      icon: const Icon(Icons.play_arrow_rounded),
                      label: const Text('Start virtual camera'),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    ],
  );
}
