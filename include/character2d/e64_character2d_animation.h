/*
	The 2D character's animation: which clip plays and which of its frames,
	read off the movement. The locomotion side is character3d::Animation
	reduced to what sprites need, by way of billboard_character_pyrite: the
	gait axis and its update are getGaitParam and getGaitAxis, the clip change
	carries the phase the way syncGridClips does, and the clip rate follows
	the speed the frames were drawn at. The action side follows
	setJumpParams and setRollParam: the crouch of a charged jump opens the
	take-off clip, the air runs the fall, the touchdown plays the landing,
	and the roll owns the body for the length of its clip. Where the 3D
	layers a weight over the locomotion, a sprite can only show one clip, so
	the action holds the frame until it ends or the body moves on.

	The clips and their playback are the entity sprite's e64::sprite::Animation:
	this picks which one plays and hands it over, the way
	character3d::Animation sets the params of the mesh's e64::Animation graph.
*/
#ifndef ENGINE64_CHARACTER2D_ANIMATION_H
#define ENGINE64_CHARACTER2D_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#include "graphics/e64_sprite_animation.h"

namespace e64 {

typedef struct Character2D Character2D;


namespace character2d {

/* Where inside the action clips the body meets the world, in seconds from
   the clip's start, the way character3d::AnimationSettings names them. */
typedef struct {

	/* The frame of the landing clip where the foot touches the ground: the
	   landing starts that long before the floor, so the contact lands on
	   the touchdown whatever the drop was. */
	float land_anim_ground;

} AnimationSettings;


typedef struct {

	const AnimationSettings *settings;

	/* Which clip plays for what the body does, as indices into the sprite's
	   clip table. Walk, run and sprint share the stride: the gait axis picks
	   among them and the phase carries across the change. Jump is the
	   take-off and runs into fall when it ends; land and roll play once
	   through. */
	uint8_t idle_animation;
	uint8_t walk_animation;
	uint8_t run_animation;
	uint8_t sprint_animation;
	uint8_t jump_animation;
	uint8_t fall_animation;
	uint8_t land_animation;
	uint8_t roll_animation;

} AnimationDef;


typedef struct Animation {

	const AnimationDef *def;

	e64::sprite::Animation *sprite; /* the entity's */

	/* ANIMATION_PARAM_WALK_GAIT of the 3D animation: 0 sits on the first
	   gait, 1 on the last, and the values between are where the speed
	   falls. */
	float gait_axis;

	/* Which movement state the action clips last answered to: what tells an
	   entry into the air or the roll from a frame already inside it. */
	uint8_t action_state;
	bool landing;

} Animation;


namespace animation {

/* Takes the entity sprite's animation and parks it on the idle. */
void init(Character2D *character, const AnimationDef *def);

/* Runs one step: reads the movement, leaves clip and frame ready to draw. */
void update(Character2D *character, float dt);

}
}

}

#endif
