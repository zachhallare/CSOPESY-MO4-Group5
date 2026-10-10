// Settings: personalization (wallpaper), sound, network, date & time, and a
// summary of the system.
#pragma once

#include "apps/app.h"

class SettingsApp : public App {
public:
    enum Page { PAGE_SYSTEM, PAGE_PERSONALIZATION, PAGE_SOUND, PAGE_NETWORK, PAGE_DATETIME, PAGE_COUNT };

    SettingsApp() : App(AppID::Settings) {}

    ImVec2 DefaultSize() const override { return ImVec2(820, 540); }
    ImVec2 MinSize() const override     { return ImVec2(620, 420); }
    void   Render(Shell& shell) override;

    void ShowPage(Page p) { page = p; }

private:
    int  page            = PAGE_SYSTEM;
    int  outputDevice    = 0;
    char customPath[256] = "";

    void RenderSystem(Shell& shell);
    void RenderPersonalization(Shell& shell);
    void RenderSound(Shell& shell);
    void RenderNetwork(Shell& shell);
    void RenderDateTime(Shell& shell);
};
