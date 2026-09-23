/*
	The 2D stage: the tile map a level is built of, authored in Tiled and
	loaded from its .stage2d binary. Once loaded it is an array of Render::Element2D,
	one per cell that holds a tile, in world pixels: the scene copies it into
	the frame through the camera the way it copies its entities, and the
	render never knows a stage exists.

	A cell of the file is one byte: 0 is empty, n is tile n-1 of the pack. A
	tile is its own sprite file, reached from the pack's first tile plus the
	number, so the map never names a file. Each tile the map uses is opened
	once and every element drawing it shares that graphic.
*/
#ifndef ENGINE64_STAGE2D_H
#define ENGINE64_STAGE2D_H

#include <stdint.h>

#include "render/e64_render.h"
#include "entity/e64_entity2d.h"
#include "camera/e64_camera2d.h"

namespace e64 {

namespace stage2d {

constexpr int MAX_LAYER = 16;
/* A cell is one byte, 0 empty: as many tiles as a byte can name. */
constexpr int MAX_TILE = 255;


/* What the prefab declares: the file. Where the stage stands comes from
   the placement, like every other prefab. */
typedef struct Def {

	const char *path;

} Def;


/* One grid of the map, in draw order, back to front: what it takes of the
   camera's scroll, its cells, and which run of the element array is its. */
typedef struct Layer {

	float parallax;
	const uint8_t *cell;

	uint16_t element_start;
	uint16_t element_count;

} Layer;

}


typedef struct Stage2D {

	/* The scene entity placed for it: its position is the world pixel of
	   the top left corner of cell (0,0). */
	Entity2D *entity;

	/* The stage as drawn: one element per cell with a tile, positions in
	   world pixels relative to the entity, layer after layer. */
	Render::Element2D *element;
	uint16_t element_count;

	uint16_t width; /* cells */
	uint16_t height;
	uint16_t stride; /* bytes per row in the grids */
	uint16_t cell_width; /* pixels */
	uint16_t cell_height;

	uint8_t layer_count;
	stage2d::Layer layer[stage2d::MAX_LAYER];

	/* One bit per tile number as the cells count them: set, the tile is
	   one the body stands on and walks into, from Tiled's "solid" tile
	   property. */
	const uint8_t *solid;

	/* The graphics of the tiles the map uses, one each, and where each
	   tile's is: slot[n] is 1 + its index in graphic, 0 for a tile no cell
	   draws. */
	Graphic *graphic;
	uint8_t graphic_count;
	uint8_t slot[stage2d::MAX_TILE + 1];

	/* The tile paths, one string per open graphic, kept for as long as the
	   sprites are open: the resource table holds the pointer, not a copy. */
	char *path;

	/* The .stage2d as read from the ROM; the grids point inside it. */
	void *file;

} Stage2D;


namespace stage2d {

Stage2D *create(const Def *def, Entity2D *entity);
void destroy(Stage2D *stage);

/* Copies the stage's elements into the frame, each carried to the screen
   through the camera with its layer's parallax, the same as the scene does
   with an entity. What lands off the screen is left out. */
void setRenderContext(const Stage2D *stage, const Camera2D *camera, Render::Context *ctx);

/* The cell a world position falls in. Outside the map gives coordinates
   outside 0..width-1 / 0..height-1, which getTile answers as empty. */
void getCell(const Stage2D *stage, Vector2 position, int32_t *x, int32_t *y);

/* What a layer holds at a cell: 0 empty, n the tile n-1. */
uint8_t getTile(const Stage2D *stage, uint8_t layer, int32_t x, int32_t y);

/* Whether any layer holds a solid tile at the cell. Outside the map is
   open. */
bool isSolid(const Stage2D *stage, int32_t x, int32_t y);

}

}

#endif
