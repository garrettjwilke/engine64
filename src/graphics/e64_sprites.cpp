#include <libdragon.h>
#include "graphics/e64_sprites.h"

namespace e64 {

/* Half of TMEM: the other half takes the palette of a colour indexed
   texture, and a tile never comes close to either. */
#define SPRITE_LOADABLE_BYTES 2048

bool sprite_isLoadable(const Sprite *element, float rotation)
{
	if (rotation != 0.0f || element->flip_x || element->tiled) return false;
	if (element->cols > 1 || element->rows > 1)                return false;

	sprite_t *s = element->asset;
	return TEX_FORMAT_PIX2BYTES(sprite_get_format(s), s->width * s->height) <= SPRITE_LOADABLE_BYTES;
}

void sprite_loadTexture(const Sprite *element)
{
	rdpq_sprite_upload(TILE0, element->asset, NULL);
}

void sprite_drawLoaded(const Sprite *element, Vector2 position, Vector2 scale)
{
	sprite_t *s = element->asset;

	rdpq_texture_rectangle_scaled(TILE0,
		position.x, position.y,
		position.x + s->width * scale.x, position.y + s->height * scale.y,
		0, 0, s->width, s->height);
}

void sprite_drawTiled(const Sprite *element, Vector2 position, Vector2 size)
{
	rdpq_texparms_t parms = {
		.s = { .repeats = REPEAT_INFINITE },
		.t = { .repeats = REPEAT_INFINITE },
	};
	rdpq_sprite_upload(TILE0, element->asset, &parms);
	rdpq_texture_rectangle(TILE0, position.x, position.y,
	                       position.x + size.x, position.y + size.y, 0, 0);
}

void sprite_draw(const Sprite *element, Vector2 position, Vector2 scale, float rotation)
{
	sprite_t *s = element->asset;
	int cols = element->cols ? element->cols : 1;
	int rows = element->rows ? element->rows : 1;
	int w    = s->width  / cols;
	int h    = s->height / rows;

	/* The cell of the frame: column across, row down. */
	int col = element->frame % cols;
	int row = element->frame / cols;

	rdpq_blitparms_t parms = {
		.s0      = col * w,
		.t0      = row * h,
		.width   = w,
		.height  = h,
		.flip_x  = element->flip_x,
		.cx      = (rotation != 0.0f) ? w / 2 : 0,
		.cy      = (rotation != 0.0f) ? h / 2 : 0,
		.scale_x = scale.x,
		.scale_y = scale.y,
		.theta   = rotation,
	};
	rdpq_sprite_blit(s, position.x, position.y, &parms);
}

}
