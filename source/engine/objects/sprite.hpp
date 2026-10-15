
#pragma once

#include <engine/texture/texture_cache.hpp>
#include <engine/parsers/fastxml.hpp>
#include <engine/util/file_handler.hpp>
#include <engine/util/log.hpp>
#include <engine/animation/sparrow_animate.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#ifdef __3DS__
#include <citro2d.h>
#endif

namespace engine::objects {

class Sprite {
public:
    float x = 0.0f;
    float y = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    float zorder = 0.0f;

    SparrowAnimate animation{};

    explicit Sprite(const std::string& texturePath, float x = 0.0f, float y = 0.0f) : x(x), y(y), texturePath(texturePath) {

#ifdef __3DS__
        texture = TextureCacheFactory::get().getTexture(texturePath);

        if (texture) {
            atlasImage = C2D_SpriteSheetGetImage(*texture, 0);
            C2D_SpriteFromSheet(&sprite, *texture, 0);
        } else {
            LOG_ERROR("Sprite: Failed to load texture: '%s'", texturePath.c_str());
        }
#else // TODO: Desktop
#endif

        loadSparrow(texturePath);
    }

    void update(float dt) {
        animation.update(dt);
    }

    void render() {
    #ifdef __3DS__
        if (!texture || !atlasImage.tex)
            return;

        const SparrowFrame* frame = animation.currentFrame();

        if (!frame) {
            C2D_SpriteSetPos(&sprite, x, y);
            C2D_SpriteSetScale(&sprite, scaleX, scaleY);
            C2D_SpriteSetRotation(&sprite, rotation);
            C2D_DrawSprite(&sprite);
            return;
        }

        const int textureWidth = atlasImage.tex->width;
        const int textureHeight = atlasImage.tex->height;

        if (textureWidth <= 0 || textureHeight <= 0 ||
            frame->x < 0 || frame->y < 0 ||
            frame->qw <= 0 || frame->qh <= 0 ||
            frame->x + frame->qw > textureWidth ||
            frame->y + frame->qh > textureHeight) {
            LOG_ERROR("Sprite: Invalid frame '%s': x=%d y=%d w=%d h=%d atlas=%dx%d", frame->name.c_str(), frame->x, frame->y, frame->qw, frame->qh, textureWidth, textureHeight);
            return;
        }

        const Tex3DS_SubTexture* sub = atlasImage.subtex;

        if (!sub || sub->width == 0 || sub->height == 0)
            return;

        const float rw = sub->right - sub->left;
        const float rh = sub->bottom - sub->top;

        const float refW = static_cast<float>(sub->width);
        const float refH = static_cast<float>(sub->height);

        frameSubTexture = {};
        frameSubTexture.width = static_cast<u16>(frame->qw);
        frameSubTexture.height = static_cast<u16>(frame->qh);

        frameSubTexture.left = sub->left + static_cast<float>(frame->x) * rw / refW;
        frameSubTexture.top = sub->top + static_cast<float>(frame->y) * rh / refH;
        frameSubTexture.right = sub->left + static_cast<float>(frame->x + frame->qw) * rw / refW;
        frameSubTexture.bottom = sub->top +  static_cast<float>(frame->y + frame->qh) * rh / refH;

        C2D_Image image = { atlasImage.tex, &frameSubTexture };

        const float drawX = x - frame->ox * std::abs(scaleX);
        const float drawY = y - frame->oy * std::abs(scaleY);

        if (frame->rotated) {
            const float centerX = drawX + frame->qh * std::abs(scaleX) * 0.5f;
            const float centerY = drawY + frame->qw * std::abs(scaleY) * 0.5f;

            C2D_DrawImageAtRotated(image, centerX, centerY, 0.0f, -1.57079632679f, nullptr, scaleX, scaleY);
        } else if (rotation == 0.0f) {
            C2D_DrawImageAt(image, drawX, drawY, 0.0f, nullptr, scaleX, scaleY);
        } else {
            C2D_DrawImageAtRotated(image, drawX, drawY, 0.0f, rotation, nullptr, scaleX, scaleY);
        }
    #endif
    }

private:
    std::string texturePath;
#ifdef __3DS__
    TextureType* texture = nullptr;
    C2D_Sprite sprite{};
    C2D_Image atlasImage{};
    Tex3DS_SubTexture frameSubTexture{};
#endif

    static int getInt(const fastxml::Node& node, std::string_view key, int fallback = 0) {
        const std::string_view value = node.attribute(key);

        if (value.empty())
            return fallback;

        std::string text(value);
        char* end = nullptr;
        errno = 0;

        const float result = std::strtof(text.c_str(), &end);

        if (errno == ERANGE || end == text.c_str() ||
            *end != '\0' || !std::isfinite(result)) {
            return fallback;
        }

        return static_cast<int>(std::lround(result));
    }

    void loadSparrow(const std::string& path) {
        const std::string xmlPath = "assets/" + path + ".xml";
        std::string xml;
        LOG_INFO(xmlPath.c_str());
        if (!engine::io::FileHandler::readText(xmlPath, xml)) {
            LOG_ERROR("Could not find %s", xmlPath.c_str());
            return;
        }

        try {
            const fastxml::Node root = fastxml::parse(xml);
            std::size_t loadedFrames = 0;

            for (const fastxml::Node* subTexture : root.childrenNamed("SubTexture")) {
                const std::string name(subTexture->attribute("name"));

                const int frameX = getInt(*subTexture, "x");
                const int frameY = getInt(*subTexture, "y");
                const int frameWidth = getInt(*subTexture, "width");
                const int frameHeight = getInt(*subTexture, "height");

                if (name.empty() || frameWidth <= 0 || frameHeight <= 0) {
                    LOG_WARNING(
                        "Sprite: Skipping invalid SubTexture in '%s'",
                        xmlPath.c_str()
                    );
                    continue;
                }

                const int originalWidth = getInt(*subTexture, "frameWidth", frameWidth);
                const int originalHeight = getInt(*subTexture, "frameHeight", frameHeight);

                const int offsetX = getInt(*subTexture, "frameX");
                const int offsetY = getInt(*subTexture, "frameY");

                const bool rotated = subTexture->attribute("rotated") == "true";

                animation.frames.emplace_back(name, frameX, frameY, frameWidth, frameHeight, 65535, 65535, offsetX, offsetY, originalWidth, originalHeight, rotated);

                ++loadedFrames;
            }

            if (loadedFrames == 0)
                LOG_WARNING("Sprite: No SubTexture elements found in '%s'", xmlPath.c_str());
        } catch (const fastxml::ParseError& error) {
            animation.clear();

            LOG_ERROR("Sprite: XML parse error in '%s' at offset %zu: %s", xmlPath.c_str(), error.offset, error.what());
        } catch (const std::exception& error) {
            animation.clear();

            LOG_ERROR("Sprite: Failed to load '%s': %s", xmlPath.c_str(), error.what());
        }
    }
};

}
