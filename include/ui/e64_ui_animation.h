#ifndef ENGINE64_UI_ANIMATION_H
#define ENGINE64_UI_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

namespace e64 {

struct UI;


class UIAnimation {

public:

	enum Easing {

		EASING_LINEAR,

		EASING_QUAD_IN,
		EASING_QUAD_OUT,
		EASING_QUAD_IN_OUT,

		EASING_CUBIC_IN,
		EASING_CUBIC_OUT,
		EASING_CUBIC_IN_OUT,

		EASING_EXPO_IN,
		EASING_EXPO_OUT,
		EASING_EXPO_IN_OUT,

		EASING_COUNT,

	};

	enum PlayMode {

		PLAY_ONCE,
		PLAY_LOOP,
		PLAY_PING_PONG,

	};

	/* What of an entity a track writes. The name carries the type: the
	   first block is written as a float, the next as a byte, hidden as a
	   flag. */
	enum Field {

		FIELD_POSITION_X,
		FIELD_POSITION_Y,
		FIELD_SCALE_X,
		FIELD_SCALE_Y,
		FIELD_ROTATION,

		FIELD_TRANSPARENCY,
		FIELD_TEXT_STYLE,
		FIELD_SPRITE_FRAME,

		FIELD_COLOR_R,
		FIELD_COLOR_G,
		FIELD_COLOR_B,
		FIELD_COLOR_A,

		/* Which corner comes from the track's own corner field. */
		FIELD_GRADIENT_R,
		FIELD_GRADIENT_G,
		FIELD_GRADIENT_B,
		FIELD_GRADIENT_A,

		FIELD_HIDDEN,

	};

	/* A live source drives the value instead of time: the track names it
	   and the engine reads it fresh on every apply. */
	enum Source {

		SOURCE_NONE,
		SOURCE_MENU_INDEX, /* the menu stack cursor */

	};

	struct Track {

		/* Which widget of the live interface, by layer and placement, and
		   what of it. */
		uint8_t layer;
		uint8_t entity;
		uint8_t field; /* Field */
		uint8_t corner; /* gradient corner, 0..3 */

		float from;
		float to;
		bool from_bool;
		bool to_bool;
		float delay;
		float duration;
		Easing easing;

		uint8_t source; /* Source */
		const float *values_by_index;

	};

	struct Player {

		const UIAnimation *animation;
		PlayMode mode;
		float time;
		bool is_active;
		bool is_reversed;

	};

	const Track *track;
	uint8_t track_count;

};


namespace uiAnimation {

namespace player {

void start(UIAnimation::Player *player, UI *ui, const UIAnimation *animation, UIAnimation::PlayMode mode, bool is_reversed);
void stop(UIAnimation::Player *player);
void update(UIAnimation::Player *player, UI *ui, float dt);

}

void apply(UI *ui, const UIAnimation *animation, float time);

}

}

#endif
