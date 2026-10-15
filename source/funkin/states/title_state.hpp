#pragma once

#include <engine/objects/sprite.hpp>
#include <engine/macros.hpp>
#include <engine/engine_context.hpp>
#include <engine/state.hpp>
#include <memory>

class TitleState : public engine::State {
public:
    TitleState();

    void update(engine::EngineContext& ctx, float dt) override;

private:
    FunkinSprite logo;
    FunkinSprite gfTitle;

    bool danced = false;
};