#ifndef ENGINE64_SPRITES_H
#define ENGINE64_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

#include "physics/math/e64_vector2.h"


/* A sprite is its file. A definition names the path; whoever loads the
   scene (a UI element, a prop) or the character (its frames) writes the
   asset and frees it on the way out. The draw reads only the asset. */
typedef struct {

	const char      *path;
	struct sprite_s *asset;

	/* The sheet is a grid of equal cells, cols wide and rows tall; frame
	   counts them row by row, left to right, from 0. 0 or 1 in both means
	   the whole image is the one frame. */
	uint8_t  cols;
	uint8_t  rows;
	uint8_t  frame;

	bool     flip_x;        /* mirrored horizontally at draw time */
	bool     tiled;         /* repeated to fill a size instead of scaled */

} Sprite;


/* The RDP state these draw under is set by the render, once per run of
   sprites sharing it, not by the draw itself. */
void sprite_draw(const Sprite *element, Vector2 position, Vector2 scale, float rotation);
void sprite_drawTiled(const Sprite *element, Vector2 position, Vector2 size);

/* Whether the same texture can serve several draws: the upload is the
   expensive half, so a run of elements sharing one sprite loads it once and
   then only paints. False for anything the blit has to handle itself. */
bool sprite_isLoadable(const Sprite *element, float rotation);

/* The two halves of a draw, for a run that shares one texture. */
void sprite_loadTexture(const Sprite *element);
void sprite_drawLoaded(const Sprite *element, Vector2 position, Vector2 scale);

#endif
