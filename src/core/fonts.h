// Fonts loaded once at startup and shared by every screen.
#pragma once

struct ImFont;

struct Fonts {
    ImFont* ui        = nullptr;  // Segoe UI 16px: general interface text
    ImFont* uiSmall   = nullptr;  // Segoe UI 13px: taskbar labels
    ImFont* uiLarge   = nullptr;  // Segoe UI 24px: headings
    ImFont* mono      = nullptr;  // Consolas 16px: BIOS, terminal, editor
    ImFont* monoLarge = nullptr;  // Consolas 42px
};
