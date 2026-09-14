#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <libdragon.h>
#include <fgeom.h>
#include <t3d/t3dmodel.h>

#include "physics/math/e64_quaternion.h"
#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d.h"
#include "character3d/e64_character3d_animation.h"


/* SkeletonModifierFn: the weapon posing; context is the Character3D. */
static void character3d_weaponModifier(T3DSkeleton *skeleton, void *context)
{
	(void)skeleton;
	character3dWeapon_setBones(context);
}

Character3D *character3d_create(const Character3DDef *def, Entity3D *entity)
{
	/* The spring bone states ride in the same allocation; their only
	   references are the modifier contexts, freed with the character. */
	uint8_t spring_bones = 0;
	if (def->spring_bones)
		for (const SpringBonesDef *set = def->spring_bones; set->count; set++)
			spring_bones += set->count;

	Character3D *character = malloc(sizeof(Character3D) + spring_bones * sizeof(SpringBone));
	assert(character);

	*character = (Character3D){
		.entity    = entity,
		.stats     = (Character3DStats){ .settings = def->stats_settings, .stamina = 1.0f },
		.body      = (KinematicBody){ .position = entity->transform.position, .rotation = entity->transform.rotation },
		.movement  = (Character3DMovement){ .settings = def->movement_settings, .data.is_grounded = true, .current = MOVEMENT_STATE_IDLE },
		.animation = (Character3DAnimation){ .def = def->animation_def },
		.weapons   = (Character3DWeapons){ .def = def->weapons_def, .drawn = CHARACTER3D_WEAPON_DRAWN_NONE },
		/* No previous frame to compare against yet: a cycle of -1 crosses
		   nothing, and the body starts standing on the floor. */
		.sound     = (Character3DSound){ .def = def->sound_def, .previous_cycle = -1.0f, .previous_grounded = true },
	};

	character3dCollider_init(&character->collider,
		def->collider_settings->radius,
		(def->collider_settings->height - 2.0f * def->collider_settings->radius) * 0.5f);
	character3dCollider_setVertical(&character->collider, &character->body.position);

	/* A def without animations (a vehicle) skips the whole graph: the mesh
	   keeps a NULL skeleton and draws through the model object path. */
	if (def->animation_def) {
		character3dAnimation_initGraph(character, def->animation_def);
		entity->mesh->skeleton = &character->animation.main;
	}

	/* Aim before the weapons: the bow has to follow a spine already bent. */
	if (def->aiming_settings) {
		character3dAim_init(character, def->aiming_settings);
		skeletonModifiers_add(&character->skeleton_modifiers, character3dAim_apply, character);
	}

	skeletonModifiers_add(&character->skeleton_modifiers, character3d_weaponModifier, character);

	if (spring_bones > 0) {
		SpringBone *spring_bone = (SpringBone *)(character + 1);
		uint8_t n = 0;

		/* Chains rely on this order: the resolved joints run root to tip,
		   so each modifier runs after the one it hangs from. */
		for (const SpringBonesDef *set = def->spring_bones; set->count; set++) {
			int16_t joint[16];
			uint8_t count = springBones_resolveChain(&character->animation.main, set, joint, 16);
			if (count > set->count) count = set->count;

			for (uint8_t i = 0; i < count; i++) {
				if (!springBone_init(&spring_bone[n], &character->animation.main, joint[i],
				                     i, set, &entity->transform))
					continue;

				skeletonModifiers_add(&character->skeleton_modifiers, springBone_apply, &spring_bone[n]);
				n++;
			}
		}
	}

	/* Part 0 = body, parts 1..N = one per weapon object, def order.
	   Only the body starts visible; equipping turns weapon bits on.
	   No weapons: the whole model is the single skinned part. No skeleton
	   either: per-object blocks, exactly what a prop gets. */
	if (def->weapons_def)
		mesh_recordParts(entity->mesh, def->weapons_def->mesh, def->weapons_def->mesh_count,
			(const T3DMat4FP *)t3d_segment_placeholder(T3D_SEGMENT_SKELETON));
	else if (def->animation_def)
		mesh_recordParts(entity->mesh, NULL, 0,
			(const T3DMat4FP *)t3d_segment_placeholder(T3D_SEGMENT_SKELETON));
	else
		mesh_recordObjects(entity->mesh);

	return character;
}

void character3d_getBoneModelSpacePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation)
{
	uint16_t chain[16];
	int depth = 0;

	uint16_t idx = (uint16_t)bone;
	while (idx != 0xFFFF && depth < 16) {
		chain[depth++] = idx;
		idx = skeleton->skeletonRef->bones[idx].parentIdx;
	}

	*position = (T3DVec3){{ 0.0f, 0.0f, 0.0f }};
	*rotation = (T3DQuat){{ 0.0f, 0.0f, 0.0f, 1.0f }};

	for (int i = depth - 1; i >= 0; i--) {
		const T3DBone *b = &skeleton->bones[chain[i]];

		/* T3DQuat and T3DVec3 are laid out like the math module's types. */
		Vector3 step = quaternion_rotateVector((const Quaternion *)rotation, (const Vector3 *)&b->position);
		position->v[0] += step.x;
		position->v[1] += step.y;
		position->v[2] += step.z;

		T3DQuat next;
		t3d_quat_mul(&next, rotation, (T3DQuat *)&b->rotation);
		*rotation = next;
	}
}

/* Model-space pose of a bone, composed from the local TRS chain so it is
   current-frame (bone->matrix would lag one skeleton update behind). */
void character3d_getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation)
{
	uint16_t chain[16];
	int depth = 0;

	uint16_t idx = (uint16_t)bone;
	while (idx != 0xFFFF && depth < 16) {
		chain[depth++] = idx;
		idx = skeleton->skeletonRef->bones[idx].parentIdx;
	}

	*position = (T3DVec3){{ 0.0f, 0.0f, 0.0f }};
	*rotation = (T3DQuat){{ 0.0f, 0.0f, 0.0f, 1.0f }};

	for (int i = depth - 1; i >= 0; i--) {
		const T3DBone *b = &skeleton->bones[chain[i]];

		Vector3 step = quaternion_rotateVector((const Quaternion *)rotation, (const Vector3 *)&b->position);
		position->v[0] += step.x;
		position->v[1] += step.y;
		position->v[2] += step.z;

		T3DQuat next;
		t3d_quat_mul(&next, rotation, (T3DQuat *)&b->rotation);
		*rotation = next;
	}
}

void character3d_delete(Character3D *character)
{
	Character3DAnimation *animation = &character->animation;

	if (animation->def) {
		for (int i = 0; i < animation->def->clip_count; i++) {
			t3d_anim_destroy(&animation->clip[i]);
			if (animation->clip_data[i]) free(animation->clip_data[i]);
		}
		for (int i = 0; i < animation->def->buffer_count; i++)
			t3d_skeleton_destroy(&animation->buffer[i]);
		t3d_skeleton_destroy(&animation->main);
	}

	free(animation->clip);
	free(animation->clip_data);
	free(animation->clip_cooldown);
	free(animation->buffer);
	free(animation->node_state);
	free(animation->node_active);
	free(character);
}
