#include <stdio.h>
#include <stdarg.h>
#include <libdragon.h>

#include "debug/e64_debug.h"
#include "graphics/e64_font.h"
#include "viewport/e64_viewport.h"
#include "camera/e64_camera3d.h"
#include "camera/e64_spring_arm.h"

namespace e64 {

namespace debug {

namespace ui {

static UI u;


void init(void)
{
	rdpq_text_register_font(Font::DEBUG,
	                        rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
	u.active = true;
}

void show(bool show)
{
	u.active = show;
}

void set(uint8_t line, const char *fmt, ...)
{
	if (line >= UI::LINES) return;

	va_list args;
	va_start(args, fmt);
	vsnprintf(u.line[line], UI::LINE_MAX, fmt, args);
	va_end(args);
}

void setRight(uint8_t line, const char *fmt, ...)
{
	if (line >= UI::LINES) return;

	va_list args;
	va_start(args, fmt);
	vsnprintf(u.right[line], UI::LINE_MAX, fmt, args);
	va_end(args);
}

/* The state asks here; the frame is not attached yet, so the drawing
   itself waits for ui::draw. The request lasts one frame. */
void showFPS(void)
{
	u.show_fps = true;
}

void draw(void)
{
	if (!u.active) return;

	for (int i = 0; i < UI::LINES; i++) {
		if (u.line[i][0] == '\0') continue;
		rdpq_text_print(NULL, Font::DEBUG,
		                UI::X, UI::Y + i * UI::STEP,
		                u.line[i]);
	}

	/* The right hand column, laid out across the screen in force and pushed
	   against its right edge. The rate goes on top when it was asked for, and
	   whatever the game wrote follows underneath, in order. */
	rdpq_textparms_t right = {
		.width = (int16_t)(display_get_width() - 2 * UI::MARGIN),
		.align = ALIGN_RIGHT,
	};

	float y = UI::MARGIN + (UI::STEP * 0.5f);

	if (u.show_fps) {
		rdpq_text_printf(&right, Font::DEBUG, UI::MARGIN, y,
		                 "fps %.1f", display_get_fps());
		y += UI::STEP;
		u.show_fps = false;
	}

	for (int i = 0; i < UI::LINES; i++) {
		if (u.right[i][0] == '\0') continue;
		rdpq_text_print(&right, Font::DEBUG, UI::MARGIN, y, u.right[i]);
		y += UI::STEP;
	}
}

}

}

}
