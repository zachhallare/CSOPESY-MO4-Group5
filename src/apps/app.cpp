#include "app.h"

static const AppInfo APPS[APP_COUNT] = {
    // name                  exe             aliases                                           icon               pinned cpu   mem
    { "File Explorer",      "explorer.exe", "explorer files file-explorer this-pc",           Icon::Explorer,    true,  0.6f, 4.2f },
    { "Terminal",           "terminal.exe", "terminal cmd console shell",                     Icon::Terminal,    true,  0.3f, 2.1f },
    { "Notepad",            "notepad.exe",  "notepad editor text",                            Icon::Notepad,     true,  0.2f, 1.8f },
    { "Task Manager",       "taskmgr.exe",  "taskmgr task-manager tasks",                     Icon::TaskManager, true,  1.4f, 2.6f },
    { "Settings",           "settings.exe", "settings control-panel personalize",             Icon::Settings,    true,  0.4f, 3.1f },
    { "System Information", "sysinfo.exe",  "sysinfo msinfo about winver system-information", Icon::SystemInfo,  false, 0.1f, 1.4f },
};

const AppInfo& GetAppInfo(AppID id) {
    return APPS[(int)id];
}
