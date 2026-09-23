#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>
#include <fgeom.h>
#include <t3d/t3dmodel.h>

#include "math/e64_quaternion.h"
#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_animation.h"

namespace e64 {

namespace character3d {

/* skeleton::Modifiers::Fn: the weapon posing; context is the Character3D. */
static void weaponModifier(T3DSkeleton *skeleton, void *context)
{
	(void)skeleton;
	weapon::setBones((Character3D *)context);
}

Character3D *create(const Def *def, Entity3D *entity)
{
	/* The spring bone states ride in the same allocation; their only
	   references are the modifier contexts, freed with the character. */
	uint8_t spring_bones = 0;
	if (def->spring_bones)
		for (const SpringBonesDef *set = def->spring_bones; set->count; set++)
			spring_bones += set->count;

	Character3D *character = (Character3D *)malloc(sizeof(Character3D) + spring_bones * sizeof(SpringBone));
	assert(character);

	*character = (Character3D){
		.entity = entity,
		.body = (KinematicBody){ .position = entity->transform.position, .rotation = entity->transform.rotation },
		.movement = (Movement){ .settings = def->movement_settings, .data = { .is_grounded = true }, .current = MOVEMENT_STATE_IDLE },
		.animation = { .def = def->animation_def },
		.weapons = (Weapons){ .def = def->weapons_def, .drawn = CHARACTER3D_WEAPON_DRAWN_NONE },
		/* No previous frame to compare against yet: a cycle of -1 crosses
		   nothing, and the body starts standing on the floor. */
		.sound = (Sound){ .def = def->sound_def, .previous_cycle = -1.0f, .previous_grounded = true },
		.stats = (Stats){ .settings = def->stats_settings, .stamina = 1.0f },
	};

	collider::init(&character->collider,
		def->collider_settings->radius,
		(def->collider_settings->height - 2.0f * def->collider_settings->radius) * 0.5f);
	collider::setVertical(&character->collider, &character->body.position);

	/* A def without animations (a vehicle) skips the whole graph: the mesh
	   keeps a NULL skeleton and draws through the model object path. */
	if (def->animation_def) {
		character->animation.init(entity->mesh->model);
		entity->mesh->skeleton = &character->animation.graph.main;
	}

	/* Aim before the weapons: the bow has to follow a spine already bent. */
	if (def->aiming_settings) {
		aim::init(character, def->aiming_settings);
		skeleton::modifiers::add(&character->skeleton_modifiers, aim::apply, character);
	}

	skeleton::modifiers::add(&character->skeleton_modifiers, weaponModifier, character);

	if (spring_bones > 0) {
		SpringBone *spring_bone = (SpringBone *)(character + 1);
		uint8_t n = 0;

		/* Chains rely on this order: the resolved joints run root to tip,
		   so each modifier runs after the one it hangs from. */
		for (const SpringBonesDef *set = def->spring_bones; set->count; set++) {
			int16_t joint[16];
			uint8_t count = springBones_resolveChain(&character->animation.graph.main, set, joint, 16);
			if (count > set->count) count = set->count;

			for (uint8_t i = 0; i < count; i++) {
				if (!springBone_init(&spring_bone[n], &character->animation.graph.main, joint[i],
				                     i, set, &entity->transform))
					continue;

				skeleton::modifiers::add(&character->skeleton_modifiers, springBone_apply, &spring_bone[n]);
				n++;
			}
		}
	}

	/* Part 0 = body, parts 1..N = one per weapon object, def order.
	   Only the body starts visible; equipping turns weapon bits on.
	   No weapons: the whole model is part 0. A skinned model records
	   against the skeleton segment; without a skeleton it records like a
	   prop. */
	const T3DMat4FP *bones = def->animation_def
	                       ? (const T3DMat4FP *)t3d_segment_placeholder(T3D_SEGMENT_SKELETON)
	                       : NULL;

	if (def->weapons_def)
		mesh::record(entity->mesh, def->weapons_def->mesh, def->weapons_def->mesh_count, bones);
	else
		mesh::record(entity->mesh, NULL, 0, bones);

	return character;
}

void destroy(Character3D *character)
{
	if (character->animation.def)
		character->animation.destroy();

	free(character);
}

}

}
