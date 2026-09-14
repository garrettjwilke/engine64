#ifndef ENGINE64_GAME_STATES_H
#define ENGINE64_GAME_STATES_H

#include <stdbool.h>
#include <stdint.h>

#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"
#include "viewport/e64_viewport.h"
#include "control/e64_player_control.h"

/* Game holds the current state, so it cannot be included back from here. */
typedef struct Game Game;


/* Index into the state table the game hands to game_start. */
typedef uint8_t GameState;

/* No state: the overlay_of sentinel. A field left out of a designated
   initializer is 0, which is a valid state, so every table entry must set
   overlay_of explicitly. */
#define GAME_STATE_NONE 0xFF


typedef struct GameStateDef {

	void (*update)(void);
	void (*onEnter)(void);
	void (*onExit)(void);

	/* Holds the switch back while it answers false, so a state that plays
	   its way out is seen through. NULL leaves the moment it is asked. */
	bool (*canLeave)(void);

	/* Per-state input handling (menus, pause); NULL for none. The controller is
	   already polled: the game reads it with controller_get. */
	void (*control)(void);

	/* The scenes this state runs on, either or both: the 3D world and the
	   2D one drawn over it. */
	Scene3DDef          *scene3d;
	const Scene2DDef    *scene2d;

	/* What drives what while this state is current. Each binding names the
	   piece it moves, so the engine wires them itself once the scenes are
	   built. A state that drives nothing leaves it out. */
	const ControlsDef   *controls;

	/* The screen this state is played on. The engine opens none by itself,
	   so the first state entered is what puts one up, and a state that wants
	   the one already there declares the same. */
	const ViewportModeDef *viewport;

	/* The state this one rides on top of; GAME_STATE_NONE for none.
	   Switching between an overlay and its base leaves the base untouched. */
	GameState          overlay_of;

} GameStateDef;


/* Hands the engine the game's state table and loads the initial state.
   Runs after game_init and the game's own inits, before the first runStep. */
void game_start(const GameStateDef *states, uint8_t count, GameState initial);

const GameStateDef *gameState_get(GameState id);

void game_setState(Game *game, GameState new_state);
void game_updateState(void);


#endif
