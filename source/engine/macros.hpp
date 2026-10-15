#pragma once

#include <memory>
#include <string>
#include <engine/objects/sprite.hpp>

inline std::shared_ptr<engine::objects::Sprite> newSprite(const std::string& path) {
    return std::make_shared<engine::objects::Sprite>(path, 0.0f, 0.0f);
}

inline std::shared_ptr<engine::objects::Sprite> newSprite(const std::string& path, float x) {
    return std::make_shared<engine::objects::Sprite>(path, x, 0.0f);
}

inline std::shared_ptr<engine::objects::Sprite> newSprite(const std::string& path, float x, float y) {
    return std::make_shared<engine::objects::Sprite>(path, x, y);
}

#define FunkinSprite std::shared_ptr<engine::objects::Sprite>
