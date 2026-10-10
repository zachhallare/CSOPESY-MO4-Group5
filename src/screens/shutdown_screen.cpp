#include "shutdown_screen.h"

#include <cfloat>
#include <cmath>

ShutdownScreen::ShutdownScreen(const Fonts& fonts, bool restart)
    : fonts(fonts), restart(restart) {}

static void CenteredText(ImDrawList* dl, ImFont* font, float y, ImU32 color, const char* text) {
    ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
    dl->AddText(font, font->FontSize, ImVec2((ImGui::GetIO().DisplaySize.x - size.x) * 0.5f, y), color, text);
}

ScreenID ShutdownScreen::UpdateAndRender(float deltaTime) {
    timer += deltaTime;
    if (restart && timer >= SPIN_DURATION)
        return ScreenID::BOOT_BIOS;
    if (!restart && timer >= SPIN_DURATION + GOODBYE_DURATION)
        return ScreenID::EXIT;

    ImVec2      display = ImGui::GetIO().DisplaySize;
    ImDrawList* dl      = ImGui::GetBackgroundDrawList();
    dl->AddRectFilledMultiColor(ImVec2(0, 0), display,
        IM_COL32(8, 20, 48, 255), IM_COL32(8, 20, 48, 255),
        IM_COL32(4, 8, 20, 255),  IM_COL32(4, 8, 20, 255));

    const float centerY = display.y * 0.45f;
    if (timer < SPIN_DURATION) {
        // Spinner: a ring of dots with one bright dot chasing around.
        const int   DOTS   = 10;
        const float radius = 22.0f;
        ImVec2      center(display.x * 0.5f, centerY - 30.0f);
        float       head   = std::fmod(timer * 1.6f, 1.0f) * DOTS;
        for (int i = 0; i < DOTS; ++i) {
            float angle = (float)i / DOTS * 6.2831853f - 1.5707963f;
            float age   = std::fmod(head - (float)i + DOTS, (float)DOTS);   // 0 = brightest
            int   alpha = (int)(255 * (1.0f - age / DOTS));
            dl->AddCircleFilled(ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius),
                                3.5f, IM_COL32(255, 255, 255, alpha), 12);
        }
        CenteredText(dl, fonts.uiLarge, centerY + 20.0f, IM_COL32(235, 240, 250, 255),
                     restart ? "Restarting..." : "Shutting down...");
        CenteredText(dl, fonts.ui, centerY + 56.0f, IM_COL32(150, 160, 180, 255),
                     "Closing apps and saving settings");
    } else {
        CenteredText(dl, fonts.uiLarge, centerY, IM_COL32(255, 170, 60, 255),
                     "It's now safe to turn off your computer.");
        CenteredText(dl, fonts.ui, centerY + 40.0f, IM_COL32(150, 160, 180, 255),
                     "Thank you for using CSOPESY OS.");
    }
    return ScreenID::NONE;
}
