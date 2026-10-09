// Handles the desktop layer: wallpaper background and clock overlay.
#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif
#include <GL/gl.h>
#include "imgui.h"
#include "core/iscreen.h"
#include "ui/taskbar.h"

class DesktopScreen : public IScreen {
public:
    DesktopScreen(ImFont* uiFont, const char* wallpaperPath);
    ScreenID UpdateAndRender(float deltaTime) override;

private:
    ImFont* uiFont;
    Taskbar taskbar;

    GLuint wallpaperTex = 0;
    int    wallpaperW   = 0;
    int    wallpaperH   = 0;
    bool   wallpaperOk  = false;

    void RenderWallpaper();
    void RenderClock();
    void RenderVersionLabel();
    void RenderSysInfoWindow(bool* open);
};
