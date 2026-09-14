#ifndef ENGINE64_CHARACTER3D_PHYSICS_H
#define ENGINE64_CHARACTER3D_PHYSICS_H

#include <stdbool.h>

#include "physics/math/e64_vector3.h"
#include "physics/math/e64_transform.h"
#include "physics/shapes/e64_physics_shape.h"


typedef struct Character3D Character3D;
typedef struct CollisionMesh CollisionMesh;
typedef struct PhysicsWorld PhysicsWorld;


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


typedef struct Character3DColliderSettings {
	float radius;
	float height;
} Character3DColliderSettings;


typedef struct Character3DCollider {
	Capsule   shape;
	Transform world;    /* vertical capsule, position at the capsule center */
} Character3DCollider;


void character3dCollider_init       (Character3DCollider *collider, float radius, float half_height);
void character3dCollider_setVertical(Character3DCollider *collider, const Vector3 *position);

/* Depenetrate against the world's static bodies — every contact classified as
   floor, wall or ceiling, one combined recovery per pass — then snap to the
   floor. Runs after the movement update, before the render sync. The character
   is not simulated by the solver: it reads those shapes and resolves on its
   own. */
void character3dPhysics_collide(Character3D *character, const PhysicsWorld *world);

/* The character's standing in the world: created once, written every frame
   after collide, so the solver sees where it ended up and how fast it got
   there. Without this the character passes through every rigid body. */
void character3dPhysics_createBody(Character3D *character, PhysicsWorld *world);
void character3dPhysics_syncBody  (Character3D *character);


#endif
