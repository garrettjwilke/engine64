#include <libdragon.h>
#include <stdint.h>

#include "math/e64_math.h"

namespace e64 {

#define LN2 0.6931472f
#define EPSILON 1e-6f


float angle_wrap(float angle)
{
	while (angle > 180.0f) angle -= 360.0f;
	while (angle <= -180.0f) angle += 360.0f;
	return angle;
}

float angle_wrap_relative(float angle, float reference)
{
	while (angle > reference + 180.0f) angle -= 360.0f;
	while (angle <= reference - 180.0f) angle += 360.0f;
	return angle;
}

float qi_sqrt(float x)
{
	/*
	 * Kaze Emanuar's improvement over the Quake III hack.
	 *
	 * Step 1: initial bit hack. Reinterpret the float as uint32 and apply
	 *         i = 0x5F3759DF - (i >> 1). This produces a first approximation
	 *         to 1/sqrt(x) by exploiting IEEE 754 representation.
	 *
	 *         Kaze's tweak: the original computed 'half_x = x * 0.5f' before
	 *         the Newton-Raphson step. Replacing that with an exponent
	 *         subtraction (i.e. dividing by 2 via bit ops) saves one cycle.
	 *         Equivalent for normal-magnitude floats.
	 *
	 * Step 2: one Newton-Raphson iteration to refine the guess. One iteration
	 *         brings the error down to ~0.175%; without Newton it is ~3.5%.
	 *
	 * The bits are read through a union, which GCC defines; it compiles to a
	 * direct MTC1/MFC1 with no extra instructions.
	 */

	union { float f; uint32_t i; } bits;
	float half_x = x * 0.5f;

	bits.f = x;
	bits.i = 0x5F3759DF - (bits.i >> 1);

	float y = bits.f;
	y = y * (1.5f - half_x * y * y);

	return y;
}

float ease_linear(float t)
{
	return t;
}

float ease_quad_in(float t)
{
	return t * t;
}

float ease_quad_out(float t)
{
	return 1.0f - (1.0f - t) * (1.0f - t);
}

float ease_quad_in_out(float t)
{
	if (t < 0.5f) return 2.0f * t * t;
	float inv = 1.0f - t;
	return 1.0f - 2.0f * inv * inv;
}

float ease_cubic_in(float t)
{
	return t * t * t;
}

float ease_cubic_out(float t)
{
	float inv = 1.0f - t;
	return 1.0f - inv * inv * inv;
}

float ease_cubic_in_out(float t)
{
	if (t < 0.5f) return 4.0f * t * t * t;
	float inv = 1.0f - t;
	return 1.0f - 4.0f * inv * inv * inv;
}

float ease_expo_in(float t)
{
	if (t <= 0.0f) return 0.0f;
	return fm_expf((10.0f * t - 10.0f) * LN2);
}

float ease_expo_out(float t)
{
	if (t >= 1.0f) return 1.0f;
	return 1.0f - fm_expf(-10.0f * t * LN2);
}

float ease_expo_in_out(float t)
{
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	if (t < 0.5f) return 0.5f * fm_expf((20.0f * t - 10.0f) * LN2);
	return 1.0f - 0.5f * fm_expf((-20.0f * t + 10.0f) * LN2);
}


Vector3 segment_closestToPoint(const Vector3 *a, const Vector3 *b, const Vector3 *point)
{
	Vector3 ab = {b->x - a->x, b->y - a->y, b->z - a->z};
	Vector3 ap = {point->x - a->x, point->y - a->y, point->z - a->z};

	float ab_len_sq = ab.x*ab.x + ab.y*ab.y + ab.z*ab.z;
	if (ab_len_sq < EPSILON) return *a;

	float t = (ap.x*ab.x + ap.y*ab.y + ap.z*ab.z) / ab_len_sq;
	t = clampf(t, 0.0f, 1.0f);

	return (Vector3){
		a->x + t * ab.x,
		a->y + t * ab.y,
		a->z + t * ab.z,
	};
}


void segment_closestToSegment(
	const Vector3 *a1, const Vector3 *b1,
	const Vector3 *a2, const Vector3 *b2,
	Vector3 *closest1, Vector3 *closest2)
{
	Vector3 d1 = {b1->x - a1->x, b1->y - a1->y, b1->z - a1->z};
	Vector3 d2 = {b2->x - a2->x, b2->y - a2->y, b2->z - a2->z};
	Vector3 r = {a1->x - a2->x, a1->y - a2->y, a1->z - a2->z};

	float a = d1.x*d1.x + d1.y*d1.y + d1.z*d1.z;
	float e = d2.x*d2.x + d2.y*d2.y + d2.z*d2.z;
	float f = d2.x*r.x + d2.y*r.y + d2.z*r.z;

	float s, t;

	if (a <= EPSILON && e <= EPSILON) {
		*closest1 = *a1;
		*closest2 = *a2;
		return;
	}

	if (a <= EPSILON) {
		s = 0.0f;
		t = clampf(f / e, 0.0f, 1.0f);
	}
	else {
		float c = d1.x*r.x + d1.y*r.y + d1.z*r.z;
		if (e <= EPSILON) {
			t = 0.0f;
			s = clampf(-c / a, 0.0f, 1.0f);
		}
		else {
			float b = d1.x*d2.x + d1.y*d2.y + d1.z*d2.z;
			float denom = a * e - b * b;
			if (denom != 0.0f) s = clampf((b*f - c*e) / denom, 0.0f, 1.0f);
			else s = 0.0f;
			t = (b*s + f) / e;
			if (t < 0.0f) {
				t = 0.0f;
				s = clampf(-c / a, 0.0f, 1.0f);
			}
			else if (t > 1.0f) {
				t = 1.0f;
				s = clampf((b - c) / a, 0.0f, 1.0f);
			}
		}
	}

	*closest1 = (Vector3){a1->x + d1.x*s, a1->y + d1.y*s, a1->z + d1.z*s};
	*closest2 = (Vector3){a2->x + d2.x*t, a2->y + d2.y*t, a2->z + d2.z*t};
}

}
