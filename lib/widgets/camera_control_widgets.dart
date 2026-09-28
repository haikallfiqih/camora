import 'package:flutter/material.dart';

import '../app/camora_theme.dart';
import '../camera/camera_control.dart';
import '../camera/camera_session.dart';

class CameraControlList extends StatelessWidget {
  const CameraControlList({
    required this.session,
    this.compact = false,
    super.key,
  });

  final CameraSession session;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    if (session.isLoading) return const LinearProgressIndicator();
    if (session.controls.isEmpty) {
      return const Padding(
        padding: EdgeInsets.symmetric(vertical: 24),
        child: Center(
          child: Text(
            'This camera did not report adjustable controls.',
            textAlign: TextAlign.center,
            style: TextStyle(color: CamoraColors.muted),
          ),
        ),
      );
    }

    return Column(
      children: [
        for (var index = 0; index < session.controls.length; index++) ...[
          CameraControlEditor(
            session: session,
            control: session.controls[index],
            compact: compact,
          ),
          if (index != session.controls.length - 1)
            SizedBox(height: compact ? 12 : 18),
        ],
      ],
    );
  }
}

class CameraControlEditor extends StatelessWidget {
  const CameraControlEditor({
    required this.session,
    required this.control,
    this.compact = false,
    super.key,
  });

  final CameraSession session;
  final CameraControl control;
  final bool compact;

  @override
  Widget build(BuildContext context) => Opacity(
    opacity: control.inactive ? 0.45 : 1,
    child: IgnorePointer(
      ignoring: control.inactive,
      child: switch (control.type) {
        'boolean' => SwitchListTile(
          dense: compact,
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
              isExpanded: true,
              items: control.options
                  .map(
                    (option) => DropdownMenuItem(
                      value: option.value,
                      child: Text(
                        option.label,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  )
                  .toList(),
              onChanged: (value) {
                if (value != null) session.setControl(control, value);
              },
            ),
          ],
        ),
        'integer' => _IntegerControl(
          session: session,
          control: control,
          compact: compact,
        ),
        _ => Text(
          '${control.name} (${control.type})',
          style: Theme.of(context).textTheme.bodySmall,
        ),
      },
    ),
  );
}

class _IntegerControl extends StatelessWidget {
  const _IntegerControl({
    required this.session,
    required this.control,
    required this.compact,
  });

  final CameraSession session;
  final CameraControl control;
  final bool compact;

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
        if (!compact)
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
