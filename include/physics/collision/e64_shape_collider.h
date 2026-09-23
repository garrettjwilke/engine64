/*
	A body's collision as authored: the shapes it carries, each with its own
	offset in its .tx. The entity transform and scale apply to all of them, so
	the group stays consistent at any prop size.
*/
#ifndef ENGINE64_SHAPE_COLLIDER_H
#define ENGINE64_SHAPE_COLLIDER_H

#include <stdint.h>

#include "physics/shapes/e64_physics_shape.h"

namespace e64 {

namespace shapeCollider {

struct Def {

	const physics::Shape::Def *shape;
	uint8_t count;

};

}

}

#endif
