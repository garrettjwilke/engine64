/*
	Physics transform: position + rotation matrix. (from qu3e q3Transform).
	The render-side transform (pos+euler+scale) lives in render/e64_render.h
	as Render::Transform.
*/
#ifndef ENGINE64_TRANSFORM_H
#define ENGINE64_TRANSFORM_H

#include "math/e64_vector3.h"
#include "math/e64_matrix3.h"
#include "physics/geometry/e64_half_space.h"

namespace e64 {

typedef struct Transform {
	Vector3 position;
	Matrix3 rotation;
} Transform;


namespace transform {

static inline void init(Transform *t)
{
	t->position = vector3::zero();
	t->rotation = matrix3::identity();
}

Transform inverse(const Transform *t);

Vector3 mulVector(const Transform *t, const Vector3 *v);
Vector3 mulVectorScaled(const Transform *t, const Vector3 *scale, const Vector3 *v);

Transform product(const Transform *t, const Transform *u);

Vector3 mulVectorTransposed(const Transform *t, const Vector3 *v);
Transform productTransposed(const Transform *t, const Transform *u);

HalfSpace mulHalfSpace(const Transform *t, const HalfSpace *p);
HalfSpace mulHalfSpaceScaled(const Transform *t, const Vector3 *scale, const HalfSpace *p);
HalfSpace mulHalfSpaceTransposed(const Transform *t, const HalfSpace *p);

}

}

#endif
