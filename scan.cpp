// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"

static void ensure_separator(std::wstring& path)
{
    if (path.length())
    {
        const WCHAR ch = path.c_str()[path.length() - 1];
        if (ch != '/' && ch != '\\')
            path.append(TEXT("\\"));
    }
}

static void build_full_path(std::wstring& path, const DirNode* dir)
{
    if (dir)
    {
        build_full_path(path, dir->GetParent().get());
        ensure_separator(path);
        path.append(dir->GetName());
    }
    else
    {
        path.clear();
    }
}

inline bool is_separator(const WCHAR ch)
{
    return ch == '/' || ch == '\\';
}

static void skip_separators(const WCHAR*& path)
{
    while (is_separator(*path))
        ++path;
}

static unsigned int has_io_prefix(const WCHAR* path)
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

static void get_drive(const WCHAR* path, std::wstring& out)
{
    out.clear();

    path += has_io_prefix(path);

    if (path[0] && path[1] == ':' && unsigned(towlower(path[0]) - 'a') <= ('z' - 'a'))
    {
        WCHAR tmp[3] = { path[0], ':' };
        out = tmp;
    }
}

static std::shared_ptr<DirNode> Scan(const std::shared_ptr<DirNode> root)
{
    std::wstring find;
    build_full_path(find, root.get());

    ensure_separator(find);
    find.append(TEXT("*"));

    std::vector<std::shared_ptr<DirNode>> dirs;

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(find.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!wcscmp(fd.cFileName, TEXT(".")) || !wcscmp(fd.cFileName, TEXT("..")))
                    continue;

                dirs.emplace_back(root->AddDir(fd.cFileName));
            }
            else
            {
                ULARGE_INTEGER uli;
                uli.HighPart = fd.nFileSizeHigh;
                uli.LowPart = fd.nFileSizeLow;
                root->AddFile(fd.cFileName, uli.QuadPart);
            }
        }
        while (FindNextFile(hFind, &fd));
    }

    for (const auto dir : dirs)
        Scan(dir);

    return root;
}

std::shared_ptr<DirNode> Scan(const WCHAR* path)
{
    std::wstring tmp;
    if (!path)
    {
        WCHAR sz[1024];
        const DWORD dw = GetCurrentDirectory(_countof(sz), sz);

        if (dw > 0 && dw < _countof(sz))
            get_drive(sz, tmp);
        if (tmp.empty())
            tmp = TEXT(".");

        path = tmp.c_str();
    }

    std::shared_ptr<DirNode> root = std::make_shared<DirNode>(path);

    return Scan(root);
}

