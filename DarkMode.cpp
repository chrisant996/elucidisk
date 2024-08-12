// Derived and adapted from https://github.com/ysc3839/win32-darkmode (MIT License).

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uxtheme.h>
#include "DarkMode.h"

typedef WORD uint16_t;
#include "IatHook.h"

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
// From libloaderapi.h -- this flag was introduced in Win7.
#define LOAD_LIBRARY_SEARCH_SYSTEM32        0x00000800
#endif

enum IMMERSIVE_HC_CACHE_MODE
{
    IHCM_USE_CACHED_VALUE,
    IHCM_REFRESH
};

enum WINDOWCOMPOSITIONATTRIB
{
    WCA_UNDEFINED = 0,
    WCA_NCRENDERING_ENABLED = 1,
    WCA_NCRENDERING_POLICY = 2,
    WCA_TRANSITIONS_FORCEDISABLED = 3,
    WCA_ALLOW_NCPAINT = 4,
    WCA_CAPTION_BUTTON_BOUNDS = 5,
    WCA_NONCLIENT_RTL_LAYOUT = 6,
    WCA_FORCE_ICONIC_REPRESENTATION = 7,
    WCA_EXTENDED_FRAME_BOUNDS = 8,
    WCA_HAS_ICONIC_BITMAP = 9,
    WCA_THEME_ATTRIBUTES = 10,
    WCA_NCRENDERING_EXILED = 11,
    WCA_NCADORNMENTINFO = 12,
    WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
    WCA_VIDEO_OVERLAY_ACTIVE = 14,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
    WCA_DISALLOW_PEEK = 16,
    WCA_CLOAK = 17,
    WCA_CLOAKED = 18,
    WCA_ACCENT_POLICY = 19,
    WCA_FREEZE_REPRESENTATION = 20,
    WCA_EVER_UNCLOAKED = 21,
    WCA_VISUAL_OWNER = 22,
    WCA_HOLOGRAPHIC = 23,
    WCA_EXCLUDED_FROM_DDA = 24,
    WCA_PASSIVEUPDATEMODE = 25,
    WCA_USEDARKMODECOLORS = 26,
    WCA_LAST = 27
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using fnRtlGetNtVersionNumbers = void (WINAPI *)(LPDWORD major, LPDWORD minor, LPDWORD build);
using fnSetWindowCompositionAttribute = BOOL (WINAPI *)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*);
// 1809 17763
using fnShouldAppsUseDarkMode = bool (WINAPI *)(); // ordinal 132
using fnAllowDarkModeForWindow = bool (WINAPI *)(HWND hWnd, bool allow); // ordinal 133
using fnAllowDarkModeForApp = bool (WINAPI *)(bool allow); // ordinal 135, in 1809
using fnFlushMenuThemes = void (WINAPI *)(); // ordinal 136
using fnRefreshImmersiveColorPolicyState = void (WINAPI *)(); // ordinal 104
using fnIsDarkModeAllowedForWindow = bool (WINAPI *)(HWND hWnd); // ordinal 137
using fnGetIsImmersiveColorUsingHighContrast = bool (WINAPI *)(IMMERSIVE_HC_CACHE_MODE mode); // ordinal 106
using fnOpenNcThemeData = HTHEME(WINAPI *)(HWND hWnd, LPCWSTR pszClassList); // ordinal 49
// 1903 18362
using fnShouldSystemUseDarkMode = bool (WINAPI *)(); // ordinal 138
using fnSetPreferredAppMode = PreferredAppMode (WINAPI *)(PreferredAppMode appMode); // ordinal 135, in 1903

static fnSetWindowCompositionAttribute _SetWindowCompositionAttribute = nullptr;
static fnShouldAppsUseDarkMode _ShouldAppsUseDarkMode = nullptr;
static fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
static fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
static fnFlushMenuThemes _FlushMenuThemes = nullptr;
static fnRefreshImmersiveColorPolicyState _RefreshImmersiveColorPolicyState = nullptr;
static fnIsDarkModeAllowedForWindow _IsDarkModeAllowedForWindow = nullptr;
static fnGetIsImmersiveColorUsingHighContrast _GetIsImmersiveColorUsingHighContrast = nullptr;
static fnOpenNcThemeData _OpenNcThemeData = nullptr;
// 1903 18362
static fnShouldSystemUseDarkMode _ShouldSystemUseDarkMode = nullptr;
static fnSetPreferredAppMode _SetPreferredAppMode = nullptr;

static bool s_darkModeSupported = false;
static DWORD s_buildNumber = 0;

bool IsHighContrast()
{
    HIGHCONTRASTW highContrast = { sizeof(highContrast) };
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, FALSE))
        return !!(highContrast.dwFlags & HCF_HIGHCONTRASTON);
    return false;
}

static void RefreshTitleBarThemeColor(HWND hWnd)
{
    if (!s_darkModeSupported)
        return;

    BOOL dark = FALSE;
    if (_IsDarkModeAllowedForWindow(hWnd) &&
        _ShouldAppsUseDarkMode() &&
        !IsHighContrast())
    {
        dark = TRUE;
    }
    if (s_buildNumber < 18362)
        SetPropW(hWnd, L"UseImmersiveDarkModeColors", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(dark)));
    else if (_SetWindowCompositionAttribute)
    {
        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
        _SetWindowCompositionAttribute(hWnd, &data);
    }
}

bool IsColorSchemeChangeMessage(LPARAM lParam)
{
    bool is = false;
    if (lParam && _stricmp(reinterpret_cast<LPCSTR>(lParam), "ImmersiveColorSet") == 0)
    {
        if (_RefreshImmersiveColorPolicyState)
            _RefreshImmersiveColorPolicyState();
        is = true;
    }
    if (_GetIsImmersiveColorUsingHighContrast)
        _GetIsImmersiveColorUsingHighContrast(IHCM_REFRESH);
    return is;
}

bool IsColorSchemeChangeMessage(UINT message, LPARAM lParam)
{
    if (message == WM_SETTINGCHANGE)
        return IsColorSchemeChangeMessage(lParam);
    return false;
}

static void FixDarkScrollBar()
{
    HMODULE hComctl = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hComctl)
    {
        auto addr = FindDelayLoadThunkInModule(hComctl, "uxtheme.dll", 49); // OpenNcThemeData
        if (addr)
        {
            DWORD oldProtect;
            if (VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), PAGE_READWRITE, &oldProtect))
            {
                auto MyOpenThemeData = [](HWND hWnd, LPCWSTR classList) -> HTHEME {
                    if (wcscmp(classList, L"ScrollBar") == 0)
                    {
                        hWnd = nullptr;
                        classList = L"Explorer::ScrollBar";
                    }
                    return _OpenNcThemeData(hWnd, classList);
                };

                addr->u1.Function = reinterpret_cast<ULONG_PTR>(static_cast<fnOpenNcThemeData>(MyOpenThemeData));
                VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), oldProtect, &oldProtect);
            }
        }
    }
}

constexpr bool CheckBuildNumber(DWORD buildNumber)
{
#if 0
    return (buildNumber == 17763 || // 1809
        buildNumber == 18362 || // 1903
        buildNumber == 18363); // 1909
#else
    return (buildNumber >= 18362);
#endif
}

static bool ShouldAppsUseDarkMode()
{
    return _ShouldAppsUseDarkMode && _ShouldAppsUseDarkMode();
}

bool AllowDarkMode()
{
    auto RtlGetNtVersionNumbers = reinterpret_cast<fnRtlGetNtVersionNumbers>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers"));
    if (!RtlGetNtVersionNumbers)
        return false;

    DWORD major, minor;
    RtlGetNtVersionNumbers(&major, &minor, &s_buildNumber);
    s_buildNumber &= ~0xF0000000;
    if (major < 10)
        return false;
    if (major == 10 && !CheckBuildNumber(s_buildNumber))
        return false;

    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!hUxtheme)
        return false;

    _OpenNcThemeData = reinterpret_cast<fnOpenNcThemeData>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(49)));
    _RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));
    _GetIsImmersiveColorUsingHighContrast = reinterpret_cast<fnGetIsImmersiveColorUsingHighContrast>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(106)));
    _ShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));
    _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));

    auto ord135 = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
    if (!ord135)
        return false;

    if (s_buildNumber < 18362)
        _AllowDarkModeForApp = reinterpret_cast<fnAllowDarkModeForApp>(ord135);
    else
        _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(ord135);

    _IsDarkModeAllowedForWindow = reinterpret_cast<fnIsDarkModeAllowedForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(137)));

    _SetWindowCompositionAttribute = reinterpret_cast<fnSetWindowCompositionAttribute>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));

    if (_OpenNcThemeData &&
        _RefreshImmersiveColorPolicyState &&
        _ShouldAppsUseDarkMode &&
        _AllowDarkModeForWindow &&
        (_AllowDarkModeForApp || _SetPreferredAppMode) &&
        _IsDarkModeAllowedForWindow)
    {
        s_darkModeSupported = true;

        if (_AllowDarkModeForApp)
            _AllowDarkModeForApp(true);
        else if (_SetPreferredAppMode)
            _SetPreferredAppMode(PreferredAppMode::AllowDark);

        _RefreshImmersiveColorPolicyState();

        FixDarkScrollBar();
    }

    return s_darkModeSupported;
}

bool IsDarkModeSupported()
{
    return s_darkModeSupported;
}

bool ShouldUseDarkMode()
{
    if (!s_darkModeSupported)
        return false;

    return _ShouldAppsUseDarkMode() && !IsHighContrast();
}

bool DarkModeOnThemeChanged(HWND hWnd, DarkModeMode dmm)
{
    if (!s_darkModeSupported)
        return false;

    const bool fUseDark = ((dmm == DarkModeMode::Light) ? false :
                           (dmm == DarkModeMode::Dark) ? true :
                           ShouldUseDarkMode());

    // HACK:  In Elucidisk, the first call returns false, but a second call
    // returns true.  In another app of mine, the first call returns true.  I
    // haven't yet been able to figure out what's different between them.
    //
    // BUT!  The return value doesn't seem to be related to success; even when
    // fUseDark is true and the function returns false, it still successfully
    // applies dark mode.
    //const bool fDark =
        //_AllowDarkModeForWindow(hWnd, fUseDark) ||
        _AllowDarkModeForWindow(hWnd, fUseDark);

    RefreshTitleBarThemeColor(hWnd);
    return fUseDark;
}

UINT32 GetForeColor(bool dark_mode)
{
    return dark_mode ? 0xc0c0c0 : 0x000000;
}

UINT32 GetBackColor(bool dark_mode)
{
    return dark_mode ? 0x111111 : 0xffffff;
}
