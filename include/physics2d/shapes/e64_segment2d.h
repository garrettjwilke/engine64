/*
	Segment geometry (two endpoints in local space): the edge Tiled gives for
	a polyline, with no area. Admin fields live in physics2d::Shape2D.

	The closest-point helpers are the 2D counterparts of the segment_ ones in
	math/e64_math.h, and serve every shape built on a segment: the capsule's
	core, and the circle's as a segment of zero length.
*/
#ifndef ENGINE64_SEGMENT2D_H
#define ENGINE64_SEGMENT2D_H

#include "math/e64_vector2.h"
#include "math/e64_transform2d.h"
#include "physics2d/geometry/e64_aabb2d.h"
#include "physics2d/geometry/e64_raycast2d.h"

namespace e64 {

class Segment2D {
public:

	Vector2 a;
	Vector2 b;
};


namespace segment2d {

/* Endpoints in world space. */
void getPoints(const Segment2D *s, const Transform2D *world, Vector2 *a, Vector2 *b);

/* A segment has no area, so no point is inside it. */
int testPoint(const Segment2D *s, const Transform2D *world, const Vector2 *p);
int raycast(const Segment2D *s, const Transform2D *world, RaycastData2D *raycast);
void computeAABB(const Segment2D *s, const Transform2D *world, AABB2D *aabb);

Vector2 closestToPoint(const Vector2 *a, const Vector2 *b, const Vector2 *point);

void closestToSegment(
	const Vector2 *a1, const Vector2 *b1,
	const Vector2 *a2, const Vector2 *b2,
	Vector2 *closest1, Vector2 *closest2);

}

}

#endif
