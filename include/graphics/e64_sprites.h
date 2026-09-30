#ifndef ENGINE64_SPRITES_H
#define ENGINE64_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

#include "math/e64_vector2.h"
#include "graphics/e64_sprite_animation.h"

namespace e64 {

/* A sprite is its file, or its clips. A definition names the path or the
   clips; the entity loads them, writes the asset and frees them on the way
   out. The draw reads only the asset. */
class Sprite {
public:

	const char *path;
	struct sprite_s *asset;

	/* An animated sprite names clips instead of a path: the entity opens
	   their frames and the asset is always the frame playing. Whoever picks
	   the clip and advances it writes the asset back. */
	const sprite::Animation::Def *animation_def;
	sprite::Animation *animation; /* NULL: a still image */

	/* The sheet is a grid of equal cells, cols wide and rows tall; frame
	   counts them row by row, left to right, from 0. 0 or 1 in both means
	   the whole image is the one frame. */
	uint8_t cols;
	uint8_t rows;
	uint8_t frame;

	bool flip_x; /* mirrored horizontally at draw time */
	bool tiled; /* repeated to fill a size instead of scaled */

	/* CI4 only: the colours come from this palette of the shared table, 0
	   to 15, instead of the file's own. */
	bool shared_palette;
	uint8_t palette;
};


namespace sprite {

/* The texture the RDP has on: which image, which rectangle of it, under
   which palette number and whether it repeats, and what the TLUT holds. A
   draw that would put the same back skips the upload. Zeroed it knows of
   nothing. */
typedef struct Texture {

	const struct sprite_s *asset;
	int s0, t0, s1, t1;
	int palette;
	bool tiled;

	/* The asset whose own palette sits at colour 0, or the shared table
	   over all 256. */
	const struct sprite_s *file_palette;
	bool shared_palettes;

} Texture;

/* The 16 palettes of 16 colours the sprites with shared_palette draw with,
   256 RGBA16 colours in all. The table is the game's and may change any
   time; the frame draws with the copy copySharedPalettes took. NULL: none. */
void setSharedPalettes(const uint16_t *table);

/* Copies the shared table into the buffer of this framebuffer, so the RDP
   still reading the previous frame's copy is not written under. Once per
   frame, before the sprites. */
void copySharedPalettes(int fb_index);

/* The RDP state these draw under is set by the render, once per run of
   sprites sharing it, not by the draw itself. */
void draw(const Sprite *element, Texture *texture, Vector2 position, Vector2 scale, float rotation);
void drawTiled(const Sprite *element, Texture *texture, Vector2 position, Vector2 size);

/* Whether the whole sheet fits in TMEM at once. When it does not, the cell
   is loaded and drawn in strips. */
bool isLoadable(const Sprite *element);

/* Something else wrote TMEM: the texels, or the texels and the TLUT (a
   font loads its own palettes). */
void forgetTexture(Texture *texture);
void forgetPalettes(Texture *texture);

}

}

#endif
