#include <math.h>

#include "physics/math/e64_vector2.h"

namespace e64 {

/* Only the square-root operations live here; the rest of the arithmetic is
   inline in the header. */

float vector2_magnitude(const Vector2 *v)
{
	return sqrtf(vector2_squaredMagnitude(v));
}

void vector2_normalize(Vector2 *v)
{
	float mag = vector2_magnitude(v);
	if (mag <= 0.0f) return;
	float inv = 1.0f / mag;
	v->x *= inv;
	v->y *= inv;
}

Vector2 vector2_normalized(const Vector2 *v)
{
	Vector2 out = *v;
	vector2_normalize(&out);
	return out;
}

}
