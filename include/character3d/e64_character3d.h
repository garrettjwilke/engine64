#ifndef ENGINE64_CHARACTER3D_H
#define ENGINE64_CHARACTER3D_H

#include <stdbool.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>

#include "physics/e64_physics.h"
#include "graphics/e64_mesh.h"
#include "character3d/e64_character3d_physics.h"
#include "character3d/e64_character3d_movement.h"
#include "character3d/e64_character3d_stats.h"
#include "character3d/e64_character3d_animation.h"
#include "character3d/e64_character3d_weapon.h"
#include "character3d/e64_character3d_aim.h"
#include "character3d/e64_character3d_skeleton.h"
#include "character3d/e64_character3d_spring_bone.h"
#include "character3d/e64_character3d_sound.h"

typedef struct Entity3D Entity3D;

typedef struct Character3DDef {

	const Character3DMovementSettings *movement_settings;
	const Character3DAnimationDef *animation_def;
	const Character3DColliderSettings *collider_settings;
	const Character3DWeaponsDef *weapons_def;
	const SpringBonesDef *spring_bones;   /* optional: array of sets, one tuning each, count 0 terminates */
	const Character3DAimingSettings *aiming_settings;   /* optional: spine chain for the camera-pitch bend */
	const Character3DSoundDef *sound_def;
	const Character3DStatsSettings *stats_settings;

} Character3DDef;

typedef struct Character3D {

	Entity3D             *entity;
	KinematicBody         body;
	Character3DCollider   collider;
	Character3DMovement   movement;
	Character3DAnimation  animation;
	Character3DWeapons    weapons;
	Character3DAiming     aiming;
	Character3DSound      sound;
	SkeletonModifiers     skeleton_modifiers;
	Character3DStats      stats;

} Character3D;


Character3D *character3d_create(const Character3DDef *def, Entity3D *entity);
void character3d_delete(Character3D *character);

/* Model-space pose of a bone, composed from the local TRS chain so it is
   current-frame (bone->matrix would lag one skeleton update behind). */
void character3d_getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation);


#endif
