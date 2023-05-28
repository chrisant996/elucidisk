// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <shlobj_core.h>

class SH_SHFree { protected: void Free(LPITEMIDLIST pidl) { SHFree(pidl); } };
typedef SH<LPITEMIDLIST, NULL, SH_SHFree> SPIDL;

void ShellOpen(HWND hwnd, const WCHAR* path);
void ShellOpenRecycleBin(HWND hwnd);
bool ShellRecycle(HWND hwnd, const WCHAR* path);
bool ShellDelete(HWND hwnd, const WCHAR* path);
bool ShellEmptyRecycleBin(HWND hwnd, const WCHAR* path);
bool ShellBrowseForFolder(HWND hwnd, const WCHAR* title, std::wstring& inout);

bool IsSystemOrSpecial(const WCHAR* path);

