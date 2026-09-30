/*
	Content declaration for the 2D scene: what it draws with plus what its
	kind needs, behind a tag. Position is not here, it belongs to the
	placement, so one prefab can be placed any number of times. Mirror of
	Prefab3D.
*/
#ifndef ENGINE64_PREFAB2D_H
#define ENGINE64_PREFAB2D_H

#include "graphics/e64_graphic.h"
#include "sound/e64_sound.h"
#include "character2d/e64_character2d.h"
#include "stage2d/e64_stage2d.h"

namespace e64 {

namespace prefab2d {

typedef enum {

	PREFAB2D_WIDGET,
	PREFAB2D_PROP,
	PREFAB2D_CHARACTER,
	PREFAB2D_STAGE,

} Type;

}


typedef struct Prefab2D {

	prefab2d::Type type;

	/* The entity copies it and loads the sprite's file. A character's is a
	   sprite sheet it picks its frames out of. */
	Graphic graphic;

	/* What the kind needs. Collision comes with the 2D physics. A stage
	   draws its own tiles and leaves the graphic above unused. */
	const character2d::Def *character;
	const stage2d::Def *stage;

	/* Opened in the entity from create to delete. The looping ones play on
	   their own; the rest wait for whoever fires them. */
	const Sound::Def *const *sound;
	uint8_t sound_count;

	/* PROP only: how much of the camera's scroll it takes. 1 sits in the
	   world and moves with it, and a backdrop takes some fraction of that,
	   which is what reads as distance. A widget is screen space and never
	   asks; a character stands in the world and always takes all of it. */
	float parallax;

} Prefab2D;


}

#endif
