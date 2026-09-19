#ifndef ENGINE64_CHARACTER3D_CONTROL_H
#define ENGINE64_CHARACTER3D_CONTROL_H

#include "e64_controller.h"
#include "engine/e64_common.h"
#include "prefab/e64_prefab3d.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_movement.h"

#define PLAYER_STICK_WALK_THRESHOLD 65


/* What a body can be asked to do. What a character does not do is left out:
   an unwritten button is BTN_NONE and reads as never pressed. */
typedef struct Character3DControlBinding {

	/* Whose seat drives this body. */
	PlayerID player;

	/* Which bodies it drives, by the prefabs they were placed from. One layout
	   serves several bodies: loading a scene seats that player on the first of
	   these the scene holds, so the game binds nothing by hand. */
	const Prefab3D *const *character;
	uint8_t                character_count;

	ButtonID jump;
	ButtonID roll;
	ButtonID sprint;
	ButtonID aim;
	ButtonID shoot;
	ButtonID weapon_next;
	ButtonID weapon_prev;

} Character3DControlBinding;


typedef struct Character3DControls {

	bool  jump;
	bool  jump_held;
	bool  roll;
	bool  sprint;
	bool  aim;
	bool  shoot;            /* held: the bow draws while it stays down */
	bool  shoot_released;   /* the shot fires on this edge */
	bool  weapon_next;
	bool  weapon_prev;
	float stick_x;
	float stick_y;

} Character3DControls;


/* This frame's state of the buttons the binding names, off that player's
   controller.
   The binding is the mapping and is written once; this only reads what those
   buttons are doing now. */
void character3dControls_read(Character3DControls *controls, const Character3DControlBinding *binding);
void character3dControl_update(Character3D *character, MovementCommand *cmd, const Character3DControls *controls, float camera_angle_around);

#endif
