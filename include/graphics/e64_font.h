#ifndef ENGINE64_FONT_H
#define ENGINE64_FONT_H

#include <libdragon.h>
#include "math/e64_vector2.h"

namespace e64 {

/* The game's fonts. rdpq keeps every loaded font behind its id, so the
   engine holds no instance of its own: only the table of definitions. */
class Font {
public:

	struct Style {
		uint8_t id; /* rdpq style id */
		rdpq_fontstyle_t style; /* color, outline color, custom callback */
	};


	struct Def {
		const char *path; /* NULL = unused slot (0 is: rdpq reserves font id 0) */
		const Style *style;
		uint8_t style_count;
	};


	/* The game's font ids are rdpq ids, counting up from 1. The engine's own
	   fonts count down from the top of rdpq's 256 slots, so the two never meet. */
	static constexpr uint8_t DEBUG = 255;
};


class Text {
public:

	uint8_t font;
	uint8_t style;
	const char *text;
	const rdpq_textparms_t *parms;
};


namespace font {

/* Hands the engine the game's font table, indexed by font id. Runs once,
   before any scene loads. Nothing is loaded here: the 2D scene loads the
   fonts its texts name and frees them on the way out. */
void init(const Font::Def *fonts, uint8_t count);

void loadAsset(uint8_t id);
void unloadAsset(uint8_t id);

}


namespace text {

void draw(const Text *element, Vector2 position);

}

}

#endif
