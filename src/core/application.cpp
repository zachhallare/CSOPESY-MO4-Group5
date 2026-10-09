#include "application.h"

#include <cstdio>
#include <cstdlib>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "screens/bios_screen.h"
#include "screens/loading_screen.h"
#include "screens/desktop.h"

Application::Application() {}

Application::~Application() {
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool Application::Init() {
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

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 3.0f;
    style.ScrollbarRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    monoFont = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\consola.ttf", 16.0f);
    if (!monoFont) monoFont = io.Fonts->AddFontDefault();

    monoFontLarge = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\consola.ttf", 42.0f);
    if (!monoFontLarge) monoFontLarge = monoFont;

    uiFont = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    if (!uiFont) uiFont = io.Fonts->AddFontDefault();

    SwitchScreen(ScreenID::BOOT_BIOS);
    return true;
}

void Application::SwitchScreen(ScreenID nextScreenId) {
    currentScreenId = nextScreenId;
    currentScreen.reset();

    switch (nextScreenId) {
        case ScreenID::BOOT_BIOS:
            currentScreen = std::make_unique<BiosScreen>(monoFont);
            break;
        case ScreenID::BOOT_LOADING:
            currentScreen = std::make_unique<LoadingScreen>(monoFont, monoFontLarge);
            break;
        case ScreenID::DESKTOP:
            currentScreen = std::make_unique<DesktopScreen>(uiFont, "wallpapers/dog.jpg");
            break;
        default:
            break;
    }
}

void Application::Run() {
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentTime = (float)glfwGetTime();
        float deltaTime   = currentTime - lastTime;
        lastTime = currentTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (currentScreen) {
            ScreenID nextScreenId = currentScreen->UpdateAndRender(deltaTime);
            if (nextScreenId == ScreenID::EXIT) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            } else if (nextScreenId != ScreenID::NONE && nextScreenId != currentScreenId) {
                SwitchScreen(nextScreenId);
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
    }
}
