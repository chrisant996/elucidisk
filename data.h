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

class DirNode;
class FileNode;

class Node : public std::enable_shared_from_this<Node>
{
public:
                            Node(const WCHAR* name) : m_name(name) {}
    virtual                 ~Node() {}
    const WCHAR*            GetName() const { return m_name.c_str(); }
    virtual const DirNode*  AsDir() const { return nullptr; }
    virtual const FileNode* AsFile() const { return nullptr; }
protected:
    const std::wstring      m_name;
};

class DirNode : public Node
{
public:
                            DirNode(const WCHAR* name) : Node(name) {}
                            DirNode(const WCHAR* name, const std::shared_ptr<DirNode>& parent) : Node(name), m_parent(parent) {}
    const DirNode*          AsDir() const override { return this; }
    std::shared_ptr<DirNode> GetParent() const { return m_parent; }
    ULONGLONG               CountDirs() const;
    ULONGLONG               CountFiles() const;
    std::vector<std::shared_ptr<DirNode>> CopyDirs() const;
    std::vector<std::shared_ptr<FileNode>> CopyFiles() const;
    ULONGLONG               GetSize() const { return m_size; }
    std::shared_ptr<DirNode> AddDir(const WCHAR* name);
    void                    AddFile(const WCHAR* name, ULONGLONG size);
private:
    mutable std::mutex      m_mutex;
    std::shared_ptr<DirNode> m_parent;
    std::vector<std::shared_ptr<DirNode>> m_dirs;
    std::vector<std::shared_ptr<FileNode>> m_files;
    ULONGLONG               m_size;
};

class FileNode : public Node
{
public:
                            FileNode(const WCHAR* name, ULONGLONG size) : Node(name), m_size(size) {}
    const FileNode*         AsFile() const override { return this; }
    ULONGLONG               GetSize() const { return m_size; }
private:
    const ULONGLONG         m_size;
};

