#include <math.h>

#include "physics2d/shapes/e64_capsule2d.h"
#include "physics2d/shapes/e64_segment2d.h"

namespace e64 {
namespace capsule2d {

void getSegment(const Capsule2D *c, const Transform2D *world, Vector2 *a, Vector2 *b)
{
	Vector2 la = { 0.0f, -c->half_height };
	Vector2 lb = { 0.0f, c->half_height };
	*a = transform2d::mulVector(world, &la);
	*b = transform2d::mulVector(world, &lb);
}


int testPoint(const Capsule2D *c, const Transform2D *world, const Vector2 *p)
{
	Vector2 a, b;
	getSegment(c, world, &a, &b);

	Vector2 closest = segment2d::closestToPoint(&a, &b, p);
	Vector2 d = vector2::difference(p, &closest);
	return vector2::squaredMagnitude(&d) <= c->radius * c->radius;
}


/* Exact, in local space: the two sides x = ±radius for |y| <= half_height,
   and the outer half of each cap circle. A start inside hits at 0. */
int raycast(const Capsule2D *c, const Transform2D *world, RaycastData2D *ray)
{
	if (testPoint(c, world, &ray->start)) {
		ray->toi = 0.0f;
		ray->normal = vector2::inverted(&ray->dir);
		return 1;
	}

	Vector2 p = transform2d::mulVectorTransposed(world, &ray->start);
	Vector2 d = transform2d::rotateVectorTransposed(world, &ray->dir);

	const float epsilon = 1.0e-8f;
	float r = c->radius;
	float h = c->half_height;

	float best = ray->t;
	Vector2 n = vector2::zero();
	int hit = 0;

	if (fabsf(d.x) > epsilon) {
		for (int i = 0; i < 2; ++i) {
			float side = i ? 1.0f : -1.0f;
			if (d.x * side >= 0.0f) continue;

			float t = (side * r - p.x) / d.x;
			float y = p.y + t * d.y;
			if (t >= 0.0f && t <= best && y >= -h && y <= h) {
				best = t;
				n = (Vector2){ side, 0.0f };
				hit = 1;
			}
		}
	}

	for (int i = 0; i < 2; ++i) {
		float cy = i ? h : -h;
		float up = i ? 1.0f : -1.0f;

		Vector2 m = { p.x, p.y - cy };
		float b = vector2::dot(&m, &d);
		float cc = vector2::dot(&m, &m) - r * r;
		if (cc > 0.0f && b > 0.0f) continue;
		float disc = b * b - cc;
		if (disc < 0.0f) continue;

		float t = -b - sqrtf(disc);
		if (t < 0.0f || t > best) continue;

		Vector2 q = { p.x + t * d.x, p.y + t * d.y - cy };
		if (q.y * up < 0.0f) continue;

		best = t;
		n = vector2::normalized(&q);
		hit = 1;
	}

	if (!hit) return 0;

	ray->toi = best;
	ray->normal = transform2d::rotateVector(world, &n);
	return 1;
}


void computeAABB(const Capsule2D *c, const Transform2D *world, AABB2D *aabb)
{
	Vector2 a, b;
	getSegment(c, world, &a, &b);

	Vector2 r = { c->radius, c->radius };
	Vector2 lo = vector2::min(&a, &b);
	Vector2 hi = vector2::max(&a, &b);
	aabb->min = vector2::difference(&lo, &r);
	aabb->max = vector2::sum(&hi, &r);
}

}
}
