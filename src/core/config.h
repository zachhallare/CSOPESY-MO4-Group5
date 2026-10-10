// Runtime parameters loaded from config.txt so the emulator can be tuned
// without recompiling. Keys that are missing keep their default values.
#pragma once

#include <string>

struct Config {
    std::string wallpaper            = "wallpapers/dog.jpg";  // image path or builtin:aurora / builtin:sunset / builtin:grid
    float       biosDuration         = 4.5f;   // seconds on the BIOS screen
    float       loadingDuration      = 3.5f;   // seconds on the splash screen
    std::string username             = "csopesy";
    bool        clock24h             = false;
    bool        clockSeconds         = false;
    float       processSpawnInterval = 2.0f;   // seconds between processes generated after START

    static Config Load(const std::string& path);
};

// Finds a file relative to the working directory, falling back to the folder
// holding the executable, so assets load no matter where the IDE launches us.
std::string ResolveAssetPath(const std::string& relativePath);

// Absolute, normalized form of a path, used to compare and cache image files.
std::string CanonicalPath(const std::string& path);
