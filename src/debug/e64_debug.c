#include <stdio.h>
#include <stdarg.h>
#include <libdragon.h>

#include "debug/e64_debug.h"
#include "graphics/e64_font.h"
#include "viewport/e64_viewport.h"
#include "camera/e64_camera.h"
#include "camera/e64_spring_arm.h"


#define DEBUG_UI_LINES     8
#define DEBUG_UI_LINE_MAX 40

#define DEBUG_UI_X    16.0f
#define DEBUG_UI_Y    24.0f
#define DEBUG_UI_STEP 10.0f

/* The gap the right hand column keeps from the top and from the edge. Where
   that edge is comes from the screen in force, never from a fixed width. */
#define DEBUG_UI_MARGIN 10.0f


static char debug_line[DEBUG_UI_LINES][DEBUG_UI_LINE_MAX];
static char debug_right[DEBUG_UI_LINES][DEBUG_UI_LINE_MAX];
static bool debug_active;
static bool debug_show_fps;


void debugUI_init(void)
{
	rdpq_text_register_font(DEBUG_FONT,
	                        rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
	debug_active = true;
}

void debugUI_show(bool show)
{
	debug_active = show;
}

void debugUI_set(uint8_t line, const char *fmt, ...)
{
	if (line >= DEBUG_UI_LINES) return;

	va_list args;
	va_start(args, fmt);
	vsnprintf(debug_line[line], DEBUG_UI_LINE_MAX, fmt, args);
	va_end(args);
}

void debugUI_setRight(uint8_t line, const char *fmt, ...)
{
	if (line >= DEBUG_UI_LINES) return;

	va_list args;
	va_start(args, fmt);
	vsnprintf(debug_right[line], DEBUG_UI_LINE_MAX, fmt, args);
	va_end(args);
}

/* The state asks here; the frame is not attached yet, so the drawing
   itself waits for debugUI_draw. The request lasts one frame. */
void debugUI_showFPS(void)
{
	debug_show_fps = true;
}

void debugUI_draw(void)
{
	if (!debug_active) return;

	for (int i = 0; i < DEBUG_UI_LINES; i++) {
		if (debug_line[i][0] == '\0') continue;
		rdpq_text_print(NULL, DEBUG_FONT,
		                DEBUG_UI_X, DEBUG_UI_Y + i * DEBUG_UI_STEP,
		                debug_line[i]);
	}

	/* The right hand column, laid out across the screen in force and pushed
	   against its right edge. The rate goes on top when it was asked for, and
	   whatever the game wrote follows underneath, in order. */
	rdpq_textparms_t right = {
		.width = (int16_t)(display_get_width() - 2 * DEBUG_UI_MARGIN),
		.align = ALIGN_RIGHT,
	};

	float y = DEBUG_UI_MARGIN + (DEBUG_UI_STEP * 0.5f);

	if (debug_show_fps) {
		rdpq_text_printf(&right, DEBUG_FONT, DEBUG_UI_MARGIN, y,
		                 "fps %.1f", display_get_fps());
		y += DEBUG_UI_STEP;
		debug_show_fps = false;
	}

	for (int i = 0; i < DEBUG_UI_LINES; i++) {
		if (debug_right[i][0] == '\0') continue;
		rdpq_text_print(&right, DEBUG_FONT, DEBUG_UI_MARGIN, y, debug_right[i]);
		y += DEBUG_UI_STEP;
	}
}
