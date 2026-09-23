#include <math.h>

#include "math/e64_vector2.h"

namespace e64 {
namespace vector2 {

/* Only the square-root operations live here; the rest of the arithmetic is
   inline in the header. */

float magnitude(const Vector2 *v)
{
	return sqrtf(squaredMagnitude(v));
}

void normalize(Vector2 *v)
{
	float mag = magnitude(v);
	if (mag <= 0.0f) return;
	float inv = 1.0f / mag;
	v->x *= inv;
	v->y *= inv;
}

Vector2 normalized(const Vector2 *v)
{
	Vector2 out = *v;
	normalize(&out);
	return out;
}

}
}
