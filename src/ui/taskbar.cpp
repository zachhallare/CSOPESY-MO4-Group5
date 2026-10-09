#include "taskbar.h"

void Taskbar::Init() {
    showSysInfo = false;
}

// Small colored button styled specifically to fit in the taskbar.
static bool TaskbarButton(const char* label, const ImVec4& color,
                          float width = 0.0f)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(color.x + 0.15f, color.y + 0.15f, color.z + 0.15f, color.w));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(color.x + 0.25f, color.y + 0.25f, color.z + 0.25f, color.w));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    bool clicked;
    if (width > 0.0f)
        clicked = ImGui::Button(label, ImVec2(width, 30));
    else
        clicked = ImGui::Button(label, ImVec2(0, 30));

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

bool Taskbar::Render() {
    bool shutdownRequested = false;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Pin the taskbar right at the bottom edge of the screen
    ImGui::SetNextWindowPos(ImVec2(0, displaySize.y - BAR_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, BAR_HEIGHT));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    ImGui::Begin("##Taskbar", nullptr,
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    // Left side start menu button
    ImVec4 startCol(0.15f, 0.35f, 0.65f, 1.0f);
    if (TaskbarButton("MENU", startCol, 60)) {
        // Not hooked up yet
    }
    ImGui::SameLine();

    ImVec4 greenBtn(0.15f, 0.55f, 0.25f, 1.0f);
    ImVec4 redBtn(0.65f, 0.15f, 0.15f, 1.0f);
    ImVec4 blueBtn(0.20f, 0.40f, 0.70f, 1.0f);

    // Main action buttons
    if (TaskbarButton("INIT", greenBtn, 56))
        showSysInfo = !showSysInfo;
    ImGui::SameLine();
    if (TaskbarButton("START", greenBtn, 60)) { }
    ImGui::SameLine();
    if (TaskbarButton("STOP", redBtn, 56))   { }

    // Math out where the right tray should start
    float trayWidth = 56 * 2 + 56 + 8 * 2;
    ImGui::SameLine(displaySize.x - trayWidth - 16);

    ImVec4 trayCol(0.25f, 0.25f, 0.35f, 1.0f);
    if (TaskbarButton("VOL", trayCol, 56))   { }
    ImGui::SameLine();
    if (TaskbarButton("NET", trayCol, 56))   { }
    ImGui::SameLine();

    // Actual power button that shuts things down
    ImVec4 pwrCol(0.75f, 0.10f, 0.10f, 1.0f);
    if (TaskbarButton("PWR", pwrCol, 56))
        shutdownRequested = true;

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    return shutdownRequested;
}
