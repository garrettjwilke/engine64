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

namespace e64 {

class Character3D;


namespace character3d {

/* Per asset tuning. Stamina is normalized 0..1 and the rates are per
   second; tired caps the reachable speed at this fraction of the top
   gait, read by the movement off the tired flag. */
typedef struct {

	float stamina_drain_rate;
	float stamina_regen_rate;
	float tired_speed_scale;

} StatsSettings;

typedef struct {

	const StatsSettings *settings;

	float hp;
	float stamina;
	bool tired;

} Stats;


namespace stats {

/* Runs before the movement update: the tired flag it leaves on the body is
   what the movement caps the speed with on the same frame. The command is
   only read, for the stroke the stick is asking for. */
void update(Character3D *character, const MovementCommand *cmd, float dt);

}

}

}

#endif
