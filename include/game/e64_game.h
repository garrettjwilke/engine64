#ifndef ENGINE64_GAME_H
#define ENGINE64_GAME_H

#include <stdbool.h>

#include "e64_game_states.h"
#include "engine/e64_common.h"
#include "control/e64_controller.h"
#include "player/e64_player.h"


typedef struct Game {

	GameState state;

	/* Where it is heading. A state that asks to leave sets this and plays
	   its way out; the switch happens once the screen is done. Equal to
	   state while there is nowhere to go. */
	GameState next;

} Game;


Game *game_get(void);


void game_init(void);
void game_runStep(void);
void game_close(void);


#endif
