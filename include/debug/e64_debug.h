/*
	Debug UI: live values on screen, independent of whatever state runs.

	A special module on purpose: it skips the scene2d path and draws its
	lines straight through rdpq at the tail of every frame, with libdragon's
	builtin mono font — no game asset, no def, no load. Off, it costs one
	branch.
*/
#ifndef ENGINE64_DEBUG_H
#define ENGINE64_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

namespace e64 {

namespace debug {

class UI {
public:

	static constexpr uint8_t LINES = 8;
	static constexpr uint8_t LINE_MAX = 40;

	static constexpr float X = 16.0f;
	static constexpr float Y = 24.0f;
	static constexpr float STEP = 10.0f;

	/* The gap the right hand column keeps from the top and from the edge.
	   Where that edge is comes from the screen in force, never from a fixed
	   width. */
	static constexpr float MARGIN = 10.0f;

	char line[LINES][LINE_MAX];
	char right[LINES][LINE_MAX];
	bool active;
	bool show_fps;

};


namespace ui {

/* Registers the builtin mono font and turns the overlay on. Once, after
   game::init. Never called, the overlay stays off and draws nothing. */
void init(void);

void show(bool show);

/* Rewrites one line, printf style. Lines draw top to bottom in slot order;
   an empty slot skips its row. */
void set(uint8_t line, const char *fmt, ...);

/* The same, against the right edge instead of the left, under the framerate
   when that is showing. What is worth watching there is the game's to decide:
   the engine puts nothing of its own in this column. */
void setRight(uint8_t line, const char *fmt, ...);

/* Asks for the framerate at the top right, first line. Call it from the
   state update; the number draws at the tail of that same frame and the
   request expires with it. */
void showFPS(void);

/* Drawn by the render at the tail of the frame, over everything. */
void draw(void);

}

}

}

#endif
