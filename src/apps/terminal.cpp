#include "terminal.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {

const ImU32 COLOR_TEXT   = IM_COL32(204, 204, 204, 255);
const ImU32 COLOR_PROMPT = IM_COL32(80, 230, 120, 255);
const ImU32 COLOR_PATH   = IM_COL32(97, 175, 239, 255);
const ImU32 COLOR_ERROR  = IM_COL32(255, 95, 86, 255);
const ImU32 COLOR_DIM    = IM_COL32(130, 130, 140, 255);
const ImU32 COLOR_ACCENT = IM_COL32(120, 210, 255, 255);
const ImU32 COLOR_WARN   = IM_COL32(240, 190, 80, 255);

struct CommandHelp {
    const char* usage;
    const char* description;
};

const CommandHelp COMMANDS[] = {
    { "help",                      "Show this list" },
    { "clear | cls",               "Clear the screen" },
    { "pwd",                       "Print the current folder" },
    { "ls | dir [path]",           "List a folder (dir shows details)" },
    { "cd [path]",                 "Change folder (supports .., / and ~)" },
    { "tree [path]",               "Show the folder tree" },
    { "mkdir <name>",              "Create a folder" },
    { "touch <name>",              "Create an empty file" },
    { "cat | type <file>",         "Print a text file" },
    { "echo <text> [> | >> file]", "Print text, or write / append it to a file" },
    { "cp | copy <src> <dest>",    "Copy a file or folder" },
    { "mv | move <src> <dest>",    "Move or rename a file or folder" },
    { "ren <path> <new name>",     "Rename a file or folder" },
    { "rm | del [-r] <path>",      "Delete a file (-r for folders)" },
    { "find <text>",               "Search file and folder names" },
    { "edit <file>",               "Open a file in Notepad (creates it if missing)" },
    { "open | start <app|path>",   "Launch an app, or open a file or folder" },
    { "ps",                        "List running processes" },
    { "kill <pid>",                "End a process" },
    { "initialize",                "Initialize the scheduler (taskbar INIT)" },
    { "scheduler-start",           "Start generating processes (taskbar START)" },
    { "scheduler-stop",            "Stop generating processes (taskbar STOP)" },
    { "date | whoami | ver",       "Show the date, the user or the OS version" },
    { "sysinfo",                   "Show system information" },
    { "history",                   "Show previous commands" },
    { "exit",                      "Close the terminal" },
};

const char* COMMAND_NAMES[] = {
    "help", "clear", "cls", "pwd", "ls", "dir", "cd", "tree", "mkdir", "touch", "cat", "type",
    "echo", "cp", "copy", "mv", "move", "ren", "rename", "rm", "del", "rmdir", "find", "edit",
    "open", "start", "ps", "kill", "initialize", "scheduler-start", "scheduler-stop", "date",
    "time", "whoami", "ver", "sysinfo", "history", "exit",
};

// Splits on spaces; "double quotes" keep spaces inside one argument.
std::vector<std::string> Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false, hasToken = false;
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
            hasToken = true;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (hasToken) tokens.push_back(current);
            current.clear();
            hasToken = false;
        } else {
            current += c;
            hasToken = true;
        }
    }
    if (hasToken) tokens.push_back(current);
    return tokens;
}

std::string JoinArgs(const std::vector<std::string>& args, size_t from) {
    std::string out;
    for (size_t i = from; i < args.size(); ++i) {
        if (i > from) out += ' ';
        out += args[i];
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

std::string Terminal::Title() const {
    return "Terminal - " + cwd;
}

void Terminal::Print(const std::string& text, ImU32 color) {
    lines.push_back({ "", text, color ? color : COLOR_TEXT });
    scrollToEnd = true;
}

void Terminal::PrintError(const std::string& text) {
    Print(text, COLOR_ERROR);
}

std::string Terminal::Prompt(const Shell& shell) const {
    return shell.settings.username + "@csopesy:" + cwd + "$ ";
}

std::string Terminal::Absolute(const std::string& path) const {
    if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
        return path;
    if (path == "~")
        return "/";
    if (path.rfind("~/", 0) == 0)
        return path.substr(1);
    return VirtualFS::Join(cwd, path);
}

void Terminal::OnOpen(Shell&) {
    if (lines.empty()) {
        Print("CSOPESY OS Terminal [Version 1.0.2026]", COLOR_ACCENT);
        Print("(c) CSOPESY Megatrends Inc. Type 'help' for a list of commands.", COLOR_DIM);
        Print("");
    }
    focusInput = true;
}

void Terminal::OnClose(Shell&) {
    lines.clear();
    cwd        = "/";
    historyPos = -1;
    input[0]   = '\0';
}

// ---------------------------------------------------------------------------
// Rendering and input
// ---------------------------------------------------------------------------

void Terminal::Render(Shell& shell) {
    activeShell = &shell;
    FsNode* here = shell.fs.Resolve(cwd);
    if (!here || !here->isDir)
        cwd = "/";   // the folder was deleted somewhere else

    ImGui::PushFont(shell.fonts.mono);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(12, 12, 16, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(12, 12, 16, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    ImGui::BeginChild("##console", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

    ImGui::PushTextWrapPos(0.0f);
    for (const Line& line : lines) {
        if (!line.prompt.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR_PROMPT);
            ImGui::TextUnformatted(line.prompt.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, line.color);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();

    // Prompt and input share the last line, like a real terminal.
    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_PROMPT);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Prompt(shell).c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 0);

    if (focusInput) {
        ImGui::SetKeyboardFocusHere();
        focusInput = false;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;
    if (ImGui::InputText("##input", input, sizeof(input), flags, &Terminal::InputCallback, this)) {
        std::string command = input;
        input[0] = '\0';
        Execute(shell, command);
        focusInput  = true;
        scrollToEnd = true;
    }

    if (scrollToEnd) {
        ImGui::SetScrollHereY(1.0f);
        scrollToEnd = false;
    }

    // Clicking anywhere in the console puts the cursor back on the input line.
    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
        focusInput = true;

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopFont();
}

int Terminal::InputCallback(ImGuiInputTextCallbackData* data) {
    Terminal* self = static_cast<Terminal*>(data->UserData);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (self->history.empty())
            return 0;
        int previous = self->historyPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (self->historyPos == -1)
                self->historyPos = (int)self->history.size() - 1;
            else if (self->historyPos > 0)
                self->historyPos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (self->historyPos != -1 && ++self->historyPos >= (int)self->history.size())
                self->historyPos = -1;
        }
        if (previous != self->historyPos) {
            std::string entry = self->historyPos >= 0 ? self->history[self->historyPos] : std::string();
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, entry.c_str());
        }
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        self->Complete(data);
    }
    return 0;
}

void Terminal::Complete(ImGuiInputTextCallbackData* data) {
    if (!activeShell)
        return;

    // The word being completed runs from the previous space up to the cursor.
    int end = data->CursorPos, start = end;
    while (start > 0 && data->Buf[start - 1] != ' ')
        --start;
    std::string word(data->Buf + start, data->Buf + end);
    bool firstWord = std::all_of(data->Buf, data->Buf + start, [](char c) { return c == ' '; });

    std::vector<std::string> candidates;
    if (firstWord) {
        for (const char* name : COMMAND_NAMES)
            if (StartsWithNoCase(name, word))
                candidates.push_back(name);
    } else {
        size_t      slash    = word.find_last_of('/');
        std::string dirPart  = (slash == std::string::npos) ? "" : word.substr(0, slash + 1);
        std::string namePart = (slash == std::string::npos) ? word : word.substr(slash + 1);
        FsNode*     dir      = activeShell->fs.Resolve(dirPart.empty() ? "." : dirPart, cwd);
        if (dir && dir->isDir)
            for (const auto& child : dir->children)
                if (StartsWithNoCase(child->name, namePart))
                    candidates.push_back(dirPart + child->name + (child->isDir ? "/" : ""));
    }

    if (candidates.empty())
        return;

    if (candidates.size() == 1) {
        std::string completion = candidates[0];
        if (completion.find(' ') != std::string::npos)
            completion = "\"" + completion + "\"";
        if (firstWord)
            completion += " ";
        data->DeleteChars(start, end - start);
        data->InsertChars(start, completion.c_str());
        return;
    }

    // Several matches: complete their common prefix and list them.
    std::string common = candidates[0];
    for (const std::string& c : candidates) {
        size_t n = 0;
        while (n < common.size() && n < c.size() && std::tolower((unsigned char)common[n]) == std::tolower((unsigned char)c[n]))
            ++n;
        common.resize(n);
    }
    if (common.size() > word.size() && common.find(' ') == std::string::npos) {
        data->DeleteChars(start, end - start);
        data->InsertChars(start, common.c_str());
    }
    Print(JoinArgs(candidates, 0), COLOR_DIM);
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void Terminal::Execute(Shell& shell, const std::string& commandLine) {
    std::string line = Trim(commandLine);
    lines.push_back({ Prompt(shell), line, COLOR_TEXT });
    if (line.empty())
        return;
    if (history.empty() || history.back() != line)
        history.push_back(line);
    historyPos = -1;

    std::vector<std::string> args = Tokenize(line);
    if (args.empty())
        return;
    const std::string cmd = ToLower(args[0]);
    VirtualFS&        fs  = shell.fs;

    if (cmd == "help") {
        Print("Available commands:", COLOR_ACCENT);
        for (const CommandHelp& c : COMMANDS) {
            char buf[160];
            snprintf(buf, sizeof(buf), "  %-28s %s", c.usage, c.description);
            Print(buf);
        }
    }
    else if (cmd == "clear" || cmd == "cls") {
        lines.clear();
    }
    else if (cmd == "pwd") {
        Print(cwd);
    }
    else if (cmd == "ls" || cmd == "dir") {
        bool detailed = (cmd == "dir") || std::find(args.begin(), args.end(), "-l") != args.end();
        ListFolder(shell, args, detailed);
    }
    else if (cmd == "cd") {
        std::string target = args.size() > 1 ? args[1] : "/";
        FsNode*     node   = fs.Resolve(target, cwd);
        if (!node)
            PrintError("cd: no such folder: " + target);
        else if (!node->isDir)
            PrintError("cd: not a folder: " + target);
        else
            cwd = VirtualFS::PathOf(node);
    }
    else if (cmd == "tree") {
        FsNode* node = fs.Resolve(args.size() > 1 ? args[1] : ".", cwd);
        if (!node || !node->isDir) {
            PrintError("tree: no such folder");
        } else {
            Print(node->parent ? VirtualFS::PathOf(node) : "This PC (/)", COLOR_PATH);
            PrintTree(node, "");
        }
    }
    else if (cmd == "mkdir" || cmd == "touch") {
        if (args.size() < 2) {
            PrintError("usage: " + cmd + " <name>");
            return;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            std::string path = Absolute(args[i]);
            if (cmd == "touch") {
                if (FsNode* existing = fs.Resolve(path)) {
                    if (!existing->isDir && !existing->readOnly)
                        existing->modified = std::time(nullptr);
                    continue;
                }
            }
            FsNode* parent = fs.Resolve(VirtualFS::ParentOf(path));
            if (!parent) {
                PrintError(cmd + ": no such folder: " + VirtualFS::ParentOf(path));
                continue;
            }
            FsNode* created = (cmd == "mkdir") ? fs.NewFolder(parent, VirtualFS::NameOf(path))
                                               : fs.NewFile(parent, VirtualFS::NameOf(path));
            if (!created)
                PrintError(cmd + ": " + fs.LastError());
        }
    }
    else if (cmd == "cat" || cmd == "type") {
        if (args.size() < 2) {
            PrintError("usage: " + cmd + " <file>");
            return;
        }
        FsNode* node = fs.Resolve(args[1], cwd);
        if (!node)
            PrintError(cmd + ": no such file: " + args[1]);
        else if (node->isDir)
            PrintError(cmd + ": " + args[1] + " is a folder");
        else if (VirtualFS::KindOf(node) != FileKind::Text)
            PrintError(cmd + ": " + node->name + " is not a text file");
        else {
            size_t start = 0;
            while (start < node->content.size()) {
                size_t end = node->content.find('\n', start);
                if (end == std::string::npos) end = node->content.size();
                Print(node->content.substr(start, end - start));
                start = end + 1;
            }
        }
    }
    else if (cmd == "echo") {
        size_t redirect = args.size();
        bool   append   = false;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == ">" || args[i] == ">>") {
                redirect = i;
                append   = (args[i] == ">>");
                break;
            }
        }
        std::string text;
        for (size_t i = 1; i < redirect; ++i)
            text += (i > 1 ? " " : "") + args[i];

        if (redirect == args.size()) {
            Print(text);
            return;
        }
        if (redirect + 1 >= args.size()) {
            PrintError("echo: missing file name after " + args[redirect]);
            return;
        }

        std::string path = Absolute(args[redirect + 1]);
        FsNode*     node = fs.Resolve(path);
        if (node && node->isDir) {
            PrintError("echo: " + args[redirect + 1] + " is a folder");
        } else if (node) {
            if (!fs.WriteFile(node, (append ? node->content : "") + text + "\n"))
                PrintError("echo: " + fs.LastError());
        } else {
            FsNode* parent = fs.Resolve(VirtualFS::ParentOf(path));
            if (!parent)
                PrintError("echo: no such folder: " + VirtualFS::ParentOf(path));
            else if (!fs.NewFile(parent, VirtualFS::NameOf(path), text + "\n"))
                PrintError("echo: " + fs.LastError());
        }
    }
    else if (cmd == "cp" || cmd == "copy" || cmd == "mv" || cmd == "move") {
        MoveOrCopy(shell, args, cmd == "cp" || cmd == "copy");
    }
    else if (cmd == "ren" || cmd == "rename") {
        if (args.size() < 3) {
            PrintError("usage: " + cmd + " <path> <new name>");
            return;
        }
        FsNode* node = fs.Resolve(args[1], cwd);
        if (!node)
            PrintError(cmd + ": no such file or folder: " + args[1]);
        else if (!fs.Rename(node, args[2]))
            PrintError(cmd + ": " + fs.LastError());
    }
    else if (cmd == "rm" || cmd == "del" || cmd == "rmdir") {
        bool                     recursive = false;
        std::vector<std::string> targets;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "-r" || args[i] == "-rf" || ToLower(args[i]) == "/s")
                recursive = true;
            else
                targets.push_back(args[i]);
        }
        if (targets.empty()) {
            PrintError("usage: " + cmd + " [-r] <path>");
            return;
        }
        for (const std::string& target : targets) {
            FsNode* node = fs.Resolve(target, cwd);
            if (!node) {
                PrintError(cmd + ": no such file or folder: " + target);
            } else if (VirtualFS::IsInside(fs.Resolve(cwd), node)) {
                PrintError(cmd + ": can't delete the folder you are in");
            } else if (cmd == "rmdir" && !node->isDir) {
                PrintError("rmdir: not a folder: " + target);
            } else if (cmd == "rmdir" && !node->children.empty() && !recursive) {
                PrintError("rmdir: folder is not empty: " + target);
            } else if (cmd != "rmdir" && node->isDir && !recursive) {
                PrintError(cmd + ": " + target + " is a folder (use " + cmd + " -r)");
            } else if (!fs.Remove(node)) {
                PrintError(cmd + ": " + fs.LastError());
            }
        }
    }
    else if (cmd == "find") {
        if (args.size() < 2) {
            PrintError("usage: find <text>");
            return;
        }
        std::vector<FsNode*> results;
        fs.Find(fs.Root(), JoinArgs(args, 1), results);
        for (FsNode* node : results)
            Print(VirtualFS::PathOf(node) + (node->isDir ? "/" : ""), node->isDir ? COLOR_PATH : COLOR_TEXT);
        Print(std::to_string(results.size()) + " match(es)", COLOR_DIM);
    }
    else if (cmd == "edit") {
        if (args.size() < 2) {
            shell.OpenApp(AppID::Notepad);
            return;
        }
        std::string path = Absolute(args[1]);
        FsNode*     node = fs.Resolve(path);
        if (!node) {
            FsNode* parent = fs.Resolve(VirtualFS::ParentOf(path));
            node = parent ? fs.NewFile(parent, VirtualFS::NameOf(path)) : nullptr;
            if (!node) {
                PrintError("edit: " + (parent ? fs.LastError() : "no such folder: " + VirtualFS::ParentOf(path)));
                return;
            }
        }
        if (node->isDir)
            PrintError("edit: " + args[1] + " is a folder");
        else
            shell.OpenPath(VirtualFS::PathOf(node));
    }
    else if (cmd == "open" || cmd == "start") {
        if (args.size() < 2) {
            PrintError("usage: " + cmd + " <app|path>");
            return;
        }
        std::string target = JoinArgs(args, 1);
        if (FsNode* node = fs.Resolve(target, cwd))
            shell.OpenPath(VirtualFS::PathOf(node));
        else if (!shell.Launch(target))
            PrintError(cmd + ": can't find '" + target + "'");
    }
    else if (cmd == "ps") {
        std::vector<const Process*> sorted;
        for (const Process& p : shell.procs.Processes())
            sorted.push_back(&p);
        std::sort(sorted.begin(), sorted.end(), [](const Process* a, const Process* b) { return a->pid < b->pid; });

        char buf[160];
        snprintf(buf, sizeof(buf), "%6s  %-26s %-16s %6s %10s", "PID", "NAME", "USER", "CPU", "MEMORY");
        Print(buf, COLOR_ACCENT);
        for (const Process* p : sorted) {
            snprintf(buf, sizeof(buf), "%6d  %-26.26s %-16.16s %5.1f%% %7.1f MB",
                     p->pid, p->name.c_str(), p->user.c_str(), p->cpu, p->mem);
            Print(buf);
        }
        snprintf(buf, sizeof(buf), "%d processes | CPU %.0f%% | Memory %.1f / %.1f MB",
                 (int)sorted.size(), shell.procs.TotalCpu(), shell.procs.TotalMem(), ProcessManager::TOTAL_RAM_MB);
        Print(buf, COLOR_DIM);
    }
    else if (cmd == "kill") {
        if (args.size() < 2) {
            PrintError("usage: kill <pid>");
            return;
        }
        char* end = nullptr;
        long  pid = std::strtol(args[1].c_str(), &end, 10);
        if (end == args[1].c_str() || *end != '\0') {
            PrintError("kill: invalid PID: " + args[1]);
            return;
        }
        const Process* proc = shell.procs.Find((int)pid);
        std::string    name = proc ? proc->name : "";
        std::string    error;
        if (shell.EndProcess((int)pid, &error))
            Print("Ended process " + std::to_string(pid) + " (" + name + ").");
        else
            PrintError("kill: " + error);
    }
    else if (cmd == "initialize" || cmd == "scheduler-start" || cmd == "scheduler-stop") {
        std::string message;
        bool ok = (cmd == "initialize")      ? shell.SchedulerInit(message)
                : (cmd == "scheduler-start") ? shell.SchedulerStart(message)
                                             : shell.SchedulerStop(message);
        Print(message, ok ? COLOR_TEXT : COLOR_WARN);
    }
    else if (cmd == "date" || cmd == "time") {
        std::time_t now = std::time(nullptr);
        std::tm     lt;
#ifdef _WIN32
        localtime_s(&lt, &now);
#else
        localtime_r(&now, &lt);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%A, %B %d, %Y  %I:%M:%S %p", &lt);
        Print(buf);
    }
    else if (cmd == "whoami") {
        Print("csopesy-pc\\" + shell.settings.username);
    }
    else if (cmd == "ver") {
        Print("CSOPESY OS [Version 1.0.2026]");
    }
    else if (cmd == "sysinfo") {
        PrintSysInfo(shell);
    }
    else if (cmd == "history") {
        for (size_t i = 0; i < history.size(); ++i) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%4d  ", (int)i + 1);
            Print(buf + history[i]);
        }
    }
    else if (cmd == "exit") {
        shell.CloseApp(AppID::Terminal);
    }
    else {
        PrintError("'" + args[0] + "' is not recognized as a command. Type 'help' for a list.");
    }
}

void Terminal::ListFolder(Shell& shell, const std::vector<std::string>& args, bool detailed) {
    std::string target = ".";
    for (size_t i = 1; i < args.size(); ++i)
        if (args[i] != "-l" && args[i] != "-a")
            target = args[i];

    FsNode* node = shell.fs.Resolve(target, cwd);
    if (!node) {
        PrintError(args[0] + ": no such file or folder: " + target);
        return;
    }
    if (!node->isDir) {
        Print(node->name);
        return;
    }

    if (!detailed) {
        if (node->children.empty())
            Print("(empty)", COLOR_DIM);
        for (const auto& child : node->children) {
            ImU32 color = child->isDir ? COLOR_PATH
                        : VirtualFS::KindOf(child.get()) == FileKind::Program ? COLOR_PROMPT
                        : COLOR_TEXT;
            Print(child->name + (child->isDir ? "/" : ""), color);
        }
        return;
    }

    Print(" Folder of " + VirtualFS::PathOf(node), COLOR_DIM);
    Print("");
    unsigned long long files = 0, folders = 0, bytes = 0;
    for (const auto& child : node->children) {
        char        buf[200];
        std::string when = VirtualFS::FormatTime(child->modified);
        if (child->isDir) {
            snprintf(buf, sizeof(buf), "%s    <DIR>          %s", when.c_str(), child->name.c_str());
            ++folders;
        } else {
            unsigned long long size = VirtualFS::SizeOf(child.get());
            snprintf(buf, sizeof(buf), "%s    %14llu %s", when.c_str(), size, child->name.c_str());
            bytes += size;
            ++files;
        }
        Print(buf, child->isDir ? COLOR_PATH : COLOR_TEXT);
    }
    char summary[96];
    snprintf(summary, sizeof(summary), "%15llu File(s) %14llu bytes", files, bytes);
    Print(summary, COLOR_DIM);
    snprintf(summary, sizeof(summary), "%15llu Dir(s)", folders);
    Print(summary, COLOR_DIM);
}

void Terminal::PrintTree(const FsNode* node, const std::string& indent) {
    for (size_t i = 0; i < node->children.size(); ++i) {
        const FsNode* child = node->children[i].get();
        bool          last  = (i + 1 == node->children.size());
        Print(indent + (last ? "`-- " : "|-- ") + child->name + (child->isDir ? "/" : ""),
              child->isDir ? COLOR_PATH : COLOR_TEXT);
        if (child->isDir)
            PrintTree(child, indent + (last ? "    " : "|   "));
    }
}

void Terminal::MoveOrCopy(Shell& shell, const std::vector<std::string>& args, bool copy) {
    const std::string name = copy ? "cp" : "mv";
    if (args.size() < 3) {
        PrintError("usage: " + name + " <source> <destination>");
        return;
    }

    VirtualFS& fs  = shell.fs;
    FsNode*    src = fs.Resolve(args[1], cwd);
    if (!src) {
        PrintError(name + ": no such file or folder: " + args[1]);
        return;
    }

    // "mv a.txt Documents" moves into the folder; "mv a.txt b.txt" renames.
    std::string destPath = Absolute(args[2]);
    FsNode*     dest     = fs.Resolve(destPath);
    FsNode*     folder   = nullptr;
    std::string newName;
    if (dest && dest->isDir) {
        folder = dest;
    } else if (dest) {
        PrintError(name + ": '" + args[2] + "' already exists");
        return;
    } else {
        folder  = fs.Resolve(VirtualFS::ParentOf(destPath));
        newName = VirtualFS::NameOf(destPath);
        if (!folder || !folder->isDir) {
            PrintError(name + ": no such folder: " + VirtualFS::ParentOf(destPath));
            return;
        }
    }

    FsNode* result = copy ? fs.Copy(src, folder) : fs.Move(src, folder);
    if (!result) {
        PrintError(name + ": " + fs.LastError());
        return;
    }
    if (!newName.empty() && result->name != newName && !fs.Rename(result, newName)) {
        std::string error = fs.LastError();
        if (copy)
            fs.Remove(result);   // don't leave a half-finished copy behind
        PrintError(name + ": " + error);
    }
}

void Terminal::PrintSysInfo(Shell& shell) {
    static const char* LOGO[] = {
        R"(  ___  ___   ___   ___  ___  ___ __   __)",
        R"( / __|/ __| / _ \ | _ \| __|/ __|\ \ / /)",
        R"(| (__ \__ \| (_) ||  _/| _| \__ \ \ V / )",
        R"( \___||___/ \___/ |_|  |___||___/  |_|  )",
    };
    for (const char* row : LOGO)
        Print(row, COLOR_ACCENT);
    Print("");

    const ProcessManager& pm = shell.procs;
    char buf[128];
    Print("OS:         CSOPESY OS v1.0 (Build 2026)");
    Print("Host:       CSOPESY Megatrends Desktop");
    Print("BIOS:       BCN SIT 1989-1994 Special CSOPESY");
    Print("CPU:        Pentium III @ 733 MHz");
    snprintf(buf, sizeof(buf), "Memory:     %.1f MB / %.1f MB", pm.TotalMem(), ProcessManager::TOTAL_RAM_MB);
    Print(buf);
    Print("Disk:       420 MB WDCAC2420H");
    ImVec2 display = ImGui::GetIO().DisplaySize;
    snprintf(buf, sizeof(buf), "Display:    %d x %d", (int)display.x, (int)display.y);
    Print(buf);
    Print("Uptime:     " + ProcessManager::FormatUptime(pm.Uptime()));
    Print("Processes:  " + std::to_string(pm.Processes().size()));
    Print("User:       " + shell.settings.username);
}
