# CSOPESY Desktop OS Emulator

A minimal, real-time graphical desktop operating system mockup built with C++, OpenGL, and Dear ImGui.

<!-- ABOUT THE PROJECT -->
## About The Project

CSOPESY Desktop OS Emulator is an interactive desktop operating system mockup designed to simulate core OS features and components in real time. It features a desktop shell with window management, a mock process scheduler, an in-memory virtual file system, and essential built-in utilities (Terminal, File Explorer, Task Manager, Notepad, and Settings).

### Built With

* [C++](https://en.cppreference.com/)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [OpenGL](https://www.opengl.org/) / [GLFW](https://www.glfw.org/)
* [CMake](https://cmake.org/)

<!-- GETTING STARTED -->
## Getting Started

Follow these steps to set up and run the OS emulator locally on Windows.

### Prerequisites

Make sure you have CMake and Visual Studio (MSVC) with C++ build tools installed.

* **CMake** (v3.16+)
  ```powershell
  cmake --version
  ```
  *(If not installed: `winget install kitware.cmake`)*

* **Visual Studio 2022** (with **"Desktop development with C++"** workload)
  ```powershell
  & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest
  ```

### Installation

1. Clone the repository
   ```powershell
   git clone https://github.com/zachhallare/CSOPESY-MO4-Group5.git
   cd CSOPESY-MO4-Group5
   ```

2. Configure the project
   ```powershell
   cmake -B build -S .
   ```
   *(CMake will automatically fetch GLFW and Dear ImGui).*

3. Build the executable
   ```powershell
   cmake --build build --config Release
   ```

4. Run the OS Emulator
   ```powershell
   Start-Process "build\Release\CSOPESY_OS_Emulator.exe"
   ```
   *(Or double-click `CSOPESY_OS_Emulator.exe` inside `build\Release`)*

<!-- USAGE EXAMPLES -->
## Usage

### Desktop Shell & Navigation
* **Taskbar:** Access the Start Menu, App Search, Process Scheduler controls (**INIT / START / STOP**), App shortcuts, and System Tray (**VOL / NET / PWR**).
* **Power Controls:** Click **PWR** (or close the window) to cleanly trigger the Shutdown / Restart menu.
* **Virtual File System:** Maintains an in-memory file system across session restarts. Custom images placed in `wallpapers/` can be accessed in `/Pictures` and selected in **Settings > Personalization**.
* **Terminal:** Launch the terminal and execute `help` to see all available commands.

### Configuration (`config.txt`)
Edit `config.txt` in the root folder to customize settings without recompiling:
* **Wallpaper:** Set custom file paths or select built-in styles (`builtin:aurora`, `builtin:sunset`, `builtin:grid`).
* **Durations:** Adjust BIOS boot time and OS loading screen duration.
* **System Settings:** Configure default username, clock display format, and scheduler process generation rates.

<!-- CONTACT -->
## Contact

**Group 5:**
* Zach Hallare
* Marga Ramos
* Neil Tan

Project Link: [https://github.com/zachhallare/CSOPESY-MO4-Group5](https://github.com/zachhallare/CSOPESY-MO4-Group5)

<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements

* [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut
* [GLFW](https://www.glfw.org/)
* [Best-README-Template](https://github.com/othneildrew/Best-README-Template)