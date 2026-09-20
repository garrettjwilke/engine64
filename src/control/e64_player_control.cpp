#include "player/e64_player.h"
#include "control/e64_player_control.h"
#include "control/e64_character3d_control.h"
#include "control/e64_character2d_control.h"
#include "control/e64_menu_control.h"
#include "control/e64_controller.h"
#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"
#include "viewport/e64_viewport.h"
#include "game/e64_game.h"

namespace e64 {

namespace controls {

/* Whether the binding names that prefab. A layout can name several bodies,
   and which of them is there is the scene's to decide. */
static bool namesPrefab(const character3d::ControlBinding *binding, const Prefab3D *prefab)
{
	for (uint8_t i = 0; i < binding->character_count; i++)
		if (binding->character[i] == prefab) return true;

	return false;
}

static bool namesPrefab(const character2d::ControlBinding *binding, const Prefab2D *prefab)
{
	for (uint8_t i = 0; i < binding->character_count; i++)
		if (binding->character[i] == prefab) return true;

	return false;
}


void bind(const Def *controls, const Scene3DDef *scene)
{
	if (controls == NULL || scene == NULL) return;

	/* The buttons find the camera, not the other way around: they answer for
	   the camera they name, and this scene's takes them if it is that one. */
	if (controls->camera)
		viewport_get()->camera.binding =
			(controls->camera->camera == scene->camera) ? controls->camera : NULL;

	if (controls->character3d == NULL || controls->character3d->character_count == 0) return;

	/* Characters come out of the load in placement order, so counting the
	   character rows of the declaration finds the one built from the prefab
	   the binding drives. */
	uint8_t index = 0;
	for (int i = 0; i < scene->prefab_count; i++) {

		const Prefab3D *prefab = scene->prefab[i].prefab;
		if (prefab == NULL || prefab->type != PREFAB3D_CHARACTER) continue;

		if (namesPrefab(controls->character3d, prefab)) {
			player::setCharacter3D(scene3d_getCharacter3D(index), controls->character3d);
			return;
		}
		index++;
	}
}

/* The same on the 2D side, where the prefabs are spread across the layers and
   the characters come out in the order those are walked. */
void bind2D(const Def *controls, const Scene2DDef *scene)
{
	if (controls == NULL || scene == NULL) return;
	if (controls->character2d == NULL || controls->character2d->character_count == 0) return;

	uint8_t index = 0;
	for (int l = 0; l < scene->layer_count; l++) {

		const Scene2DLayer *layer = &scene->layer[l];

		for (int p = 0; p < layer->prefab_count; p++) {

			const Prefab2D *prefab = layer->prefab[p].prefab;
			if (prefab == NULL || prefab->type != PREFAB2D_CHARACTER) continue;

			if (namesPrefab(controls->character2d, prefab)) {
				player::setCharacter2D(scene2d_getCharacter2D(index), controls->character2d);
				return;
			}
			index++;
		}
	}
}

}


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
		camera::getAngleAround(&viewport->camera, &player->character3d.character->entity->transform.position)
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
