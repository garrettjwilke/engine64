#ifndef ENGINE64_MATH_H
#define ENGINE64_MATH_H

#include <math.h>
#include <float.h>

#include "math/e64_vector3.h"

namespace e64 {

constexpr float PI = 3.141592f;
constexpr float PI_TIMES_2 = 6.283185f;

constexpr float TOLERANCE = 0.000001f;

/* One-liners are inline: the body is a single float op, a call is not. */

static inline float deg_to_rad(float angle)
{
	return PI / 180 * angle;
}

static inline float rad_to_deg(float rad)
{
	return 180 / PI * rad;
}

static inline float lerpf(float a, float b, float t)
{
	return a + t * (b - a);
}

/* Two compares, inline. */
static inline float clampf(float v, float lo, float hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

float angle_wrap(float angle);
float angle_wrap_relative(float angle, float reference);

/*
	Fast inverse square root, Kaze's variant of the Quake III Q_rsqrt.
	Approximates 1/sqrt(x) in ~6 cycles instead of the ~58 that 1.0f / sqrtf(x)
	costs on N64 (29 for the sqrt, 29 for the divide).

	Only worth it when all three hold:
	  1. You need 1/sqrt(x), not sqrt(x). For sqrt use the hardware sqrtf.
	  2. The caller is already in icache. A miss loading the 8 extra
	     instructions kills the gain.
	  3. ~3% error is acceptable. Never in physics, contact normals, raycasts
	     or anything that accumulates.
*/
float qi_sqrt(float x);

float ease_linear(float t);

float ease_quad_in(float t);
float ease_quad_out(float t);
float ease_quad_in_out(float t);

float ease_cubic_in(float t);
float ease_cubic_out(float t);
float ease_cubic_in_out(float t);

float ease_expo_in(float t);
float ease_expo_out(float t);
float ease_expo_in_out(float t);

/* A segment is its two ends: nothing here holds one, so they are passed
   apart the way every caller already has them. */
Vector3 segment_closestToPoint(const Vector3 *a, const Vector3 *b, const Vector3 *point);

void segment_closestToSegment(
	const Vector3 *a1, const Vector3 *b1,
	const Vector3 *a2, const Vector3 *b2,
	Vector3 *closest1, Vector3 *closest2);


}

#endif
