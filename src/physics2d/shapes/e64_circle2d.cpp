#include <math.h>

#include "physics2d/shapes/e64_circle2d.h"

namespace e64 {
namespace circle2d {

int testPoint(const Circle2D *c, const Transform2D *world, const Vector2 *p)
{
	Vector2 d = vector2::difference(p, &world->position);
	return vector2::squaredMagnitude(&d) <= c->radius * c->radius;
}


int raycast(const Circle2D *c, const Transform2D *world, RaycastData2D *ray)
{
	Vector2 m = vector2::difference(&ray->start, &world->position);
	float b = vector2::dot(&m, &ray->dir);
	float cc = vector2::dot(&m, &m) - c->radius * c->radius;

	if (cc > 0.0f && b > 0.0f) return 0;
	float disc = b * b - cc;
	if (disc < 0.0f) return 0;

	float t = -b - sqrtf(disc);
	if (t < 0.0f) t = 0.0f;
	if (t > ray->t) return 0;

	ray->toi = t;
	Vector2 hit = vector2::scaled(&ray->dir, t);
	hit = vector2::sum(&ray->start, &hit);
	Vector2 n = vector2::difference(&hit, &world->position);
	ray->normal = vector2::normalized(&n);
	return 1;
}


void computeAABB(const Circle2D *c, const Transform2D *world, AABB2D *aabb)
{
	Vector2 r = { c->radius, c->radius };
	aabb->min = vector2::difference(&world->position, &r);
	aabb->max = vector2::sum(&world->position, &r);
}

}
}
