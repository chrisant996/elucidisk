// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <windows.h>

LONG RegisterShellIntegration();
LONG UnregisterShellIntegration();
int IsShellIntegrationRegistered(); // 1=registered, 0=not, -1=error
