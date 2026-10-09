// Main application class encapsulating the window and main loop.
#pragma once

#include <memory>
#include <map>
#include "core/iscreen.h"

struct GLFWwindow;
struct ImFont;

class Application {
public:
    Application();
    ~Application();

    bool Init();
    void Run();

private:
    GLFWwindow* window = nullptr;

    ImFont* monoFont = nullptr;
    ImFont* monoFontLarge = nullptr;
    ImFont* uiFont = nullptr;

    ScreenID currentScreenId = ScreenID::BOOT_BIOS;
    std::unique_ptr<IScreen> currentScreen;

    void SwitchScreen(ScreenID nextScreenId);
};
