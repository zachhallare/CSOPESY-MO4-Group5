// Shown after PWR is confirmed: "Shutting down..." (or "Restarting...")
// before the emulator closes itself or boots back into the BIOS.
#pragma once

#include "core/fonts.h"
#include "core/iscreen.h"
#include "imgui.h"

class ShutdownScreen : public IScreen {
public:
    ShutdownScreen(const Fonts& fonts, bool restart);
    ScreenID UpdateAndRender(float deltaTime) override;

private:
    Fonts fonts;
    bool  restart;
    float timer = 0.0f;

    static constexpr float SPIN_DURATION    = 2.2f;   // "Shutting down..." with a spinner
    static constexpr float GOODBYE_DURATION = 1.2f;   // final message before exiting
};
