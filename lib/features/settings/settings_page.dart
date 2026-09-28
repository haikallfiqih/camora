import 'package:flutter/material.dart';

import '../../app/camora_theme.dart';
import '../../widgets/ui_components.dart';

class SettingsPage extends StatelessWidget {
  const SettingsPage({super.key});

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
            const _SettingsSection(
              title: 'General',
              children: [
                _UnavailableSetting(
                  title: 'Start preview on launch',
                  subtitle: 'Automatic preview startup is not implemented yet.',
                ),
                _UnavailableSetting(
                  title: 'Minimize to tray',
                  subtitle: 'System tray integration is not implemented yet.',
                ),
              ],
            ),
            const SizedBox(height: 16),
            const _SettingsSection(
              title: 'Performance',
              children: [
                _UnavailableSetting(
                  title: 'Hardware acceleration',
                  subtitle: 'Runtime acceleration selection is not available.',
                ),
                ListTile(
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

class _UnavailableSetting extends StatelessWidget {
  const _UnavailableSetting({required this.title, required this.subtitle});
  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) => ListTile(
    contentPadding: EdgeInsets.zero,
    title: Text(title),
    subtitle: Text(subtitle),
    trailing: const StatusPill('Coming soon'),
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
