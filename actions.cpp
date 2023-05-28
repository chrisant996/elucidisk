// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "actions.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

void ShellOpen(HWND hwnd, const WCHAR* path)
{
    ShellExecute(hwnd, nullptr, path, nullptr, nullptr, SW_NORMAL);
}

void ShellOpenRecycleBin(HWND hwnd)
{
    ShellExecute(hwnd, nullptr, TEXT("shell:RecycleBinFolder"), nullptr, nullptr, SW_NORMAL);
}

bool ShellRecycle(HWND hwnd, const WCHAR* _path)
{
    const size_t len = wcslen(_path);
    WCHAR* pathzz = (WCHAR*)calloc(len + 2, sizeof(*pathzz));
    memcpy(pathzz, _path, len * sizeof(*pathzz));

    WCHAR message[2048];
    swprintf_s(message, _countof(message), TEXT("Are you sure you want to move \"%s\" to the Recycle Bin?"), _path);

    switch (MessageBox(hwnd, message, TEXT("Confirm Recycle"), MB_YESNOCANCEL|MB_ICONQUESTION))
    {
    case IDYES:
        {
            SHFILEOPSTRUCT op = { 0 };
            op.hwnd = hwnd;
            op.wFunc = FO_DELETE;
            op.pFrom = pathzz;
            op.fFlags = FOF_ALLOWUNDO|FOF_NO_CONNECTED_ELEMENTS|FOF_SIMPLEPROGRESS|FOF_WANTNUKEWARNING;
            op.lpszProgressTitle = TEXT("Recycling");

            int const nError = SHFileOperation(&op);
            if (nError == 0)
                return true;
        }
        break;
    }

    return false;
}

bool ShellDelete(HWND hwnd, const WCHAR* path)
{
    // FUTURE: Permanently delete?
    return false;
}

bool ShellEmptyRecycleBin(HWND hwnd, const WCHAR* path)
{
    if (is_drive(path))
    {
        if (SUCCEEDED(SHEmptyRecycleBin(hwnd, path, 0)))
            return true;
    }

    return false;
}

