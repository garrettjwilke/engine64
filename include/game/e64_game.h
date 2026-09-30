#ifndef ENGINE64_GAME_H
#define ENGINE64_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/e64_common.h"
#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"
#include "ui/e64_ui.h"
#include "viewport/e64_viewport.h"
#include "controller/e64_controls.h"

namespace e64 {

class Game {

public:

	class State {

	public:

		/* Index into the state table the game hands to game::state::start. */
		typedef uint8_t ID;

		struct Def {

			void (*update)(void);
			void (*onEnter)(void);
			void (*onExit)(void);

			/* Holds the switch back while it answers false, so a state that plays
			   its way out is seen through. NULL leaves the moment it is asked. */
			bool (*canLeave)(void);

			/* Per-state input handling (menus, pause); NULL for none. The controller is
			   already polled: the game reads it with controller::get. */
			void (*control)(void);

			/* The scenes this state runs on, either or both: the 3D world and the
			   2D one drawn over it. */
			scene3d::Def *scene3d;
			const scene2d::Def *scene2d;

			/* The interface drawn over them; NULL for none. An overlay's
			   takes the base's place while it is up. */
			const ui::Def *ui;

			/* What drives what while this state is current. Each binding names the
			   piece it moves, so the engine wires them itself once the scenes are
			   built. A state that drives nothing leaves it out. */
			const controls::Def *controls;

			/* The screen this state is played on. The engine opens none by itself,
			   so the first state entered is what puts one up, and a state that wants
			   the one already there declares the same. */
			const Viewport::ModeDef *viewport;

			/* The table entries this one is an overlay of, and how many; NULL
			   and zero, the default, for none. Entered from one of them, the
			   overlay leaves that base loaded underneath and goes back to it
			   as it is. A pause shared by several games lists them all. */
			const Def *const *overlay_of;
			uint8_t overlay_count;

		};

		/* The game's table, handed over at game::state::start. */
		const Def *table;
		uint8_t count;

		ID current;

		/* Where it is heading. A state that asks to leave sets this and plays
		   its way out; the switch happens once the screen is done. Equal to
		   current while there is nowhere to go. */
		ID next;

		/* The state an overlay was entered from, kept for as long as the
		   overlay is up: where continuing goes, and what leaving for good
		   takes down. Meaningless while the current state is no overlay. */
		ID base;

	};

	State state;

};


namespace game {

Game *get(void);

void init(void);
void runStep(void);
void close(void);

}

}

#endif
