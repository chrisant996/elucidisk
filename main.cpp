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

    // Create UI.

    InitCommonControls();
    InitializeD2D();

    g_use_compressed_size = !!ReadRegLong(TEXT("UseCompressedSize"), false);
    g_show_free_space = !!ReadRegLong(TEXT("ShowFreeSpace"), true);

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
        DWORD cb;
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

