/*
	Capsule geometry (local Y axis): rectangle + half circles, the 2D
	counterpart of Capsule. Admin fields live in physics2d::Shape2D.
*/
#ifndef ENGINE64_CAPSULE2D_H
#define ENGINE64_CAPSULE2D_H

#include "math/e64_vector2.h"
#include "math/e64_transform2d.h"
#include "physics2d/geometry/e64_aabb2d.h"
#include "physics2d/geometry/e64_raycast2d.h"

namespace e64 {

class Capsule2D {
public:

	float radius;
	float half_height; /* half-height along local Y, excluding caps */
};


namespace capsule2d {

/* Endpoints of the inner segment (center ± half_height along Y), in world space. */
void getSegment(const Capsule2D *c, const Transform2D *world, Vector2 *a, Vector2 *b);

int testPoint(const Capsule2D *c, const Transform2D *world, const Vector2 *p);
int raycast(const Capsule2D *c, const Transform2D *world, RaycastData2D *raycast);
void computeAABB(const Capsule2D *c, const Transform2D *world, AABB2D *aabb);

}

}

#endif
