// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"

//#define USE_FAKE_DATA

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

std::shared_ptr<DirNode> MakeRoot(const WCHAR* path)
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

    return std::make_shared<DirNode>(path);
}

#ifdef USE_FAKE_DATA
static void FakeScan(const std::shared_ptr<DirNode> root, size_t index, bool include_free_space)
{
    std::vector<std::shared_ptr<DirNode>> dirs;

    if (include_free_space)
    {
        dirs.emplace_back(root->AddDir(TEXT("Abc")));
        dirs.emplace_back(root->AddDir(TEXT("Def")));
        root->AddFreeSpace(1000, 2000);
    }
    else if (root->GetParent() && root->GetParent()->GetParent())
    {
        return;
    }
    else
    {
        root->AddFile(TEXT("Red"), 4000);
        root->AddFile(TEXT("Green"), 8000);
        if (index > 0)
            root->AddFile(TEXT("Blue"), 12000);
    }

    for (size_t ii = 0; ii < dirs.size(); ++ii)
        FakeScan(dirs[ii], ii, false);
}
#endif

void Scan(const std::shared_ptr<DirNode> root, const LONG this_generation, volatile LONG* current_generation, std::recursive_mutex& mutex)
{
    std::wstring find;
    root->GetFullPath(find);
    ensure_separator(find);

// TODO: Support free space on UNC shares.
    const bool include_free_space = (find.length() == 3 && find.c_str()[1] == ':' && is_separator(find.c_str()[2]));

#ifdef USE_FAKE_DATA
    if (GetTickCount() != 0)
    {
        FakeScan(root, 0, include_free_space);
        return;
    }
#endif

    if (include_free_space)
    {
        DWORD sectors_per_cluster;
        DWORD bytes_per_sector;
        DWORD free_clusters;
        DWORD total_clusters;
        if (GetDiskFreeSpace(find.c_str(), &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters))
        {
            const ULONGLONG bytes_per_cluster = sectors_per_cluster * bytes_per_sector;
            root->AddFreeSpace(ULONGLONG(free_clusters) * bytes_per_cluster, ULONGLONG(total_clusters) * bytes_per_cluster);
        }
    }

    find.append(TEXT("*"));

    std::vector<std::shared_ptr<DirNode>> dirs;

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(find.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);

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
        while (this_generation == *current_generation && FindNextFile(hFind, &fd));

        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }

    for (const auto dir : dirs)
    {
        if (this_generation != *current_generation)
            break;
        Scan(dir, this_generation, current_generation, mutex);
    }
}

