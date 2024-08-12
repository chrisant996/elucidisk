#pragma once

// 1903 18362
enum class PreferredAppMode
{
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};

enum class DarkModeMode
{
    Auto,
    Light,
    Dark,
};

// Initializes dark mode if possible, and returns whether it was successful.
bool AllowDarkMode();
bool IsDarkModeSupported();

// Returns whether the app should use dark mode.  This is a combination of
// whether dark mode is available, plus whether high contrast mode is active.
bool ShouldUseDarkMode();

bool IsHighContrast();
bool IsColorSchemeChangeMessage(LPARAM lParam);
bool IsColorSchemeChangeMessage(UINT message, LPARAM lParam);
bool DarkModeOnThemeChanged(HWND hWnd, DarkModeMode dmm=DarkModeMode::Auto);

UINT32 GetForeColor(bool dark_mode);
UINT32 GetBackColor(bool dark_mode);
