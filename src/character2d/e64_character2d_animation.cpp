/*
	Port of billboard_character_pyrite's character_animation.cpp, which is
	character3d_animation reduced to what sprites need, with the action side
	of setJumpParams and setRollParam brought across the same way. What there
	drove a blend graph over skeleton buffers has no meaning for sprites and
	did not come across.
*/
#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <fmath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "character2d/e64_character2d.h"
#include "resource/e64_resource.h"


/* --- frames ----------------------------------------------------------------- */

/* Opens every frame of every clip. A clip names its first frame; the number
   before the extension counts the rest, with as many digits as it was
   written with. */
static void character2dAnimation_loadFrames(Character2DAnimation *animation)
{
	const Character2DAnimationDef *def = animation->def;

	uint16_t total   = 0;
	size_t   longest = 0;
	animation->frame_start = (uint16_t *)malloc(def->clip_count * sizeof(uint16_t));
	assert(animation->frame_start);

	for (int c = 0; c < def->clip_count; c++) {
		animation->frame_start[c] = total;
		total += def->clip[c].frame_count;
		size_t len = strlen(def->clip[c].path) + 1;
		if (len > longest) longest = len;
	}

	animation->frame_sprite = (sprite_t **)malloc(total * sizeof(sprite_t *));
	animation->path         = (char *)malloc(total * longest);
	assert(animation->frame_sprite && animation->path);

	for (int c = 0; c < def->clip_count; c++) {
		const char *first = def->clip[c].path;

		const char *dot = strrchr(first, '.');
		assert(dot);
		const char *num = dot;
		while (num > first && isdigit((unsigned char)num[-1])) num--;
		int digits = (int)(dot - num);
		assert(digits > 0);
		int base = atoi(num);

		for (int f = 0; f < def->clip[c].frame_count; f++) {
			uint16_t index = animation->frame_start[c] + f;
			char    *path  = animation->path + index * longest;

			snprintf(path, longest, "%.*s%0*d%s", (int)(num - first), first, digits, base + f, dot);

			animation->frame_sprite[index] = (sprite_t *)resource_load(path, RESOURCE_SPRITE, NULL);
			assert(animation->frame_sprite[index]);
		}
	}
}

void character2dAnimation_init(Character2D *character, const Character2DAnimationDef *def)
{
	assert(def && def->clip_count);

	Character2DAnimation *animation = &character->animation;

	*animation = (Character2DAnimation){
		.def          = def,
		.action_state = MOVEMENT2D_STATE_IDLE,
		.clip         = def->idle_animation,
	};
	character2dAnimation_loadFrames(animation);
}

void character2dAnimation_free(Character2D *character)
{
	Character2DAnimation *animation = &character->animation;
	const Character2DAnimationDef *def = animation->def;

	uint16_t total = animation->frame_start[def->clip_count - 1] + def->clip[def->clip_count - 1].frame_count;
	for (int i = 0; i < total; i++)
		resource_unload(animation->frame_sprite[i]);

	free(animation->frame_sprite);
	free(animation->frame_start);
	free(animation->path);
}

sprite_t *character2dAnimation_getSprite(const Character2D *character)
{
	const Character2DAnimation *animation = &character->animation;
	return animation->frame_sprite[animation->frame_start[animation->clip] + animation->frame];
}


/* --- gait ------------------------------------------------------------------- */

/* Whether the character is carrying itself somewhere, as opposed to
   standing. The 3D animation asks this with a blend weight, because there
   the idle and the locomotion grid overlap and it needs to know by how
   much. Nothing overlaps here: it is idle or it is walking. */
static bool character2dAnimation_isMoving(float speed)
{
	return speed > 0.0f;
}

/* The clips of the locomotion grid: the same stride at different speeds.
   Only these hand their phase over, the way the 3D grid clips do. */
static bool character2dAnimation_sharesStride(const Character2DAnimationDef *def, uint8_t clip)
{
	return clip == def->walk_animation || clip == def->run_animation || clip == def->sprint_animation;
}

/* gait axis: gait i sits at i / (count - 1) */
static float character2dAnimation_getGaitParam(float speed, const Character2DMovementSettings *settings)
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

static void character2dAnimation_getGaitAxis(Character2D *character, float dt)
{
	Character2DAnimation *animation = &character->animation;
	const Character2DMovement *movement = &character->movement;
	const Character2DMovementSettings *settings = movement->settings;
	const float speed = movement->data.horizontal_speed;

	const float raw_gait  = character2dAnimation_getGaitParam(speed, settings);
	const float prev_gait = animation->gait_axis;

	uint8_t state = movement->current;
	if (!character2dMovement_isLocomotion(state)) state = movement->locomotion;

	if (!character2dAnimation_isMoving(speed)) { animation->gait_axis = raw_gait; return; }

	/* Standing still the axis is frozen: recomputing it at zero speed would
	   drop the character back to the first gait every time it stops. */
	if (state == MOVEMENT2D_STATE_IDLE) return;

	const uint8_t last = settings->gait_count - 1;
	uint8_t gait = movement->data.gait;
	if (gait > last) gait = last;

	/* The stick can ask for a gait the speed has not reached yet, and the
	   axis converges on whichever of the two is higher. */
	const float asked_gait  = (last > 0) ? (float)gait / last : 0.0f;
	const float target_gait = (asked_gait > raw_gait) ? asked_gait : raw_gait;

	const float factor = fm_expf(-settings->gait[gait].response_rate * dt);
	animation->gait_axis = prev_gait * factor + target_gait * (1.0f - factor);
}

/* The locomotion clip at the axis. Where the 3D animation would blend
   between the gaits of the grid, the axis is rounded to the nearest one. */
static uint8_t character2dAnimation_selectLocomotionClip(const Character2D *character)
{
	const Character2DAnimation *animation = &character->animation;
	const Character2DAnimationDef *def = animation->def;
	const Character2DMovement *movement = &character->movement;

	if (!character2dAnimation_isMoving(movement->data.horizontal_speed)) return def->idle_animation;

	const uint8_t last = movement->settings->gait_count - 1;
	if (last == 0) return def->walk_animation;

	/* The axis is a position over the gait table and the nearest gait wins:
	   the first is the walk, the last the sprint, anything between the run. */
	const uint8_t gait = (uint8_t)(animation->gait_axis * last + 0.5f);
	if (gait == 0)    return def->walk_animation;
	if (gait == last) return def->sprint_animation;
	return def->run_animation;
}


/* --- actions ---------------------------------------------------------------- */

/* In the air, or about to be: the crouch that starts a charged jump runs on
   the ground but already belongs to the air. */
static bool character2dAnimation_isAerial(const Character2D *character)
{
	return character->movement.current == MOVEMENT2D_STATE_FALLING
	    || character->movement.data.jump_timer > 0.0f;
}

/* The action clips: whichever one the movement state hands the body to, or
   none when the locomotion owns it. Mirrors setJumpParams and setRollParam:
   the state change is the entry mark, and a clip entered plays through. */
static uint8_t character2dAnimation_selectActionClip(Character2D *character, bool *restart)
{
	Character2DAnimation *animation = &character->animation;
	const Character2DAnimationDef *def = animation->def;
	const Character2DMovement *movement = &character->movement;
	uint8_t  cur = movement->current;
	uint8_t *as  = &animation->action_state;

	*restart = false;

	/* The roll owns the body for as long as its state lasts. */
	if (cur == MOVEMENT2D_STATE_ROLLING) {
		if (*as != MOVEMENT2D_STATE_ROLLING) { *as = cur; *restart = true; animation->landing = false; }
		return def->roll_animation;
	}
	if (*as == MOVEMENT2D_STATE_ROLLING) *as = cur;

	bool aerial = character2dAnimation_isAerial(character);

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
		float fall_speed     = movement->data.velocity.y;
		if (floor_distance >= 0.0f && fall_speed > 0.0f
		 && floor_distance <= fall_speed * def->settings->land_anim_ground) {
			animation->landing = true;
			*restart = true;
			return def->land_animation;
		}

		if (animation->clip == def->jump_animation && !character2dAnimation_isFinished(character))
			return def->jump_animation;
		if (animation->clip != def->fall_animation) *restart = true;
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
		if (character2dAnimation_isFinished(character) || cur == MOVEMENT2D_STATE_WALKING)
			animation->landing = false;
		else
			return def->land_animation;
	}

	return def->clip_count;   /* none: the locomotion picks */
}


/* --- playback --------------------------------------------------------------- */

/* Which pair of the table a 0..1 axis falls between, and how far along.
   Returns the lower index; t is the fraction toward the next one. */
static uint8_t character2dAnimation_blendSegment(float weight, uint8_t count, float *t)
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
static float character2dAnimation_getReferenceSpeed(const Character2D *character)
{
	const Character2DAnimation *animation = &character->animation;
	const Character2DMovementSettings *settings = character->movement.settings;

	if (!character2dAnimation_sharesStride(animation->def, animation->clip)) return 0.0f;

	if (settings->gait_count == 0) return 0.0f;
	if (settings->gait_count == 1) return settings->gait[0].target_speed;

	float t;
	const uint8_t row = character2dAnimation_blendSegment(animation->gait_axis, settings->gait_count, &t);
	return settings->gait[row].target_speed
	     + t * (settings->gait[row + 1].target_speed - settings->gait[row].target_speed);
}

static void character2dAnimation_advance(Character2D *character, float dt)
{
	Character2DAnimation *animation = &character->animation;
	const Character2DAnimationClipDef *current = &animation->def->clip[animation->clip];

	/* The clip was drawn moving at that speed, so running it at any other
	   one slides the feet. The frames are scaled by the difference. */
	float rate = current->fps;
	const float reference = character2dAnimation_getReferenceSpeed(character);
	if (reference > 0.0f) rate *= character->movement.data.horizontal_speed / reference;

	animation->phase += dt * rate;

	if (current->frame_count == 0) { animation->frame = 0; return; }

	if (current->is_looping) {
		animation->phase = fmodf(animation->phase, (float)current->frame_count);
		if (animation->phase < 0.0f) animation->phase += (float)current->frame_count;
	}

	int index = (int)animation->phase;
	if (index >= current->frame_count) index = current->frame_count - 1;
	if (index < 0) index = 0;
	animation->frame = (uint8_t)index;
}

bool character2dAnimation_isFinished(const Character2D *character)
{
	const Character2DAnimation *animation = &character->animation;
	const Character2DAnimationClipDef *current = &animation->def->clip[animation->clip];
	return !current->is_looping && animation->phase >= (float)current->frame_count;
}

static void character2dAnimation_setClip(Character2DAnimation *animation, uint8_t wanted, bool restart)
{
	const Character2DAnimationDef *def = animation->def;

	if (wanted == animation->clip && !restart) return;

	const Character2DAnimationClipDef *from = &def->clip[animation->clip];
	const Character2DAnimationClipDef *to   = &def->clip[wanted];

	/* Phase carry, the 3D syncGridClips: the clip coming in starts where
	   the one going out was, measured as a fraction of its own cycle. The
	   clips have different lengths and still share the stride, so the foot
	   that was down stays down across the change.

	   Only inside the grid. A stride handed to a roll or a landing means
	   nothing, and those restart. */
	float carried = 0.0f;
	if (!restart && character2dAnimation_sharesStride(def, animation->clip)
	 && character2dAnimation_sharesStride(def, wanted) && from->frame_count > 0) {
		carried = (animation->phase / (float)from->frame_count) * (float)to->frame_count;
	}

	animation->clip  = wanted;
	animation->phase = carried;
	animation->frame = 0;
}

void character2dAnimation_update(Character2D *character, float dt)
{
	Character2DAnimation *animation = &character->animation;

	character2dAnimation_getGaitAxis(character, dt);

	bool    restart;
	uint8_t wanted = character2dAnimation_selectActionClip(character, &restart);
	if (wanted == animation->def->clip_count)
		wanted = character2dAnimation_selectLocomotionClip(character);

	character2dAnimation_setClip(animation, wanted, restart);
	character2dAnimation_advance(character, dt);
}
