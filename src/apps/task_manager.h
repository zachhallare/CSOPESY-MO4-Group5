// Task Manager modeled after the Windows one: a Processes tab with CPU,
// memory, disk and network usage, a Performance tab with live graphs, and a
// Details tab. All values come from the dummy ProcessManager.
#pragma once

#include <string>
#include "apps/app.h"

struct Process;

class TaskManager : public App {
public:
    TaskManager() : App(AppID::TaskManager) {}

    ImVec2           DefaultSize() const override { return ImVec2(780, 520); }
    ImVec2           MinSize() const override     { return ImVec2(560, 380); }
    ImGuiWindowFlags WindowFlags() const override { return ImGuiWindowFlags_MenuBar; }
    void             Render(Shell& shell) override;

private:
    int         selectedPid  = -1;
    int         perfView     = 0;      // 0 CPU, 1 Memory, 2 Disk, 3 Network
    int         pendingKill  = -1;
    bool        openRunDialog = false;
    char        runBuf[128]  = "";
    std::string runError;

    void RenderMenuBar(Shell& shell);
    void RenderProcessesTab(Shell& shell);
    void RenderPerformanceTab(Shell& shell);
    void RenderDetailsTab(Shell& shell);
    void RenderFooter(Shell& shell);
    void RenderRunDialog(Shell& shell);
    void ProcessContextMenu(Shell& shell, const Process& p);
};
