#include "engine/engine_context.hpp"
#include "engine/texture/texture_cache.hpp"
#include "funkin/states/title_state.hpp"
#include "engine/util/file_handler.hpp"
#include "engine/util/log.hpp"

int main() {
	engine::EngineContext game;
	engine::util::Log::get().init();
	engine::io::FileHandler::initialize();
	
	game.init("Funkin3DS");
	game.changeState(std::make_unique<TitleState>());
	game.runLoop(&game);

	engine::util::Log::get().close();
	engine::io::FileHandler::shutdown();

	return 0;
}
