// The splash screen shown while the OS is "loading".
#pragma once

#include "core/iscreen.h"
#include "imgui.h"

class LoadingScreen : public IScreen {
public:
    LoadingScreen(ImFont* monoFont, ImFont* largeFont, float duration);
    ScreenID UpdateAndRender(float deltaTime) override;

private:
    float timer    = 0.0f;
    float dotTimer = 0.0f;
    int   dotCount = 0;
    float duration;

    ImFont* monoFont;
    ImFont* largeFont;

    static void TextCentered(const char* text);
    static void TextCenteredColored(const ImVec4& col, const char* text);
};
