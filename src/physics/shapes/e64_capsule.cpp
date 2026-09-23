#include <math.h>

#include "physics/shapes/e64_capsule.h"
#include "physics/shapes/e64_physics_shape.h" /* MassData */
#include "math/e64_math.h" /* PI, segment_closestToPoint */

namespace e64 {
namespace capsule {

void getSegment(const Capsule *c, const Transform *world, Vector3 *a, Vector3 *b)
{
	Vector3 local_top = { 0.0f, 0.0f, c->half_height };
	Vector3 local_bottom = { 0.0f, 0.0f, -c->half_height };
	*a = transform::mulVector(world, &local_bottom);
	*b = transform::mulVector(world, &local_top);
}


int testPoint(const Capsule *c, const Transform *world, const Vector3 *p)
{
	Vector3 a, b;
	getSegment(c, world, &a, &b);
	Vector3 closest = segment_closestToPoint(&a, &b, p);
	Vector3 d = vector3::difference(p, &closest);
	return vector3::squaredMagnitude(&d) <= c->radius * c->radius;
}


int raycast(const Capsule *c, const Transform *world, RaycastData *ray)
{
	/* Cheap: enclose the capsule in a sphere at its center with radius = radius + half_height.
	   Good enough for broad raycast; exact capsule-ray iterates cylinder+caps. */
	float R = c->radius + c->half_height;
	Vector3 m = vector3::difference(&ray->start, &world->position);
	float b = vector3::dot(&m, &ray->dir);
	float cc = vector3::dot(&m, &m) - R * R;
	if (cc > 0.0f && b > 0.0f) return 0;
	float disc = b * b - cc;
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


void computeAABB(const Capsule *c, const Transform *world, AABB *aabb)
{
	Vector3 a, b;
	getSegment(c, world, &a, &b);
	Vector3 r = { c->radius, c->radius, c->radius };
	Vector3 mn = { a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z };
	Vector3 mx = { a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z };
	aabb->min = vector3::difference(&mn, &r);
	aabb->max = vector3::sum(&mx, &r);
}


void computeMass(const Capsule *c, const Transform *local, float density, physics::shape::MassData *md)
{
	float r = c->radius;
	float r2 = r * r;
	float h = 2.0f * c->half_height;

	float cyl_vol = PI * r2 * h;
	float cap_vol = (4.0f / 3.0f) * PI * r2 * r;
	float mass = (cyl_vol + cap_vol) * density;

	float cyl_mass = cyl_vol * density;
	float cap_mass = cap_vol * density;

	/* Solid cylinder around its own axis = (1/2) m r²; perpendicular = (1/12) m (3r² + h²). */
	float Izz = 0.5f * cyl_mass * r2 + 0.4f * cap_mass * r2;
	float Ixx = cyl_mass * ( (1.0f/12.0f) * h * h + 0.25f * r2 )
	          + cap_mass * ( 0.4f * r2 + 0.5f * h * h + 0.375f * r * h );
	float Iyy = Ixx;

	Matrix3 I = matrix3::diagonal(Ixx, Iyy, Izz);

	/* Rotate to local frame. */
	Matrix3 rt = matrix3::transposed(&local->rotation);
	Matrix3 tmp = matrix3::product(&local->rotation, &I);
	I = matrix3::product(&tmp, &rt);

	/* Parallel axis: I += m · (|pos|² · identity - pos⊗pos). */
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

void init(Capsule::Def *d)
{
	transform::init(&d->tx);
	d->radius = 0.5f;
	d->half_height = 0.5f;
	d->friction = 0.4f;
	d->restitution = 0.0f;
	d->density = 1.0f;
	d->sensor = 0;
}

void set(Capsule::Def *d, const Transform *tx, float radius, float half_height)
{
	d->tx = *tx;
	d->radius = radius;
	d->half_height = half_height;
}

void setFriction(Capsule::Def *d, float f) { d->friction = f; }
void setRestitution(Capsule::Def *d, float r) { d->restitution = r; }
void setDensity(Capsule::Def *d, float rho) { d->density = rho; }
void setSensor(Capsule::Def *d, int s) { d->sensor = s; }

}

}
}
