/*
	The lamp post: one model holding two separate objects, drawn apart.

	lamp_post.glb holds two objects, named "post" and "lamp" back in Blender.
	Naming one of them here cuts it out as a part the game can show and hide on
	its own; everything left unnamed stays together as the rest of the model,
	always drawn. Up to seven names can be listed this way.

	The character system uses this same mechanism for weapons. Every weapon a
	character can carry is modelled inside the character and listed as a part,
	and the ones equipped are the parts left showing.

	How the lamp is made, since the console has no emissive materials. It is an
	icosphere with its normals flipped to face inward, and the scene's point
	light placed inside it.

	The console shades a surface by the angle between its normal and the light.
	Flipped inward, the side of the sphere facing the camera has its normal
	aimed at the light in the middle, so it comes out at full brightness from
	any angle it is seen from. It reads as glass lit from within.
*/
#include "prefab/e64_prefab3d.h"


/* Both objects were modelled sitting at the origin, one inside the other. A
   part can be drawn somewhere other than where it was modelled, and that is
   what puts the lamp on top of the post here.

   The post reaches 550.12 and the sphere has a radius of 75, so the two
   together leave the lamp resting exactly on the tip. The light, declared over
   in the scene, is put at that same height so it lands in the glass.

   The post is declared as a part too, left at zero. A part at zero is drawn
   where it was modelled and costs nothing extra: it goes on the model's own
   matrix instead of getting one of its own. */
#define LAMP_HEIGHT 625.2f

extern const Prefab3D lamp_post = {

	.type  = PREFAB3D_PROP,
	.model = "rom:/models/lamp_post.t3dm",

	.part = MESH_PARTS(
		"post",
		"lamp"
	),

	.part_position = MESH_PART_POSITIONS(
		{ 0.0f, 0.0f, 0.0f        },
		{ 0.0f, 0.0f, LAMP_HEIGHT }
	),

	.part_count = 2,
};
