import 'package:flutter/material.dart';

abstract final class CamoraColors {
  static const background = Color(0xFF0B0D12);
  static const sidebar = Color(0xFF10131A);
  static const surface = Color(0xFF141821);
  static const surfaceRaised = Color(0xFF1A1F2A);
  static const border = Color(0xFF292F3C);
  static const muted = Color(0xFF929BAC);
  static const purple = Color(0xFF7957F6);
  static const purpleLight = Color(0xFF9B83FF);
  static const success = Color(0xFF3BD47A);
}

ThemeData buildCamoraTheme() {
  final scheme = ColorScheme.fromSeed(
    seedColor: CamoraColors.purple,
    brightness: Brightness.dark,
    surface: CamoraColors.surface,
  );

  return ThemeData(
    brightness: Brightness.dark,
    useMaterial3: true,
    colorScheme: scheme,
    scaffoldBackgroundColor: CamoraColors.background,
    fontFamily: 'sans-serif',
    dividerColor: CamoraColors.border,
    textTheme: const TextTheme(
      headlineMedium: TextStyle(fontSize: 26, fontWeight: FontWeight.w700),
      titleLarge: TextStyle(fontSize: 18, fontWeight: FontWeight.w600),
      titleMedium: TextStyle(fontSize: 15, fontWeight: FontWeight.w600),
      bodyMedium: TextStyle(fontSize: 14, color: Color(0xFFD6DAE3)),
      bodySmall: TextStyle(fontSize: 12, color: CamoraColors.muted),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: CamoraColors.surfaceRaised,
      contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(10),
        borderSide: const BorderSide(color: CamoraColors.border),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(10),
        borderSide: const BorderSide(color: CamoraColors.border),
      ),
    ),
    switchTheme: SwitchThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
        (states) => states.contains(WidgetState.selected)
            ? Colors.white
            : CamoraColors.muted,
      ),
      trackColor: WidgetStateProperty.resolveWith(
        (states) => states.contains(WidgetState.selected)
            ? CamoraColors.purple
            : CamoraColors.surfaceRaised,
      ),
    ),
    sliderTheme: const SliderThemeData(
      activeTrackColor: CamoraColors.purple,
      inactiveTrackColor: CamoraColors.border,
      thumbColor: Colors.white,
      overlayColor: Color(0x337957F6),
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: CamoraColors.purple,
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
    ),
  );
}
