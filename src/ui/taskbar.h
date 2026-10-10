// Taskbar pinned to the bottom of the screen: start menu and search, the
// scheduler buttons (INIT / START / STOP), pinned and running apps, and the
// system tray (VOL, NET and the PWR shutdown button).
#pragma once

class Shell;

class Taskbar {
public:
    void Render(Shell& shell);

private:
    char searchBuf[128] = "";

    void RenderStartMenu(Shell& shell);
    void RenderSearch(Shell& shell);
    void RenderSchedulerButtons(Shell& shell);
    void RenderAppButtons(Shell& shell);
    void RenderTray(Shell& shell);
};
