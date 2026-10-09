#include "desktop.h"
#include "core/texture_loader.h"

#include <ctime>
#include <cstdio>
#include <cstdint>

DesktopScreen::DesktopScreen(ImFont* uiFont, const char* wallpaperPath)
    : uiFont(uiFont)
{
    taskbar.Init();
    wallpaperOk = LoadTextureFromFile(wallpaperPath, &wallpaperTex, &wallpaperW, &wallpaperH);
    if (!wallpaperOk) {
        fprintf(stderr, "Could not load wallpaper: %s\n", wallpaperPath);
    }
}

ScreenID DesktopScreen::UpdateAndRender(float deltaTime) {
    ImGui::PushFont(uiFont);

    RenderWallpaper();
    RenderClock();
    RenderVersionLabel();
    RenderSysInfoWindow(&taskbar.showSysInfo);

    if (taskbar.Render()) {
        ImGui::PopFont();
        return ScreenID::EXIT;
    }

    ImGui::PopFont();
    return ScreenID::NONE;
}

void DesktopScreen::RenderWallpaper() {
    if (!wallpaperOk) return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddImage(
        (ImTextureID)(intptr_t)wallpaperTex,
        ImVec2(0, 0),
        displaySize);
}

void DesktopScreen::RenderClock() {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    std::time_t now = std::time(nullptr);
    std::tm     lt;
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%A, %b %d, %Y | %I:%M %p", &lt);

    ImVec2 textSize = ImGui::CalcTextSize(timeBuf);
    float padX = 12.0f, padY = 6.0f;
    float margin = 10.0f;

    ImVec2 boxMin = { displaySize.x - textSize.x - padX * 2 - margin, margin };
    ImVec2 boxMax = { displaySize.x - margin, margin + textSize.y + padY * 2 };

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 160), 6.0f);
    fg->AddText(
        ImVec2(boxMin.x + padX, boxMin.y + padY),
        IM_COL32(255, 255, 255, 240),
        timeBuf);
}

void DesktopScreen::RenderVersionLabel() {
    const char* label = "CSOPESY OS v1.0 - Group 5";
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 textSize    = ImGui::CalcTextSize(label);
    float  margin      = 10.0f;

    ImGui::GetForegroundDrawList()->AddText(
        ImVec2(margin, displaySize.y - 44 - textSize.y - margin),
        IM_COL32(255, 255, 255, 120),
        label);
}

void DesktopScreen::RenderSysInfoWindow(bool* open) {
    if (!*open) return;

    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("System Information", open);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CSOPESY OS v1.0");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("CPU:       Pentium III");
    ImGui::Text("RAM:       64 000 KB");
    ImGui::Text("Disk:      420 MB WDCAC2420H");
    ImGui::Text("Optical:   CD-ROM LTN-305A");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped(
        "This is a placeholder System Information window. "
        "It demonstrates that draggable, resizable ImGui windows "
        "work on top of the desktop wallpaper.");

    ImGui::End();
}
