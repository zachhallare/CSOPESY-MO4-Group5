// Base class for every windowed application, plus the registry that
// describes each one (name, executable, icon, dummy resource usage).
#pragma once

#include <string>
#include "imgui.h"
#include "ui/icons.h"

class Shell;

enum class AppID { FileExplorer, Terminal, Notepad, TaskManager, Settings, SystemInfo, Count };
constexpr int APP_COUNT = (int)AppID::Count;

struct AppInfo {
    const char* name;      // shown in title bars, menus and the taskbar
    const char* exe;       // executable in /Programs (and the Task Manager)
    const char* aliases;   // extra names Launch() accepts, space separated
    Icon        icon;
    bool        pinned;    // always shown on the taskbar
    float       cpu;       // dummy usage reported to the Task Manager
    float       memMB;
};

const AppInfo& GetAppInfo(AppID id);

class App {
public:
    explicit App(AppID id) : id(id) {}
    virtual ~App() = default;

    AppID          Id() const   { return id; }
    const AppInfo& Info() const { return GetAppInfo(id); }

    // Title bar text (e.g. "notes.txt - Notepad").
    virtual std::string      Title() const { return Info().name; }
    virtual ImVec2           DefaultSize() const = 0;
    virtual ImVec2           MinSize() const { return ImVec2(360, 240); }
    virtual ImGuiWindowFlags WindowFlags() const { return 0; }

    virtual void OnOpen(Shell&) {}
    virtual void Render(Shell& shell) = 0;
    // Return false to keep the window open (e.g. to ask about unsaved changes).
    virtual bool OnCloseRequest(Shell&) { return true; }
    virtual void OnClose(Shell&) {}

    // Window state, managed by the Shell.
    bool open      = false;
    bool minimized = false;
    bool wantFocus = false;
    bool placeNext = false;   // position and size the window on its next frame

private:
    AppID id;
};
