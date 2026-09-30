/*
	Tile grid collision: the cells a query reaches are found by division, not
	by a tree, and each one with collision is handed out as its shape.
*/
#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "physics2d/collision/e64_grid_collider2d.h"

namespace e64 {
namespace gridCollider2d {

GridCollider2D *create(uint16_t width, uint16_t height, uint16_t cell_width, uint16_t cell_height)
{
	assert(width && height && cell_width && cell_height);

	GridCollider2D *grid = (GridCollider2D *)malloc(sizeof(GridCollider2D));
	assert(grid);

	grid->width = width;
	grid->height = height;
	grid->cell_width = cell_width;
	grid->cell_height = cell_height;

	grid->cell = (uint8_t *)calloc(width * height, 1);
	assert(grid->cell);

	return grid;
}

void destroy(GridCollider2D *grid)
{
	if (!grid) return;

	free(grid->cell);
	free(grid);
}


void setCell(GridCollider2D *grid, int32_t x, int32_t y, uint8_t collision)
{
	assert(x >= 0 && y >= 0 && x < grid->width && y < grid->height);

	grid->cell[y * grid->width + x] = collision;
}

uint8_t getCell(const GridCollider2D *grid, int32_t x, int32_t y)
{
	if (x < 0 || y < 0 || x >= grid->width || y >= grid->height) return GridCollider2D::GRID_CELL_OPEN;

	return grid->cell[y * grid->width + x];
}

bool hasCollision(const GridCollider2D *grid, int32_t x, int32_t y)
{
	return getCell(grid, x, y) != GridCollider2D::GRID_CELL_OPEN;
}


void cellAt(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p, int32_t *x, int32_t *y)
{
	*x = (int32_t)floorf((p->x - world->position.x) / grid->cell_width);
	*y = (int32_t)floorf((p->y - world->position.y) / grid->cell_height);
}

/* The whole cell, the only shape so far: a rectangle centred on it. */
void getShape(const GridCollider2D *grid, const Transform2D *world, int32_t x, int32_t y, physics2d::Shape2D *shape)
{
	float half_w = 0.5f * grid->cell_width;
	float half_h = 0.5f * grid->cell_height;

	shape->type = physics2d::Shape2D::SHAPE_RECTANGLE;
	shape->local = (Transform2D){ { (x + 0.5f) * grid->cell_width, (y + 0.5f) * grid->cell_height }, 0.0f };
	shape->world = (Transform2D){ { world->position.x + shape->local.position.x, world->position.y + shape->local.position.y }, 0.0f };
	shape->next = NULL;
	shape->owner = (void *)grid;
	shape->sensor = 0;
	shape->rectangle = (Rectangle2D){ { half_w, half_h } };
}


void queryAABB(const GridCollider2D *grid, const Transform2D *world, const AABB2D *box,
               void *cb, GridCollider2D::QueryCallback callback)
{
	int32_t x0, y0, x1, y1;
	cellAt(grid, world, &box->min, &x0, &y0);
	cellAt(grid, world, &box->max, &x1, &y1);

	/* Outside the grid is open: only the cells inside are walked. */
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 >= grid->width) x1 = grid->width - 1;
	if (y1 >= grid->height) y1 = grid->height - 1;

	for (int32_t y = y0; y <= y1; y++) {
		for (int32_t x = x0; x <= x1; x++) {
			if (!hasCollision(grid, x, y)) continue;

			physics2d::Shape2D shape;
			getShape(grid, world, x, y, &shape);
			if (!callback(cb, &shape, x, y)) return;
		}
	}
}


int testPoint(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p)
{
	int32_t x, y;
	cellAt(grid, world, p, &x, &y);

	return hasCollision(grid, x, y);
}


float distanceDown(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p, int32_t max_cells)
{
	int32_t col, row;
	cellAt(grid, world, p, &col, &row);

	for (int32_t y = row; y <= row + max_cells; y++) {
		if (!hasCollision(grid, col, y)) continue;
		float top = world->position.y + y * grid->cell_height;
		if (top < p->y) continue; /* the cell p is already in */
		return top - p->y;
	}
	return -1.0f;
}

}
}
