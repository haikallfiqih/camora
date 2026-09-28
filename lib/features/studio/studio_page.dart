import 'package:flutter/material.dart';

import '../../camera/camera_control.dart';
import '../../camera/camera_device.dart';
import '../../camera/v4l2_camera_repository.dart';

class StudioPage extends StatefulWidget {
  const StudioPage({super.key});

  @override
  State<StudioPage> createState() =>
      _StudioPageState();
}

class _StudioPageState
    extends State<StudioPage> {
  late final V4l2CameraRepository repository;

  List<CameraDevice> cameras = [];
  List<CameraControl> controls = [];

  CameraDevice? selected;

  String? error;

  @override
  void initState() {
    super.initState();

    try {
      repository =
          V4l2CameraRepository();

      _loadCameras();
    } catch (e) {
      error = e.toString();
    }
  }

  void _loadCameras() {
    try {
      final result =
          repository.listCameras();

      CameraDevice? preferred;

      if (result.isNotEmpty) {
        preferred = result.firstWhere(
          (camera) =>
              camera.name
                  .toLowerCase()
                  .contains('emeet'),
          orElse: () => result.first,
        );
      }

      setState(() {
        cameras = result;
        selected = preferred;
        error = null;
      });

      if (preferred != null) {
        _loadControls(preferred);
      }
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  void _loadControls(
    CameraDevice camera,
  ) {
    try {
      final result =
          repository.listControls(
        camera.path,
      );

      setState(() {
        controls = result;
      });
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  void _selectCamera(
    CameraDevice? camera,
  ) {
    if (camera == null) return;

    setState(() {
      selected = camera;
      controls = [];
    });

    _loadControls(camera);
  }

  void _setControl(
    CameraControl control,
    int value,
  ) {
    final camera = selected;

    if (camera == null) return;

    final success =
        repository.setControl(
      camera.path,
      control.id,
      value,
    );

    if (!success) return;

    // Re-read everything because changing an
    // automatic control can activate/deactivate
    // dependent controls.
    _loadControls(camera);
  }

  @override
  Widget build(BuildContext context) {
    if (error != null) {
      return Scaffold(
        body: Center(
          child: SelectableText(
            error!,
            style: const TextStyle(
              color: Colors.redAccent,
            ),
          ),
        ),
      );
    }

    return Scaffold(
      body: SafeArea(
        child: Column(
          children: [
            _header(),

            Expanded(
              child: Row(
                children: [
                  Expanded(
                    flex: 7,
                    child: _preview(),
                  ),

                  SizedBox(
                    width: 430,
                    child: _controlsPanel(),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _header() {
    return Padding(
      padding:
          const EdgeInsets.all(20),
      child: Row(
        children: [
          const Text(
            'Camora',
            style: TextStyle(
              fontSize: 28,
              fontWeight:
                  FontWeight.w700,
            ),
          ),

          const SizedBox(width: 24),

          Expanded(
            child:
                DropdownButtonFormField<
                    CameraDevice>(
              initialValue: selected,
              decoration:
                  const InputDecoration(
                labelText: 'Camera',
                border:
                    OutlineInputBorder(),
              ),
              items: cameras
                  .map(
                    (camera) =>
                        DropdownMenuItem(
                      value: camera,
                      child:
                          Text(camera.name),
                    ),
                  )
                  .toList(),
              onChanged: _selectCamera,
            ),
          ),

          const SizedBox(width: 12),

          IconButton(
            onPressed: _loadCameras,
            icon:
                const Icon(Icons.refresh),
          ),
        ],
      ),
    );
  }

  Widget _preview() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(
        20,
        0,
        10,
        20,
      ),
      child: Container(
        decoration: BoxDecoration(
          color:
              const Color(0xFF090A0C),
          borderRadius:
              BorderRadius.circular(18),
          border: Border.all(
            color: Colors.white10,
          ),
        ),
        child: const Center(
          child: Column(
            mainAxisSize:
                MainAxisSize.min,
            children: [
              Icon(
                Icons.videocam_outlined,
                size: 48,
                color: Colors.white24,
              ),
              SizedBox(height: 12),
              Text(
                'Live Preview',
                style: TextStyle(
                  fontSize: 18,
                ),
              ),
              SizedBox(height: 4),
              Text(
                'Coming in M3',
                style: TextStyle(
                  color: Colors.white38,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _controlsPanel() {
    return Container(
      margin: const EdgeInsets.fromLTRB(
        10,
        0,
        20,
        20,
      ),
      decoration: BoxDecoration(
        color: const Color(0xFF17181C),
        borderRadius:
            BorderRadius.circular(18),
        border: Border.all(
          color: Colors.white10,
        ),
      ),
      child: controls.isEmpty
          ? const Center(
              child:
                  CircularProgressIndicator(),
            )
          : ListView.separated(
              padding:
                  const EdgeInsets.all(20),
              itemCount:
                  controls.length,
              separatorBuilder: (_, __) =>
                  const Divider(
                height: 32,
              ),
              itemBuilder:
                  (context, index) {
                return _controlWidget(
                  controls[index],
                );
              },
            ),
    );
  }

  Widget _controlWidget(
    CameraControl control,
  ) {
    return Opacity(
      opacity:
          control.inactive ? 0.4 : 1,
      child: IgnorePointer(
        ignoring: control.inactive,
        child: switch (control.type) {
          'boolean' =>
            _booleanControl(control),

          'menu' ||
          'integer_menu' =>
            _menuControl(control),

          'integer' =>
            _sliderControl(control),

          _ => _unsupported(control),
        },
      ),
    );
  }

  Widget _booleanControl(
    CameraControl control,
  ) {
    return Row(
      children: [
        Expanded(
          child: Text(
            control.name,
            style: const TextStyle(
              fontWeight:
                  FontWeight.w500,
            ),
          ),
        ),
        Switch(
          value: control.value != 0,
          onChanged: (value) {
            _setControl(
              control,
              value ? 1 : 0,
            );
          },
        ),
      ],
    );
  }

  Widget _menuControl(
    CameraControl control,
  ) {
    return Column(
      crossAxisAlignment:
          CrossAxisAlignment.start,
      children: [
        Text(
          control.name,
          style: const TextStyle(
            fontWeight:
                FontWeight.w500,
          ),
        ),

        const SizedBox(height: 10),

        DropdownButtonFormField<int>(
          initialValue:
              control.value,
          decoration:
              const InputDecoration(
            border:
                OutlineInputBorder(),
          ),
          items: control.options
              .map(
                (option) =>
                    DropdownMenuItem(
                  value: option.value,
                  child:
                      Text(option.label),
                ),
              )
              .toList(),
          onChanged: (value) {
            if (value != null) {
              _setControl(
                control,
                value,
              );
            }
          },
        ),
      ],
    );
  }

  Widget _sliderControl(
    CameraControl control,
  ) {
    final divisions =
        control.step > 0
            ? ((control.max -
                        control.min) ~/
                    control.step)
                .clamp(1, 1000)
            : null;

    return Column(
      crossAxisAlignment:
          CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                control.name,
                style:
                    const TextStyle(
                  fontWeight:
                      FontWeight.w500,
                ),
              ),
            ),
            Text(
              '${control.value}',
              style:
                  const TextStyle(
                color: Colors.white54,
              ),
            ),
          ],
        ),

        Slider(
          min:
              control.min.toDouble(),
          max:
              control.max.toDouble(),
          divisions: divisions,
          value: control.value
              .clamp(
                control.min,
                control.max,
              )
              .toDouble(),
          onChanged: (value) {
            final step =
                control.step <= 0
                    ? 1
                    : control.step;

            final snapped =
                control.min +
                    (((value -
                                    control
                                        .min) /
                                step)
                            .round() *
                        step);

            _setControl(
              control,
              snapped.clamp(
                control.min,
                control.max,
              ),
            );
          },
        ),

        Row(
          mainAxisAlignment:
              MainAxisAlignment
                  .spaceBetween,
          children: [
            Text(
              '${control.min}',
              style:
                  const TextStyle(
                color: Colors.white38,
                fontSize: 11,
              ),
            ),
            Text(
              'Default ${control.defaultValue}',
              style:
                  const TextStyle(
                color: Colors.white38,
                fontSize: 11,
              ),
            ),
            Text(
              '${control.max}',
              style:
                  const TextStyle(
                color: Colors.white38,
                fontSize: 11,
              ),
            ),
          ],
        ),
      ],
    );
  }

  Widget _unsupported(
    CameraControl control,
  ) {
    return Text(
      '${control.name} '
      '(${control.type})',
      style: const TextStyle(
        color: Colors.white38,
      ),
    );
  }
}
