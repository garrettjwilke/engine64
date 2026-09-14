#include <libdragon.h>
#include <t3d/t3d.h>

#include "viewport/e64_viewport.h"
#include "entity/e64_entity3d.h"
#include "control/e64_character3d_control.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_movement.h"
#include "character3d/e64_character3d_animation.h"
#include "player/e64_player.h"
#include "physics/math/e64_math_functions.h"
#include "scene3d/e64_scene3d.h"
#include "time/e64_time.h"


static Player player[PLAYER_COUNT];

Player *player_get(void) { return player; }

void player_init(void)
{
	for (int i = 0; i < PLAYER_COUNT; i++)
		player[i] = (Player){0};
}

/* The seat comes from the binding: the buttons already name whose they are. */
void player_setCharacter3D(Character3D *character, const Character3DControlBinding *control)
{
	if (control == NULL) return;

	Player *seat = &player[control->player];

	seat->type = PLAYER_CHARACTER_3D;
	seat->character3d.control   = control;
	seat->character3d.character = character;
	seat->entity = character ? character->entity : NULL;
	if (seat->entity && seat->entity->mesh)
		t3d_matrix_set(seat->entity->mesh->matrix_buffer, true);
}

/* The 2D body draws through its own scene entity, so the seat keeps no
   Entity3D and nothing of it reaches the matrix pass. */
void player_setCharacter2D(Character2D *character, const Character2DControlBinding *control)
{
	if (control == NULL) return;

	Player *seat = &player[control->player];

	seat->type = PLAYER_CHARACTER_2D;
	seat->character2d.control   = control;
	seat->character2d.character = character;
}

/* Cycles the player through the scene's characters, in either direction. */
void player_switchCharacter3D(PlayerID id, int8_t direction)
{
	Scene3D *scene = scene3d_get();
	if (scene->character3d_count < 2) return;

	Player *seat = &player[id];

	/* Only a seat already driving a 3D body switches between them. */
	if (seat->type != PLAYER_CHARACTER_3D) return;

	uint8_t current = 0;
	for (uint8_t i = 0; i < scene->character3d_count; i++)
		if (scene->character[i] == seat->character3d.character) { current = i; break; }

	uint8_t next = (uint8_t)((current + scene->character3d_count + direction) % scene->character3d_count);
	Character3D *character = scene->character[next];

	/* Same buttons on the new body: switching bodies is not re-binding. */
	player_setCharacter3D(character, seat->character3d.control);

	/* Fresh command, facing where this body already faces: anything held over
	   from the previous character would spin the new one on the spot. */
	seat->character3d.cmd = (MovementCommand){ .target_yaw = character->body.rotation.z };
}


void player_update(void)
{
	const float dt = time_get()->delta;
	for (int i = 0; i < PLAYER_COUNT; i++) {
		/* Seats nobody took: a player without a body has nothing to run. */
		if (player[i].type == PLAYER_CHARACTER_3D && player[i].character3d.character) {
			character3dStats_update(player[i].character3d.character, &player[i].character3d.cmd, dt);
			character3d_updateMovement(player[i].character3d.character, &player[i].character3d.cmd, dt);
			character3d_setAnimation(player[i].character3d.character);
			character3dSound_update(player[i].character3d.character);
		}

		/* The 2D body has no stats, and its frames are the scene's to advance:
		   the seat only drives it. */
		else if (player[i].type == PLAYER_CHARACTER_2D && player[i].character2d.character)
			character2d_updateMovement(player[i].character2d.character, &player[i].character2d.cmd, dt);
	}

	/* Scene3D characters nobody drives run on an empty command, so they idle
	   instead of freezing mid pose when the player switches away. */
	static MovementCommand idle_cmd;
	Scene3D *scene = scene3d_get();
	for (int i = 0; i < scene->character3d_count; i++) {
		Character3D *character = scene->character[i];

		bool driven = false;
		for (int p = 0; p < PLAYER_COUNT; p++)
			if (player[p].type == PLAYER_CHARACTER_3D && player[p].character3d.character == character) driven = true;
		if (driven) continue;

		/* Same pipeline as a driven body, on a controller nobody holds: the
		   released stick idles it through the control's own rule (treading
		   water if it was swimming), and idling never drains, so a body
		   left behind rests and refills on its own. */
		static const Character3DControls no_controls;

		idle_cmd.target_yaw = character->body.rotation.z;
		character3dControl_update(character, &idle_cmd, &no_controls, 0.0f);
		character3dStats_update(character, &idle_cmd, dt);
		character3d_updateMovement(character, &idle_cmd, dt);
		character3d_setAnimation(character);
		character3dSound_update(character);
	}
}

void player_setMatrix(uint8_t fb_index)
{
	for (int i = 0; i < PLAYER_COUNT; i++)
		if (player[i].entity)
			mesh_setMatrix(player[i].entity->mesh, &player[i].entity->transform, fb_index);
}
