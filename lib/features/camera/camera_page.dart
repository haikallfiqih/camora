import 'package:flutter/material.dart';

import '../../app/camora_theme.dart';
import '../../camera/camera_control.dart';
import '../../camera/camera_format.dart';
import '../../camera/camera_session.dart';
import '../../widgets/ui_components.dart';

class CameraPage extends StatelessWidget {
  const CameraPage({required this.session, super.key});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      PageHeading(
        title: 'Camera',
        subtitle: 'Select a device, capture format, and hardware controls.',
        trailing: IconButton(
          onPressed: session.refreshCameras,
          tooltip: 'Refresh cameras',
          icon: const Icon(Icons.refresh_rounded),
        ),
      ),
      const SizedBox(height: 20),
      Expanded(
        child: ListView(
          children: [
            CamoraPanel(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Camera device',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 12),
                  CameraDeviceField(session: session),
                  if (session.selectedCamera case final camera?) ...[
                    const SizedBox(height: 10),
                    Text(
                      '${camera.path}  •  ${camera.driver}  •  ${camera.bus}',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ],
              ),
            ),
            const SizedBox(height: 16),
            LayoutBuilder(
              builder: (context, constraints) {
                final wide = constraints.maxWidth >= 900;
                final formats = _FormatsPanel(session: session);
                final controls = _ControlsPanel(session: session);
                if (wide) {
                  return Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Expanded(child: formats),
                      const SizedBox(width: 16),
                      Expanded(child: controls),
                    ],
                  );
                }
                return Column(
                  children: [formats, const SizedBox(height: 16), controls],
                );
              },
            ),
          ],
        ),
      ),
    ],
  );
}

class _FormatsPanel extends StatelessWidget {
  const _FormatsPanel({required this.session});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Supported formats',
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 6),
        Text(
          '${session.formats.length} MJPEG capture modes reported by the device',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 12),
        if (session.isLoading)
          const LinearProgressIndicator()
        else if (session.formats.isEmpty)
          const _EmptyMessage('No supported MJPEG formats found.')
        else
          ...session.formats.map(
            (format) => _FormatRow(session: session, format: format),
          ),
      ],
    ),
  );
}

class _FormatRow extends StatelessWidget {
  const _FormatRow({required this.session, required this.format});
  final CameraSession session;
  final CameraFormat format;

  @override
  Widget build(BuildContext context) {
    final selected = session.selectedFormat == format;
    return ListTile(
      onTap: () => session.selectFormat(format),
      contentPadding: EdgeInsets.zero,
      leading: Icon(
        selected
            ? Icons.radio_button_checked_rounded
            : Icons.radio_button_unchecked_rounded,
        color: selected ? CamoraColors.purpleLight : CamoraColors.muted,
      ),
      title: Text('${format.width} × ${format.height}'),
      subtitle: Text('${format.fps} FPS  •  ${format.pixelFormat}'),
      trailing: selected ? const StatusPill('Selected', available: true) : null,
    );
  }
}

class _ControlsPanel extends StatelessWidget {
  const _ControlsPanel({required this.session});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('V4L2 controls', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 6),
        Text(
          'Controls are read directly from the selected camera.',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 16),
        if (session.isLoading)
          const LinearProgressIndicator()
        else if (session.controls.isEmpty)
          const _EmptyMessage('This device did not report adjustable controls.')
        else
          ...session.controls.map(
            (control) => Padding(
              padding: const EdgeInsets.only(bottom: 18),
              child: _ControlEditor(session: session, control: control),
            ),
          ),
      ],
    ),
  );
}

class _ControlEditor extends StatelessWidget {
  const _ControlEditor({required this.session, required this.control});
  final CameraSession session;
  final CameraControl control;

  @override
  Widget build(BuildContext context) => Opacity(
    opacity: control.inactive ? 0.45 : 1,
    child: IgnorePointer(
      ignoring: control.inactive,
      child: switch (control.type) {
        'boolean' => SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: Text(control.name),
          value: control.value != 0,
          onChanged: (value) => session.setControl(control, value ? 1 : 0),
        ),
        'menu' || 'integer_menu' => Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(control.name),
            const SizedBox(height: 8),
            DropdownButtonFormField<int>(
              initialValue: control.value,
              items: control.options
                  .map(
                    (option) => DropdownMenuItem(
                      value: option.value,
                      child: Text(option.label),
                    ),
                  )
                  .toList(),
              onChanged: (value) {
                if (value != null) session.setControl(control, value);
              },
            ),
          ],
        ),
        'integer' => _IntegerControl(session: session, control: control),
        _ => Text(
          '${control.name} (${control.type})',
          style: Theme.of(context).textTheme.bodySmall,
        ),
      },
    ),
  );
}

class _IntegerControl extends StatelessWidget {
  const _IntegerControl({required this.session, required this.control});
  final CameraSession session;
  final CameraControl control;

  @override
  Widget build(BuildContext context) {
    final divisions = control.step > 0
        ? ((control.max - control.min) ~/ control.step).clamp(1, 1000)
        : null;
    return Column(
      children: [
        Row(
          children: [
            Expanded(child: Text(control.name)),
            Text(
              '${control.value}',
              style: Theme.of(context).textTheme.bodySmall,
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
            session.setControl(
              control,
              snapped.clamp(control.min, control.max),
            );
          },
        ),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              '${control.min}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            Text(
              'Default ${control.defaultValue}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            Text(
              '${control.max}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
        ),
      ],
    );
  }
}

class _EmptyMessage extends StatelessWidget {
  const _EmptyMessage(this.text);
  final String text;
  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 24),
    child: Center(
      child: Text(text, style: const TextStyle(color: CamoraColors.muted)),
    ),
  );
}
