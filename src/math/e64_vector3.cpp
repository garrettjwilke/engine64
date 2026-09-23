#include <math.h>

#include "math/e64_vector3.h"

namespace e64 {
namespace vector3 {

/* Only the square-root operations live here; the rest of the arithmetic is
   inline in the header. */

float magnitude(const Vector3 *v)
{
	return sqrtf(squaredMagnitude(v));
}

void normalize(Vector3 *v)
{
	float mag = magnitude(v);
	if (mag <= 0.0f) return;
	float inv = 1.0f / mag;
	v->x *= inv;
	v->y *= inv;
	v->z *= inv;
}

Vector3 normalized(const Vector3 *v)
{
	Vector3 out = *v;
	normalize(&out);
	return out;
}

/* The axis furthest from a decides which cross product is stable: taking the
   one closest to it would give a near-zero vector and a basis of noise. */
void computeBasis(const Vector3 *a, Vector3 *b, Vector3 *c)
{
	if (fabsf(a->x) >= 0.57735027f) {
		*b = (Vector3){ a->y, -a->x, 0.0f };
	} else {
		*b = (Vector3){ 0.0f, a->z, -a->y };
	}
	*b = normalized(b);
	*c = cross(a, b);
}

}
}
