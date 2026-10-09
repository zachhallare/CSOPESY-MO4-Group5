// Taskbar shown at the bottom of the screen.
// Provides basic system buttons and a working power button to exit.
#pragma once

#include "imgui.h"

class Taskbar {
public:
    void Init();

    // Renders the taskbar and returns true if the user clicked the power button.
    bool Render();

    // Simple flags to let the main loop know which windows should be shown.
    bool showSysInfo = false;

private:
    static constexpr float BAR_HEIGHT = 44.0f;
};
