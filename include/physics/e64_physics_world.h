/*
	Ported from qu3e q3Scene.h — altered source, not the original software.

	Copyright (c) 2014 Randy Gaul http://www.randygaul.net

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
	     claim that you wrote the original software. If you use this software
	     in a product, an acknowledgment in the product documentation would be
	     appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
	     be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
*/

/*
	Top-level world: bodies, broadphase and contact manager. Owns the memory
	allocators.
*/
#ifndef ENGINE64_PHYSICS_WORLD_H
#define ENGINE64_PHYSICS_WORLD_H

#include <stdint.h>

#include "math/e64_vector3.h"
#include "memory/e64_stack.h"
#include "memory/e64_heap.h"
#include "memory/e64_paged_allocator.h"
#include "physics/collision/e64_contact_manager.h"
#include "physics/e64_rigid_body.h"
#include "physics/e64_buoyancy.h"
#include "physics/e64_cloth.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_raycast.h"

namespace e64 {

namespace physics {

class World {
public:

	struct ContactListener {
		void *user_data;
		void (*begin_contact)(void *user_data, const Contact::Constraint *contact);
		void (*end_contact) (void *user_data, const Contact::Constraint *contact);
	};


	typedef int (*QueryCallback)(void *user_data, Shape *shape);


	/* Registered water volumes; one per water surface. */
	static constexpr int32_t MAX_BUOYANCY_VOLUMES = 2;


	Contact::Manager contact_manager;
	memory::PagedAllocator shape_allocator;

	int32_t body_count;
	RigidBody *body_list;

	int32_t cloth_count;
	Cloth *cloth_list;

	const buoyancy::Volume *buoyancy[MAX_BUOYANCY_VOLUMES];
	int32_t buoyancy_count;

	memory::Stack stack;
	memory::Heap heap;

	Vector3 gravity;
	Vector3 wind; /* pushes cloths only; write it per frame */
	float dt;
	float accumulator; /* cloth clock; rigid bodies step on the frame's dt */
	int32_t iterations;

	int new_shape;
	int allow_sleep;
	int enable_friction;

	ContactListener *contact_listener;
};


namespace world {

void init (World *s, float dt, Vector3 gravity, int32_t iterations);
void shutdown(World *s);

RigidBody *createBody (World *s, const RigidBody::Def *def);
void removeBody (World *s, RigidBody *body);
void removeAllBodies(World *s);

/* Cloths are stepped by physics::step along with the bodies. The def names
   the welded collision mesh that seeds the particles; it is loaded here,
   read, and dropped, so the caller never handles it. */
Cloth *createCloth (World *s, const Cloth::Def *def);
void removeCloth (World *s, Cloth *cloth);
void removeAllCloths(World *s);

void setAllowSleep (World *s, int allow_sleep);
void setIterations (World *s, int32_t iterations);
void setEnableFriction(World *s, int enabled);

Vector3 getGravity(const World *s);
void setGravity(World *s, Vector3 gravity);

/* Only cloths feel it. Meant to be rewritten every frame, gusts included. */
void setWind(World *s, Vector3 wind);

/* The volume is borrowed, not copied: its owner keeps it alive for the
   world's lifetime. Buoyancy runs inside physics::step on every dynamic
   body overlapping the volume's sensor shape. */
void addBuoyancy(World *s, const buoyancy::Volume *volume);

void setContactListener(World *s, World::ContactListener *listener);

void queryAABB (const World *s, void *cb_user_data, World::QueryCallback cb, AABB aabb);
void queryPoint(const World *s, void *cb_user_data, World::QueryCallback cb, Vector3 point);
void rayCast (const World *s, void *cb_user_data, World::QueryCallback cb, RaycastData *raycast);


/* Shape pool entry points used by rigidBody. */
void allocShape (World *s, Shape **out);
void freeShape (World *s, Shape *shape);
void markNewShape(World *s);

}

}

}

#endif
