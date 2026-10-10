#include "shell.h"
#include "apps/file_explorer.h"
#include "apps/notepad.h"
#include "apps/settings_app.h"
#include "apps/system_info.h"
#include "apps/task_manager.h"
#include "apps/terminal.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <sstream>

static constexpr float TOAST_LIFETIME = 4.5f;
static constexpr int   MAX_TOASTS     = 5;

Shell::Shell(VirtualFS& fs, const Config& config, const Fonts& fonts)
    : fs(fs), fonts(fonts), procs(config.username, config.processSpawnInterval)
{
    settings.username     = config.username;
    settings.clock24h     = config.clock24h;
    settings.clockSeconds = config.clockSeconds;

    apps.resize(APP_COUNT);
    apps[(int)AppID::FileExplorer] = std::make_unique<FileExplorer>();
    apps[(int)AppID::Terminal]     = std::make_unique<Terminal>();
    apps[(int)AppID::Notepad]      = std::make_unique<Notepad>();
    apps[(int)AppID::TaskManager]  = std::make_unique<TaskManager>();
    apps[(int)AppID::Settings]     = std::make_unique<SettingsApp>();
    apps[(int)AppID::SystemInfo]   = std::make_unique<SystemInfo>();

    settings.wallpaper = "builtin:aurora";
    SetWallpaper(config.wallpaper);
}

Shell::~Shell() = default;

// ---------------------------------------------------------------------------
// Window management
// ---------------------------------------------------------------------------

App* Shell::GetApp(AppID id) const {
    return apps[(int)id].get();
}

FileExplorer& Shell::Explorer() const {
    return *static_cast<FileExplorer*>(GetApp(AppID::FileExplorer));
}

Notepad& Shell::NotepadApp() const {
    return *static_cast<Notepad*>(GetApp(AppID::Notepad));
}

bool Shell::IsOpen(AppID id) const {
    return GetApp(id)->open;
}

bool Shell::IsActive(AppID id) const {
    const App* app = GetApp(id);
    return app->open && !app->minimized && lastFocused == id;
}

void Shell::ClearFocus() {
    lastFocused = AppID::Count;
}

void Shell::OpenApp(AppID id) {
    App* app = GetApp(id);
    if (!app->open) {
        const AppInfo& info = app->Info();
        app->open      = true;
        app->placeNext = true;
        procs.AddAppProcess((int)id, info.name, info.exe, info.cpu, info.memMB);
        app->OnOpen(*this);
    }
    app->minimized = false;
    app->wantFocus = true;
}

void Shell::CloseApp(AppID id, bool force) {
    App* app = GetApp(id);
    if (!app->open)
        return;
    if (!force && !app->OnCloseRequest(*this)) {
        app->minimized = false;
        app->wantFocus = true;
        return;
    }
    app->open      = false;
    app->minimized = false;
    procs.RemoveAppProcess((int)id);
    app->OnClose(*this);
    if (lastFocused == id)
        lastFocused = AppID::Count;
}

void Shell::TaskbarClick(AppID id) {
    App* app = GetApp(id);
    if (!app->open) {
        OpenApp(id);
    } else if (app->minimized) {
        app->minimized = false;
        app->wantFocus = true;
    } else if (lastFocused == id) {
        app->minimized = true;
        lastFocused    = AppID::Count;
    } else {
        app->wantFocus = true;
    }
}

ImVec2 Shell::WorkAreaSize() const {
    ImVec2 display = ImGui::GetIO().DisplaySize;
    return ImVec2(display.x, std::max(100.0f, display.y - TASKBAR_HEIGHT));
}

void Shell::RenderWindows() {
    const ImVec2 work = WorkAreaSize();

    for (auto& appPtr : apps) {
        App& app = *appPtr;
        if (!app.open || app.minimized)
            continue;

        if (app.placeNext) {
            // New windows open near the center, cascading so they don't stack exactly.
            ImVec2 size = app.DefaultSize();
            size.x = std::min(size.x, work.x - 20.0f);
            size.y = std::min(size.y, work.y - CLOCK_STRIP - 10.0f);
            float  offset = 28.0f * (float)(cascade++ % 6);
            ImVec2 pos((work.x - size.x) * 0.5f - 70.0f + offset,
                       (work.y - size.y) * 0.5f - 30.0f + offset);
            pos.x = std::clamp(pos.x, 10.0f, std::max(10.0f, work.x - size.x - 10.0f));
            pos.y = std::clamp(pos.y, CLOCK_STRIP, std::max(CLOCK_STRIP, work.y - size.y - 6.0f));
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            app.placeNext = false;
        }
        ImGui::SetNextWindowSizeConstraints(app.MinSize(), ImVec2(FLT_MAX, FLT_MAX));
        if (app.wantFocus) {
            ImGui::SetNextWindowFocus();
            app.wantFocus = false;
        }

        // "###exe" keeps the window identity stable while the visible title changes.
        std::string title    = app.Title() + "###" + app.Info().exe;
        bool        keepOpen = true;
        if (ImGui::Begin(title.c_str(), &keepOpen, ImGuiWindowFlags_NoCollapse | app.WindowFlags())) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                lastFocused = app.Id();

            // Keep the title bar reachable and the clock visible: never under
            // the clock strip, the taskbar, or off-screen.
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 clamped(std::clamp(pos.x, 80.0f - ImGui::GetWindowWidth(), std::max(0.0f, work.x - 80.0f)),
                           std::clamp(pos.y, CLOCK_STRIP, std::max(CLOCK_STRIP, work.y - 30.0f)));
            if (clamped.x != pos.x || clamped.y != pos.y)
                ImGui::SetWindowPos(clamped);

            app.Render(*this);
        }
        ImGui::End();

        if (!keepOpen)
            CloseApp(app.Id());
    }
}

// ---------------------------------------------------------------------------
// Launching
// ---------------------------------------------------------------------------

static std::string StripExe(std::string name) {
    name = ToLower(Trim(name));
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".exe") == 0)
        name.resize(name.size() - 4);
    return name;
}

bool Shell::LaunchApp(const std::string& name) {
    std::string wanted = StripExe(name);
    if (wanted.empty())
        return false;

    for (int i = 0; i < APP_COUNT; ++i) {
        const AppInfo& info  = GetAppInfo((AppID)i);
        bool           match = wanted == StripExe(info.exe) || wanted == ToLower(info.name);

        std::istringstream aliases(info.aliases);
        std::string        alias;
        while (!match && aliases >> alias)
            match = (wanted == alias);

        if (match) {
            OpenApp((AppID)i);
            return true;
        }
    }
    return false;
}

bool Shell::Launch(const std::string& command) {
    std::string cmd = Trim(command);
    if (cmd.empty())
        return false;
    if (LaunchApp(cmd))
        return true;
    if (fs.Resolve(cmd))
        return OpenPath(cmd);
    return false;
}

bool Shell::OpenPath(const std::string& path) {
    FsNode* node = fs.Resolve(path);
    if (!node) {
        Notify("Can't open", "'" + path + "' doesn't exist.", ToastKind::Error);
        return false;
    }

    std::string full = VirtualFS::PathOf(node);
    switch (VirtualFS::KindOf(node)) {
    case FileKind::Folder:
        Explorer().Navigate(full);
        OpenApp(AppID::FileExplorer);
        return true;

    case FileKind::Text:
        NotepadApp().OpenFile(*this, full);
        OpenApp(AppID::Notepad);
        return true;

    case FileKind::Image:
        // Explorer's preview pane shows the picture (and can set it as wallpaper).
        Explorer().Reveal(full);
        OpenApp(AppID::FileExplorer);
        return true;

    case FileKind::Program:
        if (LaunchApp(node->name))
            return true;
        Notify("Can't open", "'" + node->name + "' is not a valid CSOPESY application.", ToastKind::Error);
        return false;

    case FileKind::Binary:
        Notify("Can't open", "'" + node->name + "' is a binary system file.", ToastKind::Warning);
        return false;
    }
    return false;
}

bool Shell::EndProcess(int pid, std::string* error) {
    int appId = -1;
    if (!procs.Kill(pid, &appId, error))
        return false;
    if (appId >= 0)
        CloseApp((AppID)appId, true);
    return true;
}

bool Shell::HasUnsavedWork() const {
    return IsOpen(AppID::Notepad) && NotepadApp().HasUnsavedChanges();
}

// ---------------------------------------------------------------------------
// Scheduler (taskbar INIT / START / STOP)
// ---------------------------------------------------------------------------

bool Shell::SchedulerInit(std::string& message) {
    if (!procs.Initialize()) {
        message = "The scheduler is already initialized.";
        return false;
    }
    message = "Scheduler initialized. Press START to begin generating processes.";
    return true;
}

bool Shell::SchedulerStart(std::string& message) {
    if (procs.State() == SchedulerState::Uninitialized) {
        message = "Initialize the scheduler first (INIT).";
        return false;
    }
    if (!procs.StartScheduler()) {
        message = "The scheduler is already running.";
        return false;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "Scheduler started. A new process is generated every %.1f s.",
             procs.SpawnInterval());
    message = buf;
    return true;
}

bool Shell::SchedulerStop(std::string& message) {
    if (!procs.StopScheduler()) {
        message = (procs.State() == SchedulerState::Uninitialized)
                ? "Initialize the scheduler first (INIT)."
                : "The scheduler is not running.";
        return false;
    }
    message = "Scheduler stopped after generating " + std::to_string(procs.JobsSpawned()) +
              " processes. Running ones will finish on their own.";
    return true;
}

// ---------------------------------------------------------------------------
// Settings, notifications, power
// ---------------------------------------------------------------------------

bool Shell::SetWallpaper(const std::string& wallpaper) {
    std::string value = Trim(wallpaper);
    if (IsBuiltinWallpaper(value)) {
        for (const BuiltinWallpaper& builtin : BuiltinWallpapers()) {
            if (value == builtin.id) {
                settings.wallpaper = value;
                return true;
            }
        }
        Notify("Wallpaper", "Unknown built-in wallpaper '" + value + "'.", ToastKind::Error);
        return false;
    }

    std::string path = CanonicalPath(value);
    if (value.empty() || !textures.Get(path).ok) {
        Notify("Wallpaper", "Couldn't load '" + value + "'. Keeping the current background.",
               ToastKind::Error);
        return false;
    }
    settings.wallpaper = path;
    return true;
}

std::string Shell::WallpaperName() const {
    for (const BuiltinWallpaper& builtin : BuiltinWallpapers())
        if (settings.wallpaper == builtin.id)
            return builtin.name;
    return VirtualFS::NameOf(settings.wallpaper);
}

std::string Shell::ClockText() const {
    std::time_t now = std::time(nullptr);
    std::tm     lt;
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    static const char* DATE_FORMATS[] = { "%A, %b %d, %Y", "%m/%d/%Y", "%Y-%m-%d" };
    const char* timeFormat = settings.clock24h
        ? (settings.clockSeconds ? "%H:%M:%S" : "%H:%M")
        : (settings.clockSeconds ? "%I:%M:%S %p" : "%I:%M %p");
    std::string format = std::string(DATE_FORMATS[std::clamp(settings.dateFormat, 0, 2)]) + " | " + timeFormat;

    char buf[96];
    std::strftime(buf, sizeof(buf), format.c_str(), &lt);
    return buf;
}

void Shell::Notify(const std::string& title, const std::string& message, ToastKind kind) {
    toasts.push_back({ title, message, kind, 0.0f });
    if ((int)toasts.size() > MAX_TOASTS)
        toasts.erase(toasts.begin());
}

bool Shell::ConsumePowerDialogRequest(int* choice) {
    if (!powerDialogRequested)
        return false;
    powerDialogRequested = false;
    if (choice) *choice = powerChoice;
    return true;
}

void Shell::Update(float deltaTime) {
    procs.networkOnline = settings.wifiEnabled && !settings.airplaneMode && !settings.network.empty();
    procs.Update(deltaTime);

    // Notifications wait while a menu or dialog is open (they would cover the
    // flyouts), then show for their full time once it closes.
    if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        for (Toast& toast : toasts)
            toast.age += deltaTime;
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
                     [](const Toast& t) { return t.age >= TOAST_LIFETIME; }),
                 toasts.end());
}

void Shell::RenderNotifications() {
    if (toasts.empty() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;

    ImDrawList* fg       = ImGui::GetForegroundDrawList();
    ImFont*     font     = ImGui::GetFont();
    float       fontSize = ImGui::GetFontSize();
    ImVec2      display  = ImGui::GetIO().DisplaySize;

    const float width = 320.0f, pad = 12.0f;
    const float textW = width - pad * 2.0f - 6.0f;
    float       y     = display.y - TASKBAR_HEIGHT - 10.0f;   // stack upward from above the tray

    // Newest at the bottom, closest to the taskbar, like Windows.
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        const Toast& toast = *it;
        float alpha = std::clamp(std::min(toast.age / 0.2f, (TOAST_LIFETIME - toast.age) / 0.5f), 0.0f, 1.0f);
        auto  A     = [alpha](int a) { return (int)(a * alpha); };

        ImVec2 msgSize = font->CalcTextSizeA(fontSize, FLT_MAX, textW, toast.message.c_str());
        float  height  = pad + fontSize + 4.0f + msgSize.y + pad;
        y -= height;
        ImVec2 min(display.x - width - 10.0f, y);
        ImVec2 max(display.x - 10.0f, y + height);

        ImU32 accent;
        switch (toast.kind) {
        case ToastKind::Success: accent = IM_COL32(80, 200, 120, A(255)); break;
        case ToastKind::Warning: accent = IM_COL32(240, 180, 60, A(255)); break;
        case ToastKind::Error:   accent = IM_COL32(235, 80, 80, A(255));  break;
        default:                 accent = IM_COL32(70, 150, 255, A(255)); break;
        }

        fg->AddRectFilled(min, max, IM_COL32(28, 30, 38, A(240)), 8.0f);
        fg->AddRect(min, max, IM_COL32(255, 255, 255, A(30)), 8.0f);
        fg->AddRectFilled(min, ImVec2(min.x + 5.0f, max.y), accent, 8.0f, ImDrawFlags_RoundCornersLeft);
        fg->AddText(font, fontSize, ImVec2(min.x + pad + 6.0f, min.y + pad),
                    IM_COL32(255, 255, 255, A(255)), toast.title.c_str());
        fg->AddText(font, fontSize, ImVec2(min.x + pad + 6.0f, min.y + pad + fontSize + 4.0f),
                    IM_COL32(200, 205, 215, A(255)), toast.message.c_str(), nullptr, textW);

        y -= 8.0f;
    }
}
