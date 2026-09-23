/*
	Ported from qu3e q3DynamicAABBTree.h — altered source, not the original software.

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
	Bounding-volume hierarchy for broadphase queries. The C++ template
	Query<T> becomes a function-pointer callback.
*/
#ifndef ENGINE64_DYNAMIC_AABB_TREE_H
#define ENGINE64_DYNAMIC_AABB_TREE_H

#include <stdint.h>

#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_raycast.h"

namespace e64 {

class DynamicAABBTree {
public:

	static constexpr int32_t NULL_NODE = -1;


	struct Node {
		AABB aabb;
		int32_t parent_or_next; /* parent (active) / next pointer (free list) */
		int32_t left;
		int32_t right;
		void *user_data;
		int32_t height; /* leaf = 0, free = -1 */
	};


	typedef int (*QueryCallback)(void *cb, int32_t id);


	int32_t root;
	Node *nodes;
	int32_t count;
	int32_t capacity;
	int32_t free_list;
};


namespace collision {

namespace dynamicAABBTree {

static inline int isLeaf(const DynamicAABBTree::Node *n) {
	return n->right == DynamicAABBTree::NULL_NODE;
}


void init (DynamicAABBTree *t);
void shutdown(DynamicAABBTree *t);

int32_t insert(DynamicAABBTree *t, AABB aabb, void *user_data);
void remove(DynamicAABBTree *t, int32_t id);
int update(DynamicAABBTree *t, int32_t id, AABB bounds);

void *getUserData(const DynamicAABBTree *t, int32_t id);
AABB getFatAABB (const DynamicAABBTree *t, int32_t id);

void queryAABB(const DynamicAABBTree *t, void *cb, DynamicAABBTree::QueryCallback callback, AABB bounds);
void queryRay (const DynamicAABBTree *t, void *cb, DynamicAABBTree::QueryCallback callback, RaycastData *raycast);

}

}

}

#endif
