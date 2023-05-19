// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"

ULONGLONG DirNode::CountDirs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_dirs.size();
}

ULONGLONG DirNode::CountFiles() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_files.size();
}

std::vector<std::shared_ptr<DirNode>> DirNode::CopyDirs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_dirs;
}

std::vector<std::shared_ptr<FileNode>> DirNode::CopyFiles() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_files;
}

std::shared_ptr<DirNode> DirNode::AddDir(const WCHAR* name)
{
    std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
    std::shared_ptr<DirNode> dir = std::make_shared<DirNode>(name, parent);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_dirs.emplace_back(dir);
    }

    return dir;
}

void DirNode::AddFile(const WCHAR* name, ULONGLONG size)
{
    std::shared_ptr<FileNode> file = std::make_shared<FileNode>(name, size);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_files.emplace_back(std::move(file));

        m_size += size;

        DirNode* parent(m_parent.get());
        while (parent)
        {
            parent->m_size += size;
            parent = parent->m_parent.get();
        }
    }
}

