#include <math.h>

#include "physics3d/geometry/e64_triangle.h"

namespace e64 {
namespace triangle {

/* Möller–Trumbore. A back face is a miss: a surface only counts from the side
   its normal points to, so a floor is floor from above and nothing from below.
   Same convention as the shape raycasts: ray->t bounds the ray, and a hit
   writes the distance along dir into ray->toi. */
int raycast(const Triangle *t, RaycastData *ray)
{
	Vector3 ab = vector3::difference(&t->vertices[1], &t->vertices[0]);
	Vector3 ac = vector3::difference(&t->vertices[2], &t->vertices[0]);

	Vector3 p = vector3::cross(&ray->dir, &ac);
	float det = vector3::dot(&ab, &p);
	if (det < 1.0e-6f) return 0; /* parallel to the face, or coming from behind it */

	float inv = 1.0f / det;
	Vector3 ap = vector3::difference(&ray->start, &t->vertices[0]);

	float u = vector3::dot(&ap, &p) * inv;
	if (u < 0.0f || u > 1.0f) return 0;

	Vector3 q = vector3::cross(&ap, &ab);
	float v = vector3::dot(&ray->dir, &q) * inv;
	if (v < 0.0f || u + v > 1.0f) return 0;

	float toi = vector3::dot(&ac, &q) * inv;
	if (toi < 0.0f || toi > ray->t) return 0;

	ray->toi = toi;
	ray->normal = t->normal;
	return 1;
}

/* Ericson, Real-Time Collision Detection: the Voronoi region the point falls
   in decides the answer, tested vertex, edge and face in that order. */
Vector3 closestToPoint(const Triangle *t, const Vector3 *point)
{
	const Vector3 *a = &t->vertices[0];
	const Vector3 *b = &t->vertices[1];
	const Vector3 *c = &t->vertices[2];

	Vector3 ab = {b->x - a->x, b->y - a->y, b->z - a->z};
	Vector3 ac = {c->x - a->x, c->y - a->y, c->z - a->z};
	Vector3 ap = {point->x - a->x, point->y - a->y, point->z - a->z};

	float d1 = ab.x*ap.x + ab.y*ap.y + ab.z*ap.z;
	float d2 = ac.x*ap.x + ac.y*ap.y + ac.z*ap.z;
	if (d1 <= 0.0f && d2 <= 0.0f) return *a;

	Vector3 bp = {point->x - b->x, point->y - b->y, point->z - b->z};
	float d3 = ab.x*bp.x + ab.y*bp.y + ab.z*bp.z;
	float d4 = ac.x*bp.x + ac.y*bp.y + ac.z*bp.z;
	if (d3 >= 0.0f && d4 <= d3) return *b;

	float vc = d1*d4 - d3*d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		float v = d1 / (d1 - d3);
		return (Vector3){a->x + v*ab.x, a->y + v*ab.y, a->z + v*ab.z};
	}

	Vector3 cp = {point->x - c->x, point->y - c->y, point->z - c->z};
	float d5 = ab.x*cp.x + ab.y*cp.y + ab.z*cp.z;
	float d6 = ac.x*cp.x + ac.y*cp.y + ac.z*cp.z;
	if (d6 >= 0.0f && d5 <= d6) return *c;

	float vb = d5*d2 - d1*d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		float w = d2 / (d2 - d6);
		return (Vector3){a->x + w*ac.x, a->y + w*ac.y, a->z + w*ac.z};
	}

	float va = d3*d6 - d5*d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return (Vector3){
			b->x + w*(c->x - b->x),
			b->y + w*(c->y - b->y),
			b->z + w*(c->z - b->z),
		};
	}

	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;
	return (Vector3){
		a->x + ab.x*v + ac.x*w,
		a->y + ab.y*v + ac.y*w,
		a->z + ab.z*v + ac.z*w,
	};
}

}
}
