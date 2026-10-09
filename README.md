# CSOPESY Desktop OS Emulator

A minimal, real-time graphical desktop OS mockup built with C++, OpenGL, and Dear ImGui.

## How to Build and Run (Windows)

You need to have **CMake** and **Visual Studio** (or MSVC build tools) installed on your system.

### 0. Check Prerequisites
To verify you have the necessary tools installed, run these commands in your terminal:

**Check CMake:**
```powershell
cmake --version
```
*(If you get an error, you can install it quickly by running `winget install kitware.cmake` and restarting your terminal).*

**Check Visual Studio:**
```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest
```
*(If you get an error or no output, download Visual Studio 2022 from Microsoft's website. Make sure to select the **"Desktop development with C++"** workload during installation).*

### 1. Configure the project
Open your terminal (PowerShell or Command Prompt) in this project folder and run:
```powershell
cmake -B build -S .
```
*(This will download the required libraries like GLFW and ImGui automatically).*

### 2. Build the executable
Compile the project by running:
```powershell
cmake --build build --config Release
```

### 3. Run the OS Emulator
Once the build is complete, you can launch the app directly from the terminal:
```powershell
Start-Process "build\Release\CSOPESY_OS_Emulator.exe"
```
*(Alternatively, you can just double-click the `.exe` file inside the `build\Release` folder).*