/*
	Content declaration: a model plus what its kind needs, behind a tag.
	Position, rotation and scale are not here, they belong to the placement,
	so one prefab can be placed any number of times.
*/
#ifndef ENGINE64_PREFAB3D_H
#define ENGINE64_PREFAB3D_H

#include <stdint.h>

#include "entity/e64_entity3d.h"
#include "physics/body/e64_rigid_body.h"
#include "physics/cloth/e64_cloth.h"
#include "character3d/e64_character3d.h"
#include "shaders/e64_water.h"
#include "sound/e64_sound.h"

namespace e64 {

typedef enum {

	PREFAB3D_CHARACTER,
	PREFAB3D_PROP,
	PREFAB3D_CLOTH,
	PREFAB3D_WATER,

} Prefab3DType;


typedef struct Prefab3D {

	Prefab3DType type;
	const char *model;

	/* Objects inside the model the game drives on its own, named as they are
	   named in the model, so it can show and hide each one. Left out, the model
	   is drawn whole. */
	const char *const *part;
	
	/* Where each of those objects is drawn, in the entity's own space and in
	the same order as the names. A part left at zero stays where it was
	modelled. */
	const Vector3 *part_position;
	
	uint8_t            part_count;
	
	/* Opened in the entity from create to delete. The looping ones play on
	   their own; the rest wait for whoever fires them. */
	const SoundDef *const *sound;
	uint8_t                sound_count;

	/* Solid for a prop, sensor volume for water. NULL: no collision. */
	const entity3d::ColliderDef *collider;

	/* The kind the tag names. A prop without a body is static. */
	union {
		const character3d::Def *character;
		const RigidBodyDef *prop;
		const ClothDef     *cloth;
		const WaterDef     *water;
	};

} Prefab3D;


}

#endif
