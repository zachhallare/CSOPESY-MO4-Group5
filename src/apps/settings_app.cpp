#include "settings_app.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"
#include "ui/quick_settings.h"
#include "ui/wallpaper.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

static const ImU32 ACCENT = IM_COL32(70, 150, 255, 255);

static void InfoRow(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value.c_str());
}

void SettingsApp::Render(Shell& shell) {
    static const char* PAGES[PAGE_COUNT] = {
        "System", "Personalization", "Sound", "Network & internet", "Date & time"
    };

    // Left: account card and page list.
    ImGui::BeginChild("##nav", ImVec2(210, 0), ImGuiChildFlags_Border);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), Icon::User, pos, 40.0f);
    ImGui::Dummy(ImVec2(40, 40));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(shell.settings.username.c_str());
    ImGui::TextDisabled("Local account");
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    for (int i = 0; i < PAGE_COUNT; ++i)
        if (ImGui::Selectable(PAGES[i], page == i, 0, ImVec2(0, 30)))
            page = i;
    ImGui::PopStyleVar();
    ImGui::EndChild();

    // Right: the selected page.
    ImGui::SameLine();
    ImGui::BeginChild("##page", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(shell.fonts.uiLarge);
    ImGui::TextUnformatted(PAGES[page]);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    switch (page) {
    case PAGE_SYSTEM:          RenderSystem(shell);          break;
    case PAGE_PERSONALIZATION: RenderPersonalization(shell); break;
    case PAGE_SOUND:           RenderSound(shell);           break;
    case PAGE_NETWORK:         RenderNetwork(shell);         break;
    case PAGE_DATETIME:        RenderDateTime(shell);        break;
    }
    ImGui::EndChild();
}

void SettingsApp::RenderSystem(Shell& shell) {
    const ProcessManager& pm      = shell.procs;
    ImVec2                display = ImGui::GetIO().DisplaySize;
    char                  buf[64];

    if (ImGui::BeginTable("##system", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        InfoRow("Device name",   "CSOPESY-PC");
        InfoRow("Edition",       "CSOPESY OS v1.0 (Build 2026)");
        InfoRow("Processor",     "Pentium III @ 733 MHz");
        snprintf(buf, sizeof(buf), "64,000 KB (%.1f MB in use)", pm.TotalMem());
        InfoRow("Installed RAM", buf);
        InfoRow("System type",   "32-bit operating system, x86-based processor");
        InfoRow("Storage",       "420 MB WDCAC2420H, CD-ROM LTN-305A");
        snprintf(buf, sizeof(buf), "%d x %d", (int)display.x, (int)display.y);
        InfoRow("Display",       buf);
        InfoRow("Signed in as",  shell.settings.username);
        InfoRow("Up time",       ProcessManager::FormatUptime(pm.Uptime()));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Open System Information"))
        shell.OpenApp(AppID::SystemInfo);
    ImGui::SameLine();
    if (ImGui::Button("Open Task Manager"))
        shell.OpenApp(AppID::TaskManager);
}

void SettingsApp::RenderPersonalization(Shell& shell) {
    ShellSettings& s  = shell.settings;
    ImDrawList*    dl = ImGui::GetWindowDrawList();

    // Preview of the desktop on a little monitor.
    const ImVec2 screen(256, 144);
    ImVec2 bezel = ImGui::GetCursorScreenPos();
    ImVec2 pos   = bezel + ImVec2(6, 6);
    dl->AddRectFilled(bezel, pos + screen + ImVec2(6, 6), IM_COL32(30, 30, 36, 255), 8.0f);
    DrawWallpaper(dl, shell.textures, s.wallpaper, s.wallpaperFit, pos, pos + screen);
    dl->AddRectFilled(ImVec2(pos.x, pos.y + screen.y - 10), pos + screen, IM_COL32(20, 20, 30, 230));
    ImGui::Dummy(screen + ImVec2(12, 12));

    ImGui::SameLine(0, 20);
    ImGui::BeginGroup();
    ImGui::TextDisabled("Current background");
    ImGui::TextUnformatted(shell.WallpaperName().c_str());
    ImGui::Spacing();
    static const char* FITS[] = { "Fill", "Fit", "Stretch" };
    int fit = (int)s.wallpaperFit;
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::Combo("Choose a fit", &fit, FITS, IM_ARRAYSIZE(FITS)))
        s.wallpaperFit = (WallpaperFit)fit;
    ImGui::Checkbox("Show desktop icons", &s.showDesktopIcons);
    ImGui::EndGroup();

    // Thumbnails: every image in /Pictures plus the drawn wallpapers.
    ImGui::Spacing();
    ImGui::SeparatorText("Background");
    struct Choice { std::string id, label; };
    std::vector<Choice> choices;
    std::vector<FsNode*> images;
    shell.fs.FindImages(shell.fs.Resolve("/Pictures"), images);
    for (FsNode* image : images)
        choices.push_back({ image->hostPath, image->name });
    for (const BuiltinWallpaper& builtin : BuiltinWallpapers())
        choices.push_back({ builtin.id, std::string(builtin.name) + " (drawn)" });

    const ImVec2 thumb(144, 81);
    const float  gap    = 10.0f;
    const float  labelH = ImGui::GetTextLineHeight() + 4.0f;
    int perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (thumb.x + gap)));
    for (size_t i = 0; i < choices.size(); ++i) {
        const Choice& c = choices[i];
        if (i % perRow != 0)
            ImGui::SameLine(0, gap);
        ImGui::PushID(c.id.c_str());
        ImVec2 p       = ImGui::GetCursorScreenPos();
        bool   clicked = ImGui::InvisibleButton("##thumb", ImVec2(thumb.x, thumb.y + labelH));
        bool   hovered = ImGui::IsItemHovered();

        DrawWallpaper(dl, shell.textures, c.id, WallpaperFit::Fill, p, p + thumb);
        if (s.wallpaper == c.id)
            dl->AddRect(p - ImVec2(3, 3), p + thumb + ImVec2(3, 3), ACCENT, 5.0f, 0, 3.0f);
        else if (hovered)
            dl->AddRect(p - ImVec2(2, 2), p + thumb + ImVec2(2, 2), IM_COL32(255, 255, 255, 120), 5.0f, 0, 2.0f);
        dl->AddText(ImVec2(p.x, p.y + thumb.y + 4), ImGui::GetColorU32(ImGuiCol_Text),
                    Ellipsize(c.label, thumb.x).c_str());

        if (clicked && shell.SetWallpaper(c.id))
            shell.Notify("Personalization", "Background changed to " + c.label + ".", ToastKind::Success);
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Custom image");
    ImGui::TextDisabled("Path to a .jpg / .png / .bmp file, absolute or relative to the program folder.");
    ImGui::SetNextItemWidth(-90.0f);
    bool apply = ImGui::InputTextWithHint("##custom", "e.g. wallpapers/cat.jpg", customPath, sizeof(customPath),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    apply |= ImGui::Button("Apply", ImVec2(-FLT_MIN, 0));
    if (apply && shell.SetWallpaper(customPath))
        shell.Notify("Personalization", "Background changed to " + shell.WallpaperName() + ".", ToastKind::Success);
}

void SettingsApp::RenderSound(Shell& shell) {
    ImGui::TextDisabled("Master volume");
    ImGui::PushItemWidth(380.0f);
    ImGui::BeginGroup();
    VolumeControls(shell);
    ImGui::EndGroup();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    static const char* DEVICES[] = {
        "Speakers (CSOPESY HD Audio)", "Headphones (CSOPESY HD Audio)", "Digital Output (S/PDIF)"
    };
    ImGui::SetNextItemWidth(260.0f);
    ImGui::Combo("Output device", &outputDevice, DEVICES, IM_ARRAYSIZE(DEVICES));

    ImGui::Spacing();
    if (ImGui::Button("Test sound")) {
        if (shell.settings.muted)
            shell.Notify("Sound", "Sound is muted.", ToastKind::Warning);
        else
            shell.Notify("Sound", "Played a test tone on " + std::string(DEVICES[outputDevice]) + " at " +
                         std::to_string(shell.settings.volume) + "% volume.");
    }
}

void SettingsApp::RenderNetwork(Shell& shell) {
    ImGui::BeginChild("##networks", ImVec2(460, 230), ImGuiChildFlags_Border);
    NetworkControls(shell);
    ImGui::EndChild();

    bool online = shell.procs.networkOnline;
    ImGui::Spacing();
    ImGui::SeparatorText("Properties");
    if (ImGui::BeginTable("##netprops", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        InfoRow("Adapter",      "CSOPESY Wireless 802.11g");
        InfoRow("Status",       online ? "Connected" : "Disconnected");
        InfoRow("IPv4 address", online ? "192.168.1.42" : "-");
        InfoRow("DNS server",   online ? "192.168.1.1" : "-");
        InfoRow("MAC address",  "00-1A-2B-3C-4D-5E");
        ImGui::EndTable();
    }
}

void SettingsApp::RenderDateTime(Shell& shell) {
    ShellSettings& s = shell.settings;

    ImGui::PushFont(shell.fonts.uiLarge);
    ImGui::TextUnformatted(shell.ClockText().c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("This is exactly what the desktop clock shows. It updates every frame.");
    ImGui::Spacing();

    ImGui::Checkbox("Use 24-hour time", &s.clock24h);
    ImGui::Checkbox("Show seconds", &s.clockSeconds);
    static const char* DATE_FORMATS[] = { "Thursday, Apr 30, 2026", "04/30/2026", "2026-04-30" };
    ImGui::SetNextItemWidth(220.0f);
    ImGui::Combo("Date format", &s.dateFormat, DATE_FORMATS, IM_ARRAYSIZE(DATE_FORMATS));
    ImGui::Spacing();
    ImGui::TextDisabled("Time zone: same as this computer");
}
