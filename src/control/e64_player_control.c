#include "player/e64_player.h"
#include "control/e64_player_control.h"
#include "control/e64_character3d_control.h"
#include "control/e64_character2d_control.h"
#include "control/e64_menu_control.h"
#include "control/e64_controller.h"
#include "viewport/e64_viewport.h"
#include "game/e64_game.h"


void player_setCharacter3DControl(PlayerID id, Viewport *viewport)
{
	Player *player = &player_get()[id];
	if (player->type != PLAYER_CHARACTER_3D) return;
	if (player->character3d.character == NULL || player->character3d.control == NULL) return;

	/* Read where it is used: what the controller is doing this frame is worth
	   nothing on the next one. */
	Character3DControls controls;
	character3dControls_read(&controls, player->character3d.control);

	character3dControl_update(
		player->character3d.character,
		&player->character3d.cmd,
		&controls,
		camera_getAngleAround(&viewport->camera, &player->character3d.character->entity->transform.position)
	);
}

/* No camera to measure against: the 2D body's axis is the screen's, and left
   is left however the scene scrolls. */
void player_setCharacter2DControl(PlayerID id)
{
	Player *player = &player_get()[id];
	if (player->type != PLAYER_CHARACTER_2D) return;
	if (player->character2d.character == NULL || player->character2d.control == NULL) return;

	Character2DControls controls;
	character2dControls_read(&controls, player->character2d.control);

	character2dControl_update(
		player->character2d.character,
		&player->character2d.cmd,
		&controls
	);
}
