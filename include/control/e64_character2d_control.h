#ifndef ENGINE64_CHARACTER2D_CONTROL_H
#define ENGINE64_CHARACTER2D_CONTROL_H

#include "e64_controller.h"
#include "engine/e64_common.h"
#include "prefab/e64_prefab2d.h"
#include "character2d/e64_character2d.h"
#include "character2d/e64_character2d_movement.h"

namespace e64 {

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

	/* Which bodies it drives, by the prefabs they were placed from, in
	   whatever layer of the scene they stand. One layout serves several
	   bodies: loading a scene seats the player on the first of these the
	   scene holds, so a screen placing another one needs no binding of its
	   own. */
	const Prefab2D *const *character;
	uint8_t                character_count;

	ButtonID jump;
	ButtonID roll;
	ButtonID sprint;

	ButtonID left;
	ButtonID right;

} ControlBinding;


typedef struct Controls {

	bool  jump;
	bool  jump_held;
	bool  roll;
	bool  sprint;

	bool  left;
	bool  right;
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
