/*
	The sprite draw, on one loader. What goes up is the cell of the frame, or
	the whole sheet when it fits, or the cell in strips when it does not; the
	draw is a rectangle, or the two triangles of libdragon's tex_xblit when
	the sprite is rotated. The Texture the render hands in says what the RDP
	already has on, and whatever matches it is not uploaded again.
*/
#include <assert.h>
#include <string.h>
#include <fmath.h>
#include <libdragon.h>

#include "viewport/e64_viewport.h"
#include "graphics/e64_sprites.h"

namespace e64 {
namespace sprite {

/* The game's table, and the frame's copy of it the RDP reads: one buffer
   per framebuffer, so the frame being written never overwrites the copy an
   earlier one is still drawing with. */
static const uint16_t *shared_table;
static uint16_t shared_buffer[Viewport::FB_COUNT][256] __attribute__((aligned(16)));
static const uint16_t *shared_frame;


void setSharedPalettes(const uint16_t *table)
{
	shared_table = table;
}

void copySharedPalettes(int fb_index)
{
	shared_frame = NULL;
	if (!shared_table) return;

	uint16_t *buffer = shared_buffer[fb_index];
	memcpy(buffer, shared_table, sizeof(shared_buffer[0]));
	data_cache_hit_writeback(buffer, sizeof(shared_buffer[0]));
	shared_frame = buffer;
}

void forgetTexture(Texture *texture)
{
	texture->asset = NULL;
}

void forgetPalettes(Texture *texture)
{
	texture->asset = NULL;
	texture->file_palette = NULL;
	texture->shared_palettes = false;
}

bool isLoadable(const Sprite *element)
{
	surface_t texels = sprite_get_pixels(element->asset);
	return rdpq_tex_can_upload(&texels);
}


/* --- palettes --------------------------------------------------------------- */

/* A palette load is fenced on both sides. The palette's SET_TEXTURE_IMAGE
   reaching the RDP while a texel load is still fetching its last rows makes
   those rows come out of the palette on hardware, and the same goes for the
   next texel load after it: SYNC_LOAD holds each image change until the
   load before it is done. */
static void uploadTlut(const uint16_t *colors, int count)
{
	rdpq_sync_load();
	rdpq_tex_upload_tlut((uint16_t *)colors, 0, count);
	rdpq_sync_load();
}

/* The shared table goes over all 256 colours and a file's palette over the
   start of it: each load leaves the other one gone. */
static void setPalette(const Sprite *element, Texture *texture)
{
	sprite_t *s = element->asset;

	if (element->shared_palette) {
		assert(shared_frame && sprite_get_format(s) == FMT_CI4);
		if (texture->shared_palettes) return;

		uploadTlut(shared_frame, 256);
		texture->shared_palettes = true;
		texture->file_palette = NULL;
		return;
	}

	if (rdpq_tlut_from_format(sprite_get_format(s)) == TLUT_NONE) return;
	if (texture->file_palette == s) return;

	uploadTlut(sprite_get_palette(s), sprite_get_palette_used_colors(s));
	texture->file_palette = s;
	texture->shared_palettes = false;
}


/* --- texels ----------------------------------------------------------------- */

/* The one loader: a rectangle of the sheet into TILE0, the tile addressed in
   the sheet's own coordinates, so the draw names texels the same whatever
   was loaded around them. */
static void upload(const Sprite *element, Texture *texture, bool tiled,
                   int s0, int t0, int s1, int t1)
{
	int palette = element->shared_palette ? element->palette : 0;

	if (texture->asset == element->asset && texture->tiled == tiled
	 && texture->palette == palette
	 && texture->s0 == s0 && texture->t0 == t0
	 && texture->s1 == s1 && texture->t1 == t1) return;

	rdpq_texparms_t parms = {};
	parms.palette = palette;
	if (tiled) {
		parms.s.repeats = REPEAT_INFINITE;
		parms.t.repeats = REPEAT_INFINITE;
	}

	surface_t texels = sprite_get_pixels(element->asset);
	rdpq_tex_upload_sub(TILE0, &texels, &parms, s0, t0, s1, t1);

	texture->asset = element->asset;
	texture->tiled = tiled;
	texture->palette = palette;
	texture->s0 = s0;
	texture->t0 = t0;
	texture->s1 = s1;
	texture->t1 = t1;
}

/* How many rows of a rectangle s0 to s1 wide fit in TMEM at once, counted
   the way rdpq_tex_upload_sub lays them: 4 bpp rows widened to whole bytes,
   32 bit ones split over both halves, every row padded to 8 bytes. A
   paletted format has only the half the palette leaves. */
static int getStripHeight(const sprite_t *s, int s0, int s1)
{
	tex_format_t fmt = sprite_get_format((sprite_t *)s);

	if (TEX_FORMAT_BITDEPTH(fmt) == 4) {
		s0 &= ~1;
		s1 = (s1 + 1) & ~1;
	}

	int pitch_shift = (fmt == FMT_RGBA32 || fmt == FMT_YUV16) ? 1 : 0;
	int pitch = ((TEX_FORMAT_PIX2BYTES(fmt, s1 - s0) >> pitch_shift) + 7) & ~7;
	int size = (fmt == FMT_RGBA32 || fmt == FMT_CI4 || fmt == FMT_CI8 || fmt == FMT_YUV16) ? 2048 : 4096;

	return size / pitch;
}


/* --- draw ------------------------------------------------------------------- */

/* Where the cell lands: texel (s, t) goes to (s * m[0] + t * m[1] + m[2]),
   tex_xblit's matrix. The rotation turns about the centre of the cell;
   without one the cell hangs from its top left corner. */
typedef struct Placement {

	float m[3][2];
	int os0, os1;
	bool flip_x;
	bool rotated;

} Placement;

static Placement place(const Sprite *element, Vector2 position, Vector2 scale, float rotation,
                       int os0, int ot0, int os1, int ot1)
{
	bool rotated = rotation != 0.0f;
	float cx = os0 + (rotated ? (os1 - os0) / 2 : 0);
	float cy = ot0 + (rotated ? (ot1 - ot0) / 2 : 0);

	float sin_theta = 0.0f, cos_theta = 1.0f;
	if (rotated) fm_sincosf(rotation, &sin_theta, &cos_theta);

	return (Placement){
		.m = {
			{ cos_theta * scale.x, -sin_theta * scale.x },
			{ sin_theta * scale.y, cos_theta * scale.y },
			{ position.x - (cx * cos_theta * scale.x + cy * sin_theta * scale.y),
			  position.y - (cx * -sin_theta * scale.x + cy * cos_theta * scale.y) },
		},
		.os0 = os0,
		.os1 = os1,
		.flip_x = element->flip_x,
		.rotated = rotated,
	};
}

/* One loaded piece of the cell, s0 to s1 by t0 to t1. Mirrored, the corners
   are placed from the far side of the cell while the texels keep their
   order. */
static void drawPiece(const Placement *p, int s0, int t0, int s1, int t1)
{
	int ks0 = s0, ks1 = s1;
	if (p->flip_x) {
		ks0 = p->os1 - s0 + p->os0;
		ks1 = p->os1 - s1 + p->os0;
	}

	float k0x = p->m[0][0] * ks0 + p->m[1][0] * t0 + p->m[2][0];
	float k0y = p->m[0][1] * ks0 + p->m[1][1] * t0 + p->m[2][1];
	float k2x = p->m[0][0] * ks1 + p->m[1][0] * t1 + p->m[2][0];
	float k2y = p->m[0][1] * ks1 + p->m[1][1] * t1 + p->m[2][1];

	/* Unrotated the corners stay a rectangle: a mirrored one comes with its
	   x0 past its x1, which the rectangle turns into texels read backwards. */
	if (!p->rotated) {
		rdpq_texture_rectangle_scaled(TILE0, k0x, k0y, k2x, k2y, s0, t0, s1, t1);
		return;
	}

	float k1x = p->m[0][0] * ks1 + p->m[1][0] * t0 + p->m[2][0];
	float k1y = p->m[0][1] * ks1 + p->m[1][1] * t0 + p->m[2][1];
	float k3x = p->m[0][0] * ks0 + p->m[1][0] * t1 + p->m[2][0];
	float k3y = p->m[0][1] * ks0 + p->m[1][1] * t1 + p->m[2][1];

	float v0[5] = { k0x, k0y, (float)s0, (float)t0, 1.0f };
	float v1[5] = { k1x, k1y, (float)s1, (float)t0, 1.0f };
	float v2[5] = { k2x, k2y, (float)s1, (float)t1, 1.0f };
	float v3[5] = { k3x, k3y, (float)s0, (float)t1, 1.0f };

	rdpq_trifmt_t trifmt = TRIFMT_TEX;
	trifmt.tex_tile = TILE0;
	rdpq_triangle(&trifmt, v0, v1, v2);
	rdpq_triangle(&trifmt, v0, v2, v3);
}

void draw(const Sprite *element, Texture *texture, Vector2 position, Vector2 scale, float rotation)
{
	sprite_t *s = element->asset;
	int cols = element->cols ? element->cols : 1;
	int rows = element->rows ? element->rows : 1;
	int w = s->width / cols;
	int h = s->height / rows;

	/* The cell of the frame: column across, row down. */
	int os0 = (element->frame % cols) * w;
	int ot0 = (element->frame / cols) * h;
	int os1 = os0 + w;
	int ot1 = ot0 + h;

	Placement p = place(element, position, scale, rotation, os0, ot0, os1, ot1);

	setPalette(element, texture);

	/* The whole sheet when it fits: every frame of it is then already on
	   for the next draw. */
	if (isLoadable(element)) {
		upload(element, texture, false, 0, 0, s->width, s->height);
		drawPiece(&p, os0, ot0, os1, ot1);
		return;
	}

	/* Strips of the cell as tall as TMEM takes, each loaded and drawn
	   before the next. No filtering, so no row is shared between two. */
	int strip = getStripHeight(s, os0, os1);
	for (int t0 = ot0; t0 < ot1; ) {
		int t1 = t0 + strip < ot1 ? t0 + strip : ot1;
		upload(element, texture, false, os0, t0, os1, t1);
		drawPiece(&p, os0, t0, os1, t1);
		t0 = t1;
	}
}

void drawTiled(const Sprite *element, Texture *texture, Vector2 position, Vector2 size)
{
	sprite_t *s = element->asset;

	/* The repeat wraps inside TMEM: the whole image has to be in it. */
	assert(isLoadable(element));

	setPalette(element, texture);
	upload(element, texture, true, 0, 0, s->width, s->height);
	rdpq_texture_rectangle(TILE0, position.x, position.y,
	                       position.x + size.x, position.y + size.y, 0, 0);
}

}
}
