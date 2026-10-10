// The BIOS boot screen that prints hardware information like a classic PC.
#pragma once

#include "core/iscreen.h"
#include "imgui.h"

class BiosScreen : public IScreen {
public:
    BiosScreen(ImFont* font, float duration);
    ScreenID UpdateAndRender(float deltaTime) override;

private:
    float   timer = 0.0f;
    float   duration;
    ImFont* monoFont;
};
