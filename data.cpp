// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"

void ensure_separator(std::wstring& path)
{
    if (path.length())
    {
        const WCHAR ch = path.c_str()[path.length() - 1];
        if (ch != '/' && ch != '\\')
            path.append(TEXT("\\"));
    }
}

static void build_full_path(std::wstring& path, const Node* node)
{
    if (!node)
    {
        path.clear();
    }
    else if (node->AsFreeSpace())
    {
        path = node->GetName();
    }
    else
    {
        build_full_path(path, node->GetParent().get());
        ensure_separator(path);
        path.append(node->GetName());
    }
}

void Node::GetFullPath(std::wstring& out) const
{
    build_full_path(out, this);
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
    std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
    std::shared_ptr<FileNode> file = std::make_shared<FileNode>(name, size, parent);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_files.emplace_back(std::move(file));

        m_size += size;
        m_count_files++;

        DirNode* parent(m_parent.get());
        while (parent)
        {
            parent->m_size += size;
            parent->m_count_files++;
            parent = parent->m_parent.get();
        }
    }
}

void DirNode::AddFreeSpace(ULONGLONG free, ULONGLONG total)
{
    std::wstring name;
    name.append(TEXT("Free on "));
    name.append(GetName());

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
        m_free = std::make_shared<FreeSpaceNode>(name.c_str(), free, total, parent);
    }
}

