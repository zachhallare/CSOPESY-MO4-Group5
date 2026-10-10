// The desktop shell. It owns the app windows and the services they share
// (file system, process table, textures, settings, notifications) and acts
// as the window manager: opening, focusing, minimizing and closing windows.
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "imgui.h"
#include "apps/app.h"
#include "core/config.h"
#include "core/fonts.h"
#include "core/texture_cache.h"
#include "os/process_manager.h"
#include "ui/wallpaper.h"

class VirtualFS;
class FileExplorer;
class Notepad;

enum class ToastKind { Info, Success, Warning, Error };

struct ShellSettings {
    std::string  wallpaper;                 // image path, or "builtin:<name>"
    WallpaperFit wallpaperFit     = WallpaperFit::Fill;
    bool         showDesktopIcons = true;
    bool         clock24h         = false;
    bool         clockSeconds     = false;
    int          dateFormat       = 0;      // 0 = "Thursday, Apr 30, 2026", 1 = "04/30/2026", 2 = "2026-04-30"
    int          volume           = 60;
    bool         muted            = false;
    bool         wifiEnabled      = true;
    bool         airplaneMode     = false;
    std::string  network          = "CSOPESY-Lab";   // connected Wi-Fi, empty = none
    std::string  username;
};

class Shell {
public:
    static constexpr float TASKBAR_HEIGHT = 52.0f;
    static constexpr float CLOCK_STRIP    = 48.0f;   // top band kept clear so windows never hide the clock

    Shell(VirtualFS& fs, const Config& config, const Fonts& fonts);
    ~Shell();

    VirtualFS&     fs;
    Fonts          fonts;
    ShellSettings  settings;
    ProcessManager procs;
    TextureCache   textures;

    // --- Window management ---
    void OpenApp(AppID id);                      // launches, or restores and focuses
    void CloseApp(AppID id, bool force = false); // force skips the "unsaved changes" prompt
    void TaskbarClick(AppID id);                 // restore / focus / minimize, like Windows
    bool IsOpen(AppID id) const;
    bool IsActive(AppID id) const;               // open, visible and focused
    App* GetApp(AppID id) const;
    void ClearFocus();

    FileExplorer& Explorer() const;
    Notepad&      NotepadApp() const;

    // Opens a path with the right app: folders in Explorer, text in Notepad...
    bool OpenPath(const std::string& path);
    // Launches an app by name, exe or alias ("notepad", "taskmgr.exe");
    // falls back to OpenPath. Returns false when nothing matched.
    bool Launch(const std::string& command);
    // Ends a process; closes its window if it owns one.
    bool EndProcess(int pid, std::string* error);
    bool HasUnsavedWork() const;

    // Mock scheduler behind the taskbar INIT / START / STOP buttons.
    bool SchedulerInit(std::string& message);
    bool SchedulerStart(std::string& message);
    bool SchedulerStop(std::string& message);

    bool        SetWallpaper(const std::string& wallpaper);
    std::string WallpaperName() const;
    std::string ClockText() const;   // current date and time, formatted per settings
    void        Notify(const std::string& title, const std::string& message, ToastKind kind = ToastKind::Info);

    // The desktop shows the shut down / restart dialog when this is requested.
    void RequestPowerDialog(int choice = 0) { powerDialogRequested = true; powerChoice = choice; }
    bool ConsumePowerDialogRequest(int* choice);

    // Called by the desktop every frame.
    void   Update(float deltaTime);
    void   RenderWindows();
    void   RenderNotifications();
    ImVec2 WorkAreaSize() const;   // the display minus the taskbar

private:
    struct Toast {
        std::string title;
        std::string message;
        ToastKind   kind;
        float       age = 0.0f;
    };

    std::vector<std::unique_ptr<App>> apps;   // indexed by AppID
    std::vector<Toast> toasts;
    AppID lastFocused = AppID::Count;         // Count = no app window focused
    int   cascade     = 0;
    bool  powerDialogRequested = false;
    int   powerChoice = 0;

    bool LaunchApp(const std::string& name);  // apps only, no file fallback
};
