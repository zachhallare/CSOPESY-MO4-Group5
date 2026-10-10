// Loads images into OpenGL textures on first use and keeps them until the
// cache is destroyed. Failed loads are remembered so they aren't retried
// every frame.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include "imgui.h"

struct Texture {
    unsigned int id     = 0;   // GLuint
    int          width  = 0;
    int          height = 0;
    bool         ok     = false;

    ImTextureID ImId() const { return (ImTextureID)(intptr_t)id; }
};

class TextureCache {
public:
    TextureCache() = default;
    ~TextureCache();
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    const Texture& Get(const std::string& path);

private:
    std::map<std::string, Texture> textures;
};
