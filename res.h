// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

//----------------------------------------------------------------------------
// Icons.

#define IDI_MAIN                1

//----------------------------------------------------------------------------
// Context Menus.

#define IDR_CONTEXT_MENU        1000

//----------------------------------------------------------------------------
// Dialogs.

#define IDD_CONFIG_DONTSCAN     1000

//----------------------------------------------------------------------------
// Menu Commands.

#define IDM_OPEN_DIRECTORY      2000
#define IDM_OPEN_FILE           2001
#define IDM_RECYCLE_ENTRY       2002
#define IDM_DELETE_ENTRY        2003
#define IDM_HIDE_DIRECTORY      2004
#define IDM_SHOW_DIRECTORY      2005
#define IDM_EMPTY_RECYCLEBIN    2006
#define IDM_RESCAN              2007

#define IDM_OPTION_COMPRESSED   2100
#define IDM_OPTION_FREESPACE    2101
#define IDM_OPTION_NAMES        2102
#define IDM_OPTION_COMPBAR      2103
#define IDM_OPTION_PROPORTION   2104
#define IDM_OPTION_DONTSCAN     2105

#define IDM_OPTION_PLAIN        2170
#define IDM_OPTION_RAINBOW      2171
#define IDM_OPTION_HEATMAP      2172

#ifdef DEBUG
#define IDM_OPTION_REALDATA     2180
#define IDM_OPTION_SIMULATED    2181
#define IDM_OPTION_COLORWHEEL   2182
#define IDM_OPTION_EMPTYDRIVE   2183
#define IDM_OPTION_ONLYDIRS     2184
#endif

#define IDM_REFRESH             2200
#define IDM_BACK                2201
#define IDM_UP                  2202
#define IDM_SUMMARY             2203
#define IDM_FOLDER              2204
#define IDM_APPWIZ              2205

#define IDM_DRIVE_FIRST         2300
#define IDM_DRIVE_LAST          2349

//----------------------------------------------------------------------------
// Generic Controls.

#define IDC_STATIC1             501
#define IDC_STATIC2             502
#define IDC_STATIC3             503
#define IDC_STATIC4             504
#define IDC_STATIC5             505

#define IDC_BUTTON1             521
#define IDC_BUTTON2             522
#define IDC_BUTTON3             523
#define IDC_BUTTON4             524
#define IDC_BUTTON5             525

#define IDC_LIST1               541
#define IDC_LIST2               542
#define IDC_LIST3               543
#define IDC_LIST4               544
#define IDC_LIST5               545

//----------------------------------------------------------------------------
// Dialog Controls.

#define IDC_DONTSCAN_ADD        IDC_BUTTON1
#define IDC_DONTSCAN_REMOVE     IDC_BUTTON2
#define IDC_DONTSCAN_LIST       IDC_LIST1

