#include "application.h"

#include <cstdio>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "fs/virtual_fs.h"
#include "screens/bios_screen.h"
#include "screens/loading_screen.h"
#include "screens/desktop.h"
#include "screens/shutdown_screen.h"

Application::Application() {}

Application::~Application() {
    // Screens own GL textures, so they go before the GL context does.
    currentScreen.reset();

    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

static ImFont* LoadFont(ImGuiIO& io, const char* path, float size, ImFont* fallback) {
    ImFont* font = io.Fonts->AddFontFromFileTTF(path, size);
    return font ? font : fallback;
}

bool Application::Init() {
    config = Config::Load("config.txt");

    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(
        1280, 720, "CSOPESY Desktop OS Emulator", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;                     // every boot starts with a clean window layout
    io.ConfigWindowsMoveFromTitleBarOnly = true;  // drag windows by their title bar, like a real OS

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.PopupRounding     = 6.0f;
    style.TabRounding       = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    fonts.mono      = LoadFont(io, "C:\\Windows\\Fonts\\consola.ttf", 16.0f, nullptr);
    if (!fonts.mono) fonts.mono = io.Fonts->AddFontDefault();
    fonts.monoLarge = LoadFont(io, "C:\\Windows\\Fonts\\consola.ttf", 42.0f, fonts.mono);
    fonts.ui        = LoadFont(io, "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, fonts.mono);
    fonts.uiSmall   = LoadFont(io, "C:\\Windows\\Fonts\\segoeui.ttf", 13.0f, fonts.ui);
    fonts.uiLarge   = LoadFont(io, "C:\\Windows\\Fonts\\segoeui.ttf", 24.0f, fonts.ui);

    fileSystem = std::make_unique<VirtualFS>();

    SwitchScreen(ScreenID::BOOT_BIOS);
    return true;
}

void Application::SwitchScreen(ScreenID nextScreenId) {
    currentScreenId = nextScreenId;
    currentScreen.reset();

    switch (nextScreenId) {
        case ScreenID::BOOT_BIOS:
            currentScreen = std::make_unique<BiosScreen>(fonts.mono, config.biosDuration);
            break;
        case ScreenID::BOOT_LOADING:
            currentScreen = std::make_unique<LoadingScreen>(fonts.mono, fonts.monoLarge, config.loadingDuration);
            break;
        case ScreenID::DESKTOP:
            currentScreen = std::make_unique<DesktopScreen>(fonts, config, *fileSystem);
            break;
        case ScreenID::SHUTDOWN:
        case ScreenID::RESTART:
            currentScreen = std::make_unique<ShutdownScreen>(fonts, nextScreenId == ScreenID::RESTART);
            break;
        default:
            break;
    }
}

void Application::Run() {
    float lastTime = (float)glfwGetTime();
    bool  running  = true;

    while (running) {
        glfwPollEvents();

        // The X button must not force-exit the OS: the current screen decides
        // what happens (the desktop opens the same dialog as PWR).
        if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            if (currentScreen)
                currentScreen->OnCloseRequested();
        }

        float currentTime = (float)glfwGetTime();
        float deltaTime   = currentTime - lastTime;
        lastTime = currentTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ScreenID pendingScreen = ScreenID::NONE;
        if (currentScreen) {
            ScreenID nextScreenId = currentScreen->UpdateAndRender(deltaTime);
            if (nextScreenId == ScreenID::EXIT) {
                running = false;
            } else if (nextScreenId != ScreenID::NONE && nextScreenId != currentScreenId) {
                pendingScreen = nextScreenId;
            }
        }

        ImGui::Render();

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        // Switch only after this frame is drawn: its draw data may still
        // reference textures owned by the outgoing screen.
        if (pendingScreen != ScreenID::NONE)
            SwitchScreen(pendingScreen);
    }
}
