#include "system_info.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using InfoList = std::vector<std::pair<std::string, std::string>>;

static InfoList BuildCategory(int category, Shell& shell) {
    const ProcessManager& pm      = shell.procs;
    const ShellSettings&  s       = shell.settings;
    ImVec2                display = ImGui::GetIO().DisplaySize;
    char                  buf[64];

    switch (category) {
    case 0: {   // System Summary
        snprintf(buf, sizeof(buf), "%.1f MB", ProcessManager::TOTAL_RAM_MB - pm.TotalMem());
        return {
            { "OS Name",                   "CSOPESY OS v1.0" },
            { "Version",                   "1.0.2026 Build 2026" },
            { "OS Manufacturer",           "CSOPESY Megatrends Inc." },
            { "System Name",               "CSOPESY-PC" },
            { "System Manufacturer",       "DLSU GAME Lab" },
            { "System Model",              "CSOPESY Desktop OS Emulator" },
            { "System Type",               "x86-based PC" },
            { "Processor",                 "Pentium III, 733 MHz, 1 Core(s), 1 Logical Processor(s)" },
            { "BIOS Version/Date",         "BCN SIT 1989-1994 Special CSOPESY, 12/01/94" },
            { "Installed Physical Memory", "64,000 KB" },
            { "Available Physical Memory", buf },
            { "User Name",                 "csopesy-pc\\" + s.username },
            { "Up Time",                   ProcessManager::FormatUptime(pm.Uptime()) },
        };
    }
    case 1:     // Hardware Resources
        return {
            { "IRQ 0",       "System timer" },
            { "IRQ 1",       "Standard 101/102-key keyboard" },
            { "IRQ 3",       "Communications port (COM2)" },
            { "IRQ 4",       "Communications port (COM1)" },
            { "IRQ 12",      "PS/2 compatible mouse" },
            { "IRQ 14",      "Primary IDE controller (WDCAC2420H)" },
            { "IRQ 15",      "Secondary IDE controller (CD-ROM LTN-305A)" },
            { "DMA 2",       "Standard floppy disk controller" },
            { "I/O 0x03F8",  "Communications port (COM1)" },
        };
    case 2:     // Display
        snprintf(buf, sizeof(buf), "%d x %d", (int)display.x, (int)display.y);
        return {
            { "Name",        "CSOPESY Compositor" },
            { "Renderer",    "OpenGL 3.0 via GLFW" },
            { "UI toolkit",  std::string("Dear ImGui ") + IMGUI_VERSION },
            { "Resolution",  buf },
            { "Refresh",     "V-Sync (monitor refresh rate)" },
            { "Color depth", "32 bits per pixel" },
            { "Wallpaper",   shell.WallpaperName() },
        };
    case 3:     // Sound
        return {
            { "Device",  "CSOPESY HD Audio" },
            { "Status",  "OK" },
            { "Volume",  s.muted ? std::string("Muted") : std::to_string(s.volume) + "%" },
        };
    case 4: {   // Storage
        size_t items = VirtualFS::CountItems(shell.fs.Root());
        return {
            { "Primary Master",   "420 MB WDCAC2420H" },
            { "Primary Slave",    "None" },
            { "Secondary Master", "CD-ROM LTN-305A" },
            { "Secondary Slave",  "None" },
            { "File system",      "CSOPESY VFS (in memory)" },
            { "Files and folders", std::to_string(items) },
            { "Used space",       VirtualFS::FormatSize(VirtualFS::SizeOf(shell.fs.Root())) },
        };
    }
    case 5: {   // Network
        bool online = pm.networkOnline;
        return {
            { "Adapter",      "CSOPESY Wireless 802.11g" },
            { "Status",       online ? "Connected" : "Disconnected" },
            { "SSID",         online ? s.network : std::string("-") },
            { "IPv4 Address", online ? "192.168.1.42" : "-" },
            { "MAC Address",  "00-1A-2B-3C-4D-5E" },
        };
    }
    default: {  // Software Environment
        int running = 0;
        for (int i = 0; i < APP_COUNT; ++i)
            running += shell.IsOpen((AppID)i) ? 1 : 0;
        return {
            { "Shell",             "CSOPESY Shell" },
            { "Window manager",    "Immediate-mode (redrawn every frame)" },
            { "Running processes", std::to_string(pm.Processes().size()) },
            { "Open windows",      std::to_string(running) },
            { "Startup programs",  "None" },
            { "Environment",       "PATH=/Programs;/System" },
        };
    }
    }
}

void SystemInfo::Render(Shell& shell) {
    static const char* CATEGORIES[] = {
        "System Summary", "Hardware Resources", "Display", "Sound", "Storage", "Network", "Software Environment"
    };

    // Header
    ImVec2 pos = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), Icon::SystemInfo, pos, 36.0f);
    ImGui::Dummy(ImVec2(36, 36));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(shell.fonts.uiLarge);
    ImGui::TextUnformatted("CSOPESY OS v1.0");
    ImGui::PopFont();
    ImGui::TextDisabled("(c) CSOPESY Megatrends Inc. All rights reserved.");
    ImGui::EndGroup();
    ImGui::Separator();

    // Left: category tree. "Components" groups the device pages.
    ImGui::BeginChild("##categories", ImVec2(190, -ImGui::GetTextLineHeightWithSpacing() * 2.5f), ImGuiChildFlags_Border);
    if (ImGui::Selectable(CATEGORIES[0], category == 0)) category = 0;
    if (ImGui::Selectable(CATEGORIES[1], category == 1)) category = 1;
    ImGui::TextDisabled("Components");
    ImGui::Indent();
    for (int i = 2; i <= 5; ++i)
        if (ImGui::Selectable(CATEGORIES[i], category == i)) category = i;
    ImGui::Unindent();
    if (ImGui::Selectable(CATEGORIES[6], category == 6)) category = 6;
    ImGui::EndChild();

    // Right: item / value table for the selected category.
    ImGui::SameLine();
    ImGui::BeginChild("##items", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing() * 2.5f), ImGuiChildFlags_Border);
    if (ImGui::BeginTable("##info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                       ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& row : BuildCategory(category, shell)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.first.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row.second.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Fun fact: the CPU specs are a tribute to the Bemani Python 2 engine behind Dance Dance "
                        "Revolution SuperNova - essentially a retail PlayStation 2 inside a metal box.");
    ImGui::PopTextWrapPos();
}
