#ifndef ENGINE64_CHARACTER3D_PHYSICS_H
#define ENGINE64_CHARACTER3D_PHYSICS_H

#include <stdbool.h>

#include "physics/math/e64_vector3.h"
#include "physics/math/e64_transform.h"
#include "physics/shapes/e64_physics_shape.h"

namespace e64 {

class Character3D;
typedef struct CollisionMesh CollisionMesh;
typedef struct PhysicsWorld PhysicsWorld;


namespace character3d {

typedef struct KinematicBody {
	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 rotation;

	/* Its standing in the physics world. The solver never moves it — the fields
	   above do — but registering it is what makes the broadphase pair it with
	   rigid bodies, so the character can shove them. */
	struct RigidBody *rigid;
} KinematicBody;


typedef struct ColliderSettings {
	float radius;
	float height;
} ColliderSettings;


typedef struct Collider {
	Capsule   shape;
	Transform world;    /* vertical capsule, position at the capsule center */
} Collider;


namespace collider {

void init       (Collider *collider, float radius, float half_height);
void setVertical(Collider *collider, const Vector3 *position);

}


namespace physics {

/* Depenetrate against the world's static bodies — every contact classified as
   floor, wall or ceiling, one combined recovery per pass — then snap to the
   floor. Runs after the movement update, before the render sync. The character
   is not simulated by the solver: it reads those shapes and resolves on its
   own. */
void collide(Character3D *character, const PhysicsWorld *world);

/* The character's standing in the world: created once, written every frame
   after collide, so the solver sees where it ended up and how fast it got
   there. Without this the character passes through every rigid body. */
void createBody(Character3D *character, PhysicsWorld *world);
void syncBody  (Character3D *character);

}

}


}

#endif
