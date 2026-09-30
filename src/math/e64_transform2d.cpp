#include <fmath.h>

#include "math/e64_transform2d.h"

namespace e64 {
namespace transform2d {

static inline Vector2 rotate(float angle, const Vector2 *v)
{
	if (angle == 0.0f) return *v;

	float s, c;
	fm_sincosf(angle, &s, &c);
	return (Vector2){ c * v->x - s * v->y, s * v->x + c * v->y };
}


Transform2D inverse(const Transform2D *t)
{
	Vector2 neg = vector2::inverted(&t->position);
	return (Transform2D){ rotate(-t->rotation, &neg), -t->rotation };
}


Vector2 rotateVector(const Transform2D *t, const Vector2 *v)
{
	return rotate(t->rotation, v);
}


Vector2 rotateVectorTransposed(const Transform2D *t, const Vector2 *v)
{
	return rotate(-t->rotation, v);
}


Vector2 mulVector(const Transform2D *t, const Vector2 *v)
{
	Vector2 r = rotate(t->rotation, v);
	return vector2::sum(&r, &t->position);
}


Vector2 mulVectorTransposed(const Transform2D *t, const Vector2 *v)
{
	Vector2 d = vector2::difference(v, &t->position);
	return rotate(-t->rotation, &d);
}


Transform2D product(const Transform2D *t, const Transform2D *u)
{
	Vector2 rp = rotate(t->rotation, &u->position);
	return (Transform2D){ vector2::sum(&rp, &t->position), t->rotation + u->rotation };
}


Transform2D productTransposed(const Transform2D *t, const Transform2D *u)
{
	Vector2 d = vector2::difference(&u->position, &t->position);
	return (Transform2D){ rotate(-t->rotation, &d), u->rotation - t->rotation };
}

}
}
