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
        WCHAR tmp[3] = { towupper(path[0]), ':' };
        out = tmp;
    }
}

static void capitalize_drive_part(std::wstring& inout)
{
    std::wstring out;
    const WCHAR* path = inout.c_str();

    unsigned int pfxlen = has_io_prefix(path);
    out.append(path, pfxlen);
    path += pfxlen;

    std::wstring drive;
    get_drive(path, drive);
    out.append(drive.c_str());
    path += drive.length();

    out.append(path);

    inout = std::move(out);
}

std::shared_ptr<DirNode> MakeRoot(const WCHAR* _path)
{
    std::wstring path;
    if (!_path)
    {
        const DWORD needed = GetCurrentDirectory(0, nullptr);
        if (needed)
        {
            WCHAR* buffer = new WCHAR[needed];
            if (buffer)
            {
                const DWORD used = GetCurrentDirectory(needed, buffer);
                if (used > 0 && used < needed)
                    get_drive(buffer, path);
                delete [] buffer;
            }
        }
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
    //
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtldoesnamecontainwildcards
    //
    // The ntifs.h header file shows the definitions of DOS_DOT, DOS_QM, and
    // DOS_STAR.
    //
    // Here is the full table of wildcards:
    //  asterisk        *   Matches zero or more characters.
    //  question mark   ?   Matches a single character.
    //  DOS_DOT         <   Matches either a period or zero characters beyond
    //                      the name string.
    //  DOS_QM          >   Matches any single character or, upon encountering
    //                      a period or end of name string, advances the
    //                      expression to the end of the set of contiguous
    //                      DOS_QMs ('>' characters).
    //  DOS_STAR        "   Matches zero or more characters until encountering
    //                      and matching the final . in the name.
    if (wcspbrk(path.c_str(), TEXT("*?<>\"")))
        return nullptr;

    ensure_separator(path);

    const DWORD needed = GetFullPathName(path.c_str(), 0, nullptr, nullptr);
    if (needed)
    {
        WCHAR* buffer = new WCHAR[needed];
        if (buffer)
        {
            const DWORD used = GetFullPathName(path.c_str(), needed, buffer, nullptr);
            if (used > 0 && used < needed)
            {
                path = buffer;
                ensure_separator(path);
            }
            delete [] buffer;
        }
    }

    capitalize_drive_part(path);

    std::shared_ptr<DirNode> root;
    if (is_drive(path.c_str()))
        root = std::make_shared<DriveNode>(path.c_str());
    else
        root = std::make_shared<DirNode>(path.c_str());

    return root;
}

#ifdef DEBUG
void AddColorWheelDir(const std::shared_ptr<DirNode> parent, const WCHAR* name, int depth, ScanContext& context)
{
    depth--;

    if (!depth)
    {
        std::lock_guard<std::recursive_mutex> lock(context.mutex);

        parent->AddFile(TEXT("x"), 1024);
    }
    else
    {
        AddColorWheelDir(parent->AddDir(name), name, depth, context);
    }

    parent->Finish();
}

static void FakeScan(const std::shared_ptr<DirNode> root, size_t index, bool include_free_space, ScanContext& context)
{
    switch (g_fake_data)
    {
    case FDM_COLORWHEEL:
        for (int ii = 0; ii < 360; ii += 10)
        {
            WCHAR sz[100];
            swprintf_s(sz, _countof(sz), TEXT("%u to %u"), ii, ii + 10);
            AddColorWheelDir(root, sz, ii ? 10 : 11, context);
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

                std::lock_guard<std::recursive_mutex> lock(context.mutex);

                if (drive)
                    drive->AddFreeSpace(1000 * units, 2000 * units);
            }
            else if (root->GetParent() && root->GetParent()->GetParent())
            {
                return;
            }
            else
            {
                std::lock_guard<std::recursive_mutex> lock(context.mutex);

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
                FakeScan(dirs[ii], ii, false, context);
        }
        break;

    case FDM_EMPTYDRIVE:
        break;

    case FDM_ONLYDIRS:
        {
            std::lock_guard<std::recursive_mutex> lock(context.mutex);

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

void Scan(const std::shared_ptr<DirNode>& root, const LONG this_generation, volatile LONG* current_generation, ScanContext& context)
{
    if (root->AsRecycleBin())
    {
        std::lock_guard<std::recursive_mutex> lock(context.mutex);

        context.current = root;
        root->AsRecycleBin()->UpdateRecycleBin(context.mutex);
        root->Finish();
        return;
    }

    DriveNode* drive = (root->AsDrive() && !is_subst(root->GetName())) ? root->AsDrive() : nullptr;

    std::wstring find;
    root->GetFullPath(find);
    ensure_separator(find);

#ifdef DEBUG
    if (g_fake_data)
    {
        const bool was = SetFake(true);
        FakeScan(root, 0, true, context);
        SetFake(was);
        return;
    }
#endif

    const bool use_compressed_size = context.use_compressed_size;
    const size_t base_path_len = find.length();
    find.append(TEXT("*"));

    std::vector<std::shared_ptr<DirNode>> dirs;
    std::wstring test(find);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(find.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        DWORD tick = GetTickCount();
        ULONGLONG num = 0;

        do
        {
            std::lock_guard<std::recursive_mutex> lock(context.mutex);

            const bool compressed = (use_compressed_size && (fd.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED));

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    continue;
                if (!wcscmp(fd.cFileName, TEXT(".")) || !wcscmp(fd.cFileName, TEXT("..")))
                    continue;
                if (drive && !wcsicmp(fd.cFileName, TEXT("$recycle.bin")))
                    continue;

                if (context.dontscan.size())
                {
                    bool match = false;
                    test.resize(base_path_len);
                    test.append(fd.cFileName);
                    ensure_separator(test);
                    for (const auto& ignore : context.dontscan)
                    {
                        match = !wcsicmp(ignore.c_str(), test.c_str());
                        if (match)
                            break;
                    }
                    if (match)
                        continue;
                }

                dirs.emplace_back(root->AddDir(fd.cFileName));
                assert(dirs.back());

                if (compressed)
                    dirs.back()->SetCompressed();

                if (++num > 50 || GetTickCount() - tick > 50)
                {
                    context.current = dirs.back();
LResetFeedbackInterval:
                    tick = GetTickCount();
                    num = 0;
                }
            }
            else
            {
                ULARGE_INTEGER uli;
                if (compressed || (fd.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE))
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
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)
                    file->SetSparse();

                if (++num > 50 || GetTickCount() - tick > 50)
                {
                    context.current = file;
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
        Scan(dir, this_generation, current_generation, context);
    }

    if (this_generation == *current_generation && drive)
    {
        drive->AddRecycleBin();
        const auto recycle = drive->GetRecycleBin();

        if (recycle)
        {
            context.current = recycle;
            recycle->UpdateRecycleBin(context.mutex);
            recycle->Finish();
        }
    }

    root->Finish();
}

