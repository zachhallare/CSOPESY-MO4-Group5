// Draws the desktop background: either an image texture or one of the
// built-in wallpapers made purely from ImGui draw commands. Shared by the
// desktop and the thumbnails in Settings.
#pragma once

#include <string>
#include <vector>
#include "imgui.h"

class TextureCache;

enum class WallpaperFit { Fill, Fit, Stretch };

struct BuiltinWallpaper {
    const char* id;     // "builtin:aurora"
    const char* name;   // "Aurora"
};

const std::vector<BuiltinWallpaper>& BuiltinWallpapers();
bool IsBuiltinWallpaper(const std::string& wallpaper);

// Falls back to the Aurora gradient when an image can't be loaded.
void DrawWallpaper(ImDrawList* dl, TextureCache& textures, const std::string& wallpaper,
                   WallpaperFit fit, ImVec2 min, ImVec2 max);
