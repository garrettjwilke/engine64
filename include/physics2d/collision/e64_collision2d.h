/*
	Narrowphase in the plane, the 2D counterpart of collision. Detection
	only: it fills a manifold and nothing else.

	collide() takes a manifold and two shapes and dispatches on
	(A->type, B->type) to the right pairwise function, swapping the pair when
	the table only has it the other way round. The manifold normal always
	points from A to B, and each contact point lies on the surface of B.

	The pairs take the geometry and its world transform directly, like the
	3D static pairs, so a stage tile can be tested without building a
	Shape2D.
*/
#ifndef ENGINE64_COLLISION2D_H
#define ENGINE64_COLLISION2D_H

#include "math/e64_transform2d.h"
#include "physics2d/shapes/e64_physics_shape2d.h"
#include "physics2d/collision/e64_contact2d.h"

namespace e64 {

namespace collision2d {

void collide(Contact2D::Manifold *m, const physics2d::Shape2D *a, const physics2d::Shape2D *b);

void circleToCircle (Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                     const Circle2D *b, const Transform2D *b_world);
void circleToCapsule (Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                      const Capsule2D *b, const Transform2D *b_world);
void circleToSegment (Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                      const Segment2D *b, const Transform2D *b_world);
void circleToRectangle (Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                        const Rectangle2D *b, const Transform2D *b_world);
void capsuleToCapsule (Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                       const Capsule2D *b, const Transform2D *b_world);
void capsuleToSegment (Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                       const Segment2D *b, const Transform2D *b_world);
void capsuleToRectangle (Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                         const Rectangle2D *b, const Transform2D *b_world);
void segmentToSegment (Contact2D::Manifold *m, const Segment2D *a, const Transform2D *a_world,
                       const Segment2D *b, const Transform2D *b_world);
void segmentToRectangle (Contact2D::Manifold *m, const Segment2D *a, const Transform2D *a_world,
                         const Rectangle2D *b, const Transform2D *b_world);
void rectangleToRectangle(Contact2D::Manifold *m, const Rectangle2D *a, const Transform2D *a_world,
                          const Rectangle2D *b, const Transform2D *b_world);

}

}

#endif
