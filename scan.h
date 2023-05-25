// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include <memory>

class DirNode;

struct ScanFeedback
{
    std::recursive_mutex& mutex;
    std::shared_ptr<Node>& current;
    bool use_compressed_size = false;
};

std::shared_ptr<DirNode> MakeRoot(const WCHAR* path);
void Scan(const std::shared_ptr<DirNode>& root, LONG this_generation, volatile LONG* current_generation, ScanFeedback& feedback);

