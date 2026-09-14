#include "character3d/e64_character3d.h"
#include "physics/math/e64_math_functions.h"


/* Running at the top gait or swimming fast drains stamina, anything else
   recovers it. Hitting zero flags the body tired, and the movement caps
   locomotion and swim speed off that flag until stamina is back at full. */
void character3dStats_update(Character3D *character, const MovementCommand *cmd, float dt)
{
	const Character3DMovement *movement = &character->movement;
	Character3DStats *stats = &character->stats;
	const Character3DStatsSettings *settings = stats->settings;

	/* A character that declares none spends nothing: there is no stamina to
	   drain and nothing caps its speed. */
	if (settings == NULL) return;

	bool running = (movement->current == MOVEMENT_STATE_WALKING
	                && movement->data.gait >= (float)(movement->settings->gait_count - 1))
	            || (movement->current == MOVEMENT_STATE_SWIMMING
	                && cmd->swim_gait == CHARACTER3D_SWIM_GAIT_FAST);

	/* Transitions run first, on the value the previous frame rendered: the
	   frame that lands on zero stays at zero on screen, and regen can only
	   move it from the next update on. */
	if (stats->stamina <= 0.0f) stats->tired = true;
	if (stats->stamina >= 1.0f) stats->tired = false;

	float rate = running && !stats->tired ? -settings->stamina_drain_rate : settings->stamina_regen_rate;
	stats->stamina = clampf(stats->stamina + rate * dt, 0.0f, 1.0f);
}
