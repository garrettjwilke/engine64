/*
	The 2D stage: loads the .stage2d the importer wrote, opens the tiles the
	map uses and builds the element array once. From then on it is data.

	File layout, big-endian, every block padded to 8 bytes (the importer in
	tools/stage_importer is the authority):

	  0   "STG2"
	  4   version (2)
	  5   layer count
	  6   width, 8 height, 10 stride, 12 cell width, 14 cell height  (u16)
	  16  tile count (u16)
	  18  digits of the tile number, 19 length of the base path
	  24  base path, NUL terminated: "rom:/stages/<pack>/<prefix>"
	      solid bits: 32 bytes, bit n set means tile number n is solid
	      layer table: parallax (f32), grid offset from file start (u32)
	      grids: stride * height bytes each
*/
#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libdragon.h>

#include "stage2d/e64_stage2d.h"
#include "resource/e64_resource.h"
#include "viewport/e64_viewport.h"


static uint16_t stage2d_readU16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t stage2d_readU32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static float    stage2d_readF32(const uint8_t *p)
{
	uint32_t bits = stage2d_readU32(p);
	float v;
	memcpy(&v, &bits, sizeof v);
	return v;
}

static size_t stage2d_padded8(size_t n) { return (n + 7) & ~(size_t)7; }


Stage2D *stage2d_create(const Stage2DDef *def, Entity2D *entity)
{
	assert(def && def->path && entity);

	Stage2D *stage = (Stage2D *)calloc(1, sizeof(Stage2D));
	assert(stage);
	stage->entity = entity;

	/* --- the file ------------------------------------------------------- */
	int size;
	uint8_t *file = (uint8_t *)asset_load(def->path, &size);
	assert(file && size >= 24);
	assert(memcmp(file, "STG2", 4) == 0 && file[4] == 2);
	stage->file = file;

	stage->layer_count = file[5];
	stage->width       = stage2d_readU16(file + 6);
	stage->height      = stage2d_readU16(file + 8);
	stage->stride      = stage2d_readU16(file + 10);
	stage->cell_width  = stage2d_readU16(file + 12);
	stage->cell_height = stage2d_readU16(file + 14);
	uint16_t tile_count = stage2d_readU16(file + 16);
	uint8_t  digits     = file[18];
	uint8_t  base_len   = file[19];
	assert(stage->layer_count && stage->layer_count <= STAGE2D_MAX_LAYER);
	assert(tile_count && tile_count <= STAGE2D_MAX_TILE);

	const char    *base  = (const char *)file + 24;
	stage->solid         = file + 24 + stage2d_padded8(base_len);
	const uint8_t *table = stage->solid + 32;

	for (int i = 0; i < stage->layer_count; i++) {
		stage->layer[i].parallax = stage2d_readF32(table + i * 8);
		stage->layer[i].cell     = file + stage2d_readU32(table + i * 8 + 4);
	}

	/* --- the tiles ------------------------------------------------------ */
	/* Which tiles the map draws, and how many cells draw one: those tiles
	   get a graphic, those cells get an element. */
	uint32_t cells = 0;
	for (int i = 0; i < stage->layer_count; i++)
		for (int y = 0; y < stage->height; y++)
			for (int x = 0; x < stage->width; x++) {
				uint8_t tile = stage->layer[i].cell[y * stage->stride + x];
				if (!tile) continue;
				if (!stage->slot[tile]) stage->slot[tile] = ++stage->graphic_count;
				cells++;
			}
	assert(cells <= UINT16_MAX);

	/* One path string per open tile: base, number, extension. */
	size_t path_len = base_len + digits + sizeof ".sprite";
	stage->path    = (char *)malloc(stage->graphic_count * path_len);
	stage->graphic = (Graphic *)calloc(stage->graphic_count, sizeof(Graphic));
	assert(stage->path && stage->graphic);

	for (int tile = 1; tile <= STAGE2D_MAX_TILE; tile++) {
		if (!stage->slot[tile]) continue;
		assert(tile <= tile_count);

		char    *path    = stage->path + (stage->slot[tile] - 1) * path_len;
		Graphic *graphic = &stage->graphic[stage->slot[tile] - 1];

		snprintf(path, path_len, "%s%0*d.sprite", base, digits, tile - 1);

		*graphic = (Graphic){ .type = GRAPHIC_SPRITE, .sprite = { .path = path } };
		graphic->sprite.asset = (sprite_t *)resource_load(path, RESOURCE_SPRITE, NULL);
		assert(graphic->sprite.asset);
	}

	/* --- the elements --------------------------------------------------- */
	stage->element = (Element2D *)malloc(cells * sizeof(Element2D));
	assert(stage->element);

	/* Grouped by graphic inside each layer, not in cell order: the draw
	   uploads a tile's texture once and then paints every cell that uses it,
	   which is the whole point of keeping one graphic per tile. Cell order
	   would interleave the tiles and pay an upload per cell.

	   Layers stay apart because each carries its own parallax, so their
	   cells do not line up on the screen. */
	for (int i = 0; i < stage->layer_count; i++) {
		Stage2DLayer *layer = &stage->layer[i];
		layer->element_start = stage->element_count;

		/* Where each graphic's run starts, as cells are counted into it:
		   one pass to count, one to place. */
		uint16_t run[STAGE2D_MAX_TILE + 1] = { 0 };

		for (int y = 0; y < stage->height; y++) {
			const uint8_t *row = layer->cell + y * stage->stride;
			for (int x = 0; x < stage->width; x++)
				if (row[x]) run[stage->slot[row[x]]]++;
		}

		uint16_t next = stage->element_count;
		for (int g = 1; g <= stage->graphic_count; g++) {
			uint16_t count = run[g];
			run[g] = next;
			next  += count;
		}

		for (int y = 0; y < stage->height; y++) {
			const uint8_t *row = layer->cell + y * stage->stride;

			for (int x = 0; x < stage->width; x++) {
				uint8_t tile = row[x];
				if (!tile) continue;

				const Graphic  *graphic = &stage->graphic[stage->slot[tile] - 1];
				const sprite_t *sprite  = graphic->sprite.asset;

				/* Hung from the cell's bottom left corner, as Tiled draws a
				   tile larger than the grid. */
				stage->element[run[stage->slot[tile]]++] = (Element2D){
					.graphic  = graphic,
					.position = {
						(float)(x * stage->cell_width),
						(float)((y + 1) * stage->cell_height - sprite->height),
					},
					.scale    = { 1.0f, 1.0f },
					.rotation = 0.0f,
				};
			}
		}

		stage->element_count = next;
		layer->element_count = stage->element_count - layer->element_start;
	}

	return stage;
}

void stage2d_delete(Stage2D *stage)
{
	if (!stage) return;

	for (int i = 0; i < stage->graphic_count; i++)
		resource_unload(stage->graphic[i].sprite.asset);

	free(stage->element);
	free(stage->graphic);
	free(stage->path);
	free(stage->file);
	free(stage);
}

void stage2d_setRenderContext(const Stage2D *stage, const Camera2D *camera, RenderContext *ctx)
{
	Vector2 origin = stage->entity->position;

	/* The margin is two cells: a tile hangs from its cell's bottom left
	   corner, so one whose corner is just past the edge still shows. */
	float margin = (stage->cell_width > stage->cell_height ? stage->cell_width : stage->cell_height) * 2.0f;

	/* Asked once per frame rather than per tile, but never kept: the display
	   mode changes while the game runs, and with it what a world pixel
	   measures on the screen. */
	float   screen_width  = display_get_width();
	float   screen_height = display_get_height();
	Vector2 scale         = viewport_getScale();

	for (int l = 0; l < stage->layer_count; l++) {
		const Stage2DLayer *layer = &stage->layer[l];

		for (int i = 0; i < layer->element_count; i++) {
			const Element2D *element = &stage->element[layer->element_start + i];

			Vector2 world  = { origin.x + element->position.x, origin.y + element->position.y };
			Vector2 screen = camera2d_toScreen(camera, world, layer->parallax);

			if (screen.x < -margin || screen.x > screen_width ||
			    screen.y < -margin || screen.y > screen_height) continue;

			/* The zoom scales what is drawn as well as where: a tile is
			   cell_width world pixels wide, whatever that comes to on the
			   screen. */
			assert(ctx->element_count < RENDER_MAX_2D_ELEMENTS);
			/* Whole screen pixels: a tile blitted at a fraction lands on
			   the wrong texels and the seams show. */
			ctx->element[ctx->element_count]          = *element;
			ctx->element[ctx->element_count].position = (Vector2){ floorf(screen.x), floorf(screen.y) };
			ctx->element[ctx->element_count].scale    = (Vector2){ camera->zoom * scale.x, camera->zoom * scale.y };
			ctx->element_count++;
		}
	}
}

void stage2d_getCell(const Stage2D *stage, Vector2 position, int32_t *x, int32_t *y)
{
	Vector2 origin = stage->entity->position;

	*x = (int32_t)floorf((position.x - origin.x) / stage->cell_width);
	*y = (int32_t)floorf((position.y - origin.y) / stage->cell_height);
}

uint8_t stage2d_getTile(const Stage2D *stage, uint8_t layer, int32_t x, int32_t y)
{
	if (layer >= stage->layer_count) return 0;
	if (x < 0 || y < 0 || x >= stage->width || y >= stage->height) return 0;

	return stage->layer[layer].cell[y * stage->stride + x];
}

bool stage2d_isSolid(const Stage2D *stage, int32_t x, int32_t y)
{
	if (x < 0 || y < 0 || x >= stage->width || y >= stage->height) return false;

	for (int l = 0; l < stage->layer_count; l++) {
		uint8_t tile = stage->layer[l].cell[y * stage->stride + x];
		if (tile && (stage->solid[tile >> 3] >> (tile & 7)) & 1) return true;
	}
	return false;
}
