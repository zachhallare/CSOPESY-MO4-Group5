#include "loading_screen.h"
#include <cstring>

LoadingScreen::LoadingScreen(ImFont* monoFont, ImFont* largeFont)
    : monoFont(monoFont), largeFont(largeFont) {}

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

    if (timer >= DURATION) {
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

    if (largeFont) ImGui::PushFont(largeFont);
    TextCenteredColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "C S O P E S Y");
    if (largeFont) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 12));

    TextCenteredColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f),
        "CSOPESY Operating System Emulator v1.0");
    TextCenteredColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Created By: Dr. Neil Patrick Del Gallego");

    ImGui::Dummy(ImVec2(0, 24));

    TextCenteredColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f),
        "Part of Project Amito:");
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
