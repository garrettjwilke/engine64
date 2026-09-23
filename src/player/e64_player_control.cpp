#include "player/e64_player_control.h"
#include "player/e64_player.h"
#include "camera/e64_camera3d.h"
#include "character3d/e64_character3d_control.h"
#include "character2d/e64_character2d_control.h"
#include "viewport/e64_viewport.h"

namespace e64 {

namespace player {

void setCharacter3DControl(PlayerID id, Viewport *viewport)
{
	Player *player = &get()[id];
	if (player->type != CHARACTER_3D) return;
	if (player->character3d.character == NULL || player->character3d.control == NULL) return;

	/* Read where it is used: what the controller is doing this frame is worth
	   nothing on the next one. */
	character3d::Controls controls;
	character3d::control::read(&controls, player->character3d.control);

	character3d::control::update(
		player->character3d.character,
		&player->character3d.cmd,
		&controls,
		camera3d::getAngleAround(&viewport->camera, &player->character3d.character->entity->transform.position)
	);
}

/* No camera to measure against: the 2D body's axis is the screen's, and left
   is left however the scene scrolls. */
void setCharacter2DControl(PlayerID id)
{
	Player *player = &get()[id];
	if (player->type != CHARACTER_2D) return;
	if (player->character2d.character == NULL || player->character2d.control == NULL) return;

	character2d::Controls controls;
	character2d::control::read(&controls, player->character2d.control);

	character2d::control::update(
		player->character2d.character,
		&player->character2d.cmd,
		&controls
	);
}

}

}
