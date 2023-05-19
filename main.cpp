// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "scan.h"
#include "ui.h"
#include "sunburst.h"
#include <stdlib.h>
#include <shellapi.h>

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

