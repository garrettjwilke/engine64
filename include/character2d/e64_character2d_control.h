#ifndef ENGINE64_CHARACTER2D_CONTROL_H
#define ENGINE64_CHARACTER2D_CONTROL_H

#include "controller/e64_controller.h"
#include "engine/e64_common.h"
#include "character2d/e64_character2d.h"
#include "character2d/e64_character2d_movement.h"

namespace e64 {

namespace scene2d { struct Entity; }

#define PLAYER2D_STICK_WALK_THRESHOLD 65


namespace character2d {

/* What a 2D body can be asked to do. What a character does not do is left out:
   an unwritten button is BTN_NONE and reads as never pressed.

   The one axis is driven by the stick or by the d-pad, whichever the player
   reaches for: the pad is bound like any other button and named here so a
   character that wants it elsewhere can move it. */
typedef struct ControlBinding {

	/* Whose seat drives this body. */
	PlayerID player;

	/* Which body it drives: the scene entity declared for it, in whatever
	   layer it stands. The load seats the player on that body as it builds
	   it. NULL seats nobody. */
	const scene2d::Entity *character;

	ButtonID jump;
	ButtonID roll;
	ButtonID sprint;

	ButtonID left;
	ButtonID right;

} ControlBinding;


typedef struct Controls {

	bool jump;
	bool jump_held;
	bool roll;
	bool sprint;

	bool left;
	bool right;
	float stick_x;

} Controls;


namespace control {

/* This frame's state of the buttons the binding names, off that player's
   controller.
   The binding is the mapping and is written once; this only reads what those
   buttons are doing now. */
void read(Controls *controls, const ControlBinding *binding);
void update(Character2D *character, MovementCommand *cmd, const Controls *controls);

}
}

}

#endif
