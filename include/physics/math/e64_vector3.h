#ifndef ENGINE64_VECTOR3_H
#define ENGINE64_VECTOR3_H

#include <math.h>

namespace e64 {

typedef struct Vector3 {
	float x;
	float y;
	float z;
} Vector3;


/* The arithmetic lives here as static inline on purpose: each body is a
   handful of float ops, smaller than the call sequence it replaces, and a
   Vector3 returned by value goes through memory on this ABI, so the call
   costs a store and a reload on top of the jump. The three that take a
   square root stay real functions in the .c: their body is a library call
   plus a branch, and copying that per call site only grows the code. */

static inline Vector3 vector3_create(float x, float y, float z)
{
	return (Vector3){x, y, z};
}

static inline Vector3 vector3_zero(void)
{
	return (Vector3){0.0f, 0.0f, 0.0f};
}

static inline void vector3_scale(Vector3 *v, float scalar)
{
	v->x *= scalar;
	v->y *= scalar;
	v->z *= scalar;
}

static inline void vector3_addScaledVector(Vector3 *v, const Vector3 *w, float scalar)
{
	v->x += w->x * scalar;
	v->y += w->y * scalar;
	v->z += w->z * scalar;
}

static inline void vector3_add(Vector3 *v, const Vector3 *w)
{
	v->x += w->x;
	v->y += w->y;
	v->z += w->z;
}

static inline void vector3_sub(Vector3 *v, const Vector3 *w)
{
	v->x -= w->x;
	v->y -= w->y;
	v->z -= w->z;
}

static inline void vector3_invert(Vector3 *v)
{
	v->x = -v->x;
	v->y = -v->y;
	v->z = -v->z;
}

static inline Vector3 vector3_sum(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){a->x + b->x, a->y + b->y, a->z + b->z};
}

static inline Vector3 vector3_difference(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){a->x - b->x, a->y - b->y, a->z - b->z};
}

static inline Vector3 vector3_scaled(const Vector3 *v, float scalar)
{
	return (Vector3){v->x * scalar, v->y * scalar, v->z * scalar};
}

static inline Vector3 vector3_inverted(const Vector3 *v)
{
	return (Vector3){-v->x, -v->y, -v->z};
}

static inline Vector3 vector3_abs(const Vector3 *v)
{
	return (Vector3){ fabsf(v->x), fabsf(v->y), fabsf(v->z) };
}

static inline Vector3 vector3_cross(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){
		a->y * b->z - a->z * b->y,
		a->z * b->x - a->x * b->z,
		a->x * b->y - a->y * b->x,
	};
}

static inline float vector3_dot(const Vector3 *a, const Vector3 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float vector3_squaredMagnitude(const Vector3 *v)
{
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

static inline Vector3 vector3_reflected(const Vector3 *v, const Vector3 *normal)
{
	float t = 2.0f * vector3_dot(v, normal);
	return (Vector3){
		v->x - t * normal->x,
		v->y - t * normal->y,
		v->z - t * normal->z,
	};
}


float   vector3_magnitude(const Vector3 *v);
void    vector3_normalize(Vector3 *v);
Vector3 vector3_normalized(const Vector3 *v);


}

#endif
