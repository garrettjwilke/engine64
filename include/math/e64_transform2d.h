/*
	Physics transform in the plane: position + rotation angle, the 2D
	counterpart of Transform. Every operation takes the sine and cosine of the
	angle, and skips them when the angle is zero, which is the case of every
	axis-aligned tile.
*/
#ifndef ENGINE64_TRANSFORM2D_H
#define ENGINE64_TRANSFORM2D_H

#include "math/e64_vector2.h"

namespace e64 {

typedef struct Transform2D {
	Vector2 position;
	float rotation; /* radians; positive turns +x toward +y */
} Transform2D;


namespace transform2d {

static inline void init(Transform2D *t)
{
	t->position = vector2::zero();
	t->rotation = 0.0f;
}

Transform2D inverse(const Transform2D *t);

/* Rotation only, for directions and normals. */
Vector2 rotateVector(const Transform2D *t, const Vector2 *v);
Vector2 rotateVectorTransposed(const Transform2D *t, const Vector2 *v);

Vector2 mulVector(const Transform2D *t, const Vector2 *v);
Vector2 mulVectorTransposed(const Transform2D *t, const Vector2 *v);

Transform2D product(const Transform2D *t, const Transform2D *u);
Transform2D productTransposed(const Transform2D *t, const Transform2D *u);

}

}

#endif
