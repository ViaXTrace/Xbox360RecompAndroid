import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class AppTheme {
  static const _xboxGreen = Color(0xFF107C10);
  static const _xboxGreenLight = Color(0xFF52B043);
  static const _xboxDark = Color(0xFF0D0D0D);
  static const _xboxSurface = Color(0xFF1A1A1A);
  static const _xboxCard = Color(0xFF252525);
  static const _xboxCardHover = Color(0xFF303030);
  static const _xboxBorder = Color(0xFF333333);

  static ThemeData dark() {
    final base = ThemeData.dark();
    return base.copyWith(
      useMaterial3: true,
      colorScheme: const ColorScheme.dark(
        primary: _xboxGreen,
        primaryContainer: Color(0xFF0A4A0A),
        secondary: _xboxGreenLight,
        secondaryContainer: Color(0xFF1A5C1A),
        surface: _xboxSurface,
        background: _xboxDark,
        onBackground: Colors.white,
        onSurface: Colors.white,
        onPrimary: Colors.white,
        error: Color(0xFFCF6679),
        outline: _xboxBorder,
      ),
      scaffoldBackgroundColor: _xboxDark,
      cardTheme: const CardTheme(
        color: _xboxCard,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(12)),
        ),
      ),
      textTheme: GoogleFonts.interTextTheme(base.textTheme).copyWith(
        headlineLarge: GoogleFonts.inter(
          fontSize: 32,
          fontWeight: FontWeight.w700,
          color: Colors.white,
        ),
        headlineMedium: GoogleFonts.inter(
          fontSize: 24,
          fontWeight: FontWeight.w600,
          color: Colors.white,
        ),
        titleLarge: GoogleFonts.inter(
          fontSize: 20,
          fontWeight: FontWeight.w600,
          color: Colors.white,
        ),
        titleMedium: GoogleFonts.inter(
          fontSize: 16,
          fontWeight: FontWeight.w500,
          color: Colors.white,
        ),
        bodyLarge: GoogleFonts.inter(
          fontSize: 16,
          color: Colors.white,
        ),
        bodyMedium: GoogleFonts.inter(
          fontSize: 14,
          color: Color(0xFFCCCCCC),
        ),
        labelMedium: GoogleFonts.inter(
          fontSize: 12,
          letterSpacing: 0.5,
          color: Color(0xFF999999),
        ),
      ),
      appBarTheme: const AppBarTheme(
        backgroundColor: _xboxDark,
        elevation: 0,
        scrolledUnderElevation: 0,
        centerTitle: false,
        foregroundColor: Colors.white,
        surfaceTintColor: Colors.transparent,
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: _xboxGreen,
          foregroundColor: Colors.white,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: const RoundedRectangleBorder(
            borderRadius: BorderRadius.all(Radius.circular(8)),
          ),
          textStyle: GoogleFonts.inter(
            fontSize: 15,
            fontWeight: FontWeight.w600,
          ),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: Colors.white,
          side: const BorderSide(color: _xboxBorder),
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
          shape: const RoundedRectangleBorder(
            borderRadius: BorderRadius.all(Radius.circular(8)),
          ),
        ),
      ),
      iconButtonTheme: IconButtonThemeData(
        style: IconButton.styleFrom(
          foregroundColor: Colors.white,
        ),
      ),
      dividerTheme: const DividerThemeData(
        color: _xboxBorder,
        thickness: 1,
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: _xboxCard,
        contentTextStyle: GoogleFonts.inter(color: Colors.white),
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(8)),
        ),
        behavior: SnackBarBehavior.floating,
      ),
      progressIndicatorTheme: const ProgressIndicatorThemeData(
        color: _xboxGreen,
        linearTrackColor: _xboxBorder,
      ),
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) return _xboxGreen;
          return Colors.grey;
        }),
        trackColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) return _xboxGreenLight.withOpacity(0.3);
          return _xboxBorder;
        }),
      ),
      sliderTheme: const SliderThemeData(
        activeTrackColor: _xboxGreen,
        thumbColor: _xboxGreenLight,
        overlayColor: Color(0x2052B043),
        inactiveTrackColor: _xboxBorder,
      ),
      dialogTheme: const DialogTheme(
        backgroundColor: _xboxCard,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(16)),
        ),
      ),
      bottomSheetTheme: const BottomSheetThemeData(
        backgroundColor: _xboxSurface,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
        ),
      ),
      chipTheme: ChipThemeData(
        backgroundColor: _xboxCard,
        selectedColor: _xboxGreen.withOpacity(0.3),
        labelStyle: GoogleFonts.inter(fontSize: 12, color: Colors.white),
        side: const BorderSide(color: _xboxBorder),
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(6)),
        ),
      ),
    );
  }
}
