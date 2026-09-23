#ifndef ENGINE64_VECTOR3_H
#define ENGINE64_VECTOR3_H

#include <math.h>

namespace e64 {

typedef struct Vector3 {
	float x;
	float y;
	float z;
} Vector3;


namespace vector3 {

/* The arithmetic lives here as static inline on purpose: each body is a
   handful of float ops, smaller than the call sequence it replaces, and a
   Vector3 returned by value goes through memory on this ABI, so the call
   costs a store and a reload on top of the jump. The three that take a
   square root stay real functions in the .cpp: their body is a library call
   plus a branch, and copying that per call site only grows the code. */

static inline Vector3 create(float x, float y, float z)
{
	return (Vector3){x, y, z};
}

static inline Vector3 zero(void)
{
	return (Vector3){0.0f, 0.0f, 0.0f};
}

static inline void scale(Vector3 *v, float scalar)
{
	v->x *= scalar;
	v->y *= scalar;
	v->z *= scalar;
}

static inline void addScaledVector(Vector3 *v, const Vector3 *w, float scalar)
{
	v->x += w->x * scalar;
	v->y += w->y * scalar;
	v->z += w->z * scalar;
}

static inline void add(Vector3 *v, const Vector3 *w)
{
	v->x += w->x;
	v->y += w->y;
	v->z += w->z;
}

static inline void sub(Vector3 *v, const Vector3 *w)
{
	v->x -= w->x;
	v->y -= w->y;
	v->z -= w->z;
}

static inline void invert(Vector3 *v)
{
	v->x = -v->x;
	v->y = -v->y;
	v->z = -v->z;
}

static inline Vector3 sum(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){a->x + b->x, a->y + b->y, a->z + b->z};
}

static inline Vector3 difference(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){a->x - b->x, a->y - b->y, a->z - b->z};
}

static inline Vector3 scaled(const Vector3 *v, float scalar)
{
	return (Vector3){v->x * scalar, v->y * scalar, v->z * scalar};
}

static inline Vector3 inverted(const Vector3 *v)
{
	return (Vector3){-v->x, -v->y, -v->z};
}

static inline Vector3 abs(const Vector3 *v)
{
	return (Vector3){ fabsf(v->x), fabsf(v->y), fabsf(v->z) };
}

static inline Vector3 cross(const Vector3 *a, const Vector3 *b)
{
	return (Vector3){
		a->y * b->z - a->z * b->y,
		a->z * b->x - a->x * b->z,
		a->x * b->y - a->y * b->x,
	};
}

static inline float dot(const Vector3 *a, const Vector3 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float squaredMagnitude(const Vector3 *v)
{
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

static inline Vector3 reflected(const Vector3 *v, const Vector3 *normal)
{
	float t = 2.0f * dot(v, normal);
	return (Vector3){
		v->x - t * normal->x,
		v->y - t * normal->y,
		v->z - t * normal->z,
	};
}


float magnitude(const Vector3 *v);
void normalize(Vector3 *v);
Vector3 normalized(const Vector3 *v);

/* Two unit vectors perpendicular to a and to each other: the tangent frame
   a contact is solved in. */
void computeBasis(const Vector3 *a, Vector3 *b, Vector3 *c);

}

}

#endif
