/*
	The scene's lighting.

	Two parts. Ambient is the flat colour every surface keeps regardless of
	what reaches it, and the only thing lighting the faces no source hits.
	Then the sources: seven slots shared by every light type, read in order and
	stopped at the first empty one, so the six left unwritten here cost nothing.
*/
#include "scene3d/e64_lighting.h"


/* One point light: it radiates in every direction from .position, and .size is
   the radius it reaches, in metres. Placed above the middle of the room, high
   enough to light the floor without flattening the walls. */
const LightDef light = {

	.ambient_color = { 60, 60, 70, 0xFF },

	.source = {
		{ .type  = LIGHT_POINT,
		  .color = { 255, 245, 220, 0xFF },
		  .point = { .position = {{ 0.0f, 0.0f, 5.832f }}, .size = 25.0f } },
	},
};
