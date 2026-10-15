
#ifdef __3DS__

#include "engine_context.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

namespace engine {
namespace {

C3D_RenderTarget* topTarget = nullptr;
C3D_RenderTarget* bottomTarget = nullptr;

bool initGraphics() {
    gfxInitDefault();

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return false;
    }

    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return false;
    }

    C2D_Prepare();

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    if (!topTarget || !bottomTarget) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();

        topTarget = nullptr;
        bottomTarget = nullptr;
        return false;
    }

    return true;
}

void shutdownGraphics() {
    C2D_Fini();
    C3D_Fini();

    topTarget = nullptr;
    bottomTarget = nullptr;

    gfxExit();
}

void beginTarget(C3D_RenderTarget* target) {
    C2D_TargetClear(target, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(target);
}

}

bool EngineContext::init(std::string id) {
    return initGraphics();
}

void EngineContext::runLoop(EngineContext* ctx) {
    while (aptMainLoop()) {
        hidScanInput();

        if (hidKeysDown() & KEY_START)
            break;

        ctx->update(1.0f / 60.0f);
        ctx->draw();
    }
}

void EngineContext::draw() {
    if (!topTarget || !bottomTarget)
        return;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    beginTarget(topTarget);
    if (m_state)
        m_state->renderTop(*this);

    beginTarget(bottomTarget);
    if (m_state)
        m_state->renderBottom(*this);

    C3D_FrameEnd(0);
}

void EngineContext::exit() {
    shutdownGraphics();
}

}
#endif
