/*
	The room the world is built inside: floor and walls, and nothing else.

	A prefab is one kind of thing the world can contain: a model, plus whatever
	that kind needs to work. It says nothing about where it stands, which is
	why the same prefab can be placed as many times as wanted. Where each one
	goes is written in the scene, over in main.c.

	This is the ordinary case, and the shortest a prefab gets: a kind and a
	model. It is drawn whole, exactly as it was authored.

	The model path is a file in the ROM's filesystem, written there by the
	build. The model is authored as a .glb in Blender and left in
	assets/models; the Makefile converts it into the .t3dm the console reads.
*/
#include "prefab/e64_prefab3d.h"


/* A prop is the simplest kind there is: it gets drawn and nothing else. This
   one declares no collider, so nothing can bump into it, and no body, so it
   never moves. Example 03 gives props both. */
extern const Prefab3D room = {

	.type  = PREFAB3D_PROP,
	.model = "rom:/models/room.t3dm",
};
