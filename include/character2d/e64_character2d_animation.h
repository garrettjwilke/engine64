/*
	The 2D character's animation: which clip plays and which of its frames,
	read off the movement. The locomotion side is character3d_animation
	reduced to what sprites need, by way of billboard_character_pyrite: the
	gait axis and its update are getGaitParam and getGaitAxis, the clip change
	carries the phase the way syncGridClips does, and the clip rate follows
	the speed the frames were drawn at. The action side follows
	setJumpParams and setRollParam: the crouch of a charged jump opens the
	take-off clip, the air runs the fall, the touchdown plays the landing,
	and the roll owns the body for the length of its clip. Where the 3D
	layers a weight over the locomotion, a sprite can only show one clip, so
	the action holds the frame until it ends or the body moves on.

	A clip is a run of sprite files named after the first, "<name>_f00" and
	the number counting up. The animation opens every frame on init and hands
	the entity the current one.
*/
#ifndef ENGINE64_CHARACTER2D_ANIMATION_H
#define ENGINE64_CHARACTER2D_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Character2D Character2D;


typedef struct {

	const char *path;         /* the first frame's file */
	uint8_t     frame_count;
	float       fps;          /* frames per second the clip was authored at */
	bool        is_looping;

} Character2DAnimationClipDef;


/* Where inside the action clips the body meets the world, in seconds from
   the clip's start, the way Character3DAnimationSettings names them. */
typedef struct {

	/* The frame of the landing clip where the foot touches the ground: the
	   landing starts that long before the floor, so the contact lands on
	   the touchdown whatever the drop was. */
	float land_anim_ground;

} Character2DAnimationSettings;


typedef struct {

	const Character2DAnimationClipDef  *clip;
	const Character2DAnimationSettings *settings;
	uint8_t                             clip_count;

	/* Which clip plays for what the body does, as indices into the table
	   above. Walk, run and sprint share the stride: the gait axis picks
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

} Character2DAnimationDef;


typedef struct Character2DAnimation {

	const Character2DAnimationDef *def;

	/* Every frame of every clip, open, in clip order; frame_start is where
	   each clip's run begins. The paths are kept for as long as the frames
	   are open, since the resource table holds the pointer. */
	struct sprite_s **frame_sprite;
	uint16_t         *frame_start;
	char             *path;

	/* ANIMATION_PARAM_WALK_GAIT of the 3D animation: 0 sits on the first
	   gait, 1 on the last, and the values between are where the speed
	   falls. */
	float gait_axis;

	/* Which movement state the action clips last answered to: what tells an
	   entry into the air or the roll from a frame already inside it. */
	uint8_t action_state;
	bool    landing;

	uint8_t clip;
	float   phase;          /* frames into the clip */
	uint8_t frame;          /* frame the phase lands on */

} Character2DAnimation;


/* Opens the frames and parks the animation on the idle. */
void character2dAnimation_init(Character2D *character, const Character2DAnimationDef *def);
void character2dAnimation_free(Character2D *character);

/* Runs one step: reads the movement, leaves clip and frame ready to draw. */
void character2dAnimation_update(Character2D *character, float dt);

/* True once a non-looping clip has run past its last frame. */
bool character2dAnimation_isFinished(const Character2D *character);

/* The sprite of the frame the animation is on. */
struct sprite_s *character2dAnimation_getSprite(const Character2D *character);

#endif
