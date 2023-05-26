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
        path.append(node->GetName());
        if (node->AsDir())
            ensure_separator(path);
    }
}

bool is_root_finished(const std::shared_ptr<Node>& node)
{
    DirNode* dir = node->AsDir();
    for (DirNode* parent = dir ? dir : node->GetParent().get(); parent; parent = parent->GetParent().get())
    {
        if (!parent->Finished())
            return false;
    }
    return true;
}

#ifdef DEBUG
static volatile LONG s_cNodes = 0;
LONG CountNodes() { return s_cNodes; }
#endif

Node::Node(const WCHAR* name, const std::shared_ptr<DirNode>& parent)
: m_name(name)
, m_parent(parent)
{
#ifdef DEBUG
    InterlockedIncrement(&s_cNodes);
#endif
}

Node::~Node()
{
#ifdef DEBUG
    InterlockedDecrement(&s_cNodes);
#endif
}

bool Node::IsParentFinished() const
{
    std::shared_ptr<DirNode> parent = GetParent();
    return parent && parent->Finished();
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

        m_count_dirs++;

        std::shared_ptr<DirNode> parent(m_parent.lock());
        while (parent)
        {
            parent->m_count_dirs++;
            parent = parent->m_parent.lock();
        }
    }

    return dir;
}

std::shared_ptr<FileNode> DirNode::AddFile(const WCHAR* name, ULONGLONG size)
{
    std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
    std::shared_ptr<FileNode> file = std::make_shared<FileNode>(name, size, parent);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_files.emplace_back(file);

        m_size += size;
        m_count_files++;

        std::shared_ptr<DirNode> parent(m_parent.lock());
        while (parent)
        {
            parent->m_size += size;
            parent->m_count_files++;
            parent = parent->m_parent.lock();
        }
    }

    return file;
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

void DirNode::DeleteChild(const std::shared_ptr<Node>& node)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (node->AsDir())
    {
        DirNode* dir = node->AsDir();
        for (auto iter = m_dirs.begin(); iter != m_dirs.end(); ++iter)
        {
            if (iter->get() == dir)
            {
                std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
                while (parent)
                {
                    parent->m_size -= dir->GetSize();
                    parent->m_count_dirs -= dir->CountDirs();
                    parent->m_count_files -= dir->CountFiles();
                    parent = parent->m_parent.lock();
                }

                m_dirs.erase(iter);
                return;
            }
        }
    }
    else
    {
        FileNode* file = node->AsFile();
        for (auto iter = m_files.begin(); iter != m_files.end(); ++iter)
        {
            if (iter->get() == file)
            {
                std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
                while (parent)
                {
                    parent->m_size -= file->GetSize();
                    parent->m_count_files--;
                    parent = parent->m_parent.lock();
                }

                m_files.erase(iter);
                return;
            }
        }
    }
}

