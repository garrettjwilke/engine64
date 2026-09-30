#include <math.h>

#include "physics3d/shapes/e64_sphere.h"
#include "physics3d/shapes/e64_physics_shape.h" /* MassData */
#include "math/e64_math.h" /* PI */

namespace e64 {
namespace sphere {

int testPoint(const Sphere *s, const Transform *world, const Vector3 *p)
{
	Vector3 d = vector3::difference(p, &world->position);
	return vector3::squaredMagnitude(&d) <= s->radius * s->radius;
}


int raycast(const Sphere *s, const Transform *world, RaycastData *ray)
{
	Vector3 m = vector3::difference(&ray->start, &world->position);
	float b = vector3::dot(&m, &ray->dir);
	float c = vector3::dot(&m, &m) - s->radius * s->radius;

	if (c > 0.0f && b > 0.0f) return 0;
	float disc = b * b - c;
	if (disc < 0.0f) return 0;

	float t = -b - sqrtf(disc);
	if (t < 0.0f) t = 0.0f;
	if (t > ray->t) return 0;

	ray->toi = t;
	Vector3 hit = vector3::scaled(&ray->dir, t);
	hit = vector3::sum(&ray->start, &hit);
	Vector3 n = vector3::difference(&hit, &world->position);
	ray->normal = vector3::normalized(&n);
	return 1;
}


void computeAABB(const Sphere *s, const Transform *world, AABB *aabb)
{
	Vector3 r = { s->radius, s->radius, s->radius };
	aabb->min = vector3::difference(&world->position, &r);
	aabb->max = vector3::sum(&world->position, &r);
}


void computeMass(const Sphere *s, const Transform *local, float density, physics::shape::MassData *md)
{
	float r = s->radius;
	float r2 = r * r;
	float vol = (4.0f / 3.0f) * PI * r2 * r;
	float mass = vol * density;

	/* Solid sphere: I = (2/5) · m · r² on each axis. */
	float i = 0.4f * mass * r2;
	Matrix3 I = matrix3::diagonal(i, i, i);

	/* Parallel axis: I += m · (|c|² · I - c⊗c). */
	Matrix3 identity = matrix3::identity();
	float dot = vector3::dot(&local->position, &local->position);
	Matrix3 outer = matrix3::outerProduct(&local->position, &local->position);
	Matrix3 term = matrix3::scaled(&identity, dot);
	term = matrix3::difference(&term, &outer);
	Matrix3 scaled_term = matrix3::scaled(&term, mass);
	I = matrix3::sum(&I, &scaled_term);

	md->center = local->position;
	md->inertia = I;
	md->mass = mass;
}


namespace def {

void init(Sphere::Def *d)
{
	transform::init(&d->tx);
	d->radius = 0.5f;
	d->friction = 0.4f;
	d->restitution = 0.2f;
	d->density = 1.0f;
	d->sensor = 0;
}

void set(Sphere::Def *d, const Transform *tx, float radius)
{
	d->tx = *tx;
	d->radius = radius;
}

void setFriction(Sphere::Def *d, float f) { d->friction = f; }
void setRestitution(Sphere::Def *d, float r) { d->restitution = r; }
void setDensity(Sphere::Def *d, float rho) { d->density = rho; }
void setSensor(Sphere::Def *d, int s) { d->sensor = s; }

}

}
}
