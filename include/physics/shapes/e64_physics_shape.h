/*
	Polymorphic shape attached to a RigidBody.

	A RigidBody keeps one linked list of physics::Shape. Each shape carries
	its local transform and admin fields (friction, restitution, etc.), and a
	tagged union with the concrete geometry (Box / Sphere / Capsule). The
	narrowphase dispatches on `type`.
*/
#ifndef ENGINE64_PHYSICS_SHAPE_H
#define ENGINE64_PHYSICS_SHAPE_H

#include <stdbool.h>
#include <stdint.h>

#include "math/e64_matrix3.h"
#include "math/e64_transform.h"
#include "math/e64_vector3.h"
#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_raycast.h"
#include "physics/shapes/e64_box.h"
#include "physics/shapes/e64_sphere.h"
#include "physics/shapes/e64_capsule.h"
#include "physics/collision/e64_mesh_collider.h"

namespace e64 {

class RigidBody;


namespace physics {

namespace shape {

/* Shared by the box, sphere and capsule mass routines, whose headers only
   forward-declare it. */
struct MassData {
	Matrix3 inertia;
	Vector3 center;
	float mass;
};

}


class Shape {
public:

	enum Type {
		SHAPE_BOX,
		SHAPE_SPHERE,
		SHAPE_CAPSULE,
		SHAPE_MESH, /* static-only: no rigid body simulation */
	};


	/* What a sensor shape is a sensor *for*. Every value but NONE is truthy,
	   so the code that only asks whether a shape collides keeps reading
	   `sensor` as the flag it always was; the ones that consume a specific
	   kind of volume match on the value. */
	enum SensorType {
		SENSOR_NONE,
		SENSOR_VOLUME, /* plain overlap volume: water, triggers */
		SENSOR_CLIMBABLE, /* ladder: the character probe reads its frame */
	};


	/* Authoring-side counterpart of Shape: the geometry plus its offset,
	   before it is placed in a body or in the world. */
	struct Def {
		Type type;
		union {
			Box::Def box;
			Sphere::Def sphere;
			Capsule::Def capsule;
			MeshCollider::Def mesh;
		};
	};


	Type type;
	Transform local;
	/* body->tx composed with local, kept current by attach and by
	   rigidBody::synchronizeProxies, which runs whenever the body moves. A
	   static shape used to recompose this in every query of every frame. */
	Transform world;

	Shape *next;
	RigidBody *body;
	float friction;
	float restitution;
	float density;
	int32_t broadphase_index;
	void *owner;
	int sensor;

	union {
		Box box;
		Sphere sphere;
		Capsule capsule;
		MeshCollider *mesh;
	};
};


namespace shape {

namespace def {

/* Shape defs usually initialise .tx with a position only, leaving the rotation
   matrix zeroed: this reads an all-zero rotation as identity. */
Transform localTransform(const Transform *tx);

}


/* Builds a shape from its def, scaling both geometry and offset so one def
   serves any prop size. The offset lands in shape->local, for the caller to
   compose with wherever the shape ends up. Returns false for types that are
   not built from defs, so callers can skip them. */
bool fromDef(Shape *shape, const Shape::Def *def, Vector3 scale);

/* Frees whatever the shape owns. Only the mesh case owns anything. */
void release(Shape *shape);


/* Narrowphase / body dispatch — switches on shape->type. All three read the
   cached shape->world. */
int testPoint (const Shape *shape, const Vector3 *p);
int raycast (const Shape *shape, RaycastData *raycast);
void computeAABB(const Shape *shape, AABB *aabb);
void computeMass(const Shape *shape, MassData *md);

}

}

}

#endif
