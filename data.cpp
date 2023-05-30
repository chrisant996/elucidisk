// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include <shellapi.h>
#include <assert.h>

#ifdef DEBUG
static thread_local bool s_make_fake = false;

bool SetFake(bool fake)
{
    const bool was = s_make_fake;
    s_make_fake = fake;
    return was;
}
#endif

void ensure_separator(std::wstring& path)
{
    if (path.length())
    {
        const WCHAR ch = path.c_str()[path.length() - 1];
        if (ch != '/' && ch != '\\')
            path.append(TEXT("\\"));
    }
}

void strip_separator(std::wstring& path)
{
    while (path.length() && is_separator(path.c_str()[path.length() - 1]))
        path.resize(path.length() - 1);
}

void skip_separators(const WCHAR*& path)
{
    while (is_separator(*path))
        ++path;
}

void skip_nonseparators(const WCHAR*& path)
{
    while (*path && !is_separator(*path))
        ++path;
}

static void build_full_path(std::wstring& path, const std::shared_ptr<const Node>& node)
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
        const DirNode* dir = node->AsDir();
        if (dir && dir->IsRecycleBin())
        {
            const auto parent = dir->GetParent();
            path = dir->GetName();
            if (parent)
            {
                path.append(TEXT(" on "));
                path.append(parent->GetName());
                strip_separator(path);
            }
        }
        else
        {
            build_full_path(path, node->GetParent());
            path.append(node->GetName());
            if (dir)
                ensure_separator(path);
        }
    }
}

bool is_root_finished(const std::shared_ptr<Node>& node)
{
    DirNode* dir = node->AsDir();
    std::shared_ptr<DirNode> parent;
    for (parent = dir ? std::static_pointer_cast<DirNode>(node) : node->GetParent(); parent; parent = parent->GetParent())
    {
        if (!parent->IsFinished())
            return false;
    }
    return true;
}

bool is_drive(const WCHAR* path)
{
    // FUTURE: Recognize UNC shares and on \\?\X: drives.  But that might not
    // be sufficient to support FreeSpace and RecycleBin for those.
    return (path[0] && path[1] == ':' && (!path[2] ||
                                          (is_separator(path[2]) && !path[3])));
}

bool is_subst(const WCHAR* path)
{
    std::wstring device = path;
    strip_separator(device);

    WCHAR szTargetPath[1024];
    if (QueryDosDevice(device.c_str(), szTargetPath, _countof(szTargetPath)))
        return (wcsnicmp(szTargetPath, TEXT("\\??\\"), 4) == 0);

    return false;
}

unsigned int has_io_prefix(const WCHAR* path)
{
    const WCHAR* p = path;
    if (!is_separator(*(p++)))
        return 0;
    if (!is_separator(*(p++)))
        return 0;
    if (*(p++) != '?')
        return 0;
    if (!is_separator(*(p++)))
        return 0;
    skip_separators(p);
    return static_cast<unsigned int>(p - path);
}

bool is_unc(const WCHAR* path, const WCHAR** past_unc)
{
    if (!is_separator(path[0]) || !is_separator(path[1]))
        return false;

    const WCHAR* const in = path;
    skip_separators(path);
    size_t leading = static_cast<size_t>(path - in);

    // Check for device namespace.
    if (path[0] == '.' && (!path[1] || is_separator(path[1])))
        return false;

    // Check for \\?\UNC\ namespace.
    if (leading == 2 && path[0] == '?' && is_separator(path[1]))
    {
        ++path;
        skip_separators(path);

        if (*path != 'U' && *path != 'u')
            return false;
        ++path;
        if (*path != 'N' && *path != 'n')
            return false;
        ++path;
        if (*path != 'C' && *path != 'c')
            return false;
        ++path;

        if (!is_separator(*path))
            return false;
        skip_separators(path);
    }

    if (past_unc)
    {
        // Skip server name.
        skip_nonseparators(path);
        while (*path && !is_separator(*path))
            ++path;

        // Skip separator.
        skip_separators(path);

        // Skip share name.
        skip_nonseparators(path);

        *past_unc = path;
    }
    return true;
}

bool get_drivelike_prefix(const WCHAR* _path, std::wstring& out)
{
    const WCHAR* path = _path;

    const WCHAR* past_unc;
    if (is_unc(path, &past_unc))
    {
        out.clear();
        out.append(path, past_unc - path);
        return true;
    }

    unsigned int iop = has_io_prefix(path);
    if (iop > 0)
        path += iop;

    if (path[0] && path[1] == ':' && unsigned(towlower(path[0]) - 'a') <= ('z' - 'a'))
    {
        out.clear();
        out.append(_path, iop);
        out.append(path, 2 + is_separator(path[2]));
        return true;
    }

    return false;
}

#ifdef DEBUG
static volatile LONG s_cNodes = 0;
LONG CountNodes() { return s_cNodes; }
#endif

Node::Node(const WCHAR* name, const std::shared_ptr<DirNode>& parent)
: m_name(name)
, m_parent(parent)
#ifdef DEBUG
, m_fake(s_make_fake)
#endif
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
    return parent && parent->IsFinished();
}

void Node::GetFullPath(std::wstring& out) const
{
    std::shared_ptr<const Node> self = shared_from_this();
    build_full_path(out, self);
}

std::vector<std::shared_ptr<DirNode>> DirNode::CopyDirs(bool include_recycle) const
{
    std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

    std::vector<std::shared_ptr<DirNode>> dirs = m_dirs;
    if (include_recycle && GetRecycleBin())
        dirs.emplace_back(GetRecycleBin());
    return dirs;
}

std::vector<std::shared_ptr<FileNode>> DirNode::CopyFiles() const
{
    std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

    return m_files;
}

ULONGLONG DirNode::GetEffectiveSize() const
{
    if (!GetFreeSpace())
        return GetSize();
    else
        return std::max<ULONGLONG>(GetFreeSpace()->GetUsedSize(), GetSize());
}

std::shared_ptr<DirNode> DirNode::AddDir(const WCHAR* name)
{
    std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
    std::shared_ptr<DirNode> dir = std::make_shared<DirNode>(name, parent);

    {
        std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

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
        std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

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

void DirNode::DeleteChild(const std::shared_ptr<Node>& node)
{
    std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

    if (node->AsDir())
    {
        DirNode* dir = node->AsDir();
        if (dir->IsRecycleBin())
        {
            assert(false);
            return;
        }

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

void DirNode::Clear()
{
    std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

#ifdef DEBUG
    if (IsFake())
    {
        assert(false);
        return;
    }
#endif

    std::shared_ptr<DirNode> parent(GetParent());
    while (parent)
    {
        parent->m_size -= GetSize();
        parent->m_count_dirs -= CountDirs();
        parent->m_count_files -= CountFiles();

        std::shared_ptr<DirNode> up = parent->m_parent.lock();
        if (!up)
            parent->m_finished = false;
        parent = up;
    }

    m_dirs.clear();
    m_files.clear();
    m_count_dirs = 0;
    m_count_files = 0;
    m_size = 0;

    if (AsDrive())
        AsDrive()->AddFreeSpace();

    m_finished = false;
}

void DirNode::UpdateRecycleBinMetadata(ULONGLONG size)
{
    assert(!IsFake());

    GetParent()->m_size -= m_size;
    m_size = size;
    GetParent()->m_size += m_size;
}

void RecycleBinNode::UpdateRecycleBin()
{
    assert(!IsFake());

    const WCHAR* drive = GetParent()->GetName();
    ULONGLONG size = 0;

    SHQUERYRBINFO info = { sizeof(info) };
    if (SUCCEEDED(SHQueryRecycleBin(drive, &info)))
        size = info.i64Size;

    UpdateRecycleBinMetadata(size);
}

void DriveNode::AddRecycleBin()
{
    assert(!IsFake());

    // Skip SUBST drives.
    if (is_subst(GetName()))
        return;

    const std::shared_ptr<DirNode> parent = std::static_pointer_cast<DirNode>(shared_from_this());
    {
        std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

        m_recycle = std::make_shared<RecycleBinNode>(parent);
    }

    m_recycle->UpdateRecycleBin();
    m_recycle->Finish();
}

void DriveNode::AddFreeSpace()
{
    assert(!IsFake());

    // Skip SUBST drives.
    if (is_subst(GetName()))
        return;

    DWORD sectors_per_cluster;
    DWORD bytes_per_sector;
    DWORD free_clusters;
    DWORD total_clusters;
    if (GetDiskFreeSpace(GetName(), &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters))
    {
        const ULONGLONG bytes_per_cluster = sectors_per_cluster * bytes_per_sector;
        const ULONGLONG free = ULONGLONG(free_clusters) * bytes_per_cluster;
        const ULONGLONG total = ULONGLONG(total_clusters) * bytes_per_cluster;

        AddFreeSpace(free, total);
    }
}

void DriveNode::AddFreeSpace(ULONGLONG free, ULONGLONG total)
{
    std::wstring name;
    name.append(TEXT("Free on "));
    name.append(GetName());

    const std::shared_ptr<DirNode> parent(std::static_pointer_cast<DirNode>(shared_from_this()));
    {
        std::lock_guard<std::recursive_mutex> lock(m_node_mutex);

        m_free = std::make_shared<FreeSpaceNode>(name.c_str(), free, total, parent);
    }
}

