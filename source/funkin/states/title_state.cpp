#include "funkin/states/title_state.hpp"

TitleState::TitleState() {
    this->logo = newSprite("ui/title/logo-bumpin", -35, -33);
    this->logo->animation.addAnimation("bump", "logo bumpin", 24, true);
    this->logo->animation.play("bump");
    this->logo->zorder = 10;

    this->gfTitle = newSprite("ui/title/gf-dance-title", 400*0.5, 240 * 0.2);
    this->gfTitle->animation.addAnimationByIndices("danceLeft", "gfDance", {30, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 24, false);
    this->gfTitle->animation.addAnimationByIndices("danceRight", "gfDance", {15, 16, 17, 18 , 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}, 24, false);
    this->gfTitle->animation.play("danceLeft", true, false);
    this->gfTitle->scaleX = 1.1;
    this->gfTitle->scaleY = 1.1;
    this->gfTitle->zorder = 1;
    
    addTop(this->logo);
    addTop(this->gfTitle);
}

void TitleState::update(engine::EngineContext& ctx, float dt) {
    this->logo->update(dt);
    this->gfTitle->update(dt);

    if (this->gfTitle->animation.isFinished()) {
        danced = !danced;
        if (danced)
            this->gfTitle->animation.play("danceRight", true, false);
        else
            this->gfTitle->animation.play("danceLeft", true, false);
    }
}

