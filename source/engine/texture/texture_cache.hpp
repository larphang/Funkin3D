#pragma once

#include "texture_type.hpp"
#include <map>
#include <string>

class TextureCacheFactory {
public:
    static TextureCacheFactory& get() {
        static TextureCacheFactory instance;
        return instance;
    }

    TextureType* getTexture(const std::string& path) {
        std::string texPath = path;

#ifdef __3DS__
        texPath = "romfs:/assets/" + texPath;
        texPath += ".t3x";
#else
        texPath = "assets/" + texPath;
        texPath += ".png";
#endif

        auto it = cache.find(texPath);
        if (it != cache.end())
            return &it->second;

        TextureType texture = loadTexture(texPath);
        if (!texture)
            return nullptr;

        addTexture(texPath, texture);

        return &cache.at(texPath);
    }

    void addTexture(const std::string& path, const TextureType& texture) {
        cache[path] = texture;
    }

private:
    TextureCacheFactory() = default;

    std::map<std::string, TextureType> cache;

    TextureType loadTexture(const std::string& path) {
#ifdef __3DS__
        return C2D_SpriteSheetLoad(path.c_str());
#else
        return nullptr;
#endif
    }
};