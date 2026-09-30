/*
	Sphere geometry (radius). Admin fields live in physics::Shape.
*/
#ifndef ENGINE64_SPHERE_H
#define ENGINE64_SPHERE_H

#include "math/e64_vector3.h"
#include "math/e64_transform.h"
#include "physics3d/geometry/e64_aabb.h"
#include "physics3d/geometry/e64_raycast.h"

namespace e64 {

namespace physics { namespace shape { struct MassData; } }


class Sphere {
public:

	struct Def {
		Transform tx;
		float radius;
		float friction;
		float restitution;
		float density;
		int sensor;
	};


	float radius;
};


namespace sphere {

int testPoint(const Sphere *s, const Transform *world, const Vector3 *p);
int raycast(const Sphere *s, const Transform *world, RaycastData *raycast);
void computeAABB(const Sphere *s, const Transform *world, AABB *aabb);
void computeMass(const Sphere *s, const Transform *local, float density, physics::shape::MassData *md);


namespace def {

void init(Sphere::Def *d);
void set(Sphere::Def *d, const Transform *tx, float radius);
void setFriction(Sphere::Def *d, float f);
void setRestitution(Sphere::Def *d, float r);
void setDensity(Sphere::Def *d, float rho);
void setSensor(Sphere::Def *d, int s);

}

}

}

#endif
