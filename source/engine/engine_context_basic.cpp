#include "engine_context.hpp"

namespace engine {

void EngineContext::changeState(std::unique_ptr<State> newState) {
    if (m_state)
        m_state->leave(*this);

    m_state = std::move(newState);

    if (m_state)
        m_state->enter(*this);
}

void EngineContext::update(double dt) {
    if (m_state)
        m_state->update(*this, static_cast<float>(dt));
}

}