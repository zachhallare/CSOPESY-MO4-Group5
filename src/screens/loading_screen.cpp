#include "loading_screen.h"
#include <cfloat>
#include <cstring>

// "CSOPESY" in the figlet "small" font, as on the spec's splash screen.
static const char* LOGO[] = {
    R"(  ___  ___   ___   ___  ___  ___ __   __)",
    R"( / __|/ __| / _ \ | _ \| __|/ __|\ \ / /)",
    R"(| (__ \__ \| (_) ||  _/| _| \__ \ \ V / )",
    R"( \___||___/ \___/ |_|  |___||___/  |_|  )",
};

LoadingScreen::LoadingScreen(ImFont* monoFont, ImFont* largeFont, float duration)
    : duration(duration), monoFont(monoFont), largeFont(largeFont) {}

void LoadingScreen::TextCentered(const char* text) {
    float windowWidth = ImGui::GetWindowSize().x;
    float textWidth   = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::TextUnformatted(text);
}

void LoadingScreen::TextCenteredColored(const ImVec4& col, const char* text) {
    float windowWidth = ImGui::GetWindowSize().x;
    float textWidth   = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::TextColored(col, "%s", text);
}

ScreenID LoadingScreen::UpdateAndRender(float deltaTime) {
    timer    += deltaTime;
    dotTimer += deltaTime;

    if (timer >= duration) {
        return ScreenID::DESKTOP;
    }

    if (dotTimer >= 0.4f) {
        dotTimer -= 0.4f;
        dotCount = (dotCount + 1) % 4;
    }

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGui::PushFont(monoFont);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.08f, 1.0f));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(displaySize);

    ImGui::Begin("##Loading", nullptr,
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize  |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    ImGui::Dummy(ImVec2(0, displaySize.y * 0.22f));

    // ASCII-art logo, drawn from the large monospace font at a smaller size.
    ImFont*     logoFont = largeFont ? largeFont : monoFont;
    const float logoSize = 26.0f;
    ImDrawList* dl       = ImGui::GetWindowDrawList();
    float       y        = ImGui::GetCursorScreenPos().y;
    for (const char* row : LOGO) {
        ImVec2 size = logoFont->CalcTextSizeA(logoSize, FLT_MAX, 0.0f, row);
        float  x    = ImGui::GetWindowPos().x + (ImGui::GetWindowSize().x - size.x) * 0.5f;
        dl->AddText(logoFont, logoSize, ImVec2(x, y), IM_COL32(77, 191, 255, 255), row);
        y += size.y;
    }
    ImGui::Dummy(ImVec2(0, y - ImGui::GetCursorScreenPos().y));

    ImGui::Dummy(ImVec2(0, 12));

    TextCenteredColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f),
        "CSOPESY Operating System Emulator v1.0");
    TextCenteredColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Created By: Zach Halle, Marga Ramos, and Neil Tan");

    ImGui::Dummy(ImVec2(0, 24));

    TextCenteredColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f),
        "Part of Project Anito:");
    TextCenteredColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
        "Support for the PH Gaming Industry Towards Embracing");
    TextCenteredColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
        "In-House Game Engine Development");

    ImGui::Dummy(ImVec2(0, 36));

    char loadText[16] = "Loading";
    for (int i = 0; i < dotCount; ++i)
        strcat(loadText, ".");

    TextCenteredColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), loadText);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopFont();

    return ScreenID::NONE;
}
