#include "bios_screen.h"
#include <algorithm>

BiosScreen::BiosScreen(ImFont* font) : monoFont(font) {}

ScreenID BiosScreen::UpdateAndRender(float deltaTime) {
    timer += deltaTime;

    if (timer >= DURATION) {
        return ScreenID::BOOT_LOADING;
    }

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGui::PushFont(monoFont);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(displaySize);

    ImGui::Begin("##BIOS", nullptr,
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize  |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    const float pad = 20.0f;
    ImGui::SetCursorPos(ImVec2(pad, pad));

    struct Line { const char* text; ImVec4 color; bool spacerAfter; };
    static const Line lines[] = {
        { "CSOPESY Megatrends",                                  {1,1,1,1},             false },
        { "Released: 12/01/94",                                  {1,1,1,1},             false },
        { "DLSU GAME Lab (C)1994 CSOPESY Megatrends Inc.",       {1,1,1,1},             true  },
        { "Memory Test:",                                        {1,1,1,1},             false },
        { "Checking RAM : 64000K OK",                            {1,1,1,1},             true  },
        { "CPU Type: Pentium III",                               {1,1,1,1},             false },
        { "BIOS Version: BCN SIT 1989-1994 Special CSOPESY",    {1,1,1,1},             false },
        { "Main Processor: Pentium III",                         {1,1,1,1},             false },
        { "Numeric Processor: Built-in",                         {1,1,1,1},             true  },
        { "Primary Master:   420 MB WDCAC2420H",                {1,1,1,1},             false },
        { "Primary Slave:    None",                              {1,1,1,1},             false },
        { "Secondary Master: CD-ROM LTN-305A",                  {1,1,1,1},             false },
        { "Secondary Slave:  None",                              {1,1,1,1},             true  },
    };
    constexpr int LINE_COUNT = sizeof(lines) / sizeof(lines[0]);

    int linesToShow = std::min((int)(timer / 0.2f) + 1, LINE_COUNT);

    for (int i = 0; i < linesToShow; ++i) {
        ImGui::TextColored(lines[i].color, "%s", lines[i].text);
        if (lines[i].spacerAfter)
            ImGui::Dummy(ImVec2(0, 8));
    }

    if (linesToShow >= LINE_COUNT) {
        ImGui::PushTextWrapPos(displaySize.x - pad * 2);
        ImGui::TextColored(ImVec4(0.65f, 0.82f, 1.0f, 1.0f),
            "Fun Fact: The CPU specs is a tribute to Bemani Python 2 Engine, the OS that powers up "
            "Dance Dance Revolution SuperNova series. It was essentially a retail Sony PlayStation 2 "
            "(SCPH-50000) inside a metal box with a specialized I/O board!");
        ImGui::PopTextWrapPos();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopFont();

    return ScreenID::NONE;
}
