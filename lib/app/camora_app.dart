import 'package:flutter/material.dart';

import 'app_shell.dart';
import 'camora_theme.dart';

class CamoraApp extends StatelessWidget {
  const CamoraApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Camora',
      theme: buildCamoraTheme(),
      home: const AppShell(),
    );
  }
}
