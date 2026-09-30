/*
	Ported from qu3e q3Scene.cpp — altered source, not the original software.

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
	World assembly, body and shape management.
*/
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "physics3d/e64_physics_world.h"
#include "physics3d/e64_physics_island.h"
#include "physics3d/collision/e64_contact.h"
#include "physics3d/collision/e64_contact_solver.h"
#include "physics3d/collision/e64_contact_manager.h"
#include "physics3d/collision/e64_collision.h"
#include "physics3d/collision/e64_mesh_collider.h"
#include "physics3d/collision/e64_broad_phase.h"

namespace e64 {
namespace physics {
namespace world {

void allocShape(World *s, Shape **out)
{
	*out = (Shape *)memory::paged::allocate(&s->shape_allocator);
}

void freeShape(World *s, Shape *shape)
{
	memory::paged::free(&s->shape_allocator, shape);
}

void markNewShape(World *s)
{
	s->new_shape = 1;
}


void init(World *s, float dt, Vector3 gravity, int32_t iterations)
{
	memory::stack::init(&s->stack);
	memory::heap::init (&s->heap);
	contact::manager::init(&s->contact_manager, &s->stack);
	memory::paged::init(&s->shape_allocator, (int32_t)sizeof(Shape), 256);

	s->body_count = 0;
	s->body_list = NULL;
	s->cloth_count = 0;
	s->cloth_list = NULL;
	s->buoyancy_count = 0;
	s->wind = vector3::zero();
	s->gravity = gravity;
	s->dt = dt;
	s->accumulator = 0.0f;
	s->iterations = iterations;
	s->new_shape = 0;
	s->allow_sleep = 1;
	s->enable_friction = 1;
	s->contact_listener = NULL;
}


void removeAllBodies(World *s)
{
	RigidBody *body = s->body_list;
	while (body) {
		RigidBody *next = body->next;
		rigidBody::removeAllShapes(body);
		memory::heap::free(&s->heap, body);
		body = next;
	}
	s->body_list = NULL;
	s->body_count = 0;
}


Cloth *createCloth(World *s, const Cloth::Def *def)
{
	Cloth *cloth = (Cloth *)memory::heap::allocate(&s->heap, sizeof(Cloth));
	if (cloth == NULL) return NULL;

	/* The mesh is scaffolding: it seeds the particles and the constraints, and
	   nothing keeps a reference to it afterwards. */
	MeshCollider *mesh = meshCollider::load(def->mesh_path);
	bool built = cloth::create(cloth, mesh, def);
	meshCollider::destroy(mesh);

	if (!built) {
		memory::heap::free(&s->heap, cloth);
		return NULL;
	}

	cloth->gravity = s->gravity;
	cloth->wind = s->wind;

	cloth->next = s->cloth_list;
	s->cloth_list = cloth;
	s->cloth_count++;

	return cloth;
}


void removeCloth(World *s, Cloth *cloth)
{
	for (Cloth **link = &s->cloth_list; *link; link = &(*link)->next) {
		if (*link != cloth) continue;

		*link = cloth->next;
		cloth::destroy(cloth);
		memory::heap::free(&s->heap, cloth);
		s->cloth_count--;
		return;
	}
}


void removeAllCloths(World *s)
{
	Cloth *cloth = s->cloth_list;
	while (cloth) {
		Cloth *next = cloth->next;
		cloth::destroy(cloth);
		memory::heap::free(&s->heap, cloth);
		cloth = next;
	}
	s->cloth_list = NULL;
	s->cloth_count = 0;
}


void shutdown(World *s)
{
	removeAllCloths(s);
	removeAllBodies(s);
	memory::paged::shutdown(&s->shape_allocator);
	contact::manager::shutdown(&s->contact_manager);
	memory::heap::shutdown(&s->heap);
	memory::stack::shutdown(&s->stack);
}


RigidBody *createBody(World *s, const RigidBody::Def *def)
{
	RigidBody *body = (RigidBody *)memory::heap::allocate(&s->heap, (int32_t)sizeof(RigidBody));

	/* The heap is a fixed 256 KB and hands back NULL when it is full or too
	   fragmented to fit. Writing the body through that NULL corrupts low
	   memory and then the world walks a list holding it, which surfaces far
	   from here as garbage floats in the solver. */
	assert(body);
	rigidBody::init(body, def, s);

	body->prev = NULL;
	body->next = s->body_list;
	if (s->body_list) s->body_list->prev = body;
	s->body_list = body;
	++s->body_count;
	return body;
}


void removeBody(World *s, RigidBody *body)
{
	assert(s->body_count > 0);

	contact::manager::removeContactsFromBody(&s->contact_manager, body);
	rigidBody::removeAllShapes(body);

	if (body->next) body->next->prev = body->prev;
	if (body->prev) body->prev->next = body->next;
	if (body == s->body_list) s->body_list = body->next;
	--s->body_count;

	memory::heap::free(&s->heap, body);
}


void setAllowSleep(World *s, int allow_sleep)
{
	s->allow_sleep = allow_sleep;
	if (!allow_sleep) {
		for (RigidBody *body = s->body_list; body; body = body->next) rigidBody::setToAwake(body);
	}
}


void setIterations(World *s, int32_t iterations)
{
	s->iterations = (iterations > 1) ? iterations : 1;
}


void setEnableFriction(World *s, int enabled)
{
	s->enable_friction = enabled;
}


Vector3 getGravity(const World *s) { return s->gravity; }
void setGravity(World *s, Vector3 g) { s->gravity = g; }
void setWind (World *s, Vector3 w) { s->wind = w; }

void addBuoyancy(World *s, const buoyancy::Volume *volume)
{
	if (s->buoyancy_count >= World::MAX_BUOYANCY_VOLUMES) return;
	s->buoyancy[s->buoyancy_count++] = volume;
}


void setContactListener(World *s, World::ContactListener *listener)
{
	s->contact_listener = listener;
	s->contact_manager.contact_listener = listener;
}


typedef struct QueryAABB_ctx {
	const BroadPhase *broadphase;
	World::QueryCallback cb;
	void *cb_user_data;
	AABB aabb;
} QueryAABB_ctx;

static int queryAABB_cb(void *ctx_v, int32_t id)
{
	QueryAABB_ctx *ctx = (QueryAABB_ctx *)ctx_v;
	Shape *shape = (Shape *)collision::dynamicAABBTree::getUserData(&ctx->broadphase->tree, id);
	AABB bounds;
	shape::computeAABB(shape, &bounds);
	if (aabb::overlaps(&ctx->aabb, &bounds)) {
		return ctx->cb(ctx->cb_user_data, shape);
	}
	return 1;
}

void queryAABB(const World *s, void *cb_user_data, World::QueryCallback cb, AABB aabb)
{
	QueryAABB_ctx ctx;
	ctx.broadphase = &s->contact_manager.broadphase;
	ctx.cb = cb;
	ctx.cb_user_data = cb_user_data;
	ctx.aabb = aabb;
	collision::dynamicAABBTree::queryAABB(&s->contact_manager.broadphase.tree, &ctx, queryAABB_cb, aabb);
}


typedef struct QueryPoint_ctx {
	const BroadPhase *broadphase;
	World::QueryCallback cb;
	void *cb_user_data;
	Vector3 point;
} QueryPoint_ctx;

static int queryPoint_cb(void *ctx_v, int32_t id)
{
	QueryPoint_ctx *ctx = (QueryPoint_ctx *)ctx_v;
	Shape *shape = (Shape *)collision::dynamicAABBTree::getUserData(&ctx->broadphase->tree, id);
	if (shape::testPoint(shape, &ctx->point)) {
		ctx->cb(ctx->cb_user_data, shape);
	}
	return 1;
}

void queryPoint(const World *s, void *cb_user_data, World::QueryCallback cb, Vector3 point)
{
	QueryPoint_ctx ctx;
	ctx.broadphase = &s->contact_manager.broadphase;
	ctx.cb = cb;
	ctx.cb_user_data = cb_user_data;
	ctx.point = point;

	const float k_fattener = 0.5f;
	Vector3 v = { k_fattener, k_fattener, k_fattener };
	AABB aabb;
	aabb.min = vector3::difference(&point, &v);
	aabb.max = vector3::sum(&point, &v);
	collision::dynamicAABBTree::queryAABB(&s->contact_manager.broadphase.tree, &ctx, queryPoint_cb, aabb);
}


typedef struct QueryRaycast_ctx {
	const BroadPhase *broadphase;
	World::QueryCallback cb;
	void *cb_user_data;
	RaycastData *raycast;
} QueryRaycast_ctx;

static int queryRaycast_cb(void *ctx_v, int32_t id)
{
	QueryRaycast_ctx *ctx = (QueryRaycast_ctx *)ctx_v;
	Shape *shape = (Shape *)collision::dynamicAABBTree::getUserData(&ctx->broadphase->tree, id);
	if (shape::raycast(shape, ctx->raycast)) {
		return ctx->cb(ctx->cb_user_data, shape);
	}
	return 1;
}

void rayCast(const World *s, void *cb_user_data, World::QueryCallback cb, RaycastData *raycast)
{
	QueryRaycast_ctx ctx;
	ctx.broadphase = &s->contact_manager.broadphase;
	ctx.cb = cb;
	ctx.cb_user_data = cb_user_data;
	ctx.raycast = raycast;
	collision::dynamicAABBTree::queryRay(&s->contact_manager.broadphase.tree, &ctx, queryRaycast_cb, raycast);
}

}
}
}
