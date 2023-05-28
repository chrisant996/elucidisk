// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "actions.h"
#include <shellapi.h>
#include <shlobj_core.h>
#include <stdio.h>
#include <stdlib.h>

static BOOL CALLBACK FindTreeViewCallback(HWND hwnd, LPARAM lParam);
static int CALLBACK BFF_Callback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
static bool ShellDeleteInternal(HWND hwnd, const WCHAR* path, bool permanent);

//----------------------------------------------------------------------------
// Shell functions.

void ShellOpen(HWND hwnd, const WCHAR* path)
{
    ShellExecute(hwnd, nullptr, path, nullptr, nullptr, SW_NORMAL);
}

void ShellOpenRecycleBin(HWND hwnd)
{
    ShellExecute(hwnd, nullptr, TEXT("shell:RecycleBinFolder"), nullptr, nullptr, SW_NORMAL);
}

bool ShellRecycle(HWND hwnd, const WCHAR* path)
{
    return ShellDeleteInternal(hwnd, path, false/*permanent*/);
}

bool ShellDelete(HWND hwnd, const WCHAR* path)
{
    return ShellDeleteInternal(hwnd, path, true/*permanent*/);
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

bool ShellBrowseForFolder(HWND hwnd, const WCHAR* title, std::wstring& inout)
{
    ThreadDpiAwarenessContext dpiContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    WCHAR sz[2048];

    // First try the IFileDialog common dialog.  Usage here is based on the
    // Windows v7.1 SDK Sample "winui\shell\appplatform\CommonFileDialogModes".

    do // Not really a loop, just a convenient way to break out to fallback code.
    {
        SPI<IFileDialog> spfd;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spfd))))
            break;

        DWORD dwOptions;
        if (FAILED(spfd->GetOptions(&dwOptions)))
            break;

        spfd->SetOptions(dwOptions|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOREADONLYRETURN|FOS_DONTADDTORECENT);

        SPI<IShellItem> spsiFolder;
        if (inout.length())
            SHCreateItemFromParsingName(inout.c_str(), 0, IID_PPV_ARGS(&spsiFolder));
        else
            SHGetKnownFolderItem(FOLDERID_Documents, KF_FLAG_DEFAULT, 0, IID_PPV_ARGS(&spsiFolder));
        if (spsiFolder)
            spfd->SetFolder(spsiFolder);
        if (title && *title)
            spfd->SetTitle(title);

        HRESULT hr;

        hr = spfd->Show(hwnd);
        if (FAILED(hr))
        {
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
LShellError:
                DWORD const dwFlags = FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
                DWORD cch = FormatMessage(dwFlags, 0, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), sz, _countof(sz), 0);
                if (!cch)
                {
                    if (hr < 65536)
                        swprintf_s(sz, _countof(sz), TEXT("Error %u."), hr);
                    else
                        swprintf_s(sz, _countof(sz), TEXT("Error 0x%08X."), hr);
                }
                MessageBox(hwnd, sz, TEXT("Elucidisk"), MB_OK|MB_ICONERROR);
            }
            return false;
        }

        SPI<IShellItem> spsi;
        hr = spfd->GetResult(&spsi);
        if (FAILED(hr))
            goto LShellError;

        LPWSTR pszName;
        hr = spsi->GetDisplayName(SIGDN_FILESYSPATH, &pszName);
        if (FAILED(hr))
            goto LShellError;

        inout = pszName;
        CoTaskMemFree(pszName);
        return true;
    }
    while (false);

    // Fall back to use the legacy folder picker.
    //
    // Usage here is based on:
    // "https://msdn.microsoft.com/en-us/library/windows/desktop/bb762115(v=vs.85).aspx"

    BROWSEINFO bi = {};
    bi.hwndOwner = hwnd;
    bi.pszDisplayName = sz;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS|BIF_EDITBOX|BIF_VALIDATE|BIF_NEWDIALOGSTYLE|BIF_NONEWFOLDERBUTTON;
    bi.lpfn = BFF_Callback;
    bi.lParam = LPARAM(inout.c_str());

    LPITEMIDLIST pidl;
    pidl = SHBrowseForFolder(&bi);
    if (!pidl)
        return false;

    if (!SHGetPathFromIDList(pidl, sz))
        return false;

    inout = sz;
    return true;
}

bool IsSystemOrSpecial(const WCHAR* path)
{
// TODO: Detect system folders and special folders, to help avoid deleting
// things like \Windows\ or \Users\Joe\ or etc.
    return false;
}

//----------------------------------------------------------------------------
// Helpers.

static BOOL CALLBACK FindTreeViewCallback(HWND hwnd, LPARAM lParam)
{
    WCHAR szClassName[MAX_PATH] = {};
    GetClassName(hwnd, szClassName, _countof(szClassName));
    szClassName[_countof(szClassName) - 1] = 0;

    if (wcsicmp(szClassName, TEXT("SysTreeView32")) == 0)
    {
        HWND* phwnd = (HWND*)lParam;
        if (phwnd)
            *phwnd = hwnd;
        return false;
    }

    return true;
}

static int CALLBACK BFF_Callback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    static bool s_fProcessEnsureVisible = false;

    switch (uMsg)
    {
    case BFFM_INITIALIZED:
        {
#pragma warning(push)
#pragma warning(disable : 4996) // GetVersion is deprecated, but we'll continue to use it.
#pragma warning(disable : 28159) // GetVersion is deprecated, but we'll continue to use it.
            OSVERSIONINFO osvi;
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            s_fProcessEnsureVisible = (GetVersionEx(&osvi) &&
                    (osvi.dwMajorVersion > 6 ||
                     (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 1)));
#pragma warning(pop)
            SendMessage(hwnd, BFFM_SETSELECTION, true, lpData);
        }
        break;

    case BFFM_SELCHANGED:
        if (s_fProcessEnsureVisible)
        {
            s_fProcessEnsureVisible = false;

            HWND hwndTree = 0;
            HTREEITEM hItem = 0;
            EnumChildWindows(hwnd, FindTreeViewCallback, LPARAM(&hwndTree));
            if (hwndTree)
                hItem = TreeView_GetSelection(hwndTree);
            if (hItem)
                TreeView_EnsureVisible(hwndTree, hItem);
        }
        break;
    }

    return 0;
}

static bool ShellDeleteInternal(HWND hwnd, const WCHAR* _path, bool permanent)
{
    const size_t len = wcslen(_path);
    // NOTE: calloc zero-fills, satisfying the double nul termination required
    // by SHFileOperation.
    WCHAR* pathzz = (WCHAR*)calloc(len + 2, sizeof(*pathzz));
    memcpy(pathzz, _path, len * sizeof(*pathzz));

#if 0
    if (!permanent)
    {
        WCHAR message[2048];
        swprintf_s(message, _countof(message), TEXT("Are you sure you want to move \"%s\" to the Recycle Bin?"), _path);
        if (MessageBox(hwnd, message, TEXT("Confirm Recycle"), MB_YESNOCANCEL|MB_ICONQUESTION) != IDYES)
            return false;
    }
#endif

    SHFILEOPSTRUCT op = { 0 };
    op.hwnd = hwnd;
    op.wFunc = FO_DELETE;
    op.pFrom = pathzz;
    op.fFlags = FOF_NO_CONNECTED_ELEMENTS|FOF_SIMPLEPROGRESS|FOF_WANTNUKEWARNING;
    op.lpszProgressTitle = permanent ? TEXT("Deleting") : TEXT("Recycling");

    if (!permanent)
        op.fFlags |= FOF_ALLOWUNDO;

    const int nError = SHFileOperation(&op);
    if (nError)
        return false;

    return true;
}

