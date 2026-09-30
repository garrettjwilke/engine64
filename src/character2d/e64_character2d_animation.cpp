/*
	Port of billboard_character_pyrite's character_animation.cpp, which is
	character3d::Animation reduced to what sprites need, with the action side
	of setJumpParams and setRollParam brought across the same way. What there
	drove a blend graph over skeleton buffers has no meaning for sprites and
	did not come across.

	The clips and their playback are the entity sprite's
	e64::sprite::Animation; what is left here is the choice of clip off the
	movement.
*/
#include <assert.h>
#include <math.h>
#include <fmath.h>
#include <libdragon.h>

#include "character2d/e64_character2d.h"

namespace e64 {

namespace character2d {
namespace animation {

void init(Character2D *character, const AnimationDef *def)
{
	e64::sprite::Animation *sprite = character->entity->graphic->sprite.animation;
	assert(def && sprite);

	Animation *animation = &character->animation;

	*animation = (Animation){
		.def = def,
		.sprite = sprite,
		.action_state = MOVEMENT2D_STATE_IDLE,
	};
	e64::sprite::animation::setClip(sprite, def->idle_animation, 0.0f);
}


/* --- gait ------------------------------------------------------------------- */

/* Whether the character is carrying itself somewhere, as opposed to
   standing. The 3D animation asks this with a blend weight, because there
   the idle and the locomotion grid overlap and it needs to know by how
   much. Nothing overlaps here: it is idle or it is walking. */
static bool isMoving(float speed)
{
	return speed > 0.0f;
}

/* The clips of the locomotion grid: the same stride at different speeds.
   Only these hand their phase over, the way the 3D grid clips do. */
static bool sharesStride(const AnimationDef *def, uint8_t clip)
{
	return clip == def->walk_animation || clip == def->run_animation || clip == def->sprint_animation;
}

/* gait axis: gait i sits at i / (count - 1) */
static float getGaitParam(float speed, const MovementSettings *settings)
{
	const uint8_t last = settings->gait_count - 1;
	if (last == 0 || speed <= settings->gait[0].target_speed) return 0.0f;

	for (uint8_t i = 0; i < last; i++) {
		const float lo = settings->gait[i].target_speed;
		const float hi = settings->gait[i + 1].target_speed;
		if (speed > hi) continue;
		return (i + (speed - lo) / (hi - lo)) / last;
	}
	return 1.0f;
}

static void getGaitAxis(Character2D *character, float dt)
{
	Animation *animation = &character->animation;
	const Movement *movement = &character->movement;
	const MovementSettings *settings = movement->settings;
	const float speed = movement->data.horizontal_speed;

	const float raw_gait = getGaitParam(speed, settings);
	const float prev_gait = animation->gait_axis;

	uint8_t state = movement->current;
	if (!character2d::movement::isLocomotion(state)) state = movement->locomotion;

	if (!isMoving(speed)) { animation->gait_axis = raw_gait; return; }

	/* Standing still the axis is frozen: recomputing it at zero speed would
	   drop the character back to the first gait every time it stops. */
	if (state == MOVEMENT2D_STATE_IDLE) return;

	const uint8_t last = settings->gait_count - 1;
	uint8_t gait = movement->data.gait;
	if (gait > last) gait = last;

	/* The stick can ask for a gait the speed has not reached yet, and the
	   axis converges on whichever of the two is higher. */
	const float asked_gait = (last > 0) ? (float)gait / last : 0.0f;
	const float target_gait = (asked_gait > raw_gait) ? asked_gait : raw_gait;

	const float factor = fm_expf(-settings->gait[gait].response_rate * dt);
	animation->gait_axis = prev_gait * factor + target_gait * (1.0f - factor);
}

/* The locomotion clip at the axis. Where the 3D animation would blend
   between the gaits of the grid, the axis is rounded to the nearest one. */
static uint8_t selectLocomotionClip(const Character2D *character)
{
	const Animation *animation = &character->animation;
	const AnimationDef *def = animation->def;
	const Movement *movement = &character->movement;

	if (!isMoving(movement->data.horizontal_speed)) return def->idle_animation;

	const uint8_t last = movement->settings->gait_count - 1;
	if (last == 0) return def->walk_animation;

	/* The axis is a position over the gait table and the nearest gait wins:
	   the first is the walk, the last the sprint, anything between the run. */
	const uint8_t gait = (uint8_t)(animation->gait_axis * last + 0.5f);
	if (gait == 0) return def->walk_animation;
	if (gait == last) return def->sprint_animation;
	return def->run_animation;
}


/* --- actions ---------------------------------------------------------------- */

/* In the air, or about to be: the crouch that starts a charged jump runs on
   the ground but already belongs to the air. */
static bool isAerial(const Character2D *character)
{
	return character->movement.current == MOVEMENT2D_STATE_FALLING
	    || character->movement.data.jump_timer > 0.0f;
}

/* The action clips: whichever one the movement state hands the body to, or
   none when the locomotion owns it. Mirrors setJumpParams and setRollParam:
   the state change is the entry mark, and a clip entered plays through. */
static uint8_t selectActionClip(Character2D *character, bool *restart)
{
	Animation *animation = &character->animation;
	const AnimationDef *def = animation->def;
	const Movement *movement = &character->movement;
	uint8_t cur = movement->current;
	uint8_t *as = &animation->action_state;

	*restart = false;

	/* The roll owns the body for as long as its state lasts. */
	if (cur == MOVEMENT2D_STATE_ROLLING) {
		if (*as != MOVEMENT2D_STATE_ROLLING) { *as = cur; *restart = true; animation->landing = false; }
		return def->roll_animation;
	}
	if (*as == MOVEMENT2D_STATE_ROLLING) *as = cur;

	bool aerial = isAerial(character);

	/* One owner for the air: the crouch on the ground opens it and the fall
	   keeps it. Entering with a crouch plays the take-off clip and the fall
	   follows when it ends; entering without one starts on the fall. */
	if (aerial) {
		if (*as != MOVEMENT2D_STATE_FALLING) {
			*as = MOVEMENT2D_STATE_FALLING;
			animation->landing = false;
			*restart = true;
			return movement->data.jump_timer > 0.0f ? def->jump_animation : def->fall_animation;
		}

		/* The landing starts one clip-to-contact away from the floor: the
		   foot meets the ground on the frame the clip has it touching,
		   whatever the drop was. Once started it runs through the touchdown. */
		if (animation->landing) return def->land_animation;

		float floor_distance = movement->data.floor_distance;
		float fall_speed = movement->data.velocity.y;
		if (floor_distance >= 0.0f && fall_speed > 0.0f
		 && floor_distance <= fall_speed * def->settings->land_anim_ground) {
			animation->landing = true;
			*restart = true;
			return def->land_animation;
		}

		if (animation->sprite->clip == def->jump_animation && !e64::sprite::animation::isFinished(animation->sprite))
			return def->jump_animation;
		if (animation->sprite->clip != def->fall_animation) *restart = true;
		return def->fall_animation;
	}

	/* Touching down with no landing under way, a drop too short to have
	   seen the floor coming, starts it on the contact. The 3D lets the
	   locomotion run under its weight; a sprite shows one clip, so the
	   landing holds until it ends or the body walks out of it. */
	if (*as == MOVEMENT2D_STATE_FALLING) {
		*as = cur;
		if (!animation->landing) {
			animation->landing = true;
			*restart = true;
			return def->land_animation;
		}
	}
	if (animation->landing) {
		if (e64::sprite::animation::isFinished(animation->sprite) || cur == MOVEMENT2D_STATE_WALKING)
			animation->landing = false;
		else
			return def->land_animation;
	}

	return animation->sprite->def->clip_count; /* none: the locomotion picks */
}


/* --- playback --------------------------------------------------------------- */

/* Which pair of the table a 0..1 axis falls between, and how far along.
   Returns the lower index; t is the fraction toward the next one. */
static uint8_t blendSegment(float weight, uint8_t count, float *t)
{
	if (count < 2) { *t = 0.0f; return 0; }
	if (weight < 0.0f) weight = 0.0f;
	if (weight > 1.0f) weight = 1.0f;

	const float s = weight * (count - 1);
	uint8_t i = (uint8_t)s;
	if (i > count - 2) i = count - 2;
	*t = s - i;
	return i;
}

/* Speed the clip playing right now was drawn at, read off the gait table at
   the axis. Zero for clips that do not travel. */
static float getReferenceSpeed(const Character2D *character)
{
	const Animation *animation = &character->animation;
	const MovementSettings *settings = character->movement.settings;

	if (!sharesStride(animation->def, animation->sprite->clip)) return 0.0f;

	if (settings->gait_count == 0) return 0.0f;
	if (settings->gait_count == 1) return settings->gait[0].target_speed;

	float t;
	const uint8_t row = blendSegment(animation->gait_axis, settings->gait_count, &t);
	return settings->gait[row].target_speed
	     + t * (settings->gait[row + 1].target_speed - settings->gait[row].target_speed);
}

static void setRate(Character2D *character)
{
	/* The clip was drawn moving at that speed, so running it at any other
	   one slides the feet. The frames are scaled by the difference. */
	float rate = 1.0f;
	const float reference = getReferenceSpeed(character);
	if (reference > 0.0f) rate = character->movement.data.horizontal_speed / reference;

	e64::sprite::animation::setRate(character->animation.sprite, rate);
}

static void setClip(Animation *animation, uint8_t wanted, bool restart)
{
	const AnimationDef *def = animation->def;
	e64::sprite::Animation *sprite = animation->sprite;

	if (wanted == sprite->clip && !restart) return;

	const e64::sprite::Animation::ClipDef *from = &sprite->def->clip[sprite->clip];
	const e64::sprite::Animation::ClipDef *to = &sprite->def->clip[wanted];

	/* Phase carry, the 3D syncGridClips: the clip coming in starts where
	   the one going out was, measured as a fraction of its own cycle. The
	   clips have different lengths and still share the stride, so the foot
	   that was down stays down across the change.

	   Only inside the grid. A stride handed to a roll or a landing means
	   nothing, and those restart. */
	float carried = 0.0f;
	if (!restart && sharesStride(def, sprite->clip)
	 && sharesStride(def, wanted) && from->frame_count > 0) {
		carried = (sprite->phase / (float)from->frame_count) * (float)to->frame_count;
	}

	e64::sprite::animation::setClip(sprite, wanted, carried);
}

void update(Character2D *character, float dt)
{
	Animation *animation = &character->animation;

	getGaitAxis(character, dt);

	bool restart;
	uint8_t wanted = selectActionClip(character, &restart);
	if (wanted == animation->sprite->def->clip_count)
		wanted = selectLocomotionClip(character);

	setClip(animation, wanted, restart);
	setRate(character);
}

}
}

}
