/*
	Polymorphic shape in the plane, the 2D counterpart of physics::Shape.

	Each shape carries its local transform, the world transform its owner
	keeps current, and a tagged union with the concrete geometry (Circle2D /
	Rectangle2D / Capsule2D / Segment2D). There is no body: the system only
	detects and reports, and the narrowphase dispatches on `type`.
*/
#ifndef ENGINE64_PHYSICS_SHAPE2D_H
#define ENGINE64_PHYSICS_SHAPE2D_H

#include <stdint.h>

#include "math/e64_vector2.h"
#include "math/e64_transform2d.h"
#include "physics2d/geometry/e64_aabb2d.h"
#include "physics2d/geometry/e64_raycast2d.h"
#include "physics2d/shapes/e64_circle2d.h"
#include "physics2d/shapes/e64_rectangle2d.h"
#include "physics2d/shapes/e64_capsule2d.h"
#include "physics2d/shapes/e64_segment2d.h"

namespace e64 {

namespace physics2d {

class Shape2D {
public:

	enum Type {
		SHAPE_CIRCLE,
		SHAPE_RECTANGLE,
		SHAPE_CAPSULE,
		SHAPE_SEGMENT,
	};


	Type type;
	Transform2D local;
	/* The owner's transform composed with local, kept current by the owner. */
	Transform2D world;

	Shape2D *next;
	void *owner;
	int sensor;

	union {
		Circle2D circle;
		Rectangle2D rectangle;
		Capsule2D capsule;
		Segment2D segment;
	};
};


namespace shape2d {

/* Narrowphase dispatch — switches on shape->type. All three read the
   cached shape->world. */
int testPoint (const Shape2D *shape, const Vector2 *p);
int raycast (const Shape2D *shape, RaycastData2D *raycast);
void computeAABB(const Shape2D *shape, AABB2D *aabb);

}

}

}

#endif
