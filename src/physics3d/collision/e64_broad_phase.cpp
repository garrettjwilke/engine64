/*
	Ported from qu3e q3BroadPhase.cpp — altered source, not the original software.

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
	Pair buffer on top of the dynamic AABB tree.
*/
#include <stdlib.h>
#include <string.h>

#include "physics3d/collision/e64_broad_phase.h"
#include "physics3d/collision/e64_contact_manager.h"
#include "physics3d/shapes/e64_physics_shape.h"
#include "memory/e64_memory.h"

namespace e64 {
namespace collision {
namespace broadPhase {

static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }


static void bufferMove(BroadPhase *bp, int32_t id)
{
	if (bp->move_count == bp->move_capacity) {
		int32_t *old = bp->move_buffer;
		bp->move_capacity *= 2;
		bp->move_buffer = (int32_t *)memory::alloc((int32_t)(bp->move_capacity * sizeof(int32_t)));
		memcpy(bp->move_buffer, old, (size_t)bp->move_count * sizeof(int32_t));
		memory::free(old);
	}
	bp->move_buffer[bp->move_count++] = id;
}


int treeCallback(void *bp_void, int32_t index)
{
	BroadPhase *bp = (BroadPhase *)bp_void;

	if (index == bp->current_index) return 1;

	if (bp->pair_count == bp->pair_capacity) {
		Contact::Pair *old = bp->pair_buffer;
		bp->pair_capacity *= 2;
		bp->pair_buffer = (Contact::Pair *)memory::alloc((int32_t)(bp->pair_capacity * sizeof(Contact::Pair)));
		memcpy(bp->pair_buffer, old, (size_t)bp->pair_count * sizeof(Contact::Pair));
		memory::free(old);
	}

	int32_t iA = i32_min(index, bp->current_index);
	int32_t iB = i32_max(index, bp->current_index);

	bp->pair_buffer[bp->pair_count].A = iA;
	bp->pair_buffer[bp->pair_count].B = iB;
	++bp->pair_count;

	return 1;
}


void init(BroadPhase *bp, Contact::Manager *manager)
{
	bp->manager = manager;

	bp->pair_count = 0;
	bp->pair_capacity = 64;
	bp->pair_buffer = (Contact::Pair *)memory::alloc((int32_t)(bp->pair_capacity * sizeof(Contact::Pair)));

	bp->move_count = 0;
	bp->move_capacity = 64;
	bp->move_buffer = (int32_t *)memory::alloc((int32_t)(bp->move_capacity * sizeof(int32_t)));

	dynamicAABBTree::init(&bp->tree);
	bp->current_index = 0;
}


void shutdown(BroadPhase *bp)
{
	memory::free(bp->move_buffer);
	memory::free(bp->pair_buffer);
	dynamicAABBTree::shutdown(&bp->tree);
	bp->move_buffer = NULL;
	bp->pair_buffer = NULL;
}


void insertShape(BroadPhase *bp, physics::Shape *shape, AABB aabb)
{
	int32_t id = dynamicAABBTree::insert(&bp->tree, aabb, shape);
	shape->broadphase_index = id;
	bufferMove(bp, id);
}


void removeShape(BroadPhase *bp, const physics::Shape *shape)
{
	dynamicAABBTree::remove(&bp->tree, shape->broadphase_index);
}


static int pair_cmp(const void *a, const void *b)
{
	const Contact::Pair *lhs = (const Contact::Pair *)a;
	const Contact::Pair *rhs = (const Contact::Pair *)b;
	if (lhs->A < rhs->A) return -1;
	if (lhs->A > rhs->A) return 1;
	if (lhs->B < rhs->B) return -1;
	if (lhs->B > rhs->B) return 1;
	return 0;
}


void updatePairs(BroadPhase *bp)
{
	bp->pair_count = 0;

	for (int32_t i = 0; i < bp->move_count; ++i) {
		bp->current_index = bp->move_buffer[i];
		AABB aabb = dynamicAABBTree::getFatAABB(&bp->tree, bp->current_index);
		dynamicAABBTree::queryAABB(&bp->tree, bp, treeCallback, aabb);
	}

	bp->move_count = 0;

	qsort(bp->pair_buffer, (size_t)bp->pair_count, sizeof(Contact::Pair), pair_cmp);

	int32_t i = 0;
	while (i < bp->pair_count) {
		Contact::Pair *pair = bp->pair_buffer + i;
		physics::Shape *A = (physics::Shape *)dynamicAABBTree::getUserData(&bp->tree, pair->A);
		physics::Shape *B = (physics::Shape *)dynamicAABBTree::getUserData(&bp->tree, pair->B);
		contact::manager::addContact(bp->manager, A, B);

		++i;

		while (i < bp->pair_count) {
			Contact::Pair *dup = bp->pair_buffer + i;
			if (pair->A != dup->A || pair->B != dup->B) break;
			++i;
		}
	}
}


void update(BroadPhase *bp, int32_t id, AABB aabb)
{
	if (dynamicAABBTree::update(&bp->tree, id, aabb)) bufferMove(bp, id);
}


int testOverlap(const BroadPhase *bp, int32_t A, int32_t B)
{
	AABB a = dynamicAABBTree::getFatAABB(&bp->tree, A);
	AABB b = dynamicAABBTree::getFatAABB(&bp->tree, B);
	return aabb::overlaps(&a, &b);
}

}
}
}
