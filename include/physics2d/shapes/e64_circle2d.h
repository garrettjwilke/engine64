/*
	Circle geometry (radius), the 2D counterpart of Sphere. Admin fields live
	in physics2d::Shape2D.
*/
#ifndef ENGINE64_CIRCLE2D_H
#define ENGINE64_CIRCLE2D_H

#include "math/e64_vector2.h"
#include "math/e64_transform2d.h"
#include "physics2d/geometry/e64_aabb2d.h"
#include "physics2d/geometry/e64_raycast2d.h"

namespace e64 {

class Circle2D {
public:

	float radius;
};


namespace circle2d {

int testPoint(const Circle2D *c, const Transform2D *world, const Vector2 *p);
int raycast(const Circle2D *c, const Transform2D *world, RaycastData2D *raycast);
void computeAABB(const Circle2D *c, const Transform2D *world, AABB2D *aabb);

}

}

#endif
