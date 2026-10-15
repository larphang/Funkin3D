#pragma once
#include "basic.hpp"

namespace engine {
namespace objects {

class Text : public Basic {
public:
    std::string content;
    std::string fontName;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float alpha = 1.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    int originX = 0;
    int originY = 0;
    int x = 0;
    int y = 0;
    bool visible = true;

    Text(const std::string& text, const std::string& font = "default")
        : content(text), fontName(font) {}

    void render() override {
        if (!visible) return;



        float r = color[0];
        float g = color[1];
        float b = color[2];

       
    }

    void setColor(float r = 1.0f, float g = 1.0f, float b = 1.0f) {
        color[0] = r;
        color[1] = g;
        color[2] = b;
    }
};

} // namespace Objects
} // namespace Core
