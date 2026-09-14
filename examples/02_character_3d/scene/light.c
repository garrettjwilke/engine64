/*
	How the world is lit.

	Light comes in two parts. Ambient is the floor: the colour a surface keeps
	where nothing shines on it, and the only reason the dark side of an object
	is not black. Then the sources, seven slots shared between directional and
	point lights. They are read in order and cut at the first empty one, so the
	six left unwritten here cost nothing.
*/
#include "scene3d/e64_lighting.h"


/* The one source is a point light: it shines from a position in every
   direction, and its size is how far it carries.

   It sits at the height the lamp post puts its glass at, so it ends up inside
   it. Move the lamp and this has to follow. */
const LightDef light = {

	.ambient_color = { 60, 60, 70, 0xFF },

	.source = {
		{ .type  = LIGHT_POINT,
		  .color = { 255, 245, 220, 0xFF },
		  .point = { .position = {{ 0.0f, 0.0f, 5.832f }}, .size = 25.0f } },
	},
};
