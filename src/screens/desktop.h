// The desktop: wallpaper, desktop icons, clock and the app windows managed
// by the shell, with the taskbar on top. Also hosts the shutdown dialog.
#pragma once

#include <string>
#include "imgui.h"
#include "core/config.h"
#include "core/fonts.h"
#include "core/iscreen.h"
#include "os/shell.h"
#include "ui/taskbar.h"

class VirtualFS;

class DesktopScreen : public IScreen {
public:
    DesktopScreen(const Fonts& fonts, const Config& config, VirtualFS& fs);
    ScreenID UpdateAndRender(float deltaTime) override;
    void     OnCloseRequested() override;   // the window's X behaves like PWR

private:
    Fonts   fonts;
    Shell   shell;
    Taskbar taskbar;

    std::string selectedIcon;   // key of the selected desktop icon
    int         powerChoice = 0;

    std::string renameTarget;   // desktop rename dialog
    char        renameBuf[128] = "";
    std::string renameError;
    bool        openRename = false;

    void     RenderWallpaper();
    void     RenderVersionLabel();
    void     RenderDesktopIcons();
    void     RenderClock();
    void     RenderRenameDialog();
    ScreenID RenderPowerDialog();
    void     CreateOnDesktop(bool folder);
};
