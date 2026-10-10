#include "wallpaper.h"
#include "core/texture_cache.h"

#include <algorithm>

const std::vector<BuiltinWallpaper>& BuiltinWallpapers() {
    static const std::vector<BuiltinWallpaper> list = {
        { "builtin:aurora", "Aurora" },
        { "builtin:sunset", "Sunset" },
        { "builtin:grid",   "Grid"   },
    };
    return list;
}

bool IsBuiltinWallpaper(const std::string& wallpaper) {
    return wallpaper.rfind("builtin:", 0) == 0;
}

static void DrawAurora(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    float w = max.x - min.x, h = max.y - min.y;
    dl->AddRectFilledMultiColor(min, max,
        IM_COL32(10, 18, 48, 255),  IM_COL32(22, 40, 92, 255),
        IM_COL32(10, 92, 112, 255), IM_COL32(6, 30, 62, 255));

    // Soft glows from stacked translucent circles.
    struct Glow { float x, y, r; ImU32 col; };
    const Glow glows[] = {
        { 0.25f, 0.35f, 0.45f, IM_COL32(40, 220, 170, 18) },
        { 0.70f, 0.25f, 0.40f, IM_COL32(90, 120, 255, 20) },
        { 0.55f, 0.75f, 0.50f, IM_COL32(30, 200, 220, 16) },
    };
    for (const Glow& g : glows)
        for (int i = 0; i < 4; ++i)
            dl->AddCircleFilled(ImVec2(min.x + g.x * w, min.y + g.y * h),
                                g.r * h * (1.0f - i * 0.2f), g.col, 64);
}

static void DrawSunset(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    float w = max.x - min.x, h = max.y - min.y;
    float horizon = min.y + h * 0.70f;
    dl->AddRectFilledMultiColor(min, ImVec2(max.x, horizon),
        IM_COL32(58, 22, 92, 255),   IM_COL32(58, 22, 92, 255),
        IM_COL32(250, 140, 70, 255), IM_COL32(250, 140, 70, 255));

    ImVec2 sun(min.x + w * 0.68f, horizon);
    for (int i = 0; i < 3; ++i)
        dl->AddCircleFilled(sun, h * (0.20f - i * 0.04f), IM_COL32(255, 200, 110, 40 + i * 60), 64);

    dl->AddRectFilledMultiColor(ImVec2(min.x, horizon), max,
        IM_COL32(70, 30, 70, 255),  IM_COL32(70, 30, 70, 255),
        IM_COL32(20, 10, 30, 255),  IM_COL32(20, 10, 30, 255));
    // Sun reflections on the water.
    for (int i = 0; i < 6; ++i) {
        float y    = horizon + h * (0.03f + i * 0.045f);
        float half = w * (0.10f - i * 0.012f);
        dl->AddLine(ImVec2(sun.x - half, y), ImVec2(sun.x + half, y),
                    IM_COL32(255, 190, 120, 120 - i * 15), std::max(1.0f, h * 0.006f));
    }
}

static void DrawGrid(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    float h = max.y - min.y;
    dl->AddRectFilledMultiColor(min, max,
        IM_COL32(18, 22, 30, 255), IM_COL32(26, 32, 46, 255),
        IM_COL32(32, 40, 58, 255), IM_COL32(22, 26, 36, 255));

    float step = std::max(6.0f, h / 24.0f);
    float r    = std::max(1.0f, step * 0.07f);
    for (float y = min.y + step * 0.5f; y < max.y; y += step)
        for (float x = min.x + step * 0.5f; x < max.x; x += step)
            dl->AddCircleFilled(ImVec2(x, y), r, IM_COL32(120, 170, 255, 45), 6);

    // A few accent lines across the pattern.
    dl->AddLine(ImVec2(min.x, min.y + h * 0.80f), ImVec2(max.x, min.y + h * 0.20f),
                IM_COL32(80, 160, 255, 40), std::max(1.0f, h * 0.004f));
    dl->AddLine(ImVec2(min.x, min.y + h * 0.95f), ImVec2(max.x, min.y + h * 0.35f),
                IM_COL32(80, 160, 255, 25), std::max(1.0f, h * 0.003f));
}

void DrawWallpaper(ImDrawList* dl, TextureCache& textures, const std::string& wallpaper,
                   WallpaperFit fit, ImVec2 min, ImVec2 max)
{
    if (wallpaper == "builtin:sunset") { DrawSunset(dl, min, max); return; }
    if (wallpaper == "builtin:grid")   { DrawGrid(dl, min, max);   return; }

    const Texture* tex = IsBuiltinWallpaper(wallpaper) ? nullptr : &textures.Get(wallpaper);
    if (!tex || !tex->ok) {
        DrawAurora(dl, min, max);
        return;
    }

    float w = max.x - min.x, h = max.y - min.y;
    float imageAspect = (float)tex->width / (float)tex->height;
    float boxAspect   = w / h;
    dl->AddRectFilled(min, max, IM_COL32(0, 0, 0, 255));

    switch (fit) {
    case WallpaperFit::Stretch:
        dl->AddImage(tex->ImId(), min, max);
        break;

    case WallpaperFit::Fill: {
        // Crop the image (via UVs) so it covers the whole area without distortion.
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (imageAspect > boxAspect) {
            float visible = boxAspect / imageAspect;
            uv0.x = (1.0f - visible) * 0.5f;
            uv1.x = 1.0f - uv0.x;
        } else {
            float visible = imageAspect / boxAspect;
            uv0.y = (1.0f - visible) * 0.5f;
            uv1.y = 1.0f - uv0.y;
        }
        dl->AddImage(tex->ImId(), min, max, uv0, uv1);
        break;
    }

    case WallpaperFit::Fit: {
        ImVec2 size = (imageAspect > boxAspect) ? ImVec2(w, w / imageAspect)
                                                : ImVec2(h * imageAspect, h);
        ImVec2 p = min + (ImVec2(w, h) - size) * 0.5f;
        dl->AddImage(tex->ImId(), p, p + size);
        break;
    }
    }
}
