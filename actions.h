// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

void ShellOpen(HWND hwnd, const WCHAR* path);
bool ShellRecycle(HWND hwnd, const WCHAR* path);
bool ShellDelete(HWND hwnd, const WCHAR* path);

