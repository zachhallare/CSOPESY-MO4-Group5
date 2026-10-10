// Terminal: a command line for the virtual file system, the process table
// and the mock scheduler (type 'help' for the list of commands).
#pragma once

#include <string>
#include <vector>
#include "apps/app.h"

struct FsNode;

class Terminal : public App {
public:
    Terminal() : App(AppID::Terminal) {}

    std::string Title() const override;
    ImVec2      DefaultSize() const override { return ImVec2(740, 440); }
    void        OnOpen(Shell& shell) override;
    void        OnClose(Shell& shell) override;
    void        Render(Shell& shell) override;

private:
    struct Line {
        std::string prompt;   // echoed prompt, drawn in green before the text
        std::string text;
        ImU32       color;
    };

    std::vector<Line>        lines;
    std::vector<std::string> history;
    int         historyPos  = -1;
    char        input[512]  = "";
    std::string cwd         = "/";
    bool        scrollToEnd = false;
    bool        focusInput  = true;
    Shell*      activeShell = nullptr;   // for the input callback (tab completion)

    void        Print(const std::string& text, ImU32 color = 0);
    void        PrintError(const std::string& text);
    std::string Prompt(const Shell& shell) const;
    std::string Absolute(const std::string& path) const;

    void Execute(Shell& shell, const std::string& commandLine);
    void ListFolder(Shell& shell, const std::vector<std::string>& args, bool detailed);
    void PrintTree(const FsNode* node, const std::string& indent);
    void MoveOrCopy(Shell& shell, const std::vector<std::string>& args, bool copy);
    void PrintSysInfo(Shell& shell);
    void Complete(ImGuiInputTextCallbackData* data);

    static int InputCallback(ImGuiInputTextCallbackData* data);
};
