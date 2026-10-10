#include "taskbar.h"
#include "apps/settings_app.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"
#include "os/shell.h"
#include "ui/icons.h"
#include "ui/quick_settings.h"

#include "imgui_internal.h"   // BringWindowToDisplayFront, GetCurrentWindow

#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace {

const float BUTTON = 44.0f;   // side of the square / round taskbar buttons
const ImU32 ACCENT = IM_COL32(70, 150, 255, 255);
const ImU32 WHITE  = IM_COL32(235, 238, 245, 255);
const ImU32 DIM    = IM_COL32(140, 145, 155, 255);

const char* POPUP_START   = "##start";
const char* POPUP_SEARCH  = "##search";
const char* POPUP_VOLUME  = "##volume";
const char* POPUP_NETWORK = "##network";

// ImGui normally swallows the click that closes a popup. On a taskbar that
// feels broken (open the start menu, click INIT: nothing happens), so the
// buttons also react to that click. Modal dialogs still block them.
bool ClickedThroughPopup() {
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
           ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);   // e.g. the search box
}

// Clicking a button whose popup is already open closes it (the click itself
// dismisses the popup), so it must not be reopened right away.
void TogglePopup(const char* popupId, bool pressed, bool wasOpen) {
    if (pressed && !wasOpen)
        ImGui::OpenPopup(popupId);
}

void VerticalSeparator() {
    ImGui::SameLine(0, 8);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 8), ImVec2(p.x, p.y + BUTTON - 8),
                                        IM_COL32(255, 255, 255, 40));
    ImGui::Dummy(ImVec2(1, BUTTON));
    ImGui::SameLine(0, 8);
}

// Round labelled button used for INIT / START / STOP, as in the spec mock-up.
bool CircleButton(const char* label, ImU32 textColor, ImFont* font, bool ring, ImU32 ringColor) {
    ImVec2 pos     = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::InvisibleButton(label, ImVec2(BUTTON, BUTTON));
    bool   hovered = ImGui::IsItemHovered();
    bool   held    = ImGui::IsItemActive();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      center = pos + ImVec2(BUTTON * 0.5f, BUTTON * 0.5f);
    float       radius = BUTTON * 0.5f - 1.0f;
    ImU32 fill = held    ? IM_COL32(70, 74, 92, 255)
               : hovered ? IM_COL32(55, 58, 74, 255)
                         : IM_COL32(36, 38, 50, 255);
    dl->AddCircleFilled(center, radius, fill, 40);
    if (ring)
        dl->AddCircle(center, radius - 1.0f, ringColor, 40, 2.0f);

    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label);
    dl->AddText(font, font->FontSize, center - textSize * 0.5f, textColor, label);
    return pressed || ClickedThroughPopup();
}

// Flat text button for the tray (VOL / NET / PWR).
bool TrayButton(const char* id, const char* label, ImU32 textColor, float width,
                bool active, ImGuiButtonFlags flags = 0)
{
    ImVec2 pos     = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::InvisibleButton(id, ImVec2(width, BUTTON), flags);
    bool   hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered || active)
        dl->AddRectFilled(pos, pos + ImVec2(width, BUTTON), IM_COL32(255, 255, 255, hovered ? 30 : 20), 6.0f);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    dl->AddText(pos + (ImVec2(width, BUTTON) - textSize) * 0.5f, textColor, label);
    return pressed || ClickedThroughPopup();
}

bool TaskbarIconButton(const char* id, Icon icon, float iconSize, bool highlighted, ImGuiButtonFlags flags = 0) {
    bool pressed = IconButton(id, icon, ImVec2(BUTTON, BUTTON), iconSize, highlighted, flags);
    return pressed || ClickedThroughPopup();
}

ImVec2 FlyoutAnchor(float x) {
    return ImVec2(x, ImGui::GetIO().DisplaySize.y - Shell::TASKBAR_HEIGHT - 8.0f);
}

} // namespace

void Taskbar::Render(Shell& shell) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float  height  = Shell::TASKBAR_HEIGHT;

    ImGui::SetNextWindowPos(ImVec2(0, display.y - height));
    ImGui::SetNextWindowSize(ImVec2(display.x, height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, 0.94f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##Taskbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    // Stay above the app windows, but below any open menu or dialog.
    if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImVec2 top = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(top, ImVec2(top.x + display.x, top.y), IM_COL32(255, 255, 255, 30));

    ImGui::SetCursorPos(ImVec2(8, (height - BUTTON) * 0.5f));

    bool startOpen = ImGui::IsPopupOpen(POPUP_START);
    TogglePopup(POPUP_START,
                TaskbarIconButton("##startbtn", Icon::Start, 28, startOpen, ImGuiButtonFlags_PressedOnClick),
                startOpen);
    ImGui::SetItemTooltip("Start");

    ImGui::SameLine(0, 4);
    bool searchOpen = ImGui::IsPopupOpen(POPUP_SEARCH);
    TogglePopup(POPUP_SEARCH,
                TaskbarIconButton("##searchbtn", Icon::Search, 22, searchOpen, ImGuiButtonFlags_PressedOnClick),
                searchOpen);
    ImGui::SetItemTooltip("Search (SRCH)");

    VerticalSeparator();
    RenderSchedulerButtons(shell);
    VerticalSeparator();
    RenderAppButtons(shell);
    RenderTray(shell);

    RenderStartMenu(shell);
    RenderSearch(shell);
    ImGui::End();
}

void Taskbar::RenderSchedulerButtons(Shell& shell) {
    const SchedulerState state = shell.procs.State();
    const float          pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 4.0f);
    ImFont*              font  = shell.fonts.uiSmall;
    std::string          message;

    if (CircleButton("INIT", IM_COL32(120, 200, 255, 255), font,
                     state != SchedulerState::Uninitialized, IM_COL32(120, 200, 255, 160))) {
        bool ok = shell.SchedulerInit(message);
        shell.Notify("Scheduler", message, ok ? ToastKind::Success : ToastKind::Warning);
    }
    ImGui::SetItemTooltip("INIT - initialize the process scheduler");

    ImGui::SameLine(0, 4);
    if (CircleButton("START", IM_COL32(90, 220, 120, 255), font,
                     state == SchedulerState::Running, IM_COL32(90, 220, 120, (int)(90 + 150 * pulse)))) {
        bool ok = shell.SchedulerStart(message);
        shell.Notify("Scheduler", message, ok ? ToastKind::Success : ToastKind::Warning);
    }
    ImGui::SetItemTooltip("START - generate dummy processes (watch them in Task Manager)");

    ImGui::SameLine(0, 4);
    if (CircleButton("STOP", IM_COL32(240, 90, 90, 255), font, false, 0)) {
        bool ok = shell.SchedulerStop(message);
        shell.Notify("Scheduler", message, ok ? ToastKind::Info : ToastKind::Warning);
    }
    ImGui::SetItemTooltip("STOP - stop generating processes");
}

void Taskbar::RenderAppButtons(Shell& shell) {
    bool first = true;
    for (int i = 0; i < APP_COUNT; ++i) {
        AppID          id      = (AppID)i;
        const AppInfo& info    = GetAppInfo(id);
        bool           running = shell.IsOpen(id);
        if (!info.pinned && !running)
            continue;   // unpinned apps only show up while they run

        if (!first)
            ImGui::SameLine(0, 4);
        first = false;

        ImGui::PushID(i);
        bool   active = shell.IsActive(id);
        ImVec2 pos    = ImGui::GetCursorScreenPos();
        if (TaskbarIconButton("##app", info.icon, 26, active))
            shell.TaskbarClick(id);
        ImGui::SetItemTooltip("%s", shell.GetApp(id)->Title().c_str());

        // Running indicator: a short bar, longer and blue for the focused app.
        if (running) {
            float w = active ? 18.0f : 6.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(pos.x + (BUTTON - w) * 0.5f, pos.y + BUTTON - 4),
                ImVec2(pos.x + (BUTTON + w) * 0.5f, pos.y + BUTTON - 1),
                active ? ACCENT : DIM, 1.5f);
        }

        if (ImGui::BeginPopupContextItem("##appmenu")) {
            ImGui::TextDisabled("%s", info.name);
            ImGui::Separator();
            if (ImGui::MenuItem(running ? "Bring to front" : "Open"))
                shell.OpenApp(id);
            if (running && ImGui::MenuItem("Close window"))
                shell.CloseApp(id);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void Taskbar::RenderTray(Shell& shell) {
    const ShellSettings& s       = shell.settings;
    const float          width   = 52.0f;
    const float          spacing = 4.0f;
    const ImVec2         display = ImGui::GetIO().DisplaySize;

    ImGui::SameLine(ImGui::GetWindowWidth() - (width * 3 + spacing * 2) - 8.0f);

    bool volumeOpen = ImGui::IsPopupOpen(POPUP_VOLUME);
    TogglePopup(POPUP_VOLUME,
                TrayButton("##vol", s.muted ? "MUTE" : "VOL", s.muted ? DIM : WHITE, width, volumeOpen,
                           ImGuiButtonFlags_PressedOnClick),
                volumeOpen);
    if (s.muted) ImGui::SetItemTooltip("Volume: muted");
    else         ImGui::SetItemTooltip("Volume: %d%%", s.volume);

    ImGui::SameLine(0, spacing);
    bool online      = shell.procs.networkOnline;
    bool networkOpen = ImGui::IsPopupOpen(POPUP_NETWORK);
    TogglePopup(POPUP_NETWORK,
                TrayButton("##net", "NET", online ? WHITE : DIM, width, networkOpen, ImGuiButtonFlags_PressedOnClick),
                networkOpen);
    if (online) ImGui::SetItemTooltip("Connected to %s", s.network.c_str());
    else        ImGui::SetItemTooltip("Not connected");

    ImGui::SameLine(0, spacing);
    if (TrayButton("##pwr", "PWR", IM_COL32(255, 90, 90, 255), width, false))
        shell.RequestPowerDialog(0);
    ImGui::SetItemTooltip("Power - shut down or restart CSOPESY OS");

    // Flyouts open above the tray.
    ImGui::SetNextWindowPos(FlyoutAnchor(display.x - 10.0f), ImGuiCond_Always, ImVec2(1, 1));
    ImGui::SetNextWindowSize(ImVec2(300, 0));
    if (ImGui::BeginPopup(POPUP_VOLUME)) {
        ImGui::TextUnformatted("Volume");
        ImGui::Separator();
        VolumeControls(shell);
        ImGui::TextDisabled("Output: Speakers (CSOPESY HD Audio)");
        if (ImGui::Button("Sound settings")) {
            static_cast<SettingsApp*>(shell.GetApp(AppID::Settings))->ShowPage(SettingsApp::PAGE_SOUND);
            shell.OpenApp(AppID::Settings);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(FlyoutAnchor(display.x - 10.0f), ImGuiCond_Always, ImVec2(1, 1));
    ImGui::SetNextWindowSize(ImVec2(380, 0));
    if (ImGui::BeginPopup(POPUP_NETWORK)) {
        ImGui::TextUnformatted("Network");
        ImGui::Separator();
        NetworkControls(shell);
        ImGui::Separator();
        if (ImGui::Button("Network settings")) {
            static_cast<SettingsApp*>(shell.GetApp(AppID::Settings))->ShowPage(SettingsApp::PAGE_NETWORK);
            shell.OpenApp(AppID::Settings);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Taskbar::RenderStartMenu(Shell& shell) {
    ImGui::SetNextWindowPos(FlyoutAnchor(8.0f), ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowSize(ImVec2(372, 0));
    if (!ImGui::BeginPopup(POPUP_START))
        return;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    DrawIcon(dl, Icon::Start, pos, 36.0f);
    ImGui::Dummy(ImVec2(36, 36));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(shell.fonts.uiLarge);
    ImGui::TextUnformatted("CSOPESY OS");
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::TextDisabled("Apps");

    const ImVec2 tile(112, 84);
    for (int i = 0; i < APP_COUNT; ++i) {
        const AppInfo& info = GetAppInfo((AppID)i);
        if (i % 3 != 0)
            ImGui::SameLine(0, 6);
        ImGui::PushID(i);
        if (IconTile("##tile", info.icon, info.name, false, tile, false)) {
            shell.OpenApp((AppID)i);
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    float iconSize = ImGui::GetFrameHeight();
    pos = ImGui::GetCursorScreenPos();
    DrawIcon(dl, Icon::User, pos, iconSize);
    ImGui::Dummy(ImVec2(iconSize, iconSize));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(shell.settings.username.c_str());

    ImGui::SameLine();
    const float buttonsW = 80.0f + 90.0f + ImGui::GetStyle().ItemSpacing.x;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > buttonsW)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonsW);
    if (ImGui::Button("Restart", ImVec2(80, 0))) {
        shell.RequestPowerDialog(1);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Shut down", ImVec2(90, 0))) {
        shell.RequestPowerDialog(0);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void Taskbar::RenderSearch(Shell& shell) {
    ImGui::SetNextWindowPos(FlyoutAnchor(8.0f + BUTTON + 4.0f), ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowSize(ImVec2(440, 0));
    if (!ImGui::BeginPopup(POPUP_SEARCH))
        return;

    if (ImGui::IsWindowAppearing()) {
        searchBuf[0] = '\0';
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool        enter = ImGui::InputTextWithHint("##query", "Search apps, files and folders", searchBuf,
                                                 sizeof(searchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    std::string query = Trim(searchBuf);

    std::vector<AppID> apps;
    for (int i = 0; i < APP_COUNT; ++i) {
        const AppInfo& info = GetAppInfo((AppID)i);
        if (query.empty() || ContainsNoCase(info.name, query) || ContainsNoCase(info.exe, query))
            apps.push_back((AppID)i);
    }
    std::vector<FsNode*> files;
    if (!query.empty())
        shell.fs.Find(shell.fs.Root(), query, files);

    bool close = false;
    if (enter) {   // Enter opens the top result
        if (!apps.empty())       { shell.OpenApp(apps[0]);                         close = true; }
        else if (!files.empty()) { shell.OpenPath(VirtualFS::PathOf(files[0]));    close = true; }
    }

    ImGui::Spacing();
    if (!apps.empty()) {
        ImGui::TextDisabled(query.empty() ? "All apps" : "Apps");
        for (AppID id : apps) {
            const AppInfo& info = GetAppInfo(id);
            ImGui::PushID((int)id);
            if (IconSelectable(info.name, info.icon)) {
                shell.OpenApp(id);
                close = true;
            }
            ImGui::PopID();
        }
    }

    const size_t MAX_FILES = 8;
    if (!files.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Files and folders");
        for (size_t i = 0; i < files.size() && i < MAX_FILES; ++i) {
            std::string path  = VirtualFS::PathOf(files[i]);
            std::string label = files[i]->name + "   (" + VirtualFS::PathOf(files[i]->parent) + ")";
            ImGui::PushID(path.c_str());
            if (IconSelectable(label.c_str(), IconForNode(files[i]))) {
                shell.OpenPath(path);
                close = true;
            }
            ImGui::PopID();
        }
        if (files.size() > MAX_FILES)
            ImGui::TextDisabled("...and %d more", (int)(files.size() - MAX_FILES));
    }

    if (apps.empty() && files.empty())
        ImGui::TextDisabled("No results for '%s'", query.c_str());

    if (close)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
