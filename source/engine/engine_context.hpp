#pragma once

#include <string>
#include <engine/state.hpp>

namespace engine {
class EngineContext {
public:
    bool running = false;

    bool init(std::string id);
    void runLoop(EngineContext* ctx);
    void changeState(std::unique_ptr<State>);
    void update(double dt);
    void draw();

    void exit();
private:
    std::unique_ptr<State> m_state;
protected:
};

}