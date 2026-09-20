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

namespace e64 {

typedef struct Entity3D Entity3D;

/* An aggregate on purpose: no constructor, public data. character3d::create
   builds it with a designated initializer over one allocation that also
   carries the spring bones behind it. */
class Character3D {
public:

	Entity3D                   *entity;
	character3d::KinematicBody  body;
	character3d::Collider       collider;
	character3d::Movement       movement;
	character3d::Animation      animation;
	character3d::Weapons        weapons;
	character3d::Aiming         aiming;
	character3d::Sound          sound;
	SkeletonModifiers           skeleton_modifiers;
	character3d::Stats          stats;

	void updateMovement(character3d::MovementCommand *cmd, float dt);
	void setAnimation();
};


namespace character3d {

typedef struct Def {

	const MovementSettings *movement_settings;
	const AnimationDef *animation_def;
	const ColliderSettings *collider_settings;
	const WeaponsDef *weapons_def;
	const SpringBonesDef *spring_bones;   /* optional: array of sets, one tuning each, count 0 terminates */
	const AimingSettings *aiming_settings;   /* optional: spine chain for the camera-pitch bend */
	const SoundDef *sound_def;
	const StatsSettings *stats_settings;

} Def;


Character3D *create(const Def *def, Entity3D *entity);
void destroy(Character3D *character);

/* Model-space pose of a bone, composed from the local TRS chain so it is
   current-frame (bone->matrix would lag one skeleton update behind). */
void getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation);

}


}

#endif
