// In-memory file system behind the File Explorer, Notepad, Terminal and the
// desktop icons. Paths are Unix-style: "/" is the root (shown as "This PC")
// and "/Documents/notes.txt" is a file inside the Documents folder.
// Name lookups are case-insensitive, like on Windows.
#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>

enum class FileKind { Folder, Text, Image, Program, Binary };

struct FsNode {
    std::string name;
    bool        isDir    = false;
    bool        locked   = false;   // can't be renamed, moved or deleted (system items)
    bool        readOnly = false;   // contents can't change (files: no writes, folders: no new/removed items)
    std::string content;            // body of text files
    std::string hostPath;           // real image on disk backing this file (wallpapers)
    size_t      fakeSize = 0;       // reported size for files without real content
    std::time_t modified = 0;
    FsNode*     parent   = nullptr;
    std::vector<std::unique_ptr<FsNode>> children;   // folders only; folders first, then by name
};

class VirtualFS {
public:
    VirtualFS();

    FsNode* Root() const { return root.get(); }

    // Absolute ("/a/b") or relative to `cwd`; understands ".", ".." and "~".
    // Returns nullptr when nothing exists at that path.
    FsNode* Resolve(const std::string& path, const std::string& cwd = "/") const;
    static FsNode* Child(const FsNode* dir, const std::string& name);

    static std::string PathOf(const FsNode* node);
    static std::string Join(const std::string& dir, const std::string& name);
    static std::string ParentOf(const std::string& path);
    static std::string NameOf(const std::string& path);
    static bool        IsInside(const FsNode* node, const FsNode* ancestor);

    static FileKind    KindOf(const FsNode* node);
    static std::string TypeName(const FsNode* node);
    static size_t      SizeOf(const FsNode* node);
    static size_t      CountItems(const FsNode* dir);   // recursive
    static std::string FormatSize(size_t bytes);
    static std::string FormatTime(std::time_t t);

    // These return nullptr / false on failure; LastError() explains why.
    FsNode* NewFolder(FsNode* parent, const std::string& name);
    FsNode* NewFile(FsNode* parent, const std::string& name, const std::string& content = "");
    bool    WriteFile(FsNode* file, const std::string& content);
    bool    Rename(FsNode* node, const std::string& newName);
    FsNode* Move(FsNode* node, FsNode* destFolder);
    FsNode* Copy(const FsNode* node, FsNode* destFolder);
    bool    Remove(FsNode* node);

    // "New folder" -> "New folder (2)" when the name is taken.
    std::string UniqueName(const FsNode* dir, const std::string& wanted) const;
    // Every item under `under` whose name contains `query` (case-insensitive).
    void Find(const FsNode* under, const std::string& query, std::vector<FsNode*>& out) const;
    // Every image backed by a real file (used for wallpaper pickers).
    void FindImages(const FsNode* under, std::vector<FsNode*>& out) const;

    const std::string& LastError() const { return lastError; }

private:
    std::unique_ptr<FsNode> root;
    mutable std::string     lastError;

    bool    Fail(const std::string& message) const;
    bool    CanAddTo(const FsNode* folder) const;
    bool    ValidateName(const FsNode* dir, const std::string& name, const FsNode* self) const;
    FsNode* Insert(FsNode* parent, std::unique_ptr<FsNode> node);
    std::unique_ptr<FsNode> Detach(FsNode* node);
    static std::unique_ptr<FsNode> Clone(const FsNode* src);
    void    Seed();
};
