import 'package:flutter/material.dart';

import '../app/camora_theme.dart';
import '../camera/camera_device.dart';
import '../camera/camera_format.dart';
import '../camera/camera_session.dart';

class CamoraPanel extends StatelessWidget {
  const CamoraPanel({
    required this.child,
    this.padding = const EdgeInsets.all(20),
    super.key,
  });
  final Widget child;
  final EdgeInsetsGeometry padding;

  @override
  Widget build(BuildContext context) => Container(
    padding: padding,
    decoration: BoxDecoration(
      color: CamoraColors.surface,
      borderRadius: BorderRadius.circular(14),
      border: Border.all(color: CamoraColors.border),
    ),
    child: child,
  );
}

class StatusPill extends StatelessWidget {
  const StatusPill(this.label, {this.available = false, super.key});
  final String label;
  final bool available;

  @override
  Widget build(BuildContext context) {
    final color = available ? CamoraColors.success : CamoraColors.muted;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withValues(alpha: 0.25)),
      ),
      child: Text(
        label,
        style: TextStyle(
          color: color,
          fontSize: 11,
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
}

class PageHeading extends StatelessWidget {
  const PageHeading({
    required this.title,
    required this.subtitle,
    this.trailing,
    super.key,
  });
  final String title;
  final String subtitle;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) => Row(
    crossAxisAlignment: CrossAxisAlignment.start,
    children: [
      Expanded(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.headlineMedium),
            const SizedBox(height: 4),
            Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
      ),
      ?trailing,
    ],
  );
}

class CameraDeviceField extends StatelessWidget {
  const CameraDeviceField({required this.session, super.key});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => DropdownButtonFormField<CameraDevice>(
    key: ValueKey(session.selectedCamera?.path),
    initialValue: session.selectedCamera,
    isExpanded: true,
    decoration: const InputDecoration(
      labelText: 'Camera device',
      prefixIcon: Icon(Icons.videocam_outlined, size: 19),
    ),
    items: session.cameras
        .map(
          (camera) => DropdownMenuItem(
            value: camera,
            child: Text(camera.name, overflow: TextOverflow.ellipsis),
          ),
        )
        .toList(),
    onChanged: (camera) {
      if (camera != null) session.selectCamera(camera);
    },
  );
}

class CameraFormatField extends StatelessWidget {
  const CameraFormatField({required this.session, super.key});
  final CameraSession session;

  @override
  Widget build(BuildContext context) => DropdownButtonFormField<CameraFormat>(
    key: ValueKey(session.selectedFormat),
    initialValue: session.selectedFormat,
    isExpanded: true,
    decoration: const InputDecoration(
      labelText: 'Capture format',
      prefixIcon: Icon(Icons.aspect_ratio_outlined, size: 19),
    ),
    items: session.formats
        .map(
          (format) => DropdownMenuItem(
            value: format,
            child: Text(format.label, overflow: TextOverflow.ellipsis),
          ),
        )
        .toList(),
    onChanged: (format) {
      if (format != null) session.selectFormat(format);
    },
  );
}

class CameraPreview extends StatelessWidget {
  const CameraPreview({required this.session, super.key});
  final CameraSession session;

  @override
  Widget build(BuildContext context) {
    final format = session.selectedFormat;
    final ratio = format == null ? 16 / 9 : format.width / format.height;
    return Container(
      clipBehavior: Clip.antiAlias,
      decoration: BoxDecoration(
        color: Colors.black,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: CamoraColors.border),
      ),
      child: Stack(
        fit: StackFit.expand,
        children: [
          if (session.textureId != null)
            Center(
              child: AspectRatio(
                aspectRatio: ratio,
                child: Transform(
                  alignment: Alignment.center,
                  transform: Matrix4.diagonal3Values(-1, 1, 1),
                  child: Texture(textureId: session.textureId!),
                ),
              ),
            )
          else
            _EmptyPreview(session: session),
          Positioned(
            left: 14,
            top: 14,
            child: _PreviewStatus(isLive: session.isPreviewing),
          ),
        ],
      ),
    );
  }
}

class _EmptyPreview extends StatelessWidget {
  const _EmptyPreview({required this.session});
  final CameraSession session;

  @override
  Widget build(BuildContext context) {
    if (session.previewLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(
              Icons.videocam_outlined,
              size: 54,
              color: Colors.white24,
            ),
            const SizedBox(height: 14),
            const Text(
              'Camera preview is stopped',
              style: TextStyle(fontSize: 17),
            ),
            const SizedBox(height: 6),
            Text(
              session.selectedCamera == null
                  ? 'Connect a supported V4L2 camera to begin.'
                  : 'Start the preview to view ${session.selectedCamera!.name}.',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodySmall,
            ),
            if (session.previewError != null) ...[
              const SizedBox(height: 12),
              Text(
                session.previewError!,
                textAlign: TextAlign.center,
                style: const TextStyle(color: Colors.redAccent, fontSize: 12),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _PreviewStatus extends StatelessWidget {
  const _PreviewStatus({required this.isLive});
  final bool isLive;

  @override
  Widget build(BuildContext context) => DecoratedBox(
    decoration: BoxDecoration(
      color: const Color(0xCC10131A),
      borderRadius: BorderRadius.circular(20),
      border: Border.all(color: Colors.white12),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 7,
            height: 7,
            decoration: BoxDecoration(
              color: isLive ? CamoraColors.success : CamoraColors.muted,
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 7),
          Text(
            isLive ? 'LIVE' : 'PREVIEW OFF',
            style: const TextStyle(fontSize: 10, fontWeight: FontWeight.w700),
          ),
        ],
      ),
    ),
  );
}
