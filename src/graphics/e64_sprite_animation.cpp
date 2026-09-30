/*
	The clip playback that was inside character2d::Animation, taken out so a
	prop or the game can drive a sprite's frames too. The choice of clip
	stayed with the character.
*/
#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdragon.h>

#include "graphics/e64_sprite_animation.h"
#include "resource/e64_resource.h"

namespace e64 {

namespace sprite {
namespace animation {

/* Opens every frame of every clip. A clip names its first frame; the number
   before the extension counts the rest, with as many digits as it was
   written with. */
static void loadFrames(Animation *animation)
{
	const Animation::Def *def = animation->def;

	uint16_t total = 0;
	size_t longest = 0;
	animation->frame_start = (uint16_t *)malloc(def->clip_count * sizeof(uint16_t));
	assert(animation->frame_start);

	for (int c = 0; c < def->clip_count; c++) {
		animation->frame_start[c] = total;
		total += def->clip[c].frame_count;
		size_t len = strlen(def->clip[c].path) + 1;
		if (len > longest) longest = len;
	}

	animation->frame_sprite = (sprite_t **)malloc(total * sizeof(sprite_t *));
	animation->path = (char *)malloc(total * longest);
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
			char *path = animation->path + index * longest;

			snprintf(path, longest, "%.*s%0*d%s", (int)(num - first), first, digits, base + f, dot);

			animation->frame_sprite[index] = (sprite_t *)resource::load(path, Resource::SPRITE, NULL);
			assert(animation->frame_sprite[index]);
		}
	}
}

void init(Animation *animation, const Animation::Def *def, uint8_t clip)
{
	assert(def && def->clip_count && clip < def->clip_count);

	*animation = (Animation){
		.def = def,
		.clip = clip,
	};
	loadFrames(animation);
}

void destroy(Animation *animation)
{
	const Animation::Def *def = animation->def;

	uint16_t total = animation->frame_start[def->clip_count - 1] + def->clip[def->clip_count - 1].frame_count;
	for (int i = 0; i < total; i++)
		resource::unload(animation->frame_sprite[i]);

	::free(animation->frame_sprite);
	::free(animation->frame_start);
	::free(animation->path);
}

/* The frame the phase lands on: wrapped into the clip when it loops, held on
   the last one when it does not. */
static void setFrameFromPhase(Animation *animation)
{
	const Animation::ClipDef *current = &animation->def->clip[animation->clip];

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

void setClip(Animation *animation, uint8_t clip, float phase)
{
	assert(clip < animation->def->clip_count);

	animation->clip = clip;
	animation->phase = phase;
	animation->frame = 0;
}

void setFrame(Animation *animation, uint8_t frame)
{
	animation->phase = (float)frame;
	setFrameFromPhase(animation);
}

void setRate(Animation *animation, float rate)
{
	animation->rate = rate;
}

void advance(Animation *animation, float dt)
{
	const Animation::ClipDef *current = &animation->def->clip[animation->clip];

	animation->phase += dt * current->fps * animation->rate;
	setFrameFromPhase(animation);
}

bool isFinished(const Animation *animation)
{
	const Animation::ClipDef *current = &animation->def->clip[animation->clip];
	return !current->is_looping && animation->phase >= (float)current->frame_count;
}

sprite_t *getSprite(const Animation *animation)
{
	return animation->frame_sprite[animation->frame_start[animation->clip] + animation->frame];
}

}
}

}
