#include <libdragon.h>
#include <t3d/t3d.h>

#include "viewport/e64_viewport.h"
#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d_control.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_movement.h"
#include "character3d/e64_character3d_animation.h"
#include "player/e64_player.h"
#include "math/e64_math.h"
#include "scene3d/e64_scene3d.h"
#include "time/e64_time.h"

namespace e64 {

namespace player {

static Player player[PLAYER_COUNT];

Player *get(void) { return player; }

void init(void)
{
	for (int i = 0; i < PLAYER_COUNT; i++)
		player[i] = (Player){};
}

/* The seat comes from the binding: the buttons already name whose they are. */
void setCharacter3D(Character3D *character, const character3d::ControlBinding *control)
{
	if (control == NULL) return;

	Player *seat = &player[control->player];

	seat->type = CHARACTER_3D;
	seat->character3d.control = control;
	seat->character3d.character = character;
	seat->entity = character ? character->entity : NULL;
}

/* The 2D body draws through its own scene entity, so the seat keeps no
   Entity3D and nothing of it reaches the matrix pass. */
void setCharacter2D(Character2D *character, const character2d::ControlBinding *control)
{
	if (control == NULL) return;

	Player *seat = &player[control->player];

	seat->type = CHARACTER_2D;
	seat->character2d.control = control;
	seat->character2d.character = character;
}

/* Cycles the player through the scene's characters, in either direction. */
void switchCharacter3D(PlayerID id, int8_t direction)
{
	Scene3D *scene = scene3d::get();
	if (scene->character3d_count < 2) return;

	Player *seat = &player[id];

	/* Only a seat already driving a 3D body switches between them. */
	if (seat->type != CHARACTER_3D) return;

	uint8_t current = 0;
	for (uint8_t i = 0; i < scene->character3d_count; i++)
		if (scene->character[i] == seat->character3d.character) { current = i; break; }

	uint8_t next = (uint8_t)((current + scene->character3d_count + direction) % scene->character3d_count);
	Character3D *character = scene->character[next];

	/* Same buttons on the new body: switching bodies is not re-binding. */
	setCharacter3D(character, seat->character3d.control);

	/* Fresh command, facing where this body already faces: anything held over
	   from the previous character would spin the new one on the spot. */
	seat->character3d.cmd = (character3d::MovementCommand){ .target_yaw = character->body.rotation.z };
}


void update(void)
{
	const float dt = time::get()->delta;
	for (int i = 0; i < PLAYER_COUNT; i++) {
		/* Seats nobody took: a player without a body has nothing to run. */
		if (player[i].type == CHARACTER_3D && player[i].character3d.character) {
			character3d::stats::update(player[i].character3d.character, &player[i].character3d.cmd, dt);
			player[i].character3d.character->updateMovement(&player[i].character3d.cmd, dt);
			player[i].character3d.character->setAnimation();
			character3d::sound::update(player[i].character3d.character);
		}

		/* The 2D body has no stats, and its frames are the scene's to advance:
		   the seat only drives it. */
		else if (player[i].type == CHARACTER_2D && player[i].character2d.character)
			player[i].character2d.character->updateMovement(&player[i].character2d.cmd, dt);
	}

	/* Scene3D characters nobody drives run on an empty command, so they idle
	   instead of freezing mid pose when the player switches away. */
	static character3d::MovementCommand idle_cmd;
	Scene3D *scene = scene3d::get();
	for (int i = 0; i < scene->character3d_count; i++) {
		Character3D *character = scene->character[i];

		bool driven = false;
		for (int p = 0; p < PLAYER_COUNT; p++)
			if (player[p].type == CHARACTER_3D && player[p].character3d.character == character) driven = true;
		if (driven) continue;

		/* Same pipeline as a driven body, on a controller nobody holds: the
		   released stick idles it through the control's own rule (treading
		   water if it was swimming), and idling never drains, so a body
		   left behind rests and refills on its own. */
		static const character3d::Controls no_controls = {};

		idle_cmd.target_yaw = character->body.rotation.z;
		character3d::control::update(character, &idle_cmd, &no_controls, 0.0f);
		character3d::stats::update(character, &idle_cmd, dt);
		character->updateMovement(&idle_cmd, dt);
		character->setAnimation();
		character3d::sound::update(character);
	}
}

void setMatrix(uint8_t fb_index)
{
	for (int i = 0; i < PLAYER_COUNT; i++)
		if (player[i].entity)
			mesh::setMatrix(player[i].entity->mesh, &player[i].entity->transform, fb_index);
}

}

}
