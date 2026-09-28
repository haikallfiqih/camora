import 'package:flutter/material.dart';

import '../../app/app_state.dart';
import '../../app/camora_theme.dart';
import '../../widgets/ui_components.dart';

class SettingsPage extends StatelessWidget {
  const SettingsPage({required this.appState, super.key});
  final AppState appState;

  @override
  Widget build(BuildContext context) => Column(
    children: [
      const PageHeading(
        title: 'Settings',
        subtitle: 'Customize your Camora experience.',
      ),
      const SizedBox(height: 20),
      Expanded(
        child: ListView(
          children: [
            _SettingsSection(
              title: 'General',
              children: [
                _SettingSwitch(
                  title: 'Start preview on launch',
                  subtitle: 'Automatically open the selected camera when Camora starts.',
                  value: appState.startPreviewOnLaunch,
                  onChanged: appState.setStartPreviewOnLaunch,
                ),
                _SettingSwitch(
                  title: 'Minimize to tray',
                  subtitle: 'Keep Camora available when its window is closed.',
                  value: appState.minimizeToTray,
                  onChanged: appState.setMinimizeToTray,
                ),
              ],
            ),
            const SizedBox(height: 16),
            _SettingsSection(
              title: 'Performance',
              children: [
                _SettingSwitch(
                  title: 'Hardware acceleration',
                  subtitle: 'Use Flutter hardware acceleration for the application UI.',
                  value: appState.hardwareAcceleration,
                  onChanged: appState.setHardwareAcceleration,
                ),
                const ListTile(
                  contentPadding: EdgeInsets.zero,
                  title: Text('Video processing'),
                  subtitle: Text(
                    'No effects processing backend is currently active.',
                  ),
                  trailing: StatusPill('Unavailable'),
                ),
              ],
            ),
            const SizedBox(height: 16),
            CamoraPanel(
              child: Row(
                children: [
                  const _CamoraMark(size: 48),
                  const SizedBox(width: 14),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Camora',
                          style: Theme.of(context).textTheme.titleLarge,
                        ),
                        const SizedBox(height: 3),
                        const Text(
                          'Professional camera controls for Linux',
                          style: TextStyle(color: CamoraColors.muted),
                        ),
                      ],
                    ),
                  ),
                  const Column(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Text('Version 1.0.0'),
                      SizedBox(height: 4),
                      Text(
                        'Flutter for Linux',
                        style: TextStyle(
                          color: CamoraColors.muted,
                          fontSize: 12,
                        ),
                      ),
                    ],
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

class _SettingsSection extends StatelessWidget {
  const _SettingsSection({required this.title, required this.children});
  final String title;
  final List<Widget> children;
  @override
  Widget build(BuildContext context) => CamoraPanel(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 8),
        ...children,
      ],
    ),
  );
}

class _SettingSwitch extends StatelessWidget {
  const _SettingSwitch({
    required this.title,
    required this.subtitle,
    required this.value,
    required this.onChanged,
  });
  final String title;
  final String subtitle;
  final bool value;
  final ValueChanged<bool> onChanged;
  @override
  Widget build(BuildContext context) => SwitchListTile(
    contentPadding: EdgeInsets.zero,
    title: Text(title),
    subtitle: Text(subtitle),
    value: value,
    onChanged: onChanged,
  );
}

class _CamoraMark extends StatelessWidget {
  const _CamoraMark({required this.size});
  final double size;
  @override
  Widget build(BuildContext context) => Container(
    width: size,
    height: size,
    decoration: BoxDecoration(
      borderRadius: BorderRadius.circular(size * 0.32),
      gradient: const LinearGradient(
        colors: [Color(0xFF55A9FF), CamoraColors.purple, Color(0xFFF05BD1)],
        begin: Alignment.bottomLeft,
        end: Alignment.topRight,
      ),
    ),
    child: Icon(Icons.videocam_rounded, size: size * 0.55, color: Colors.white),
  );
}
