import 'package:flutter/material.dart';

import 'features/studio/studio_page.dart';

void main() {
  runApp(const CamoraApp());
}

class CamoraApp extends StatelessWidget {
  const CamoraApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Camora',
      theme: ThemeData(
        brightness: Brightness.dark,
        useMaterial3: true,
        scaffoldBackgroundColor: const Color(0xFF101114),
      ),
      home: const StudioPage(),
    );
  }
}
