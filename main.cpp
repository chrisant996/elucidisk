// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "scan.h"
#include "ui.h"
#include "sunburst.h"
#include <stdlib.h>
#include <shellapi.h>

static const WCHAR c_reg_root[] = TEXT("Software\\Elucidisk");

bool g_use_compressed_size = false;
bool g_show_free_space = true;
bool g_show_names = true;
bool g_show_comparison_bar = true;
bool g_show_proportional_area = true;
long g_color_mode = CM_RAINBOW;
#ifdef DEBUG
long g_fake_data = FDM_REAL;
#endif

int PASCAL WinMain(
    _In_ HINSTANCE hinstCurrent,
    _In_opt_ HINSTANCE /*hinstPrevious*/,
    _In_ LPSTR /*lpszCmdLine*/,
    _In_ int /*nCmdShow*/)
{
    MSG msg = { 0 };

    int argc;
    const WCHAR **argv = const_cast<const WCHAR**>(CommandLineToArgvW(GetCommandLine(), &argc));

    if (argc)
    {
        argc--;
        argv++;
    }

    // Options (there are no options yet).

    // FUTURE: An option to generate the .ico file programmatically using D2D.

    // Create UI.

    INITCOMMONCONTROLSEX icce = { sizeof(icce), ICC_LISTVIEW_CLASSES };

    CoInitialize(0);

    InitCommonControls();
    InitCommonControlsEx(&icce);
    InitializeD2D();
    InitializeDWrite();

    g_use_compressed_size = !!ReadRegLong(TEXT("UseCompressedSize"), false);
    g_show_free_space = !!ReadRegLong(TEXT("ShowFreeSpace"), true);
    g_show_names = !!ReadRegLong(TEXT("ShowNames"), true);
    g_show_comparison_bar = !!ReadRegLong(TEXT("ShowComparisonBar"), true);
    g_show_proportional_area = !!ReadRegLong(TEXT("ShowProportionalArea"), true);
    g_color_mode = ReadRegLong(TEXT("ColorMode"), CM_RAINBOW);
#ifdef DEBUG
    g_fake_data = ReadRegLong(TEXT("DbgFakeData"), FDM_REAL);
#endif

    const HWND hwnd = MakeUi(hinstCurrent, argc, argv);

    // Main message loop.

    if (hwnd)
    {
        while (true)
        {
            if (!GetMessage(&msg, nullptr, 0, 0))
            break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup.

    {
        MSG tmp;
        do {} while(PeekMessage(&tmp, 0, WM_QUIT, WM_QUIT, PM_REMOVE));
    }

    return int(msg.wParam);
}

LONG ReadRegLong(const WCHAR* name, LONG default_value)
{
    HKEY hkey;
    LONG ret = default_value;

    if (ERROR_SUCCESS == RegOpenKey(HKEY_CURRENT_USER, c_reg_root, &hkey))
    {
        DWORD type;
        LONG value;
        DWORD cb = sizeof(value);
        if (ERROR_SUCCESS == RegQueryValueEx(hkey, name, 0, &type, reinterpret_cast<BYTE*>(&value), &cb) &&
            type == REG_DWORD &&
            cb == sizeof(value))
        {
            ret = value;
        }
        RegCloseKey(hkey);
    }

    return ret;
}

void WriteRegLong(const WCHAR* name, LONG value)
{
    HKEY hkey;

    if (ERROR_SUCCESS == RegCreateKey(HKEY_CURRENT_USER, c_reg_root, &hkey))
    {
        RegSetValueEx(hkey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(hkey);
    }
}

bool ReadRegStrings(const WCHAR* name, std::vector<std::wstring>& out)
{
    bool ok = false;
    DWORD cb = 0;
    out.clear();
    if (ERROR_SUCCESS == RegGetValue(HKEY_CURRENT_USER, c_reg_root, name, RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &cb))
    {
        WCHAR* data = (WCHAR*)calloc(cb, 1);
        if (ERROR_SUCCESS == RegGetValue(HKEY_CURRENT_USER, c_reg_root, name, RRF_RT_REG_MULTI_SZ, nullptr, data, &cb))
        {
            for (const WCHAR* s = data; *s;)
            {
                const size_t len = wcslen(s);
                out.emplace_back(s);
                s += len + 1;
                if (!*s)
                    break;
            }
            ok = true;
        }
        free(data);
    }
    return ok;
}

void WriteRegStrings(const WCHAR* name, const std::vector<std::wstring>& in)
{
    DWORD cb = sizeof(WCHAR);
    for (const auto& s : in)
        cb += DWORD((s.length() + 1) * sizeof(WCHAR));

    WCHAR* data = (WCHAR*)calloc(cb, 1);
    WCHAR* to = data;
    for (const auto& s : in)
    {
        memcpy(to, s.c_str(), (s.length() + 1) * sizeof(*s.c_str()));
        to += s.length() + 1;
    }

    HKEY hkey;
    if (ERROR_SUCCESS == RegCreateKey(HKEY_CURRENT_USER, c_reg_root, &hkey))
    {
        RegSetValueEx(hkey, name, 0, REG_MULTI_SZ, reinterpret_cast<const BYTE*>(data), cb);
        RegCloseKey(hkey);
    }

    free(data);
}

void MakeMenuPretty(HMENU hmenu)
{
    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(mii);
#ifdef MIIM_FTYPE
    mii.fMask = MIIM_FTYPE|MIIM_SUBMENU;
#else
    mii.fMask = MIIM_TYPE|MIIM_SUBMENU;
#endif

    bool fPrevSep = true;
    for (int ii = 0; true; ii++)
    {
        const bool fEnd = !GetMenuItemInfo(hmenu, ii, true, &mii);

        if (fEnd || (mii.fType & MFT_SEPARATOR))
        {
            if (fPrevSep)
            {
                DeleteMenu(hmenu, ii - fEnd, MF_BYPOSITION);
                ii--;
            }

            if (fEnd)
                break;

            fPrevSep = true;
        }
        else
        {
            fPrevSep = false;
        }

        if (mii.hSubMenu)
            MakeMenuPretty(mii.hSubMenu);
    }
}

UnitScale AutoUnitScale(ULONGLONG size)
{
    size /= ULONGLONG(10) * 1024 * 1024;
    if (!size)
        return UnitScale::KB;

    size /= 1024;
    if (!size)
        return UnitScale::MB;

    return UnitScale::GB;
}

void FormatSize(const ULONGLONG _size, std::wstring& text, std::wstring& units, UnitScale scale, int places)
{
    WCHAR sz[100];
    double size = double(_size);

    if (scale == UnitScale::Auto)
        scale = AutoUnitScale(_size);

    switch (scale)
    {
    case UnitScale::KB:
        units = TEXT("KB");
        size /= 1024;
        break;
    case UnitScale::MB:
        units = TEXT("MB");
        size /= 1024;
        size /= 1024;
        break;
    default:
        units = TEXT("GB");
        size /= 1024;
        size /= 1024;
        size /= 1024;
        break;
    }

    if (places < 0)
    {
        if (size >= 100.0f)
            places = 0;
        else if (size >= 10.0f)
            places = 1;
        else if (size >= 1.0f)
            places = 2;
        else
            places = 3;
    }

    swprintf_s(sz, _countof(sz), TEXT("%.*f"), places, size);
    text = sz;
}

void FormatCount(const ULONGLONG count, std::wstring& text)
{
    WCHAR sz[100];
    swprintf_s(sz, _countof(sz), TEXT("%llu"), count);

    WCHAR* commas = sz + _countof(sz);
    *(--commas) = '\0';

    size_t ii = wcslen(sz);
    for (int count = 0; ii--; ++count)
    {
        if (count == 3)
        {
            count = 0;
            *(--commas) = ',';
        }
        *(--commas) = sz[ii];
    }

    text = commas;
}

