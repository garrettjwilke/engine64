#include <math.h>

#include "math/e64_math.h"
#include "physics2d/shapes/e64_segment2d.h"

namespace e64 {
namespace segment2d {

static const float EPSILON_2D = 1.0e-8f;


void getPoints(const Segment2D *s, const Transform2D *world, Vector2 *a, Vector2 *b)
{
	*a = transform2d::mulVector(world, &s->a);
	*b = transform2d::mulVector(world, &s->b);
}


int testPoint(const Segment2D *s, const Transform2D *world, const Vector2 *p)
{
	(void)s;
	(void)world;
	(void)p;
	return 0;
}


/* Ray p + t*d against a + u*e: both parameters come out of cross products,
   and the hit counts only inside the segment and inside the ray. */
int raycast(const Segment2D *s, const Transform2D *world, RaycastData2D *ray)
{
	Vector2 a, b;
	getPoints(s, world, &a, &b);

	Vector2 e = vector2::difference(&b, &a);
	float denom = vector2::cross(&ray->dir, &e);
	if (fabsf(denom) < EPSILON_2D) return 0;

	Vector2 w = vector2::difference(&a, &ray->start);
	float t = vector2::cross(&w, &e) / denom;
	float u = vector2::cross(&w, &ray->dir) / denom;

	if (t < 0.0f || t > ray->t) return 0;
	if (u < 0.0f || u > 1.0f) return 0;

	Vector2 n = { -e.y, e.x };
	if (vector2::dot(&n, &ray->dir) > 0.0f) vector2::invert(&n);

	ray->toi = t;
	ray->normal = vector2::normalized(&n);
	return 1;
}


void computeAABB(const Segment2D *s, const Transform2D *world, AABB2D *aabb)
{
	Vector2 a, b;
	getPoints(s, world, &a, &b);
	aabb->min = vector2::min(&a, &b);
	aabb->max = vector2::max(&a, &b);
}


Vector2 closestToPoint(const Vector2 *a, const Vector2 *b, const Vector2 *point)
{
	Vector2 ab = vector2::difference(b, a);
	Vector2 ap = vector2::difference(point, a);

	float ab_len_sq = vector2::squaredMagnitude(&ab);
	if (ab_len_sq < EPSILON_2D) return *a;

	float t = clampf(vector2::dot(&ap, &ab) / ab_len_sq, 0.0f, 1.0f);
	return (Vector2){ a->x + t * ab.x, a->y + t * ab.y };
}


void closestToSegment(
	const Vector2 *a1, const Vector2 *b1,
	const Vector2 *a2, const Vector2 *b2,
	Vector2 *closest1, Vector2 *closest2)
{
	Vector2 d1 = vector2::difference(b1, a1);
	Vector2 d2 = vector2::difference(b2, a2);
	Vector2 r = vector2::difference(a1, a2);

	float a = vector2::dot(&d1, &d1);
	float e = vector2::dot(&d2, &d2);
	float f = vector2::dot(&d2, &r);

	float s, t;

	if (a <= EPSILON_2D && e <= EPSILON_2D) {
		*closest1 = *a1;
		*closest2 = *a2;
		return;
	}

	if (a <= EPSILON_2D) {
		s = 0.0f;
		t = clampf(f / e, 0.0f, 1.0f);
	}
	else {
		float c = vector2::dot(&d1, &r);
		if (e <= EPSILON_2D) {
			t = 0.0f;
			s = clampf(-c / a, 0.0f, 1.0f);
		}
		else {
			float b = vector2::dot(&d1, &d2);
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

	*closest1 = (Vector2){ a1->x + d1.x*s, a1->y + d1.y*s };
	*closest2 = (Vector2){ a2->x + d2.x*t, a2->y + d2.y*t };
}

}
}
