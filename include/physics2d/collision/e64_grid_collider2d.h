/*
	Static tile grid collision data, the 2D counterpart of MeshCollider: the
	shape of a stage. Detection only, like the rest of physics2d.

	One byte per cell: the collision of its tile, 0 open. The grid does not
	read it past that; the shape of every cell with collision is the whole
	cell as a rectangle.

	The grid is axis-aligned. Its world transform places cell (0,0): the
	position is the top left corner of that cell, the rotation is ignored.
	Outside the grid is open.

	queryAABB is the grid's meshCollider::queryAABB: it hands every cell
	with collision a box reaches to a callback, as a Shape2D in world space, and the
	caller runs the collision2d pair it needs and reacts to that cell.
*/
#ifndef ENGINE64_GRID_COLLIDER2D_H
#define ENGINE64_GRID_COLLIDER2D_H

#include <stdint.h>

#include "math/e64_vector2.h"
#include "math/e64_transform2d.h"
#include "physics2d/geometry/e64_aabb2d.h"
#include "physics2d/shapes/e64_physics_shape2d.h"

namespace e64 {

class GridCollider2D {
public:

	enum Cell : uint8_t {
		GRID_CELL_OPEN = 0,
		GRID_CELL_SOLID = 1,
	};

	/* Return 0 to stop the query. */
	typedef int (*QueryCallback)(void *cb, const physics2d::Shape2D *cell, int32_t x, int32_t y);


	uint16_t width; /* cells */
	uint16_t height;
	uint16_t cell_width; /* pixels */
	uint16_t cell_height;

	uint8_t *cell; /* width * height, row after row */
};


namespace gridCollider2d {

GridCollider2D *create(uint16_t width, uint16_t height, uint16_t cell_width, uint16_t cell_height);
void destroy(GridCollider2D *grid);

void setCell(GridCollider2D *grid, int32_t x, int32_t y, uint8_t collision);
uint8_t getCell(const GridCollider2D *grid, int32_t x, int32_t y);
bool hasCollision(const GridCollider2D *grid, int32_t x, int32_t y);

/* The cell a world point falls in, whether it is inside the grid or not. */
void cellAt(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p, int32_t *x, int32_t *y);

/* The shape of a cell with collision in world space. */
void getShape(const GridCollider2D *grid, const Transform2D *world, int32_t x, int32_t y, physics2d::Shape2D *shape);

/* Every cell with collision the world box reaches, in rows top to bottom. */
void queryAABB(const GridCollider2D *grid, const Transform2D *world, const AABB2D *box,
               void *cb, GridCollider2D::QueryCallback callback);

/* Whether the point falls in a cell with collision. */
int testPoint(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p);

/* How far down from p the top of the first cell with collision is, looking through
   the cell of p and max_cells more under it. A cell whose top is above p is
   the one p is already in, and is skipped. Negative when there is none. */
float distanceDown(const GridCollider2D *grid, const Transform2D *world, const Vector2 *p, int32_t max_cells);

}

}

#endif
