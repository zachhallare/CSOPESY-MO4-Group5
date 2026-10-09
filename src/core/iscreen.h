// Interface for all screens in the application.
#pragma once

enum class ScreenID {
    NONE,
    BOOT_BIOS,
    BOOT_LOADING,
    DESKTOP,
    EXIT
};

class IScreen {
public:
    virtual ~IScreen() = default;

    // Renders the screen and returns the next screen to transition to.
    // Return ScreenID::NONE to stay on the current screen.
    // Return ScreenID::EXIT to shut down the application.
    virtual ScreenID UpdateAndRender(float deltaTime) = 0;
};
