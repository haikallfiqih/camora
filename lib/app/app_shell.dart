import 'package:flutter/material.dart';

import '../camera/camera_session.dart';
import '../features/camera/camera_page.dart';
import '../features/effects/effects_page.dart';
import '../features/settings/settings_page.dart';
import '../features/studio/studio_page.dart';
import '../features/virtual_camera/virtual_camera_page.dart';
import 'app_state.dart';
import 'camora_theme.dart';

class AppShell extends StatefulWidget {
  const AppShell({super.key});

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> {
  late final CameraSession session;
  late final AppState appState;

  @override
  void initState() {
    super.initState();
    appState = AppState();
    session = CameraSession(appState.effects);
    session.initialize();
  }

  @override
  void dispose() {
    session.dispose();
    appState.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: AnimatedBuilder(
          animation: Listenable.merge([session, appState]),
          builder: (context, _) => LayoutBuilder(
            builder: (context, constraints) {
              final expanded = constraints.maxWidth >= 920;
              return Row(
                children: [
                  _Sidebar(appState: appState, expanded: expanded),
                  Expanded(
                    child: Column(
                      children: [
                        if (session.error != null)
                          _ErrorBanner(
                            message: session.error!,
                            onRetry: session.refreshCameras,
                          ),
                        Expanded(
                          child: Padding(
                            padding: EdgeInsets.fromLTRB(
                              expanded ? 20 : 16,
                              18,
                              expanded ? 20 : 16,
                              18,
                            ),
                            child: _currentPage(),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }

  Widget _currentPage() => switch (appState.currentPage) {
    CamoraPage.studio => StudioPage(session: session, appState: appState),
    CamoraPage.camera => CameraPage(session: session, appState: appState),
    CamoraPage.effects => EffectsPage(session: session, appState: appState),
    CamoraPage.virtualCamera => VirtualCameraPage(session: session),
    CamoraPage.settings => const SettingsPage(),
  };
}

class _Sidebar extends StatelessWidget {
  const _Sidebar({required this.appState, required this.expanded});
  final AppState appState;
  final bool expanded;

  static const destinations = [
    (CamoraPage.studio, 'Studio', Icons.home_filled),
    (CamoraPage.camera, 'Camera', Icons.videocam_outlined),
    (CamoraPage.effects, 'Effects', Icons.auto_awesome_outlined),
    (CamoraPage.virtualCamera, 'Virtual Camera', Icons.connected_tv_outlined),
    (CamoraPage.settings, 'Settings', Icons.settings_outlined),
  ];

  @override
  Widget build(BuildContext context) => Container(
    width: expanded ? 220 : 64,
    decoration: const BoxDecoration(
      color: CamoraColors.sidebar,
      border: Border(right: BorderSide(color: CamoraColors.border)),
    ),
    child: Column(
      children: [
        Padding(
          padding: EdgeInsets.symmetric(
            horizontal: expanded ? 16 : 12,
            vertical: 18,
          ),
          child: Row(
            mainAxisAlignment: expanded
                ? MainAxisAlignment.start
                : MainAxisAlignment.center,
            children: [
              const _BrandMark(),
              if (expanded) ...[
                const SizedBox(width: 8),
                // const Flexible(
                //   child: Text(
                //     'Camora',
                //     maxLines: 1,
                //     overflow: TextOverflow.ellipsis,
                //     style: TextStyle(fontSize: 20, fontWeight: FontWeight.w700),
                //   ),
                // ),
              ],
            ],
          ),
        ),
        const SizedBox(height: 4),
        ...destinations.map(
          (item) => Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
            child: _NavigationItem(
              icon: item.$3,
              label: item.$2,
              selected: appState.currentPage == item.$1,
              expanded: expanded,
              onTap: () => appState.navigate(item.$1),
            ),
          ),
        ),
        const Spacer(),
        if (expanded)
          const Padding(
            padding: EdgeInsets.all(16),
            child: Text(
              'Camora by Taptic Labs',
              style: TextStyle(color: CamoraColors.muted, fontSize: 11),
            ),
          ),
      ],
    ),
  );
}

class _NavigationItem extends StatelessWidget {
  const _NavigationItem({
    required this.icon,
    required this.label,
    required this.selected,
    required this.expanded,
    required this.onTap,
  });
  final IconData icon;
  final String label;
  final bool selected;
  final bool expanded;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => Tooltip(
    message: expanded ? '' : label,
    child: Material(
      color: selected
          ? CamoraColors.purple.withValues(alpha: 0.24)
          : Colors.transparent,
      borderRadius: BorderRadius.circular(10),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(10),
        child: Padding(
          padding: EdgeInsets.symmetric(
            horizontal: expanded ? 10 : 0,
            vertical: 10,
          ),
          child: Row(
            mainAxisAlignment: expanded
                ? MainAxisAlignment.start
                : MainAxisAlignment.center,
            children: [
              Icon(
                icon,
                size: 21,
                color: selected ? Colors.white : CamoraColors.muted,
              ),
              if (expanded) ...[
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    label,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      fontSize: 14,
                      color: selected ? Colors.white : const Color(0xFFD2D6DF),
                      fontWeight: selected ? FontWeight.w600 : FontWeight.w400,
                    ),
                  ),
                ),
              ],
            ],
          ),
        ),
      ),
    ),
  );
}

class _BrandMark extends StatelessWidget {
  const _BrandMark();

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 120,
      height: 80,
      child: Image.asset(
        'assets/images/camora_logo.png',
        fit: BoxFit.contain,
        filterQuality: FilterQuality.high,
      ),
    );
  }
}

class _ErrorBanner extends StatelessWidget {
  const _ErrorBanner({required this.message, required this.onRetry});
  final String message;
  final VoidCallback onRetry;
  @override
  Widget build(BuildContext context) => MaterialBanner(
    content: Text(message, maxLines: 2, overflow: TextOverflow.ellipsis),
    leading: const Icon(Icons.error_outline, color: Colors.redAccent),
    actions: [TextButton(onPressed: onRetry, child: const Text('Retry'))],
  );
}
