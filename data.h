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
class FreeSpaceNode;

class Node : public std::enable_shared_from_this<Node>
{
public:
                            Node(const WCHAR* name, const std::shared_ptr<DirNode>& parent) : m_name(name), m_parent(parent) {}
    virtual                 ~Node() {}
    std::shared_ptr<DirNode> GetParent() const { return m_parent; }
    const WCHAR*            GetName() const { return m_name.c_str(); }
    void                    GetFullPath(std::wstring& out) const;
    virtual DirNode*        AsDir() { return nullptr; }
    virtual const DirNode*  AsDir() const { return nullptr; }
    virtual FileNode*       AsFile() { return nullptr; }
    virtual const FileNode* AsFile() const { return nullptr; }
    virtual const FreeSpaceNode* AsFreeSpace() const { return nullptr; }
protected:
    const std::shared_ptr<DirNode> m_parent;
    const std::wstring      m_name;
};

class DirNode : public Node
{
public:
                            DirNode(const WCHAR* name) : Node(name, std::shared_ptr<DirNode>(nullptr)) {}
                            DirNode(const WCHAR* name, const std::shared_ptr<DirNode>& parent) : Node(name, parent) {}
    DirNode*                AsDir() override { return this; }
    const DirNode*          AsDir() const override { return this; }
    ULONGLONG               CountFiles() const { return m_count_files; }
    std::vector<std::shared_ptr<DirNode>> CopyDirs() const;
    std::vector<std::shared_ptr<FileNode>> CopyFiles() const;
    std::shared_ptr<FreeSpaceNode> GetFreeSpace() const { return m_free; }
    ULONGLONG               GetSize() const { return m_size; }
    void                    Hide(bool hide=true) { m_hide = hide; }
    bool                    Hidden() const { return m_hide; }
    std::shared_ptr<DirNode> AddDir(const WCHAR* name);
    void                    AddFile(const WCHAR* name, ULONGLONG size);
    void                    AddFreeSpace(ULONGLONG free, ULONGLONG total);
    void                    Finish() { m_finished = true; }
    bool                    Finished() const { return m_finished; }
private:
    mutable std::mutex      m_mutex;
    std::vector<std::shared_ptr<DirNode>> m_dirs;
    std::vector<std::shared_ptr<FileNode>> m_files;
    std::shared_ptr<FreeSpaceNode> m_free; // TODO: Only needed on a RootDirNode.
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

class FreeSpaceNode : public Node
{
public:
                            FreeSpaceNode(const WCHAR* name, ULONGLONG free, ULONGLONG total, const std::shared_ptr<DirNode>& parent) : Node(name, parent), m_free(free), m_total(total) {}
    const FreeSpaceNode*    AsFreeSpace() const override { return this; }
    ULONGLONG               GetFreeSize() const { return m_free; }
    ULONGLONG               GetTotalSize() const { return m_total; }
private:
    const ULONGLONG         m_free;
    const ULONGLONG         m_total;
};

inline bool is_separator(const WCHAR ch) { return ch == '/' || ch == '\\'; }
void ensure_separator(std::wstring& path);

