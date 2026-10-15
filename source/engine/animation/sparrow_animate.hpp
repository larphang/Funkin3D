
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <engine/util/log.hpp>

class SparrowFrame {
public:
    std::string name;

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    int qw = 0;
    int qh = 0;

    int sw = 0;
    int sh = 0;

    int ox = 0;
    int oy = 0;

    bool rotated = false;

    SparrowFrame() = default;

    SparrowFrame(std::string name,
        int x, int y,
        int width, int height,
        int sheetWidth, int sheetHeight,
        int ox = 0, int oy = 0,
        int oWidth = -1, int oHeight = -1,
        bool rotated = false
    )
        : name(std::move(name)),
            x(x),
            y(y),
            w(oWidth >= 0 ? oWidth : width),
            h(oHeight >= 0 ? oHeight : height),
            qw(std::max(0, std::min(width, sheetWidth - x))),
            qh(std::max(0, std::min(height, sheetHeight - y))),
            sw(sheetWidth),
            sh(sheetHeight),
            ox(ox),
            oy(oy),
            rotated(rotated) 
        {
        if (qw != width || qh != height) {
            LOG_WARNING("SparrowFrame: '%s' exceeds sheet bounds (x=%d y=%d w=%d h=%d sheet=%dx%d)",
                this->name.c_str(), x, y, width, height, sheetWidth, sheetHeight
            );
        }
    }
};

class SparrowAnimate {
public:
    struct Animation {
        std::string name;
        float framerate = 24.0f;
        bool looped = true;
        std::vector<std::size_t> frames;
    };

    std::vector<SparrowFrame> frames;
    std::vector<Animation> animations;

    const SparrowFrame* currentFrame() const {
        if (frames.empty()) {
            LOG_WARNING("SparrowAnimate: currentFrame() called with no frames");
            return nullptr;
        }

        if (currentAnimation < 0 ||
            static_cast<std::size_t>(currentAnimation) >= animations.size()) {
            return &frames.front();
        }

        const Animation& anim = animations[currentAnimation];

        if (anim.frames.empty() || currentFrameIndex >= anim.frames.size()) {
            LOG_WARNING("SparrowAnimate: invalid frame index %zu for animation '%s'", currentFrameIndex, anim.name.c_str());
            return nullptr;
        }

        const std::size_t index = anim.frames[currentFrameIndex];

        if (index >= frames.size()) {
            LOG_ERROR("SparrowAnimate: animation '%s' references invalid frame %zu", anim.name.c_str(), index);
            return nullptr;
        }

        return &frames[index];
    }

    bool addAnimation(std::string name, std::string_view prefix, float framerate = 24.0f, bool looped = true) {
        Animation anim;
        anim.name = std::move(name);
        anim.framerate = framerate > 0.0f ? framerate : 24.0f;
        anim.looped = looped;

        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].name.compare(0, prefix.size(), prefix) == 0)
                anim.frames.push_back(i);
        }

        if (anim.frames.empty()) {
            LOG_WARNING(
                "SparrowAnimate: no frames match prefix '%.*s' for animation '%s' (available frames: %zu)",
                static_cast<int>(prefix.size()), prefix.data(), anim.name.c_str(), frames.size()
            );
            return false;
        }

        std::stable_sort(
            anim.frames.begin(),
            anim.frames.end(),
            [this](std::size_t a, std::size_t b) {
                const auto number = [](const std::string& name) {
                    std::size_t end = name.size();
                    std::size_t start = end;

                    while (start > 0 && name[start - 1] >= '0' && name[start - 1] <= '9') {
                        --start;
                    }

                    unsigned long value = 0;

                    for (std::size_t i = start; i < end; ++i)
                        value = value * 10 + (name[i] - '0');

                    return std::pair<std::string, unsigned long>(name.substr(0, start), value);
                };

                return number(frames[a].name) < number(frames[b].name);
            }
        );

        for (Animation& existing : animations) {
            if (existing.name == anim.name) {
                existing = std::move(anim);
                return true;
            }
        }

        animations.push_back(std::move(anim));
        return true;
    }

    bool addAnimationByIndices(std::string name, std::string_view prefix, const std::vector<std::size_t>& indices, float framerate = 24.0f, bool looped = true) {
        Animation anim;
        anim.name = std::move(name);
        anim.framerate = framerate > 0.0f ? framerate : 24.0f;
        anim.looped = looped;

        std::vector<std::size_t> matching;

        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].name.compare(0, prefix.size(), prefix) == 0)
                matching.push_back(i);
        }

        std::stable_sort(
            matching.begin(),
            matching.end(),
            [this](std::size_t a, std::size_t b) {
                return frames[a].name < frames[b].name;
            }
        );

        for (std::size_t index : indices) {
            if (index < matching.size()) {
                anim.frames.push_back(matching[index]);
            } else {
                LOG_WARNING(
                    "SparrowAnimate: index %zu is out of range for prefix '%.*s' (%zu matching frames)",
                    index, static_cast<int>(prefix.size()), prefix.data(), matching.size()
                );
            }
        }

        if (anim.frames.empty()) {
            return false;
        }

        for (Animation& existing : animations) {
            if (existing.name == anim.name) {
                existing = std::move(anim);

                return true;
            }
        }

        animations.push_back(std::move(anim));
        return true;
    }

    bool hasAnimation(std::string_view name) const {
        return findAnimation(name) >= 0;
    }

    bool play(std::string_view name, bool force = false, bool looped = true, std::size_t startFrame = 0) {
        const int index = findAnimation(name);

        if (index < 0) {
            LOG_WARNING("SparrowAnimate: cannot play missing animation '%.*s' (registered animations: %zu)",
                static_cast<int>(name.size()), name.data(), animations.size()
            );
            return false;
        }

        if (!force && currentAnimation == index && !finished) {
            paused = false;
            return true;
        }

        currentAnimation = index;

        Animation& anim = animations[index];
        anim.looped = looped;

        currentFrameIndex = startFrame < anim.frames.size() ? startFrame : 0;

        timer = 0.0f;
        finished = false;
        paused = false;

        return true;
    }

    void update(float dt) {
        if (paused || finished || currentAnimation < 0 || dt <= 0.0f)
            return;

        if (static_cast<std::size_t>(currentAnimation) >= animations.size()) {
            LOG_ERROR("SparrowAnimate: current animation index is invalid");
            currentAnimation = -1;
            return;
        }

        Animation& anim = animations[currentAnimation];

        if (anim.frames.empty()) {
            LOG_WARNING("SparrowAnimate: animation '%s' contains no frames",
                anim.name.c_str()
            );
            return;
        }

        timer += dt;

        const float frameDuration = 1.0f / (anim.framerate > 0.0f ? anim.framerate : 24.0f);

        while (timer >= frameDuration) {
            timer -= frameDuration;
            ++currentFrameIndex;

            if (currentFrameIndex >= anim.frames.size()) {
                if (anim.looped) {
                    currentFrameIndex = 0;
                } else {
                    currentFrameIndex = anim.frames.size() - 1;
                    finished = true;
                    timer = 0.0f;

                    break;
                }
            }
        }
    }

    void stop() {
        paused = true;
        timer = 0.0f;
    }

    void resume() {
        if (currentAnimation < 0) {
            return;
        }

        paused = false;
    }

    void reset() {
        currentFrameIndex = 0;
        timer = 0.0f;
        finished = false;
    }

    const Animation* getCurrentAnimation() const {
        if (currentAnimation < 0 ||
            static_cast<std::size_t>(currentAnimation) >= animations.size()) {
            return nullptr;
        }

        return &animations[currentAnimation];
    }

    std::size_t getCurrentFrameIndex() const {
        return currentFrameIndex;
    }

    bool isFinished() const {
        return finished;
    }

    bool isPlaying() const {
        return !paused && currentAnimation >= 0 && !finished;
    }

    void clear() {
        frames.clear();
        animations.clear();
        currentAnimation = -1;
        currentFrameIndex = 0;
        timer = 0.0f;
        paused = false;
        finished = false;
    }

private:
    int currentAnimation = -1;
    std::size_t currentFrameIndex = 0;
    float timer = 0.0f;

    bool paused = false;
    bool finished = false;

    int findAnimation(std::string_view name) const {
        for (std::size_t i = 0; i < animations.size(); ++i) {
            if (animations[i].name == name)
                return static_cast<int>(i);
        }

        return -1;
    }
};
