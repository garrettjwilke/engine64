#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "graphics/e64_font.h"
#include "resource/e64_resource.h"
#include "viewport/e64_viewport.h"

namespace e64 {

namespace font {

/* The game's table, handed over at init. rdpq keeps the loaded font behind
   its id, so nothing else is stored here. */
static const Font::Def *font_def;
static uint8_t font_count;

/* Who holds each id open: the 2D scene and the interface load the fonts
   their texts name on their own, and rdpq refuses an id registered twice.
   The first ask registers it, the last release unregisters it. */
static uint8_t font_users[256];

/* The path each id was loaded from, with the display filled in. The
   resource table keeps the pointer it is handed, so the string has to
   outlive the load. */
static constexpr int PATH_MAX_LENGTH = 96;
static char (*font_path)[PATH_MAX_LENGTH];


void init(const Font::Def *fonts, uint8_t count)
{
	font_def = fonts;
	font_count = count;

	font_path = (char (*)[PATH_MAX_LENGTH])calloc(count, PATH_MAX_LENGTH);
	assert(font_path);
}

void loadAsset(uint8_t id)
{
	assert(id < font_count && font_def[id].path);

	if (font_users[id]++) return;

	const Font::Def *def = &font_def[id];

	/* The copy rasterized for the screen in force: the shape of a glyph
	   follows the shape of the pixel, so each video mode has its own. */
	const Viewport::ModeDef *mode = &viewport::get()->mode;
	char display[16];
	snprintf(display, sizeof display, "%ldx%ld", (long)mode->width, (long)mode->height);
	snprintf(font_path[id], PATH_MAX_LENGTH, def->path, display);

	rdpq_font_t *font = (rdpq_font_t *)resource::load(font_path[id], Resource::FONT, NULL);
	assert(font);

	for (int i = 0; i < def->style_count; i++)
		rdpq_font_style(font, def->style[i].id, &def->style[i].style);

	rdpq_text_register_font(id, font);
}

void unloadAsset(uint8_t id)
{
	assert(font_users[id]);
	if (--font_users[id]) return;

	rdpq_font_t *font = (rdpq_font_t *)rdpq_text_get_font(id);
	rdpq_text_unregister_font(id);
	resource::unload(font);
}

}


namespace text {

void draw(const Text *element, Vector2 position)
{
	if (element->parms == NULL) {
		rdpq_text_printf(NULL, element->font, position.x, position.y, "^%02d%s", element->style, element->text);
		return;
	}

	/* The box is declared in the pixels of the base screen, like the
	   position: a mode that splits its columns lays two of them down for
	   every one across. */
	Vector2 scale = viewport::getScale();
	rdpq_textparms_t parms = *element->parms;
	parms.width *= scale.x;
	parms.height *= scale.y;

	rdpq_text_printf(&parms, element->font, position.x, position.y, "^%02d%s", element->style, element->text);
}

}

}
