// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <memory>

class DirNode;

std::shared_ptr<DirNode> Scan(const WCHAR* path);

