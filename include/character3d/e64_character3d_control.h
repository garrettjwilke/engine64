#ifndef ENGINE64_CHARACTER3D_CONTROL_H
#define ENGINE64_CHARACTER3D_CONTROL_H

#include "controller/e64_controller.h"
#include "engine/e64_common.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_movement.h"

namespace e64 {

namespace scene3d { struct Entity; }

#define PLAYER_STICK_WALK_THRESHOLD 65


namespace character3d {

/* What a body can be asked to do. What a character does not do is left out:
   an unwritten button is BTN_NONE and reads as never pressed. */
typedef struct ControlBinding {

	/* Whose seat drives this body. */
	PlayerID player;

	/* Which body it drives: the scene entity declared for it. The load seats
	   the player on that body as it builds it. */
	const scene3d::Entity *character;

	ButtonID jump;
	ButtonID roll;
	ButtonID sprint;
	ButtonID aim;
	ButtonID shoot;
	ButtonID weapon_next;
	ButtonID weapon_prev;

} ControlBinding;


typedef struct Controls {

	bool jump;
	bool jump_held;
	bool roll;
	bool sprint;
	bool aim;
	bool shoot; /* held: the bow draws while it stays down */
	bool shoot_released; /* the shot fires on this edge */
	bool weapon_next;
	bool weapon_prev;
	float stick_x;
	float stick_y;

} Controls;


namespace control {

/* This frame's state of the buttons the binding names, off that player's
   controller.
   The binding is the mapping and is written once; this only reads what those
   buttons are doing now. */
void read(Controls *controls, const ControlBinding *binding);
void update(Character3D *character, MovementCommand *cmd, const Controls *controls, float camera_angle_around);

}

}

}

#endif
