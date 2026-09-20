#include <math.h>

#include "physics/math/e64_vector3.h"

namespace e64 {

/* Only the square-root operations live here; the rest of the arithmetic is
   inline in the header. */

float vector3_magnitude(const Vector3 *v)
{
	return sqrtf(vector3_squaredMagnitude(v));
}

void vector3_normalize(Vector3 *v)
{
	float mag = vector3_magnitude(v);
	if (mag <= 0.0f) return;
	float inv = 1.0f / mag;
	v->x *= inv;
	v->y *= inv;
	v->z *= inv;
}

Vector3 vector3_normalized(const Vector3 *v)
{
	Vector3 out = *v;
	vector3_normalize(&out);
	return out;
}

}
