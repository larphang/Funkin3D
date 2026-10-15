#ifndef __3DS__

#include "engine_context.hpp"

void EngineContext::draw() {
    if (m_state)
        m_state->render(*this);
}

#endif
