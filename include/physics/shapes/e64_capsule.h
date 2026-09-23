/*
	Capsule geometry (local Z axis): cylinder + hemispheres. Admin fields live
	in physics::Shape.
*/
#ifndef ENGINE64_CAPSULE_H
#define ENGINE64_CAPSULE_H

#include "math/e64_vector3.h"
#include "math/e64_transform.h"
#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_raycast.h"

namespace e64 {

namespace physics { namespace shape { struct MassData; } }


class Capsule {
public:

	struct Def {
		Transform tx;
		float radius;
		float half_height;
		float friction;
		float restitution;
		float density;
		int sensor;
	};


	float radius;
	float half_height; /* half-height along local Z, excluding caps */
};


namespace capsule {

/* Endpoints of the inner segment (center ± half_height along Z), in world space. */
void getSegment(const Capsule *c, const Transform *world, Vector3 *a, Vector3 *b);

int testPoint(const Capsule *c, const Transform *world, const Vector3 *p);
int raycast(const Capsule *c, const Transform *world, RaycastData *raycast);
void computeAABB(const Capsule *c, const Transform *world, AABB *aabb);
void computeMass(const Capsule *c, const Transform *local, float density, physics::shape::MassData *md);


namespace def {

void init(Capsule::Def *d);
void set(Capsule::Def *d, const Transform *tx, float radius, float half_height);
void setFriction(Capsule::Def *d, float f);
void setRestitution(Capsule::Def *d, float r);
void setDensity(Capsule::Def *d, float rho);
void setSensor(Capsule::Def *d, int s);

}

}

}

#endif
