#include "config.h"
#include "core/strings.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

static bool PathExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string ResolveAssetPath(const std::string& relativePath) {
    if (relativePath.empty() || PathExists(relativePath))
        return relativePath;

#ifdef _WIN32
    char exePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string dir(exePath, len);
        size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) {
            std::string candidate = dir.substr(0, slash + 1) + relativePath;
            if (PathExists(candidate))
                return candidate;
        }
    }
#endif
    return relativePath;
}

std::string CanonicalPath(const std::string& path) {
    std::error_code ec;
    std::filesystem::path canonical =
        std::filesystem::weakly_canonical(ResolveAssetPath(path), ec);
    return ec ? path : canonical.string();
}

static float ParseFloat(const std::string& value, float fallback) {
    char* end = nullptr;
    float v = std::strtof(value.c_str(), &end);
    return (end != value.c_str() && v >= 0.0f) ? v : fallback;
}

static bool ParseBool(const std::string& value, bool fallback) {
    std::string v = ToLower(value);
    if (v == "true"  || v == "1" || v == "yes" || v == "on")  return true;
    if (v == "false" || v == "0" || v == "no"  || v == "off") return false;
    return fallback;
}

Config Config::Load(const std::string& path) {
    Config cfg;
    std::ifstream in(ResolveAssetPath(path));
    if (!in) {
        fprintf(stderr, "%s not found, using default settings\n", path.c_str());
        return cfg;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        // Accepts "key value", "key = value" and "key=value".
        size_t sep = line.find_first_of(" \t=");
        std::string key   = ToLower(line.substr(0, sep));
        std::string value = (sep == std::string::npos) ? "" : Trim(line.substr(sep));
        if (!value.empty() && value[0] == '=')
            value = Trim(value.substr(1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);

        if      (key == "wallpaper")              cfg.wallpaper            = value;
        else if (key == "bios-duration")          cfg.biosDuration         = ParseFloat(value, cfg.biosDuration);
        else if (key == "loading-duration")       cfg.loadingDuration      = ParseFloat(value, cfg.loadingDuration);
        else if (key == "username")               cfg.username             = value.empty() ? cfg.username : value;
        else if (key == "clock-24h")              cfg.clock24h             = ParseBool(value, cfg.clock24h);
        else if (key == "clock-seconds")          cfg.clockSeconds         = ParseBool(value, cfg.clockSeconds);
        else if (key == "process-spawn-interval") cfg.processSpawnInterval = ParseFloat(value, cfg.processSpawnInterval);
        else fprintf(stderr, "%s: unknown key '%s'\n", path.c_str(), key.c_str());
    }

    if (cfg.processSpawnInterval < 0.1f)
        cfg.processSpawnInterval = 0.1f;
    return cfg;
}
