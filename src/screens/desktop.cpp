#include "desktop.h"
#include "apps/file_explorer.h"
#include "apps/settings_app.h"
#include "fs/virtual_fs.h"
#include "ui/icons.h"
#include "ui/wallpaper.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static const ImVec4 ERROR_COLOR(1.0f, 0.45f, 0.45f, 1.0f);
static const ImVec4 WARN_COLOR(1.0f, 0.75f, 0.30f, 1.0f);

DesktopScreen::DesktopScreen(const Fonts& fonts, const Config& config, VirtualFS& fs)
    : fonts(fonts), shell(fs, config, fonts)
{
    shell.Notify("Welcome to CSOPESY OS",
                 "Hi " + config.username + "! Double-click a desktop icon or use the taskbar to get started.");
}

ScreenID DesktopScreen::UpdateAndRender(float deltaTime) {
    shell.Update(deltaTime);
    ImGui::PushFont(fonts.ui);

    RenderWallpaper();      // base layer, drawn behind everything else
    RenderVersionLabel();
    RenderDesktopIcons();
    shell.RenderWindows();
    taskbar.Render(shell);
    RenderClock();
    shell.RenderNotifications();
    RenderRenameDialog();
    ScreenID next = RenderPowerDialog();

    ImGui::PopFont();
    return next;
}

void DesktopScreen::OnCloseRequested() {
    shell.RequestPowerDialog(0);
}

void DesktopScreen::RenderWallpaper() {
    DrawWallpaper(ImGui::GetBackgroundDrawList(), shell.textures, shell.settings.wallpaper,
                  shell.settings.wallpaperFit, ImVec2(0, 0), ImGui::GetIO().DisplaySize);
}

void DesktopScreen::RenderVersionLabel() {
    const char* label  = "CSOPESY OS v1.0 - Group 5";
    ImVec2      work   = shell.WorkAreaSize();
    ImVec2      size   = ImGui::CalcTextSize(label);
    const float margin = 10.0f;

    ImGui::GetBackgroundDrawList()->AddText(
        ImVec2(work.x - size.x - margin, work.y - size.y - margin),
        IM_COL32(255, 255, 255, 120),
        label);
}

void DesktopScreen::RenderClock() {
    ImVec2      displaySize = ImGui::GetIO().DisplaySize;
    std::string timeText    = shell.ClockText();   // re-read every frame

    ImVec2 textSize = ImGui::CalcTextSize(timeText.c_str());
    float  padX = 12.0f, padY = 6.0f;
    float  margin = 10.0f;

    ImVec2 boxMin = { displaySize.x - textSize.x - padX * 2 - margin, margin };
    ImVec2 boxMax = { displaySize.x - margin, margin + textSize.y + padY * 2 };

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 160), 6.0f);
    fg->AddText(
        ImVec2(boxMin.x + padX, boxMin.y + padY),
        IM_COL32(255, 255, 255, 240),
        timeText.c_str());
}

void DesktopScreen::RenderDesktopIcons() {
    const ImVec2 work = shell.WorkAreaSize();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(work);
    ImGui::Begin("##DesktopIcons", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);

    // Clicking the desktop takes focus away from the app windows.
    if (ImGui::IsWindowFocused())
        shell.ClearFocus();

    struct Entry {
        std::string key;     // "app:<exe>" for shortcuts, the file path otherwise
        std::string label;
        Icon        icon;
        int         app;     // AppID for shortcuts, -1 for files
    };
    std::vector<Entry> entries;
    if (shell.settings.showDesktopIcons) {
        static const AppID SHORTCUTS[] = {
            AppID::FileExplorer, AppID::Terminal, AppID::Notepad,
            AppID::TaskManager, AppID::Settings, AppID::SystemInfo,
        };
        for (AppID id : SHORTCUTS) {
            const AppInfo& info = GetAppInfo(id);
            entries.push_back({ std::string("app:") + info.exe,
                                id == AppID::FileExplorer ? "This PC" : info.name, info.icon, (int)id });
        }
        // Files and folders saved in /Desktop show up here too.
        if (FsNode* desktop = shell.fs.Resolve("/Desktop"))
            for (const auto& child : desktop->children)
                entries.push_back({ VirtualFS::PathOf(child.get()), child->name, IconForNode(child.get()), -1 });
    }

    auto open = [this](const Entry& e) {
        if (e.app == (int)AppID::FileExplorer)
            shell.Explorer().Navigate("/");
        if (e.app >= 0)
            shell.OpenApp((AppID)e.app);
        else
            shell.OpenPath(e.key);
    };

    // Column-major grid from the top-left corner, like Windows.
    const ImVec2 cell(84, 88);
    const float  gap = 6.0f, margin = 12.0f;
    const int    rows = std::max(1, (int)((work.y - margin * 2 + gap) / (cell.y + gap)));
    for (size_t i = 0; i < entries.size(); ++i) {
        const Entry& e = entries[i];
        ImGui::SetCursorPos(ImVec2(margin + (float)(i / rows) * (cell.x + gap),
                                   margin + (float)(i % rows) * (cell.y + gap)));
        ImGui::PushID(e.key.c_str());

        bool doubleClicked = false;
        if (IconTile("##icon", e.icon, e.label.c_str(), selectedIcon == e.key, cell, true, &doubleClicked))
            selectedIcon = e.key;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            selectedIcon = e.key;
        if (doubleClicked)
            open(e);

        if (ImGui::BeginPopupContextItem("##iconmenu")) {
            if (ImGui::MenuItem("Open"))
                open(e);
            if (e.app < 0) {
                if (ImGui::MenuItem("Show in File Explorer")) {
                    shell.Explorer().Reveal(e.key);
                    shell.OpenApp(AppID::FileExplorer);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Rename")) {
                    renameTarget = e.key;
                    snprintf(renameBuf, sizeof(renameBuf), "%s", e.label.c_str());
                    renameError.clear();
                    openRename = true;
                }
                if (ImGui::MenuItem("Delete")) {
                    FsNode* node = shell.fs.Resolve(e.key);
                    if (node && !shell.fs.Remove(node))
                        shell.Notify("Desktop", shell.fs.LastError(), ToastKind::Error);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Right-click on empty desktop space.
    if (ImGui::BeginPopupContextWindow("##desktopmenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("New")) {
            if (ImGui::MenuItem("Folder"))        CreateOnDesktop(true);
            if (ImGui::MenuItem("Text document")) CreateOnDesktop(false);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Open Terminal"))  shell.OpenApp(AppID::Terminal);
        if (ImGui::MenuItem("Task Manager"))   shell.OpenApp(AppID::TaskManager);
        ImGui::Separator();
        if (ImGui::MenuItem("Show desktop icons", nullptr, shell.settings.showDesktopIcons))
            shell.settings.showDesktopIcons = !shell.settings.showDesktopIcons;
        if (ImGui::MenuItem("Personalize")) {
            static_cast<SettingsApp*>(shell.GetApp(AppID::Settings))->ShowPage(SettingsApp::PAGE_PERSONALIZATION);
            shell.OpenApp(AppID::Settings);
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        selectedIcon.clear();

    ImGui::End();
}

void DesktopScreen::CreateOnDesktop(bool folder) {
    VirtualFS&  fs      = shell.fs;
    FsNode*     desktop = fs.Resolve("/Desktop");
    std::string name    = fs.UniqueName(desktop, folder ? "New folder" : "New Text Document.txt");
    FsNode*     node    = folder ? fs.NewFolder(desktop, name) : fs.NewFile(desktop, name);
    if (!node) {
        shell.Notify("Desktop", fs.LastError(), ToastKind::Error);
        return;
    }
    selectedIcon = VirtualFS::PathOf(node);
    renameTarget = selectedIcon;
    snprintf(renameBuf, sizeof(renameBuf), "%s", name.c_str());
    renameError.clear();
    openRename = true;
}

void DesktopScreen::RenderRenameDialog() {
    if (openRename) {
        ImGui::OpenPopup("Rename##desktop");
        openRename = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("Rename##desktop", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Rename '%s' to:", VirtualFS::NameOf(renameTarget).c_str());
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(320.0f);
    bool submit = ImGui::InputText("##name", renameBuf, sizeof(renameBuf),
                                   ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (!renameError.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", renameError.c_str());

    submit |= ImGui::Button("Rename", ImVec2(120, 0));
    ImGui::SameLine();
    bool cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (submit) {
        FsNode* node = shell.fs.Resolve(renameTarget);
        if (node && shell.fs.Rename(node, renameBuf)) {
            selectedIcon = VirtualFS::PathOf(node);
            ImGui::CloseCurrentPopup();
        } else {
            renameError = node ? shell.fs.LastError() : "The item no longer exists.";
        }
    }
    if (cancel)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

ScreenID DesktopScreen::RenderPowerDialog() {
    int choice = 0;
    if (shell.ConsumePowerDialogRequest(&choice)) {
        powerChoice = choice;
        ImGui::OpenPopup("Shut Down CSOPESY OS");
    }

    ScreenID result = ScreenID::NONE;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Shut Down CSOPESY OS", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Power, pos, 44.0f);
        ImGui::Dummy(ImVec2(44, 44));
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextUnformatted("What do you want the computer to do?");
        ImGui::RadioButton("Shut down", &powerChoice, 0);
        ImGui::SameLine(0, 20);
        ImGui::RadioButton("Restart", &powerChoice, 1);
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::TextDisabled(powerChoice == 0 ? "Closes all apps and turns off CSOPESY OS."
                                             : "Closes all apps and boots CSOPESY OS again.");
        if (shell.HasUnsavedWork())
            ImGui::TextColored(WARN_COLOR, "Notepad has unsaved changes that will be lost.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            result = (powerChoice == 0) ? ScreenID::SHUTDOWN : ScreenID::RESTART;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return result;
}
