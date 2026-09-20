#ifndef ENGINE64_VECTOR2_H
#define ENGINE64_VECTOR2_H

#include <math.h>

namespace e64 {

typedef struct Vector2 {
	float x;
	float y;
} Vector2;


/* Same split as Vector3: the arithmetic is static inline because each body is
   smaller than the call it replaces, and the one that takes a square root
   stays a real function in the .c. */

static inline Vector2 vector2_create(float x, float y)
{
	return (Vector2){x, y};
}

static inline Vector2 vector2_zero(void)
{
	return (Vector2){0.0f, 0.0f};
}

static inline void vector2_scale(Vector2 *v, float scalar)
{
	v->x *= scalar;
	v->y *= scalar;
}

static inline void vector2_addScaledVector(Vector2 *v, const Vector2 *w, float scalar)
{
	v->x += w->x * scalar;
	v->y += w->y * scalar;
}

static inline void vector2_add(Vector2 *v, const Vector2 *w)
{
	v->x += w->x;
	v->y += w->y;
}

static inline void vector2_sub(Vector2 *v, const Vector2 *w)
{
	v->x -= w->x;
	v->y -= w->y;
}

static inline void vector2_invert(Vector2 *v)
{
	v->x = -v->x;
	v->y = -v->y;
}

static inline Vector2 vector2_sum(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x + b->x, a->y + b->y};
}

static inline Vector2 vector2_difference(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x - b->x, a->y - b->y};
}

static inline Vector2 vector2_scaled(const Vector2 *v, float scalar)
{
	return (Vector2){v->x * scalar, v->y * scalar};
}

static inline Vector2 vector2_inverted(const Vector2 *v)
{
	return (Vector2){-v->x, -v->y};
}

static inline Vector2 vector2_abs(const Vector2 *v)
{
	return (Vector2){fabsf(v->x), fabsf(v->y)};
}

static inline float vector2_dot(const Vector2 *a, const Vector2 *b)
{
	return a->x * b->x + a->y * b->y;
}

/* Positive when b turns counterclockwise from a: the z of the 3D cross, which
   in two dimensions is all a cross product has left. */
static inline float vector2_cross(const Vector2 *a, const Vector2 *b)
{
	return a->x * b->y - a->y * b->x;
}

static inline float vector2_squaredMagnitude(const Vector2 *v)
{
	return v->x * v->x + v->y * v->y;
}

static inline Vector2 vector2_min(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x < b->x ? a->x : b->x, a->y < b->y ? a->y : b->y};
}

static inline Vector2 vector2_max(const Vector2 *a, const Vector2 *b)
{
	return (Vector2){a->x > b->x ? a->x : b->x, a->y > b->y ? a->y : b->y};
}

float   vector2_magnitude(const Vector2 *v);
void    vector2_normalize(Vector2 *v);
Vector2 vector2_normalized(const Vector2 *v);

}

#endif
