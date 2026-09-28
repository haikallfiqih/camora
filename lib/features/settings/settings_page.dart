import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../camera/camera_session.dart';
import '../../runtime/gpu_runtime_manager.dart';
import '../../widgets/ui_components.dart';

class SettingsPage extends StatelessWidget {
  const SettingsPage({
    required this.session,
    required this.appState,
    super.key,
  });

  final CameraSession session;
  final AppState appState;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      const PageHeading(
        title: 'Settings',
        subtitle: 'Configure Camora output and review the active pipeline.',
      ),
      const SizedBox(height: 20),
      Expanded(
        child: ListView(
          children: [
            _SettingsSection(
              icon: Icons.flip_rounded,
              title: 'Orientation',
              subtitle: 'Camera and background orientation are independent.',
              children: [
                _SettingSwitch(
                  icon: Icons.videocam_outlined,
                  title: 'Mirror camera',
                  subtitle: 'Reflect the camera and person in the preview and output.',
                  value: appState.effects.cameraMirrored,
                  onChanged: appState.effects.setCameraMirrored,
                ),
                const Divider(height: 1),
                _SettingSwitch(
                  icon: Icons.image_outlined,
                  title: 'Mirror virtual background',
                  subtitle: 'Reflect only replacement images, GIFs, SVGs, and videos.',
                  value: appState.effects.backgroundMirrored,
                  onChanged: appState.effects.setBackgroundMirrored,
                ),
              ],
            ),
            const SizedBox(height: 16),
            _SettingsSection(
              icon: Icons.speed_rounded,
              title: 'GPU acceleration',
              subtitle: 'Optional private NVIDIA runtime.',
              children: [
                _GpuRuntimeControls(
                  runtime: appState.gpuRuntime,
                  backend: session.aiBackend,
                ),
              ],
            ),
            const SizedBox(height: 16),
            _SettingsSection(
              icon: Icons.memory_rounded,
              title: 'Processing',
              subtitle: 'Current Camora runtime status.',
              children: [
                _StatusRow(
                  icon: Icons.preview_outlined,
                  title: 'Camera preview',
                  subtitle: session.isPreviewing
                      ? 'The processed camera pipeline is active.'
                      : 'Start the preview from Studio when you are ready.',
                  label: session.isPreviewing ? 'Live' : 'Stopped',
                  available: session.isPreviewing,
                ),
                const Divider(height: 1),
                _StatusRow(
                  icon: Icons.auto_awesome_outlined,
                  title: 'Effects pipeline',
                  subtitle: session.nativeEffectsAvailable
                      ? 'Native effects are available and update live.'
                      : 'The native effects backend needs attention.',
                  label: session.nativeEffectsAvailable
                      ? 'Available'
                      : 'Unavailable',
                  available: session.nativeEffectsAvailable,
                ),
                const Divider(height: 1),
                _StatusRow(
                  icon: Icons.connected_tv_outlined,
                  title: 'Camora Virtual Camera',
                  subtitle: session.isVirtualCameraRunning
                      ? 'Other applications can use the processed output.'
                      : 'Start it from the Virtual Camera page.',
                  label: session.isVirtualCameraRunning ? 'Running' : 'Stopped',
                  available: session.isVirtualCameraRunning,
                ),
              ],
            ),
            const SizedBox(height: 16),
            const _AboutPanel(),
          ],
        ),
      ),
    ],
  );
}

class _SettingsSection extends StatelessWidget {
  const _SettingsSection({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.children,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(icon, size: 20, color: CamoraColors.purpleLight),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                title,
                style: Theme.of(context).textTheme.titleMedium,
              ),
            ),
          ],
        ),
        const SizedBox(height: 5),
        Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
        const SizedBox(height: 10),
        ...children,
      ],
    ),
  );
}

class _SettingSwitch extends StatelessWidget {
  const _SettingSwitch({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.value,
    required this.onChanged,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) => SwitchListTile(
    contentPadding: EdgeInsets.zero,
    secondary: Icon(icon, color: CamoraColors.muted),
    title: Text(title),
    subtitle: Text(subtitle),
    value: value,
    onChanged: onChanged,
  );
}

class _GpuRuntimeControls extends StatelessWidget {
  const _GpuRuntimeControls({required this.runtime, required this.backend});

  final GpuRuntimeManager runtime;
  final String backend;

  @override
  Widget build(BuildContext context) {
    final downloading = runtime.phase == GpuRuntimePhase.downloading;
    return Column(
      children: [
        _StatusRow(
          icon: backend == 'NVIDIA CUDA'
              ? Icons.bolt_rounded
              : Icons.memory_outlined,
          title: 'Current backend',
          subtitle: runtime.availabilityMessage,
          label: backend,
          available: backend == 'NVIDIA CUDA',
        ),
        if (downloading) ...[
          const SizedBox(height: 4),
          LinearProgressIndicator(value: runtime.progress),
          const SizedBox(height: 8),
          Text(
            runtime.progress == null
                ? 'Downloading GPU runtime…'
                : 'Downloading ${(runtime.progress! * 100).round()}%',
          ),
        ],
        if (runtime.error != null) ...[
          const SizedBox(height: 8),
          Align(
            alignment: Alignment.centerLeft,
            child: Text(
              runtime.error!,
              style: TextStyle(color: Theme.of(context).colorScheme.error),
            ),
          ),
        ],
        const SizedBox(height: 10),
        Align(
          alignment: Alignment.centerLeft,
          child: runtime.installed
              ? OutlinedButton.icon(
                  onPressed: downloading
                      ? null
                      : () => _confirmRemoval(context),
                  icon: const Icon(Icons.delete_outline_rounded),
                  label: const Text('Remove GPU runtime'),
                )
              : FilledButton.icon(
                  onPressed: runtime.canInstall ? runtime.install : null,
                  icon: const Icon(Icons.download_rounded),
                  label: const Text('Enable GPU Acceleration'),
                ),
        ),
      ],
    );
  }

  Future<void> _confirmRemoval(BuildContext context) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Remove GPU runtime?'),
        content: const Text(
          'Camora will continue on CPU. Restart Camora after removal.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Remove'),
          ),
        ],
      ),
    );
    if (confirmed == true) await runtime.remove();
  }
}

class _StatusRow extends StatelessWidget {
  const _StatusRow({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.label,
    required this.available,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final String label;
  final bool available;

  @override
  Widget build(BuildContext context) => ListTile(
    contentPadding: EdgeInsets.zero,
    leading: Icon(icon, color: CamoraColors.muted),
    title: Text(title),
    subtitle: Text(subtitle),
    trailing: StatusPill(label, available: available),
  );
}

class _AboutPanel extends StatelessWidget {
  const _AboutPanel();

  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: LayoutBuilder(
      builder: (context, constraints) {
        final compact = constraints.maxWidth < 560;
        final logo = Image.asset(
          'assets/images/camora_logo.png',
          width: 120,
          height: 64,
          fit: BoxFit.contain,
          filterQuality: FilterQuality.high,
        );
        const details = Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Professional camera controls for Linux',
              style: TextStyle(color: CamoraColors.muted),
            ),
            SizedBox(height: 5),
            Text('Version 1.0.0', style: TextStyle(fontSize: 12)),
            SizedBox(height: 3),
            Text(
              'Made with ❤️ by Taptic Labs',
              style: TextStyle(color: CamoraColors.muted, fontSize: 12),
            ),
          ],
        );

        if (compact) {
          return Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [logo, const SizedBox(height: 10), details],
          );
        }
        return Row(
          children: [
            logo,
            const SizedBox(width: 20),
            const Expanded(child: details),
          ],
        );
      },
    ),
  );
}
