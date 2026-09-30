/*
	Ported from qu3e q3BroadPhase.h — altered source, not the original software.

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
	Pair-finding broadphase over a dynamic AABB tree.
*/
#ifndef ENGINE64_BROAD_PHASE_H
#define ENGINE64_BROAD_PHASE_H

#include <stdint.h>

#include "physics3d/geometry/e64_aabb.h"
#include "physics3d/collision/e64_contact.h"
#include "physics3d/collision/e64_dynamic_aabb_tree.h"

namespace e64 {

class BroadPhase {
public:

	Contact::Manager *manager;

	Contact::Pair *pair_buffer;
	int32_t pair_count;
	int32_t pair_capacity;

	int32_t *move_buffer;
	int32_t move_count;
	int32_t move_capacity;

	DynamicAABBTree tree;
	int32_t current_index;
};


namespace collision {

namespace broadPhase {

void init (BroadPhase *bp, Contact::Manager *manager);
void shutdown(BroadPhase *bp);

void insertShape(BroadPhase *bp, physics::Shape *shape, AABB aabb);
void removeShape(BroadPhase *bp, const physics::Shape *shape);
void updatePairs(BroadPhase *bp);
void update (BroadPhase *bp, int32_t id, AABB aabb);
int testOverlap(const BroadPhase *bp, int32_t A, int32_t B);

int treeCallback(void *bp_void, int32_t index);

}

}

}

#endif
