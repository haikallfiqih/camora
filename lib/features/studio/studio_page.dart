import 'package:flutter/material.dart';

import '../../camera/camera_control.dart';
import '../../camera/camera_device.dart';
import '../../camera/camera_format.dart';
import '../../camera/v4l2_camera_repository.dart';
import '../../camera/camora_video.dart';

class StudioPage extends StatefulWidget {
  const StudioPage({super.key});

  @override
  State<StudioPage> createState() => _StudioPageState();
}

class _StudioPageState extends State<StudioPage> {
  late final V4l2CameraRepository repository;

  List<CameraDevice> cameras = [];
  List<CameraControl> controls = [];
  List<CameraFormat> formats = [];

  CameraFormat? selectedFormat;

  CameraDevice? selected;

  String? error;

  int? textureId;
  bool previewLoading = false;
  String? previewError;

  @override
  void initState() {
    super.initState();

    repository = V4l2CameraRepository();
    _loadCameras();
  }

  Future<void> _loadCameras() async {
    try {
      final result = await repository.listCameras();

      CameraDevice? preferred;

      if (result.isNotEmpty) {
        preferred = result.firstWhere(
          (camera) => camera.name.toLowerCase().contains('emeet'),
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
        _loadFormats(preferred);
      }
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  Future<void> _loadFormats(CameraDevice camera) async {
    try {
      final result = await repository.listFormats(camera.path);

      CameraFormat? preferred;

      if (result.isNotEmpty) {
        // Prefer 1080p60 when available.
        for (final format in result) {
          if (format.width == 1920 &&
              format.height == 1080 &&
              format.fps == 60) {
            preferred = format;
            break;
          }
        }

        preferred ??= result.first;
      }

      setState(() {
        formats = result;
        selectedFormat = preferred;
      });
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  Future<void> _loadControls(CameraDevice camera) async {
    try {
      final result = await repository.listControls(camera.path);

      setState(() {
        controls = result;
      });
    } catch (e) {
      setState(() {
        error = e.toString();
      });
    }
  }

  void _selectCamera(CameraDevice? camera) {
    if (camera == null) return;

    final previewWasRunning = textureId != null;

    setState(() {
      selected = camera;
      controls = [];
      formats = [];
      selectedFormat = null;
    });

    _loadControls(camera);
    _loadFormats(camera);

    if (previewWasRunning) {
      _restartPreviewAfterCameraChange();
    }
  }

  Future<void> _setControl(CameraControl control, int value) async {
    final camera = selected;

    if (camera == null) return;

    final success = await repository.setControl(camera.path, control.id, value);

    if (!success) return;

    // Re-read everything because changing an
    // automatic control can activate/deactivate
    // dependent controls.
    _loadControls(camera);
  }

  Future<void> _restartPreviewAfterCameraChange() async {
    await _stopPreview();

    // Allow format discovery/state update to finish.
    await Future<void>.delayed(const Duration(milliseconds: 100));

    if (!mounted) return;

    await _startPreview();
  }

  Future<void> _selectFormat(CameraFormat? format) async {
    if (format == null) return;

    final wasRunning = textureId != null;

    setState(() {
      selectedFormat = format;
    });

    if (!wasRunning) return;

    await _stopPreview();

    if (!mounted) return;

    await _startPreview();
  }

  Future<void> _startPreview() async {
    final camera = selected;

    if (camera == null) return;

    setState(() {
      previewLoading = true;
      previewError = null;
    });

    try {
      final format = selectedFormat;

      if (format == null) {
        throw StateError('No supported camera format selected.');
      }

      final id = await CamoraVideo.start(
        device: camera.path,
        width: format.width,
        height: format.height,
        fps: format.fps,
      );

      if (!mounted) return;

      setState(() {
        textureId = id;
        previewLoading = false;
      });
    } catch (e) {
      if (!mounted) return;

      setState(() {
        previewLoading = false;
        previewError = e.toString();
      });
    }
  }

  Future<void> _stopPreview() async {
    await CamoraVideo.stop();

    if (!mounted) return;

    setState(() {
      textureId = null;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (error != null) {
      return Scaffold(
        body: Center(
          child: SelectableText(
            error!,
            style: const TextStyle(color: Colors.redAccent),
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
                  Expanded(flex: 7, child: _preview()),

                  SizedBox(width: 430, child: _controlsPanel()),
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
      padding: const EdgeInsets.all(20),
      child: Row(
        children: [
          const Text(
            'Camora',
            style: TextStyle(fontSize: 28, fontWeight: FontWeight.w700),
          ),

          const SizedBox(width: 24),

          Expanded(
            child: DropdownButtonFormField<CameraDevice>(
              initialValue: selected,
              decoration: const InputDecoration(
                labelText: 'Camera',
                border: OutlineInputBorder(),
              ),
              items: cameras
                  .map(
                    (camera) => DropdownMenuItem(
                      value: camera,
                      child: Text(camera.name),
                    ),
                  )
                  .toList(),
              onChanged: _selectCamera,
            ),
          ),

          const SizedBox(width: 12),

          SizedBox(
            width: 260,
            child: DropdownButtonFormField<CameraFormat>(
              initialValue: selectedFormat,
              decoration: const InputDecoration(
                labelText: 'Format',
                border: OutlineInputBorder(),
              ),
              items: formats
                  .map(
                    (format) => DropdownMenuItem(
                      value: format,
                      child: Text(
                        format.label,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  )
                  .toList(),
              onChanged: _selectFormat,
            ),
          ),

          const SizedBox(width: 12),

          IconButton(onPressed: _loadCameras, icon: const Icon(Icons.refresh)),
        ],
      ),
    );
  }

  Widget _preview() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(20, 0, 10, 20),
      child: Container(
        clipBehavior: Clip.antiAlias,
        decoration: BoxDecoration(
          color: const Color(0xFF090A0C),
          borderRadius: BorderRadius.circular(18),
          border: Border.all(color: Colors.white10),
        ),
        child: Stack(
          fit: StackFit.expand,
          children: [
            if (textureId != null)
              Center(
                child: AspectRatio(
                  aspectRatio: 16 / 9,
                  child: Transform(
                    alignment: Alignment.center,
                    transform: Matrix4.diagonal3Values(-1.0, 1.0, 1.0),
                    child: Texture(textureId: textureId!),
                  ),
                ),
              )
            else
              Center(
                child: previewLoading
                    ? const CircularProgressIndicator()
                    : Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const Icon(
                            Icons.videocam_outlined,
                            size: 48,
                            color: Colors.white24,
                          ),
                          const SizedBox(height: 12),
                          const Text(
                            'Live Preview',
                            style: TextStyle(fontSize: 18),
                          ),
                          const SizedBox(height: 16),
                          FilledButton.icon(
                            onPressed: _startPreview,
                            icon: const Icon(Icons.play_arrow),
                            label: const Text('Start Preview'),
                          ),
                          if (previewError != null) ...[
                            const SizedBox(height: 12),
                            Padding(
                              padding: const EdgeInsets.symmetric(
                                horizontal: 24,
                              ),
                              child: Text(
                                previewError!,
                                textAlign: TextAlign.center,
                                style: const TextStyle(color: Colors.redAccent),
                              ),
                            ),
                          ],
                        ],
                      ),
              ),

            if (textureId != null)
              Positioned(
                left: 16,
                bottom: 16,
                child: FilledButton.icon(
                  onPressed: _stopPreview,
                  icon: const Icon(Icons.stop),
                  label: const Text('Stop Preview'),
                ),
              ),
          ],
        ),
      ),
    );
  }

  Widget _controlsPanel() {
    return Container(
      margin: const EdgeInsets.fromLTRB(10, 0, 20, 20),
      decoration: BoxDecoration(
        color: const Color(0xFF17181C),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: Colors.white10),
      ),
      child: controls.isEmpty
          ? const Center(child: CircularProgressIndicator())
          : ListView.separated(
              padding: const EdgeInsets.all(20),
              itemCount: controls.length,
              separatorBuilder: (_, _) => const Divider(height: 32),
              itemBuilder: (context, index) {
                return _controlWidget(controls[index]);
              },
            ),
    );
  }

  Widget _controlWidget(CameraControl control) {
    return Opacity(
      opacity: control.inactive ? 0.4 : 1,
      child: IgnorePointer(
        ignoring: control.inactive,
        child: switch (control.type) {
          'boolean' => _booleanControl(control),

          'menu' || 'integer_menu' => _menuControl(control),

          'integer' => _sliderControl(control),

          _ => _unsupported(control),
        },
      ),
    );
  }

  Widget _booleanControl(CameraControl control) {
    return Row(
      children: [
        Expanded(
          child: Text(
            control.name,
            style: const TextStyle(fontWeight: FontWeight.w500),
          ),
        ),
        Switch(
          value: control.value != 0,
          onChanged: (value) {
            _setControl(control, value ? 1 : 0);
          },
        ),
      ],
    );
  }

  Widget _menuControl(CameraControl control) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(control.name, style: const TextStyle(fontWeight: FontWeight.w500)),

        const SizedBox(height: 10),

        DropdownButtonFormField<int>(
          initialValue: control.value,
          decoration: const InputDecoration(border: OutlineInputBorder()),
          items: control.options
              .map(
                (option) => DropdownMenuItem(
                  value: option.value,
                  child: Text(option.label),
                ),
              )
              .toList(),
          onChanged: (value) {
            if (value != null) {
              _setControl(control, value);
            }
          },
        ),
      ],
    );
  }

  Widget _sliderControl(CameraControl control) {
    final divisions = control.step > 0
        ? ((control.max - control.min) ~/ control.step).clamp(1, 1000)
        : null;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                control.name,
                style: const TextStyle(fontWeight: FontWeight.w500),
              ),
            ),
            Text(
              '${control.value}',
              style: const TextStyle(color: Colors.white54),
            ),
          ],
        ),

        Slider(
          min: control.min.toDouble(),
          max: control.max.toDouble(),
          divisions: divisions,
          value: control.value.clamp(control.min, control.max).toDouble(),
          onChanged: (value) {
            final step = control.step <= 0 ? 1 : control.step;

            final snapped =
                control.min + (((value - control.min) / step).round() * step);

            _setControl(control, snapped.clamp(control.min, control.max));
          },
        ),

        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              '${control.min}',
              style: const TextStyle(color: Colors.white38, fontSize: 11),
            ),
            Text(
              'Default ${control.defaultValue}',
              style: const TextStyle(color: Colors.white38, fontSize: 11),
            ),
            Text(
              '${control.max}',
              style: const TextStyle(color: Colors.white38, fontSize: 11),
            ),
          ],
        ),
      ],
    );
  }

  Widget _unsupported(CameraControl control) {
    return Text(
      '${control.name} '
      '(${control.type})',
      style: const TextStyle(color: Colors.white38),
    );
  }
}
