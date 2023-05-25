// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "dpi.h"

#define USE_RAINBOW

template<class T>
void ReleaseI(T*& p)
{
    if (p)
        p->Release();
    p = nullptr;
}

LONG ReadRegLong(const WCHAR* name, LONG default_value);
void WriteRegLong(const WCHAR* name, LONG value);

extern bool g_use_compressed_size;
extern bool g_show_free_space;

