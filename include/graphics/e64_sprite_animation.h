#ifndef ENGINE64_SPRITE_ANIMATION_H
#define ENGINE64_SPRITE_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

namespace e64 {

namespace sprite {

/* The animation of a sprite: a table of clips, each a run of sprite files,
   and the one playing with the frame it is on. It advances the clip it is
   given and nothing else. Which clip plays, and when, is the owner's
   business: a character picks it from its movement, a prop or the game from
   their own logic, and the frame can be set by hand as well.

   A clip is a run of sprite files named after the first, "<name>_f00" and
   the number counting up. Every frame of every clip is opened on init. */
class Animation {
public:

	struct ClipDef {

		const char *path; /* the first frame's file */
		uint8_t frame_count;
		float fps; /* frames per second the clip was authored at */
		bool is_looping;

	};

	struct Def {

		const ClipDef *clip;
		uint8_t clip_count;

	};


	const Def *def;

	/* Every frame of every clip, open, in clip order; frame_start is where
	   each clip's run begins. The paths are kept for as long as the frames
	   are open, since the resource table holds the pointer. */
	struct sprite_s **frame_sprite;
	uint16_t *frame_start;
	char *path;

	uint8_t clip;
	float phase; /* frames into the clip */
	uint8_t frame; /* frame the phase lands on */

	/* Speed the clip runs at, times its fps: 1 plays it as authored, 0
	   holds it. Starts at 0, until the owner sets one. */
	float rate;

};


namespace animation {

/* Opens the frames and parks the animation on the first frame of clip. */
void init(Animation *animation, const Animation::Def *def, uint8_t clip);
void destroy(Animation *animation);

/* Enters clip at phase, in frames into it. */
void setClip(Animation *animation, uint8_t clip, float phase);

/* Puts the current clip on frame, for whoever drives the frames by hand. */
void setFrame(Animation *animation, uint8_t frame);

/* Sets the speed the clip runs at, for the advances that follow. */
void setRate(Animation *animation, float rate);

/* Runs the current clip dt seconds at its fps times the rate. A looping
   clip wraps, any other stops on its last frame. */
void advance(Animation *animation, float dt);

/* True once a non-looping clip has run past its last frame. */
bool isFinished(const Animation *animation);

/* The sprite of the frame the animation is on. */
struct sprite_s *getSprite(const Animation *animation);

}
}

}

#endif
