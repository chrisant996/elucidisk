// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "data.h"
#include "scan.h"
#include <shellapi.h>

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

std::shared_ptr<DirNode> MakeRoot(const WCHAR* _path)
{
    std::wstring path;
    if (!_path)
    {
        WCHAR sz[1024];
        const DWORD dw = GetCurrentDirectory(_countof(sz), sz);

        if (dw > 0 && dw < _countof(sz))
            get_drive(sz, path);
        if (path.empty())
            path = TEXT(".");
    }
    else
    {
        path = _path;
    }

    if (path.empty())
        return nullptr;

    // Everyone knows about "*" and "?" wildcards.  But Windows actually
    // supports FIVE wildcards!
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtldoesnamecontainwildcards
    if (wcspbrk(path.c_str(), TEXT("*?<>\"")))
        return nullptr;

    ensure_separator(path);

    std::shared_ptr<DirNode> root;
    if (is_drive(path.c_str()))
    {
        WCHAR sz[MAX_PATH];
        wcscpy_s(sz, _countof(sz), path.c_str());
        _wcsupr_s(sz, _countof(sz)); // Returns nullptr, contrary to reference docs.
        path = sz;
        root = std::make_shared<DriveNode>(path.c_str());
    }
    else
    {
        root = std::make_shared<DirNode>(path.c_str());
    }

#ifdef DEBUG
    if (!g_fake_data)
#endif
    {
        DriveNode* drive = root->AsDrive();
        if (drive)
            drive->AddFreeSpace();
    }

    return root;
}

#ifdef DEBUG
void AddColorWheelDir(const std::shared_ptr<DirNode> parent, const WCHAR* name, int depth, ScanFeedback& feedback)
{
    depth--;

    if (!depth)
    {
        std::lock_guard<std::recursive_mutex> lock(feedback.mutex);

        parent->AddFile(TEXT("x"), 1000);
    }
    else
    {
        AddColorWheelDir(parent->AddDir(name), name, depth, feedback);
    }

    parent->Finish();
}

static void FakeScan(const std::shared_ptr<DirNode> root, size_t index, bool include_free_space, ScanFeedback& feedback)
{
    switch (g_fake_data)
    {
    case FDM_COLORWHEEL:
        for (int ii = 0; ii < 360; ii += 10)
        {
            WCHAR sz[100];
            swprintf_s(sz, _countof(sz), TEXT("%u to %u"), ii, ii + 10);
            AddColorWheelDir(root, sz, ii ? 10 : 11, feedback);
        }
        break;

    default:
    case FDM_SIMULATED:
        {
            static const ULONGLONG units = 1024;

            std::vector<std::shared_ptr<DirNode>> dirs;

            if (include_free_space)
            {
                DriveNode* drive = root->AsDrive();
                dirs.emplace_back(root->AddDir(TEXT("Abc")));
                dirs.emplace_back(root->AddDir(TEXT("Def")));

                std::lock_guard<std::recursive_mutex> lock(feedback.mutex);

                drive->AddFreeSpace(1000 * units, 2000 * units);
            }
            else if (root->GetParent() && root->GetParent()->GetParent())
            {
                return;
            }
            else
            {
                std::lock_guard<std::recursive_mutex> lock(feedback.mutex);

                root->AddFile(TEXT("Red"), 4000 * units);
                root->AddFile(TEXT("Green"), 8000 * units);
                if (index > 0)
                {
                    std::shared_ptr<DirNode> d = root->AddDir(TEXT("Blue"));
                    d->AddFile(TEXT("Lightning"), 12000 * units);
                    d->Finish();
                }
            }

            for (size_t ii = 0; ii < dirs.size(); ++ii)
                FakeScan(dirs[ii], ii, false, feedback);
        }
        break;

    case FDM_EMPTYDRIVE:
        break;

    case FDM_ONLYDIRS:
        {
            std::lock_guard<std::recursive_mutex> lock(feedback.mutex);

            std::vector<std::shared_ptr<DirNode>> dirs;
            dirs.emplace_back(root->AddDir(TEXT("Abc")));
            dirs.emplace_back(root->AddDir(TEXT("Def")));
            dirs.emplace_back(root->AddDir(TEXT("Ghi")));

            for (auto& dir : dirs)
                dir->Finish();
        }
        break;
    }

    root->Finish();
}
#endif

void Scan(const std::shared_ptr<DirNode>& root, const LONG this_generation, volatile LONG* current_generation, ScanFeedback& feedback)
{
    std::wstring find;
    root->GetFullPath(find);
    ensure_separator(find);

#ifdef DEBUG
    if (g_fake_data)
    {
        const bool was = SetFake(true);
        FakeScan(root, 0, true, feedback);
        SetFake(was);
        return;
    }
#endif

    const bool use_compressed_size = feedback.use_compressed_size;
    const size_t base_path_len = find.length();
    find.append(TEXT("*"));

    std::vector<std::shared_ptr<DirNode>> dirs;

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(find.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        DWORD tick = GetTickCount();
        ULONGLONG num = 0;

        do
        {
            std::lock_guard<std::recursive_mutex> lock(feedback.mutex);

            const bool compressed = (use_compressed_size && (fd.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED));

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!wcscmp(fd.cFileName, TEXT(".")) || !wcscmp(fd.cFileName, TEXT("..")))
                    continue;

                dirs.emplace_back(root->AddDir(fd.cFileName));
                assert(dirs.back());

                if (compressed)
                    dirs.back()->SetCompressed();

                if (++num > 50 || GetTickCount() - tick > 50)
                {
                    feedback.current = dirs.back();
LResetFeedbackInterval:
                    tick = GetTickCount();
                    num = 0;
                }
            }
            else
            {
                ULARGE_INTEGER uli;
                if (compressed)
                {
                    find.resize(base_path_len);
                    find.append(fd.cFileName);
                    uli.LowPart = GetCompressedFileSize(find.c_str(), &uli.HighPart);
                }
                else
                {
                    uli.HighPart = fd.nFileSizeHigh;
                    uli.LowPart = fd.nFileSizeLow;
                }

                std::shared_ptr<FileNode> file = root->AddFile(fd.cFileName, uli.QuadPart);
                assert(file);

                if (compressed)
                    file->SetCompressed();

                if (++num > 50 || GetTickCount() - tick > 50)
                {
                    feedback.current = file;
                    goto LResetFeedbackInterval;
                }
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
        Scan(dir, this_generation, current_generation, feedback);
    }

    DriveNode* drive = root->AsDrive();
    if (drive)
        drive->AddRecycleBin();

    root->Finish();
}

