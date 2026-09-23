/*
	Static triangle mesh collision data.

	Loads the binary written by tools/collision_importer and builds a
	dynamic AABB tree with one leaf per triangle, in mesh-local space.
	File layout based on pyrite64's mesh collider (Max Bebök, Kevin Reier, MIT).
*/
#ifndef ENGINE64_MESH_COLLIDER_H
#define ENGINE64_MESH_COLLIDER_H

#include <stdint.h>

#include "math/e64_vector3.h"
#include "math/e64_transform.h"
#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_triangle.h"
#include "physics/collision/e64_dynamic_aabb_tree.h"

namespace e64 {

class MeshCollider {
public:

	/* Authoring side, matching the other shape defs. There is no density: a
	   mesh only ever hangs off a static body, so it has no mass to compute. */
	struct Def {
		Transform tx;
		const char *path;
		float friction;
		float restitution;
	};


	uint16_t triangle_count;
	uint16_t vertex_count;

	const uint16_t *indices; /* 3 per triangle */
	const int16_t *packed_normals; /* 3 per triangle, scaled by 32767 */
	const Vector3 *vertices;
	const uint8_t *active_edges; /* 1 per triangle, see Triangle */

	void *asset; /* buffer from asset_load, owns the data above */
	DynamicAABBTree tree; /* leaf per triangle, user_data = triangle index */
};


namespace meshCollider {

MeshCollider *load (const char *path);
void destroy(MeshCollider *mesh);

void getTriangle(const MeshCollider *mesh, int32_t index, Triangle *out);

void queryAABB(const MeshCollider *mesh, void *cb, DynamicAABBTree::QueryCallback callback, AABB aabb);

int raycast (const MeshCollider *mesh, const Transform *world, RaycastData *raycast);

}

}

#endif
