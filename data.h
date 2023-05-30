// Copyright (c) 2023 Christopher Antos
// License: http://opensource.org/licenses/MIT

// Node represents a directory or file.
//
// DirNode contains other DirNode and FileNode instances.
// Querying and adding children are threadsafe operations.
//
// FileNode contains info about the file.

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#undef GetFreeSpace

class DirNode;
class FileNode;
class RecycleBinNode;
class FreeSpaceNode;
class DriveNode;

#ifdef DEBUG
LONG CountNodes();
bool SetFake(bool fake);
#endif

class Node : public std::enable_shared_from_this<Node>
{
public:
                            Node(const WCHAR* name, const std::shared_ptr<DirNode>& parent);
    virtual                 ~Node();
    std::shared_ptr<DirNode> GetParent() const { return m_parent.lock(); }
    bool                    IsParentFinished() const;
    const WCHAR*            GetName() const { return m_name.c_str(); }
    void                    GetFullPath(std::wstring& out) const;
    virtual DirNode*        AsDir() { return nullptr; }
    virtual const DirNode*  AsDir() const { return nullptr; }
    virtual FileNode*       AsFile() { return nullptr; }
    virtual const FileNode* AsFile() const { return nullptr; }
    virtual RecycleBinNode* AsRecycleBin() { return nullptr; }
    virtual const RecycleBinNode* AsRecycleBin() const { return nullptr; }
    virtual const FreeSpaceNode* AsFreeSpace() const { return nullptr; }
    virtual DriveNode*      AsDrive() { return nullptr; }
    virtual const DriveNode* AsDrive() const { return nullptr; }
    void                    SetCompressed(bool compressed=true) { m_compressed = compressed; }
    bool                    IsCompressed() const { return m_compressed; }
    virtual bool            IsRecycleBin() const { return false; }
    virtual bool            IsDrive() const { return false; }
#ifdef DEBUG
    bool                    IsFake() const { return m_fake; }
#endif
protected:
    const std::weak_ptr<DirNode> m_parent;
    const std::wstring      m_name;
    bool                    m_compressed = false;
#ifdef DEBUG
    const bool              m_fake = false;
#endif
};

class DirNode : public Node
{
public:
                            DirNode(const WCHAR* name) : Node(name, nullptr) {}
                            DirNode(const WCHAR* name, const std::shared_ptr<DirNode>& parent) : Node(name, parent) {}
    DirNode*                AsDir() override { return this; }
    const DirNode*          AsDir() const override { return this; }
    ULONGLONG               CountDirs(bool include_recycle=false) const { return m_count_dirs + (include_recycle && GetRecycleBin()); }
    ULONGLONG               CountFiles() const { return m_count_files; }
    std::vector<std::shared_ptr<DirNode>> CopyDirs(bool include_recycle=false) const;
    std::vector<std::shared_ptr<FileNode>> CopyFiles() const;
    virtual std::shared_ptr<RecycleBinNode> GetRecycleBin() const { return nullptr; }
    virtual std::shared_ptr<FreeSpaceNode> GetFreeSpace() const { return nullptr; }
    ULONGLONG               GetSize() const { return m_size; }
    ULONGLONG               GetEffectiveSize() const;
    void                    Hide(bool hide=true) { m_hide = hide; }
    bool                    IsHidden() const { return m_hide; }
    std::shared_ptr<DirNode> AddDir(const WCHAR* name);
    std::shared_ptr<FileNode> AddFile(const WCHAR* name, ULONGLONG size);
    void                    DeleteChild(const std::shared_ptr<Node>& node);
    void                    Clear();
    void                    Finish() { m_finished = true; }
    bool                    IsFinished() const { return m_finished; }
protected:
    void                    UpdateRecycleBinMetadata(ULONGLONG size);
    mutable std::recursive_mutex m_node_mutex;
private:
    std::vector<std::shared_ptr<DirNode>> m_dirs;
    std::vector<std::shared_ptr<FileNode>> m_files;
    ULONGLONG               m_count_dirs = 0;
    ULONGLONG               m_count_files = 0;
    ULONGLONG               m_size = 0;
    bool                    m_finished = false;
    bool                    m_hide = false;
};

class FileNode : public Node
{
public:
                            FileNode(const WCHAR* name, ULONGLONG size, const std::shared_ptr<DirNode>& parent) : Node(name, parent), m_size(size) {}
    FileNode*               AsFile() override { return this; }
    const FileNode*         AsFile() const override { return this; }
    ULONGLONG               GetSize() const { return m_size; }
private:
    const ULONGLONG         m_size;
};

class RecycleBinNode : public DirNode
{
public:
                            RecycleBinNode(const std::shared_ptr<DirNode>& parent) : DirNode(TEXT("Recycle Bin"), parent) {}
    virtual RecycleBinNode* AsRecycleBin() { return this; }
    void                    UpdateRecycleBin(std::recursive_mutex& ui_mutex);
    bool                    IsRecycleBin() const override { return true; }
};

class FreeSpaceNode : public Node
{
public:
                            FreeSpaceNode(const WCHAR* drive, ULONGLONG free, ULONGLONG total, const std::shared_ptr<DirNode>& parent);
    const FreeSpaceNode*    AsFreeSpace() const override { return this; }
    ULONGLONG               GetFreeSize() const { return m_free; }
    ULONGLONG               GetUsedSize() const { return m_total - m_free; }
    ULONGLONG               GetTotalSize() const { return m_total; }
private:
    const ULONGLONG         m_free;
    const ULONGLONG         m_total;
};

class DriveNode : public DirNode
{
public:
                            DriveNode(const WCHAR* name) : DirNode(name, nullptr) {}
    virtual DriveNode*      AsDrive() { return this; }
    virtual const DriveNode* AsDrive() const { return this; }
    void                    AddRecycleBin();
    void                    AddFreeSpace();
    void                    AddFreeSpace(ULONGLONG free, ULONGLONG total);
    virtual std::shared_ptr<RecycleBinNode> GetRecycleBin() const override { return m_recycle; }
    virtual std::shared_ptr<FreeSpaceNode> GetFreeSpace() const override { return m_free; }
    bool                    IsDrive() const override { return true; }
private:
    std::shared_ptr<RecycleBinNode> m_recycle;
    std::shared_ptr<FreeSpaceNode> m_free;
};

inline bool is_separator(const WCHAR ch) { return ch == '/' || ch == '\\'; }
void ensure_separator(std::wstring& path);
void strip_separator(std::wstring& path);
void skip_separators(const WCHAR*& path);
void skip_nonseparators(const WCHAR*& path);
unsigned int has_io_prefix(const WCHAR* path);

bool is_root_finished(const std::shared_ptr<Node>& node);
bool is_drive(const WCHAR* path);
bool is_subst(const WCHAR* path);
bool is_unc(const WCHAR* path, const WCHAR** past_unc);
bool get_drivelike_prefix(const WCHAR* path, std::wstring&out);

