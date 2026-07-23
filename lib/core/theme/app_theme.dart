import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class AppTheme {
  AppTheme._();

  // Xbox brand colors
  static const Color xboxGreen = Color(0xFF107C10);
  static const Color xboxGreenLight = Color(0xFF52B043);
  static const Color xboxGreenDim = Color(0xFF0D5C0D);

  // Surface hierarchy
  static const Color bgBase = Color(0xFF0D0D0D);
  static const Color surface = Color(0xFF1A1A1A);
  static const Color surfaceVariant = Color(0xFF232323);
  static const Color card = Color(0xFF1E1E1E);
  static const Color border = Color(0xFF2E2E2E);
  static const Color borderLight = Color(0xFF3A3A3A);

  // Button colors (Xbox ABXY)
  static const Color buttonA = Color(0xFF52B043);
  static const Color buttonB = Color(0xFFD62B2B);
  static const Color buttonX = Color(0xFF1F72C4);
  static const Color buttonY = Color(0xFFD4AC0A);

  // Status colors
  static const Color statusPlayable = Color(0xFF52B043);
  static const Color statusIngame = Color(0xFF4A9DE8);
  static const Color statusBoots = Color(0xFFD4AC0A);
  static const Color statusNothing = Color(0xFFCF6679);
  static const Color statusUntested = Color(0xFF777777);

  static ThemeData dark() {
    final cs = ColorScheme.dark(
      primary: xboxGreen,
      onPrimary: Colors.white,
      primaryContainer: xboxGreenDim,
      onPrimaryContainer: xboxGreenLight,
      secondary: xboxGreenLight,
      onSecondary: Colors.white,
      surface: surface,
      onSurface: Colors.white,
      surfaceContainerHighest: surfaceVariant,
      onSurfaceVariant: Colors.white70,
      outline: border,
      outlineVariant: borderLight,
      error: const Color(0xFFCF6679),
      onError: Colors.white,
      shadow: Colors.black,
      scrim: Colors.black87,
      inverseSurface: Colors.white,
      onInverseSurface: Colors.black,
    );

    final textTheme = GoogleFonts.interTextTheme(ThemeData.dark().textTheme).copyWith(
      displayLarge: GoogleFonts.inter(fontSize: 32, fontWeight: FontWeight.w800, color: Colors.white, letterSpacing: -0.5),
      displayMedium: GoogleFonts.inter(fontSize: 26, fontWeight: FontWeight.w700, color: Colors.white),
      headlineLarge: GoogleFonts.inter(fontSize: 22, fontWeight: FontWeight.w700, color: Colors.white),
      headlineMedium: GoogleFonts.inter(fontSize: 18, fontWeight: FontWeight.w600, color: Colors.white),
      headlineSmall: GoogleFonts.inter(fontSize: 16, fontWeight: FontWeight.w600, color: Colors.white),
      titleLarge: GoogleFonts.inter(fontSize: 15, fontWeight: FontWeight.w600, color: Colors.white),
      titleMedium: GoogleFonts.inter(fontSize: 14, fontWeight: FontWeight.w500, color: Colors.white),
      titleSmall: GoogleFonts.inter(fontSize: 13, fontWeight: FontWeight.w500, color: Colors.white70),
      bodyLarge: GoogleFonts.inter(fontSize: 15, color: Colors.white),
      bodyMedium: GoogleFonts.inter(fontSize: 14, color: Colors.white70),
      bodySmall: GoogleFonts.inter(fontSize: 12, color: Colors.white54),
      labelLarge: GoogleFonts.inter(fontSize: 13, fontWeight: FontWeight.w600, color: Colors.white),
      labelMedium: GoogleFonts.inter(fontSize: 11, fontWeight: FontWeight.w600, color: Colors.white70, letterSpacing: 0.5),
      labelSmall: GoogleFonts.inter(fontSize: 10, fontWeight: FontWeight.w500, color: Colors.white54, letterSpacing: 0.5),
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: cs,
      textTheme: textTheme,
      scaffoldBackgroundColor: bgBase,
      appBarTheme: AppBarTheme(
        backgroundColor: bgBase,
        surfaceTintColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0,
        centerTitle: false,
        iconTheme: const IconThemeData(color: Colors.white),
        titleTextStyle: GoogleFonts.inter(
          fontSize: 20, fontWeight: FontWeight.w700, color: Colors.white, letterSpacing: -0.3,
        ),
      ),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: surface,
        surfaceTintColor: Colors.transparent,
        indicatorColor: xboxGreen.withAlpha(40),
        iconTheme: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return const IconThemeData(color: xboxGreenLight, size: 24);
          }
          return const IconThemeData(color: Colors.white38, size: 24);
        }),
        labelTextStyle: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return GoogleFonts.inter(fontSize: 11, fontWeight: FontWeight.w600, color: xboxGreenLight);
          }
          return GoogleFonts.inter(fontSize: 11, color: Colors.white38);
        }),
        elevation: 0,
        shadowColor: Colors.transparent,
        height: 64,
      ),
      cardTheme: CardThemeData(
        color: card,
        elevation: 0,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(14),
          side: const BorderSide(color: border, width: 1),
        ),
        margin: EdgeInsets.zero,
        clipBehavior: Clip.antiAlias,
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: xboxGreen,
          foregroundColor: Colors.white,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
          textStyle: GoogleFonts.inter(fontSize: 14, fontWeight: FontWeight.w600),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: Colors.white,
          side: const BorderSide(color: border),
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
          textStyle: GoogleFonts.inter(fontSize: 14, fontWeight: FontWeight.w500),
        ),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
          foregroundColor: xboxGreenLight,
          textStyle: GoogleFonts.inter(fontSize: 14, fontWeight: FontWeight.w500),
        ),
      ),
      floatingActionButtonTheme: const FloatingActionButtonThemeData(
        backgroundColor: xboxGreen,
        foregroundColor: Colors.white,
        elevation: 4,
        shape: CircleBorder(),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: surfaceVariant,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: border),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: border),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: xboxGreen, width: 1.5),
        ),
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        hintStyle: GoogleFonts.inter(color: Colors.white38, fontSize: 14),
        prefixIconColor: Colors.white38,
        suffixIconColor: Colors.white38,
      ),
      dividerTheme: const DividerThemeData(color: border, thickness: 1, space: 1),
      listTileTheme: ListTileThemeData(
        tileColor: Colors.transparent,
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
        titleTextStyle: GoogleFonts.inter(fontSize: 14, fontWeight: FontWeight.w500, color: Colors.white),
        subtitleTextStyle: GoogleFonts.inter(fontSize: 12, color: Colors.white54),
        iconColor: Colors.white54,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith((s) =>
            s.contains(WidgetState.selected) ? xboxGreenLight : Colors.grey[600]),
        trackColor: WidgetStateProperty.resolveWith((s) =>
            s.contains(WidgetState.selected) ? xboxGreen.withAlpha(80) : surfaceVariant),
        trackOutlineColor: WidgetStateProperty.resolveWith((s) =>
            s.contains(WidgetState.selected) ? xboxGreen : border),
      ),
      sliderTheme: const SliderThemeData(
        activeTrackColor: xboxGreen,
        thumbColor: xboxGreenLight,
        overlayColor: Color(0x2552B043),
        inactiveTrackColor: border,
        trackHeight: 3,
      ),
      chipTheme: ChipThemeData(
        backgroundColor: surfaceVariant,
        selectedColor: xboxGreen.withAlpha(50),
        labelStyle: GoogleFonts.inter(fontSize: 12, fontWeight: FontWeight.w500, color: Colors.white),
        side: const BorderSide(color: border),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      ),
      progressIndicatorTheme: const ProgressIndicatorThemeData(
        color: xboxGreen,
        linearTrackColor: border,
        circularTrackColor: border,
        linearMinHeight: 4,
      ),
      dialogTheme: DialogThemeData(
        backgroundColor: surface,
        surfaceTintColor: Colors.transparent,
        elevation: 8,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(20),
          side: const BorderSide(color: border),
        ),
        titleTextStyle: GoogleFonts.inter(fontSize: 17, fontWeight: FontWeight.w700, color: Colors.white),
        contentTextStyle: GoogleFonts.inter(fontSize: 14, color: Colors.white70),
      ),
      bottomSheetTheme: const BottomSheetThemeData(
        backgroundColor: surface,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
        ),
        dragHandleColor: Color(0xFF444444),
        showDragHandle: true,
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: surfaceVariant,
        contentTextStyle: GoogleFonts.inter(fontSize: 13, color: Colors.white),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        behavior: SnackBarBehavior.floating,
        elevation: 4,
        actionTextColor: xboxGreenLight,
      ),
      tabBarTheme: TabBarThemeData(
        labelColor: xboxGreenLight,
        unselectedLabelColor: Colors.white38,
        indicatorColor: xboxGreen,
        labelStyle: GoogleFonts.inter(fontSize: 13, fontWeight: FontWeight.w600),
        unselectedLabelStyle: GoogleFonts.inter(fontSize: 13, fontWeight: FontWeight.w400),
        dividerColor: border,
        indicatorSize: TabBarIndicatorSize.label,
      ),
      segmentedButtonTheme: SegmentedButtonThemeData(
        style: ButtonStyle(
          backgroundColor: WidgetStateProperty.resolveWith((s) =>
              s.contains(WidgetState.selected) ? xboxGreen.withAlpha(60) : Colors.transparent),
          foregroundColor: WidgetStateProperty.resolveWith((s) =>
              s.contains(WidgetState.selected) ? xboxGreenLight : Colors.white54),
          side: WidgetStateProperty.all(const BorderSide(color: border)),
          textStyle: WidgetStateProperty.all(GoogleFonts.inter(fontSize: 12, fontWeight: FontWeight.w600)),
        ),
      ),
    );
  }
}
