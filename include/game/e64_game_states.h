#ifndef ENGINE64_GAME_STATES_H
#define ENGINE64_GAME_STATES_H

#include <stdint.h>

#include "game/e64_game.h"

namespace e64 {

namespace game {

namespace state {

/* Hands the engine the game's state table and loads the initial state.
   Runs after game::init and the game's own inits, before the first runStep. */
void start(const Game::State::Def *table, uint8_t count, Game::State::ID initial);

const Game::State::Def *get(Game::State::ID id);

void set(Game::State::ID new_state);
void update(void);

}

}

}

#endif
