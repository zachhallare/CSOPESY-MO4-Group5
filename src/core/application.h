// Main application class encapsulating the window and main loop.
#pragma once

#include <memory>
#include "core/config.h"
#include "core/fonts.h"
#include "core/iscreen.h"

struct GLFWwindow;
class VirtualFS;

class Application {
public:
    Application();
    ~Application();

    bool Init();
    void Run();

private:
    GLFWwindow* window = nullptr;
    Fonts       fonts;
    Config      config;

    // Owned here rather than by the desktop so files survive a restart.
    std::unique_ptr<VirtualFS> fileSystem;

    ScreenID currentScreenId = ScreenID::BOOT_BIOS;
    std::unique_ptr<IScreen> currentScreen;

    void SwitchScreen(ScreenID nextScreenId);
};
