#include <libdragon.h>
#include "graphics/e64_sprites.h"

namespace e64 {
namespace sprite {

/* Half of TMEM: the other half takes the palette of a colour indexed
   texture, and a tile never comes close to either. */
#define SPRITE_LOADABLE_BYTES 2048

bool isLoadable(const Sprite *element, float rotation)
{
	if (rotation != 0.0f || element->flip_x || element->tiled) return false;
	if (element->cols > 1 || element->rows > 1) return false;

	sprite_t *s = element->asset;
	return TEX_FORMAT_PIX2BYTES(sprite_get_format(s), s->width * s->height) <= SPRITE_LOADABLE_BYTES;
}

void loadTexture(const Sprite *element)
{
	sprite_t *s = element->asset;
	rdpq_tlut_t tlut = rdpq_tlut_from_format(sprite_get_format(s));

	if (tlut == TLUT_NONE) {
		rdpq_sprite_upload(TILE0, s, NULL);
		return;
	}

	/* A palette sprite is loaded by hand. rdpq_sprite_upload loads the texels
	   and then the palette, and the palette's SET_TEXTURE_IMAGE reaches the
	   RDP while the texel load is still fetching its last rows: on hardware
	   those rows come out of the palette. The load is fenced before the image
	   changes, which is what SYNC_LOAD is for. */
	surface_t texels = sprite_get_pixels(s);
	rdpq_tex_upload(TILE0, &texels, NULL);
	rdpq_sync_load();

	rdpq_mode_tlut(tlut);
	rdpq_tex_upload_tlut(sprite_get_palette(s), 0, sprite_get_palette_used_colors(s));
}

void drawLoaded(const Sprite *element, Vector2 position, Vector2 scale)
{
	sprite_t *s = element->asset;

	rdpq_texture_rectangle_scaled(TILE0,
		position.x, position.y,
		position.x + s->width * scale.x, position.y + s->height * scale.y,
		0, 0, s->width, s->height);
}

void drawTiled(const Sprite *element, Vector2 position, Vector2 size)
{
	rdpq_texparms_t parms = {
		.s = { .repeats = REPEAT_INFINITE },
		.t = { .repeats = REPEAT_INFINITE },
	};
	rdpq_sprite_upload(TILE0, element->asset, &parms);
	rdpq_texture_rectangle(TILE0, position.x, position.y,
	                       position.x + size.x, position.y + size.y, 0, 0);
}

void draw(const Sprite *element, Vector2 position, Vector2 scale, float rotation)
{
	sprite_t *s = element->asset;
	int cols = element->cols ? element->cols : 1;
	int rows = element->rows ? element->rows : 1;
	int w = s->width / cols;
	int h = s->height / rows;

	/* The cell of the frame: column across, row down. */
	int col = element->frame % cols;
	int row = element->frame / cols;

	rdpq_blitparms_t parms = {
		.s0 = col * w,
		.t0 = row * h,
		.width = w,
		.height = h,
		.flip_x = element->flip_x,
		.cx = (rotation != 0.0f) ? w / 2 : 0,
		.cy = (rotation != 0.0f) ? h / 2 : 0,
		.scale_x = scale.x,
		.scale_y = scale.y,
		.theta = rotation,
	};
	rdpq_sprite_blit(s, position.x, position.y, &parms);
}

}
}
