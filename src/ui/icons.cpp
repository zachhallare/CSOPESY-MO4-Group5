#include "icons.h"
#include "apps/app.h"
#include "core/strings.h"
#include "fs/virtual_fs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float PI = 3.14159265f;

// Maps unit-square coordinates (0..1) onto the icon's square.
struct Canvas {
    ImDrawList* dl;
    ImVec2      o;
    float       s;

    ImVec2 P(float x, float y) const { return ImVec2(o.x + x * s, o.y + y * s); }

    void Rect(float x0, float y0, float x1, float y1, ImU32 col,
              float rounding = 0.0f, ImDrawFlags flags = 0) const {
        dl->AddRectFilled(P(x0, y0), P(x1, y1), col, rounding * s, flags);
    }
    void Line(float x0, float y0, float x1, float y1, ImU32 col, float thickness) const {
        dl->AddLine(P(x0, y0), P(x1, y1), col, std::max(1.0f, thickness * s));
    }
    void Circle(float x, float y, float r, ImU32 col) const {
        dl->AddCircleFilled(P(x, y), r * s, col, 24);
    }
};

const ImU32 WHITE  = IM_COL32(255, 255, 255, 255);
const ImU32 ACCENT = IM_COL32(45, 125, 245, 255);
const ImU32 INK    = IM_COL32(130, 140, 155, 255);

void DrawPage(const Canvas& c, ImU32 paper = IM_COL32(245, 247, 250, 255)) {
    ImVec2 pts[5] = { c.P(0.20f, 0.08f), c.P(0.60f, 0.08f), c.P(0.80f, 0.28f),
                      c.P(0.80f, 0.92f), c.P(0.20f, 0.92f) };
    c.dl->AddConvexPolyFilled(pts, 5, paper);
    c.dl->AddPolyline(pts, 5, IM_COL32(120, 130, 145, 255), ImDrawFlags_Closed,
                      std::max(1.0f, c.s * 0.03f));
    c.dl->AddTriangleFilled(c.P(0.60f, 0.08f), c.P(0.60f, 0.28f), c.P(0.80f, 0.28f),
                            IM_COL32(200, 206, 215, 255));
}

void DrawFolder(const Canvas& c) {
    c.Rect(0.06f, 0.18f, 0.44f, 0.34f, IM_COL32(214, 158, 46, 255), 0.05f);   // tab
    c.Rect(0.06f, 0.26f, 0.94f, 0.84f, IM_COL32(214, 158, 46, 255), 0.06f);   // back
    c.Rect(0.06f, 0.36f, 0.94f, 0.86f, IM_COL32(250, 200, 80, 255), 0.06f);   // front
}

void DrawTextLines(const Canvas& c, float top, int count) {
    for (int i = 0; i < count; ++i) {
        float y = top + i * 0.12f;
        c.Line(0.30f, y, i == count - 1 ? 0.55f : 0.70f, y, INK, 0.04f);
    }
}

} // namespace

void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 pos, float size) {
    Canvas c{ dl, pos, size };

    switch (icon) {
    case Icon::Folder:
        DrawFolder(c);
        break;

    case Icon::File:
        DrawPage(c);
        break;

    case Icon::TextFile:
        DrawPage(c);
        DrawTextLines(c, 0.40f, 4);
        break;

    case Icon::ImageFile:
        DrawPage(c);
        c.Rect(0.28f, 0.36f, 0.72f, 0.80f, IM_COL32(110, 180, 240, 255));
        dl->AddTriangleFilled(c.P(0.28f, 0.80f), c.P(0.44f, 0.54f), c.P(0.60f, 0.80f), IM_COL32(70, 160, 80, 255));
        dl->AddTriangleFilled(c.P(0.48f, 0.80f), c.P(0.61f, 0.60f), c.P(0.72f, 0.80f), IM_COL32(50, 130, 65, 255));
        c.Circle(0.62f, 0.46f, 0.05f, IM_COL32(255, 220, 90, 255));
        break;

    case Icon::Program:
        c.Rect(0.10f, 0.18f, 0.90f, 0.82f, IM_COL32(225, 230, 238, 255), 0.06f);
        c.Rect(0.10f, 0.18f, 0.90f, 0.32f, ACCENT, 0.06f, ImDrawFlags_RoundCornersTop);
        c.Rect(0.18f, 0.42f, 0.48f, 0.72f, IM_COL32(170, 180, 195, 255));
        c.Line(0.56f, 0.46f, 0.82f, 0.46f, INK, 0.04f);
        c.Line(0.56f, 0.58f, 0.82f, 0.58f, INK, 0.04f);
        c.Line(0.56f, 0.70f, 0.74f, 0.70f, INK, 0.04f);
        break;

    case Icon::SystemFile:
        DrawPage(c, IM_COL32(215, 220, 228, 255));
        dl->AddCircle(c.P(0.50f, 0.58f), 0.14f * size, IM_COL32(90, 100, 115, 255), 16,
                      std::max(1.0f, 0.06f * size));
        c.Circle(0.50f, 0.58f, 0.04f, IM_COL32(90, 100, 115, 255));
        break;

    case Icon::Explorer:
        DrawFolder(c);
        c.Rect(0.30f, 0.60f, 0.70f, 0.74f, ACCENT, 0.03f);
        break;

    case Icon::Terminal: {
        c.Rect(0.08f, 0.14f, 0.92f, 0.86f, IM_COL32(30, 30, 36, 255), 0.10f);
        dl->AddRect(c.P(0.08f, 0.14f), c.P(0.92f, 0.86f), IM_COL32(110, 110, 125, 255),
                    0.10f * size, 0, std::max(1.0f, size * 0.03f));
        ImVec2 chevron[3] = { c.P(0.22f, 0.36f), c.P(0.38f, 0.50f), c.P(0.22f, 0.64f) };
        dl->AddPolyline(chevron, 3, IM_COL32(80, 230, 120, 255), ImDrawFlags_None, size * 0.07f);
        c.Line(0.44f, 0.66f, 0.70f, 0.66f, IM_COL32(80, 230, 120, 255), 0.07f);
        break;
    }

    case Icon::Notepad:
        c.Rect(0.18f, 0.08f, 0.82f, 0.92f, IM_COL32(245, 247, 250, 255), 0.06f);
        c.Rect(0.18f, 0.08f, 0.82f, 0.24f, ACCENT, 0.06f, ImDrawFlags_RoundCornersTop);
        DrawTextLines(c, 0.38f, 4);
        break;

    case Icon::TaskManager: {
        c.Rect(0.08f, 0.14f, 0.92f, 0.86f, IM_COL32(24, 52, 92, 255), 0.10f);
        ImVec2 graph[6] = { c.P(0.16f, 0.60f), c.P(0.32f, 0.60f), c.P(0.42f, 0.32f),
                            c.P(0.56f, 0.72f), c.P(0.66f, 0.48f), c.P(0.84f, 0.48f) };
        dl->AddPolyline(graph, 6, IM_COL32(90, 200, 255, 255), ImDrawFlags_None, size * 0.07f);
        break;
    }

    case Icon::Settings: {
        ImVec2 center = c.P(0.5f, 0.5f);
        ImU32  gear   = IM_COL32(170, 178, 192, 255);
        for (int i = 0; i < 8; ++i) {
            float a = i * PI / 4.0f;
            ImVec2 tip(center.x + std::cos(a) * size * 0.40f, center.y + std::sin(a) * size * 0.40f);
            dl->AddLine(center, tip, gear, size * 0.16f);
        }
        dl->AddCircleFilled(center, size * 0.30f, gear, 32);
        dl->AddCircleFilled(center, size * 0.12f, IM_COL32(60, 64, 76, 255), 24);
        break;
    }

    case Icon::SystemInfo:
        c.Circle(0.5f, 0.5f, 0.42f, ACCENT);
        c.Circle(0.5f, 0.30f, 0.06f, WHITE);
        c.Rect(0.44f, 0.42f, 0.56f, 0.74f, WHITE, 0.02f);
        break;

    case Icon::Start: {
        ImVec2 center = c.P(0.5f, 0.5f);
        dl->AddNgonFilled(center, size * 0.44f, IM_COL32(45, 156, 219, 255), 6);
        dl->AddNgon(center, size * 0.22f, WHITE, 6, size * 0.08f);
        break;
    }

    case Icon::Search:
        dl->AddCircle(c.P(0.42f, 0.42f), size * 0.24f, WHITE, 24, size * 0.08f);
        c.Line(0.60f, 0.60f, 0.84f, 0.84f, WHITE, 0.10f);
        break;

    case Icon::User:
        c.Circle(0.5f, 0.5f, 0.46f, IM_COL32(90, 100, 125, 255));
        c.Circle(0.5f, 0.38f, 0.15f, IM_COL32(220, 225, 235, 255));
        dl->PathArcTo(c.P(0.5f, 0.86f), size * 0.27f, PI, 2.0f * PI, 16);
        dl->PathFillConvex(IM_COL32(220, 225, 235, 255));
        break;

    case Icon::Power: {
        ImU32 red = IM_COL32(240, 80, 80, 255);
        dl->PathArcTo(c.P(0.5f, 0.54f), size * 0.30f, -PI * 0.5f + 0.75f, PI * 1.5f - 0.75f, 24);
        dl->PathStroke(red, ImDrawFlags_None, size * 0.09f);
        c.Line(0.5f, 0.14f, 0.5f, 0.50f, red, 0.09f);
        break;
    }
    }
}

Icon IconForNode(const FsNode* node) {
    switch (VirtualFS::KindOf(node)) {
    case FileKind::Folder: return Icon::Folder;
    case FileKind::Image:  return Icon::ImageFile;
    case FileKind::Binary: return Icon::SystemFile;
    case FileKind::Program:
        for (int i = 0; i < APP_COUNT; ++i)
            if (EqualsNoCase(node->name, GetAppInfo((AppID)i).exe))
                return GetAppInfo((AppID)i).icon;
        return Icon::Program;
    case FileKind::Text:
    default:
        return Icon::TextFile;
    }
}

bool IconButton(const char* id, Icon icon, ImVec2 size, float iconSize,
                bool highlighted, ImGuiButtonFlags flags)
{
    ImVec2 pos     = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::InvisibleButton(id, size, flags);
    bool   hovered = ImGui::IsItemHovered();
    bool   held    = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (held || hovered || highlighted) {
        ImU32 bg = held    ? IM_COL32(255, 255, 255, 45)
                 : hovered ? IM_COL32(255, 255, 255, 30)
                           : IM_COL32(255, 255, 255, 20);
        dl->AddRectFilled(pos, pos + size, bg, 6.0f);
    }
    DrawIcon(dl, icon, pos + (size - ImVec2(iconSize, iconSize)) * 0.5f, iconSize);
    return pressed;
}

bool IconSelectable(const char* label, Icon icon, bool selected, ImGuiSelectableFlags flags) {
    float  lineH = ImGui::GetTextLineHeight();
    ImVec2 pos   = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    bool pressed = ImGui::Selectable("##item", selected, flags, ImVec2(0, lineH));
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawIcon(dl, icon, pos, lineH);
    dl->AddText(ImVec2(pos.x + lineH + 6.0f, pos.y), ImGui::GetColorU32(ImGuiCol_Text), label);
    return pressed;
}

bool IconTile(const char* id, Icon icon, const char* label, bool selected,
              ImVec2 size, bool onWallpaper, bool* doubleClicked)
{
    ImVec2 pos     = ImGui::GetCursorScreenPos();
    bool   clicked = ImGui::InvisibleButton(id, size);
    bool   hovered = ImGui::IsItemHovered();
    if (doubleClicked)
        *doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        dl->AddRectFilled(pos, pos + size, IM_COL32(80, 140, 255, 90), 4.0f);
        dl->AddRect(pos, pos + size, IM_COL32(120, 170, 255, 160), 4.0f);
    } else if (hovered) {
        dl->AddRectFilled(pos, pos + size, IM_COL32(255, 255, 255, 40), 4.0f);
    }

    float iconSize = std::min(size.x, size.y) * 0.46f;
    DrawIcon(dl, icon, ImVec2(pos.x + (size.x - iconSize) * 0.5f, pos.y + 6.0f), iconSize);

    // Caption: up to two centered lines, the second one clipped with "...".
    ImFont*     font     = ImGui::GetFont();
    float       fontSize = ImGui::GetFontSize();
    float       wrapW    = size.x - 8.0f;
    const char* text     = label;
    const char* end      = label + strlen(label);
    float       y        = pos.y + 6.0f + iconSize + 4.0f;

    for (int line = 0; line < 2 && text < end; ++line) {
        const char* lineEnd = font->CalcWordWrapPositionA(1.0f, text, end, wrapW);
        if (lineEnd <= text) lineEnd = text + 1;
        std::string part = (line == 1) ? Ellipsize(std::string(text, end), wrapW)
                                       : Trim(std::string(text, lineEnd));

        float  w  = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, part.c_str()).x;
        ImVec2 tp(pos.x + (size.x - w) * 0.5f, y);
        if (onWallpaper)
            dl->AddText(font, fontSize, tp + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), part.c_str());
        dl->AddText(font, fontSize, tp,
                    onWallpaper ? WHITE : ImGui::GetColorU32(ImGuiCol_Text), part.c_str());

        y   += fontSize;
        text = lineEnd;
        while (text < end && *text == ' ') ++text;
    }
    return clicked;
}

std::string Ellipsize(const std::string& text, float maxWidth) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
        return text;
    std::string s = text;
    while (!s.empty() && ImGui::CalcTextSize((s + "...").c_str()).x > maxWidth)
        s.pop_back();
    return s + "...";
}
