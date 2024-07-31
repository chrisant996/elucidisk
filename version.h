// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#define VERSION_MAJOR           1
#define VERSION_MINOR           3

#define COPYRIGHT_STR           "Copyright (C) 2023-2024 Christopher Antos"

#define IND_VER2( a, b ) #a ## "." ## #b
#define IND_VER4( a, b, c, d ) L#a ## L"." ## L#b ## L"." ## L#c ## L"." ## L#d
#define DOT_VER2( a, b ) IND_VER2( a, b )
#define DOT_VER4( a, b, c, d ) IND_VER4( a, b, c, d )

#define RC_VERSION                VERSION_MAJOR, VERSION_MINOR, 0, 0
#define RC_VERSION_STR  DOT_VER4( VERSION_MAJOR, VERSION_MINOR, 0, 0 )

#define    VERSION_STR  DOT_VER2( VERSION_MAJOR, VERSION_MINOR )

