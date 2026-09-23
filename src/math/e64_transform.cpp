#include "math/e64_transform.h"

namespace e64 {
namespace transform {

Transform inverse(const Transform *t)
{
	Transform inv;
	inv.rotation = matrix3::transposed(&t->rotation);
	Vector3 neg = vector3::inverted(&t->position);
	inv.position = matrix3::transformVector(&inv.rotation, &neg);
	return inv;
}


Vector3 mulVector(const Transform *t, const Vector3 *v)
{
	Vector3 r = matrix3::transformVector(&t->rotation, v);
	return vector3::sum(&r, &t->position);
}


Vector3 mulVectorScaled(const Transform *t, const Vector3 *scale, const Vector3 *v)
{
	Vector3 scaled = { scale->x * v->x, scale->y * v->y, scale->z * v->z };
	Vector3 r = matrix3::transformVector(&t->rotation, &scaled);
	return vector3::sum(&r, &t->position);
}


Transform product(const Transform *t, const Transform *u)
{
	Transform out;
	out.rotation = matrix3::product(&t->rotation, &u->rotation);
	Vector3 rp = matrix3::transformVector(&t->rotation, &u->position);
	out.position = vector3::sum(&rp, &t->position);
	return out;
}


Vector3 mulVectorTransposed(const Transform *t, const Vector3 *v)
{
	Vector3 d = vector3::difference(v, &t->position);
	return matrix3::transformVectorTransposed(&t->rotation, &d);
}


Transform productTransposed(const Transform *t, const Transform *u)
{
	Transform out;
	Matrix3 rt = matrix3::transposed(&t->rotation);
	out.rotation = matrix3::product(&rt, &u->rotation);
	Vector3 d = vector3::difference(&u->position, &t->position);
	out.position = matrix3::transformVector(&rt, &d);
	return out;
}


HalfSpace mulHalfSpace(const Transform *t, const HalfSpace *p)
{
	Vector3 origin = halfSpace::origin(p);
	Vector3 worldOrigin = mulVector(t, &origin);
	Vector3 worldNormal = matrix3::transformVector(&t->rotation, &p->normal);
	HalfSpace out;
	out.normal = worldNormal;
	out.distance = vector3::dot(&worldOrigin, &worldNormal);
	return out;
}


HalfSpace mulHalfSpaceScaled(const Transform *t, const Vector3 *scale, const HalfSpace *p)
{
	Vector3 origin = halfSpace::origin(p);
	Vector3 worldOrigin = mulVectorScaled(t, scale, &origin);
	Vector3 worldNormal = matrix3::transformVector(&t->rotation, &p->normal);
	HalfSpace out;
	out.normal = worldNormal;
	out.distance = vector3::dot(&worldOrigin, &worldNormal);
	return out;
}


HalfSpace mulHalfSpaceTransposed(const Transform *t, const HalfSpace *p)
{
	Vector3 planeOrigin = vector3::scaled(&p->normal, p->distance);
	Vector3 localOrigin = mulVectorTransposed(t, &planeOrigin);
	Vector3 localNormal = matrix3::transformVectorTransposed(&t->rotation, &p->normal);
	HalfSpace out;
	out.normal = localNormal;
	out.distance = vector3::dot(&localOrigin, &localNormal);
	return out;
}

}
}
