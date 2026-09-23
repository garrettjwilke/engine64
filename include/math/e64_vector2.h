#ifndef ENGINE64_VECTOR2_H
#define ENGINE64_VECTOR2_H

#include <math.h>

namespace e64 {

typedef struct Vector2 {
	float x;
	float y;
} Vector2;


namespace vector2 {

/* Same split as Vector3: the arithmetic is static inline because each body is
   smaller than the call it replaces, and the one that takes a square root
   stays a real function in the .cpp. */

static inline Vector2 create(float x, float y)
{
	return (Vector2){x, y};
}

static inline Vector2 zero(void)
{
	return (Vector2){0.0f, 0.0f};
}

static inline void scale(Vector2 *v, float scalar)
{
	v->x *= scalar;
	v->y *= scalar;
}

static inline void addScaledVector(Vector2 *v, const Vector2 *w, float scalar)
{
	v->x += w->x * scalar;
	v->y += w->y * scalar;
}

static inline void add(Vector2 *v, const Vector2 *w)
{
	v->x += w->x;
	v->y += w->y;
}

static inline void sub(Vector2 *v, const Vector2 *w)
{
	v->x -= w->x;
	v->y -= w->y;
}

static inline void invert(Vector2 *v)
{
	v->x = -v->x;
	v->y = -v->y;
}

static inline Vector2 sum(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x + b->x, a->y + b->y};
}

static inline Vector2 difference(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x - b->x, a->y - b->y};
}

static inline Vector2 scaled(const Vector2 *v, float scalar)
{
	return (Vector2){v->x * scalar, v->y * scalar};
}

static inline Vector2 inverted(const Vector2 *v)
{
	return (Vector2){-v->x, -v->y};
}

static inline Vector2 abs(const Vector2 *v)
{
	return (Vector2){fabsf(v->x), fabsf(v->y)};
}

static inline float dot(const Vector2 *a, const Vector2 *b)
{
	return a->x * b->x + a->y * b->y;
}

/* Positive when b turns counterclockwise from a: the z of the 3D cross, which
   in two dimensions is all a cross product has left. */
static inline float cross(const Vector2 *a, const Vector2 *b)
{
	return a->x * b->y - a->y * b->x;
}

static inline float squaredMagnitude(const Vector2 *v)
{
	return v->x * v->x + v->y * v->y;
}

static inline Vector2 min(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x < b->x ? a->x : b->x, a->y < b->y ? a->y : b->y};
}

static inline Vector2 max(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x > b->x ? a->x : b->x, a->y > b->y ? a->y : b->y};
}

float magnitude(const Vector2 *v);
void normalize(Vector2 *v);
Vector2 normalized(const Vector2 *v);

}

}

#endif
