#include "texture_cache.h"
#include "core/config.h"
#include "core/texture_loader.h"

#include <cstdio>

TextureCache::~TextureCache() {
    for (auto& entry : textures) {
        if (entry.second.ok) {
            GLuint id = entry.second.id;
            glDeleteTextures(1, &id);
        }
    }
}

const Texture& TextureCache::Get(const std::string& path) {
    auto it = textures.find(path);
    if (it != textures.end())
        return it->second;

    Texture tex;
    GLuint id = 0;
    tex.ok = LoadTextureFromFile(ResolveAssetPath(path).c_str(), &id, &tex.width, &tex.height);
    tex.id = id;
    if (!tex.ok)
        fprintf(stderr, "Could not load image: %s\n", path.c_str());

    return textures.emplace(path, tex).first->second;
}
