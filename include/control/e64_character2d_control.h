#ifndef ENGINE64_CHARACTER2D_CONTROL_H
#define ENGINE64_CHARACTER2D_CONTROL_H

#include "e64_controller.h"
#include "character2d/e64_character2d.h"
#include "character2d/e64_character2d_movement.h"

#define PLAYER2D_STICK_WALK_THRESHOLD 65


/* What a 2D body can be asked to do. What a character does not do is left out:
   an unwritten button is BTN_NONE and reads as never pressed.

   The one axis is driven by the stick or by the d-pad, whichever the player
   reaches for: the pad is bound like any other button and named here so a
   character that wants it elsewhere can move it. */
typedef struct Character2DControlBinding {

	/* Whose seat drives this body. */
	PlayerID player;

	ButtonID jump;
	ButtonID roll;
	ButtonID sprint;

	ButtonID left;
	ButtonID right;

} Character2DControlBinding;


typedef struct Character2DControls {

	bool  jump;
	bool  jump_held;
	bool  roll;
	bool  sprint;

	bool  left;
	bool  right;
	float stick_x;

} Character2DControls;


/* This frame's state of the buttons the binding names, off that player's
   controller.
   The binding is the mapping and is written once; this only reads what those
   buttons are doing now. */
void character2dControls_read(Character2DControls *controls, const Character2DControlBinding *binding);
void character2dControl_update(Character2D *character, Movement2DCommand *cmd, const Character2DControls *controls);

#endif
