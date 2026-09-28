import 'package:flutter/material.dart';

import '../../camera/camera_device.dart';
import '../../camera/v4l2_camera_repository.dart';

class StudioPage extends StatefulWidget {
  const StudioPage({super.key});

  @override
  State<StudioPage> createState() => _StudioPageState();
}

class _StudioPageState extends State<StudioPage> {
  final repository = V4l2CameraRepository();

  List<CameraDevice> cameras = [];
  CameraDevice? selected;
  String? error;

  @override
  void initState() {
    super.initState();
    loadCameras();
  }

  void loadCameras() {
    try {
      final result = repository.listCameras();

      setState(() {
        cameras = result;

        if (result.isNotEmpty) {
          selected = result.firstWhere(
            (camera) =>
                camera.name.toLowerCase().contains('emeet'),
            orElse: () => result.first,
          );
        }

        error = null;
      });
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  const Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Camora',
                          style: TextStyle(
                            fontSize: 30,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        SizedBox(height: 4),
                        Text(
                          'Linux camera control',
                          style: TextStyle(
                            color: Colors.white54,
                          ),
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    onPressed: loadCameras,
                    tooltip: 'Refresh cameras',
                    icon: const Icon(Icons.refresh),
                  ),
                ],
              ),
              const SizedBox(height: 32),

              if (error != null)
                SelectableText(
                  error!,
                  style: const TextStyle(color: Colors.redAccent),
                )
              else if (cameras.isEmpty)
                const Text('No V4L2 capture camera found.')
              else ...[
                const Text(
                  'CAMERA',
                  style: TextStyle(
                    fontSize: 12,
                    letterSpacing: 1.5,
                    color: Colors.white54,
                  ),
                ),
                const SizedBox(height: 8),

                DropdownButtonFormField<CameraDevice>(
                  initialValue: selected,
                  decoration: const InputDecoration(
                    border: OutlineInputBorder(),
                  ),
                  items: cameras.map((camera) {
                    return DropdownMenuItem(
                      value: camera,
                      child: Text(camera.name),
                    );
                  }).toList(),
                  onChanged: (camera) {
                    setState(() {
                      selected = camera;
                    });
                  },
                ),

                const SizedBox(height: 24),

                if (selected != null)
                  Container(
                    width: double.infinity,
                    padding: const EdgeInsets.all(20),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(16),
                      color: Colors.white.withValues(alpha: 0.05),
                      border: Border.all(
                        color: Colors.white.withValues(alpha: 0.08),
                      ),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          selected!.name,
                          style: const TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                        const SizedBox(height: 16),
                        _Info('Device', selected!.path),
                        _Info('Driver', selected!.driver),
                        _Info('Bus', selected!.bus),
                      ],
                    ),
                  ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _Info extends StatelessWidget {
  final String label;
  final String value;

  const _Info(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        children: [
          SizedBox(
            width: 80,
            child: Text(
              label,
              style: const TextStyle(color: Colors.white54),
            ),
          ),
          Expanded(
            child: SelectableText(value),
          ),
        ],
      ),
    );
  }
}
