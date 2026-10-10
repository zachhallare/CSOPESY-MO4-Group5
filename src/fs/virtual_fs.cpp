#include "virtual_fs.h"
#include "core/config.h"
#include "core/strings.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

// ---------------------------------------------------------------------------
// Default contents
// ---------------------------------------------------------------------------

static const char* WELCOME_TXT = R"(Welcome to CSOPESY OS v1.0!

This desktop is a mock-up of an operating system shell. Everything you see
is redrawn every frame by a compositor built on GLFW + OpenGL + Dear ImGui.

Things to try:
  - Double-click the icons on the desktop to open apps.
  - Use File Explorer to create, rename, copy, move and delete files.
  - Open Task Manager, then press INIT and START on the taskbar to watch
    the mock scheduler spawn processes. Press STOP to stop it.
  - Type 'help' in the Terminal for a list of commands.
  - Change the wallpaper in Settings > Personalization.
  - Press PWR on the taskbar to shut down.
)";

static const char* NOTES_TXT = R"(CSOPESY - Operating Systems
===========================
Process states: New -> Ready -> Running -> Waiting -> Terminated
Scheduling: FCFS, SJF, Round Robin (quantum based), Priority
Memory: paging, segmentation, virtual memory
Page replacement: FIFO, LRU, Optimal
)";

static const char* TODO_TXT = R"([x] BIOS boot screen
[x] Loading screen
[x] Desktop with wallpaper and real-time clock
[x] Taskbar with app buttons
[x] Task Manager with process table
[ ] Record the demo video
[ ] Finish the technical report (PPT)
)";

static const char* MO1_TXT = R"(MO1: Process Scheduler and CLI
- initialize: reads config.txt (num-cpu, scheduler, quantum-cycles, ...)
- scheduler-start: generates dummy processes in the background
- scheduler-stop: stops generating processes
- screen -ls: lists running and finished processes
- report-util: saves CPU utilization to csopesy-log.txt
)";

static const char* MO2_TXT = R"(MO2: Desktop-Style OS Mock-up (Dear ImGui)
Checklist:
- Desktop: full-screen wallpaper, real-time clock, PWR shutdown button
- Taskbar: at least three icon buttons; two open unique apps,
  the third opens the Task Manager
- Task Manager: process table with CPU and memory usage (dummy values)
)";

static const char* INSTALL_LOG = R"([08:00:01] CSOPESY OS Setup started
[08:00:03] Copying system files... done
[08:00:07] Installing drivers: display.drv, audio.drv, network.drv... done
[08:00:09] Creating user profile... done
[08:00:10] Setup completed successfully
)";

static const char* BOOT_INI = R"([boot loader]
timeout=30
default=disk(0)partition(1)\CSOPESY

[operating systems]
disk(0)partition(1)\CSOPESY="CSOPESY OS v1.0" /fastdetect
)";

static const char* CONFIG_SYS = R"(DEVICE=C:\SYSTEM\DRIVERS\DISPLAY.DRV
DEVICE=C:\SYSTEM\DRIVERS\AUDIO.DRV
DEVICE=C:\SYSTEM\DRIVERS\NETWORK.DRV
FILES=40
BUFFERS=20
)";

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

VirtualFS::VirtualFS() {
    root = std::make_unique<FsNode>();
    root->isDir    = true;
    root->locked   = true;
    root->modified = std::time(nullptr);
    Seed();
}

static void SetFlagsRecursive(FsNode* node, bool locked, bool readOnly) {
    node->locked   = locked;
    node->readOnly = readOnly;
    for (auto& child : node->children)
        SetFlagsRecursive(child.get(), locked, readOnly);
}

static void SetTimeRecursive(FsNode* node, std::time_t t) {
    node->modified = t;
    for (auto& child : node->children)
        SetTimeRecursive(child.get(), t);
}

void VirtualFS::Seed() {
    const std::time_t now = std::time(nullptr);
    auto daysAgo = [now](int days, int minutes) {
        return now - (std::time_t)days * 86400 - (std::time_t)minutes * 60;
    };

    FsNode* r         = root.get();
    FsNode* desktop   = NewFolder(r, "Desktop");
    FsNode* documents = NewFolder(r, "Documents");
    FsNode* downloads = NewFolder(r, "Downloads");
    FsNode* pictures  = NewFolder(r, "Pictures");
    FsNode* programs  = NewFolder(r, "Programs");
    FsNode* system    = NewFolder(r, "System");

    NewFile(desktop, "Welcome.txt", WELCOME_TXT);

    NewFile(documents, "notes.txt", NOTES_TXT)->modified = daysAgo(2, 95);
    NewFile(documents, "todo.txt",  TODO_TXT)->modified  = daysAgo(0, 42);
    FsNode* course = NewFolder(documents, "CSOPESY");
    NewFile(course, "MO1 - Process Scheduler.txt", MO1_TXT)->modified = daysAgo(21, 300);
    NewFile(course, "MO2 - Desktop OS Mockup.txt", MO2_TXT)->modified = daysAgo(1, 610);
    course->modified = daysAgo(1, 610);

    NewFile(downloads, "install_log.txt", INSTALL_LOG)->modified = daysAgo(30, 0);

    // Pictures mirrors the real wallpapers folder, so dropping a new image
    // there makes it show up in Explorer and Settings without recompiling.
    try {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(ResolveAssetPath("wallpapers"), ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = ExtensionOf(entry.path().filename().string());
            if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".bmp") continue;

            FsNode* image = NewFile(pictures, entry.path().filename().string());
            if (!image) continue;
            image->hostPath = CanonicalPath(entry.path().string());
            image->fakeSize = (size_t)entry.file_size(ec);
            image->modified = daysAgo(5, 0);
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Could not scan wallpapers folder: %s\n", e.what());
    }

    struct Program { const char* exe; size_t size; };
    static const Program PROGRAMS[] = {
        { "explorer.exe", 245760 }, { "terminal.exe", 98304 },  { "notepad.exe", 69632 },
        { "taskmgr.exe",  131072 }, { "settings.exe", 184320 }, { "sysinfo.exe", 57344 },
    };
    for (const Program& p : PROGRAMS)
        NewFile(programs, p.exe)->fakeSize = p.size;

    NewFile(system, "boot.ini", BOOT_INI);
    NewFile(system, "config.sys", CONFIG_SYS);
    NewFile(system, "kernel.sys")->fakeSize = 1245184;
    FsNode* drivers = NewFolder(system, "drivers");
    NewFile(drivers, "display.drv")->fakeSize = 49152;
    NewFile(drivers, "audio.drv")->fakeSize   = 32768;
    NewFile(drivers, "network.drv")->fakeSize = 40960;

    SetTimeRecursive(programs, daysAgo(120, 0));
    SetTimeRecursive(system,   daysAgo(120, 0));

    // "This PC" only holds the top-level folders, and those stay put.
    // Programs and System are off limits entirely.
    r->readOnly = true;
    for (auto& child : r->children)
        child->locked = true;
    SetFlagsRecursive(programs, true, true);
    SetFlagsRecursive(system,   true, true);
}

// ---------------------------------------------------------------------------
// Paths and lookups
// ---------------------------------------------------------------------------

FsNode* VirtualFS::Child(const FsNode* dir, const std::string& name) {
    if (!dir || !dir->isDir) return nullptr;
    for (const auto& child : dir->children)
        if (EqualsNoCase(child->name, name))
            return child.get();
    return nullptr;
}

FsNode* VirtualFS::Resolve(const std::string& path, const std::string& cwd) const {
    bool absolute = !path.empty() && (path[0] == '/' || path[0] == '\\');
    std::string full = absolute ? path : cwd + "/" + path;

    FsNode* node = root.get();
    size_t start = 0;
    while (start <= full.size()) {
        size_t end = full.find_first_of("/\\", start);
        if (end == std::string::npos) end = full.size();
        std::string part = full.substr(start, end - start);
        start = end + 1;

        if (part.empty() || part == ".") continue;
        if (part == "~")  { node = root.get(); continue; }
        if (part == "..") { if (node->parent) node = node->parent; continue; }

        node = Child(node, part);
        if (!node) return nullptr;
    }
    return node;
}

std::string VirtualFS::PathOf(const FsNode* node) {
    if (!node || !node->parent) return "/";
    std::string path;
    for (const FsNode* n = node; n->parent; n = n->parent)
        path = "/" + n->name + path;
    return path;
}

std::string VirtualFS::Join(const std::string& dir, const std::string& name) {
    if (dir.empty() || dir == "/") return "/" + name;
    return dir + "/" + name;
}

std::string VirtualFS::ParentOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos || slash == 0) return "/";
    return path.substr(0, slash);
}

std::string VirtualFS::NameOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool VirtualFS::IsInside(const FsNode* node, const FsNode* ancestor) {
    for (const FsNode* n = node; n; n = n->parent)
        if (n == ancestor) return true;
    return false;
}

// ---------------------------------------------------------------------------
// File info
// ---------------------------------------------------------------------------

FileKind VirtualFS::KindOf(const FsNode* node) {
    if (node->isDir) return FileKind::Folder;
    std::string ext = ExtensionOf(node->name);
    if (ext == ".exe") return FileKind::Program;
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") return FileKind::Image;
    if (ext == ".sys" || ext == ".drv" || ext == ".bin" || ext == ".dll") {
        // Plain-text system files (config.sys) can still be read.
        return node->content.empty() ? FileKind::Binary : FileKind::Text;
    }
    return FileKind::Text;
}

std::string VirtualFS::TypeName(const FsNode* node) {
    if (node->isDir) return "File folder";
    std::string ext = ExtensionOf(node->name);
    if (ext.empty())                      return "File";
    if (ext == ".txt")                    return "Text Document";
    if (ext == ".md")                     return "Markdown File";
    if (ext == ".log")                    return "Log File";
    if (ext == ".ini" || ext == ".cfg")   return "Configuration File";
    if (ext == ".exe")                    return "Application";
    if (ext == ".sys")                    return "System File";
    if (ext == ".drv")                    return "Device Driver";
    if (ext == ".jpg" || ext == ".jpeg")  return "JPEG Image";
    if (ext == ".png")                    return "PNG Image";
    if (ext == ".bmp")                    return "Bitmap Image";

    std::string upper = ext.substr(1);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return upper + " File";
}

size_t VirtualFS::SizeOf(const FsNode* node) {
    if (!node->isDir)
        return node->fakeSize ? node->fakeSize : node->content.size();
    size_t total = 0;
    for (const auto& child : node->children)
        total += SizeOf(child.get());
    return total;
}

size_t VirtualFS::CountItems(const FsNode* dir) {
    size_t count = 0;
    for (const auto& child : dir->children)
        count += 1 + (child->isDir ? CountItems(child.get()) : 0);
    return count;
}

std::string VirtualFS::FormatSize(size_t bytes) {
    char buf[32];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%llu bytes", (unsigned long long)bytes);
    else if (bytes < 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

std::string VirtualFS::FormatTime(std::time_t t) {
    std::tm lt;
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M %p", &lt);
    return buf;
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

bool VirtualFS::Fail(const std::string& message) const {
    lastError = message;
    return false;
}

bool VirtualFS::CanAddTo(const FsNode* folder) const {
    if (!folder || !folder->isDir)
        return Fail("The destination is not a folder.");
    if (folder->readOnly)
        return Fail("Access denied: '" + (folder->parent ? folder->name : std::string("This PC")) +
                    "' is a read-only system folder.");
    return true;
}

bool VirtualFS::ValidateName(const FsNode* dir, const std::string& name, const FsNode* self) const {
    if (name.empty() || name == "." || name == "..")
        return Fail("Please enter a valid name.");
    if (name.find_first_of("/\\:*?\"<>|") != std::string::npos)
        return Fail("A name can't contain any of these characters: \\ / : * ? \" < > |");
    if (name.size() > 64)
        return Fail("That name is too long (64 characters max).");
    FsNode* existing = Child(dir, name);
    if (existing && existing != self)
        return Fail("An item named '" + existing->name + "' already exists here.");
    return true;
}

static bool SortsBefore(const FsNode* a, const FsNode* b) {
    if (a->isDir != b->isDir) return a->isDir;
    return ToLower(a->name) < ToLower(b->name);
}

FsNode* VirtualFS::Insert(FsNode* parent, std::unique_ptr<FsNode> node) {
    node->parent = parent;
    auto pos = std::find_if(parent->children.begin(), parent->children.end(),
        [&](const std::unique_ptr<FsNode>& c) { return SortsBefore(node.get(), c.get()); });
    FsNode* raw = node.get();
    parent->children.insert(pos, std::move(node));
    parent->modified = std::time(nullptr);
    return raw;
}

std::unique_ptr<FsNode> VirtualFS::Detach(FsNode* node) {
    FsNode* parent = node->parent;
    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&](const std::unique_ptr<FsNode>& c) { return c.get() == node; });
    std::unique_ptr<FsNode> owned = std::move(*it);
    parent->children.erase(it);
    parent->modified = std::time(nullptr);
    owned->parent = nullptr;
    return owned;
}

std::unique_ptr<FsNode> VirtualFS::Clone(const FsNode* src) {
    auto copy = std::make_unique<FsNode>();
    copy->name     = src->name;
    copy->isDir    = src->isDir;
    copy->content  = src->content;
    copy->hostPath = src->hostPath;
    copy->fakeSize = src->fakeSize;
    copy->modified = std::time(nullptr);
    for (const auto& child : src->children) {
        auto c = Clone(child.get());
        c->parent = copy.get();
        copy->children.push_back(std::move(c));
    }
    return copy;
}

FsNode* VirtualFS::NewFolder(FsNode* parent, const std::string& name) {
    std::string clean = Trim(name);
    if (!CanAddTo(parent) || !ValidateName(parent, clean, nullptr))
        return nullptr;
    auto node = std::make_unique<FsNode>();
    node->name     = clean;
    node->isDir    = true;
    node->modified = std::time(nullptr);
    return Insert(parent, std::move(node));
}

FsNode* VirtualFS::NewFile(FsNode* parent, const std::string& name, const std::string& content) {
    std::string clean = Trim(name);
    if (!CanAddTo(parent) || !ValidateName(parent, clean, nullptr))
        return nullptr;
    auto node = std::make_unique<FsNode>();
    node->name     = clean;
    node->content  = content;
    node->modified = std::time(nullptr);
    return Insert(parent, std::move(node));
}

bool VirtualFS::WriteFile(FsNode* file, const std::string& content) {
    if (!file || file->isDir)
        return Fail("That is not a file.");
    if (file->readOnly)
        return Fail("Access denied: '" + file->name + "' is read-only.");
    file->content  = content;
    file->fakeSize = 0;
    file->hostPath.clear();
    file->modified = std::time(nullptr);
    return true;
}

bool VirtualFS::Rename(FsNode* node, const std::string& newName) {
    std::string clean = Trim(newName);
    if (!node || !node->parent)
        return Fail("This item can't be renamed.");
    if (node->locked)
        return Fail("'" + node->name + "' is a protected system item and can't be renamed.");
    if (!ValidateName(node->parent, clean, node))
        return false;
    if (clean == node->name)
        return true;

    FsNode* parent = node->parent;
    std::unique_ptr<FsNode> owned = Detach(node);
    owned->name     = clean;
    owned->modified = std::time(nullptr);
    Insert(parent, std::move(owned));
    return true;
}

FsNode* VirtualFS::Move(FsNode* node, FsNode* destFolder) {
    if (!node || !node->parent) {
        Fail("This item can't be moved.");
        return nullptr;
    }
    if (node->locked) {
        Fail("'" + node->name + "' is a protected system item and can't be moved.");
        return nullptr;
    }
    if (!CanAddTo(destFolder))
        return nullptr;
    if (destFolder == node->parent)
        return node;
    if (IsInside(destFolder, node)) {
        Fail("The destination folder is inside the folder being moved.");
        return nullptr;
    }
    if (!ValidateName(destFolder, node->name, nullptr))
        return nullptr;

    std::unique_ptr<FsNode> owned = Detach(node);
    owned->modified = std::time(nullptr);
    return Insert(destFolder, std::move(owned));
}

FsNode* VirtualFS::Copy(const FsNode* node, FsNode* destFolder) {
    if (!node || !node->parent) {
        Fail("This item can't be copied.");
        return nullptr;
    }
    if (!CanAddTo(destFolder))
        return nullptr;
    if (node->isDir && IsInside(destFolder, node)) {
        Fail("The destination folder is inside the folder being copied.");
        return nullptr;
    }

    // Same rule as Windows: "notes.txt" becomes "notes - Copy.txt".
    std::string name = node->name;
    if (Child(destFolder, name)) {
        std::string ext  = node->isDir ? "" : name.substr(name.size() - ExtensionOf(name).size());
        std::string stem = name.substr(0, name.size() - ext.size());
        name = UniqueName(destFolder, stem + " - Copy" + ext);
    }

    std::unique_ptr<FsNode> copy = Clone(node);
    copy->name = name;
    return Insert(destFolder, std::move(copy));
}

bool VirtualFS::Remove(FsNode* node) {
    if (!node || !node->parent)
        return Fail("This item can't be deleted.");
    if (node->locked)
        return Fail("'" + node->name + "' is a protected system item and can't be deleted.");
    if (node->parent->readOnly)
        return Fail("Access denied: '" + node->parent->name + "' is a read-only system folder.");
    Detach(node);   // the returned unique_ptr frees the whole subtree
    return true;
}

std::string VirtualFS::UniqueName(const FsNode* dir, const std::string& wanted) const {
    if (!Child(dir, wanted))
        return wanted;

    std::string ext  = ExtensionOf(wanted);
    std::string stem = wanted.substr(0, wanted.size() - ext.size());
    ext = wanted.substr(stem.size());   // keep the original casing
    for (int n = 2; ; ++n) {
        std::string candidate = stem + " (" + std::to_string(n) + ")" + ext;
        if (!Child(dir, candidate))
            return candidate;
    }
}

void VirtualFS::Find(const FsNode* under, const std::string& query, std::vector<FsNode*>& out) const {
    if (!under) return;
    for (const auto& child : under->children) {
        if (ContainsNoCase(child->name, query))
            out.push_back(child.get());
        if (child->isDir)
            Find(child.get(), query, out);
    }
}

void VirtualFS::FindImages(const FsNode* under, std::vector<FsNode*>& out) const {
    if (!under) return;
    for (const auto& child : under->children) {
        if (child->isDir)
            FindImages(child.get(), out);
        else if (!child->hostPath.empty())
            out.push_back(child.get());
    }
}
