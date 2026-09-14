/*
	The body's own condition: hp, stamina, and whatever it earns later.

	The stats belong to the character, not to the player driving it —
	switching bodies leaves the fatigue with the one that earned it, and a
	body left behind recovers on its own idle command.
*/
#ifndef ENGINE64_CHARACTER3D_STATS_H
#define ENGINE64_CHARACTER3D_STATS_H

#include <stdbool.h>

#include "character3d/e64_character3d_movement.h"


typedef struct Character3D Character3D;


/* Per asset tuning. Stamina is normalized 0..1 and the rates are per
   second; tired caps the reachable speed at this fraction of the top
   gait, read by the movement off the tired flag. */
typedef struct {

	float stamina_drain_rate;
	float stamina_regen_rate;
	float tired_speed_scale;

} Character3DStatsSettings;

typedef struct {

	const Character3DStatsSettings *settings;

	float hp;
	float stamina;
	bool  tired;

} Character3DStats;


/* Runs before the movement update: the tired flag it leaves on the body is
   what the movement caps the speed with on the same frame. The command is
   only read, for the stroke the stick is asking for. */
void character3dStats_update(Character3D *character, const MovementCommand *cmd, float dt);

#endif
