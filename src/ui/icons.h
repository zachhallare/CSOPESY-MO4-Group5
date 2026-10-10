// Vector icons drawn with ImDrawList commands (no image assets needed), plus
// the small widgets built on top of them.
#pragma once

#include <string>
#include "imgui.h"

struct FsNode;

enum class Icon {
    Folder, File, TextFile, ImageFile, Program, SystemFile,
    Explorer, Terminal, Notepad, TaskManager, Settings, SystemInfo,
    Start, Search, User, Power
};

// Draws `icon` inside the square starting at `pos` with the given side length.
void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 pos, float size);
Icon IconForNode(const FsNode* node);

// Square button showing only an icon. `highlighted` keeps the hover look on.
bool IconButton(const char* id, Icon icon, ImVec2 size, float iconSize,
                bool highlighted = false, ImGuiButtonFlags flags = 0);

// Selectable row with a small icon in front of the label (lists, menus, tables).
bool IconSelectable(const char* label, Icon icon, bool selected = false,
                    ImGuiSelectableFlags flags = 0);

// Large icon with a caption underneath (desktop, Explorer icon view, start menu).
// `onWallpaper` adds a text shadow so captions stay readable on any background.
bool IconTile(const char* id, Icon icon, const char* label, bool selected,
              ImVec2 size, bool onWallpaper, bool* doubleClicked = nullptr);

// Text clipped with "..." so it fits in `maxWidth` pixels (current font).
std::string Ellipsize(const std::string& text, float maxWidth);
