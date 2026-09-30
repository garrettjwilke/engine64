#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <libdragon.h>

#include "physics3d/collision/e64_mesh_collider.h"

namespace e64 {
namespace meshCollider {

typedef struct RawCollisionHeader {
	uint32_t tri_count;
	uint32_t vert_count;
	float coll_scale;
	uint32_t vertex_ptr; /* reserved, written as 0 by the importer */
	uint32_t normals_ptr; /* reserved, written as 0 by the importer */
	uint32_t bvh_ptr; /* reserved, written as 0 by the importer */
} RawCollisionHeader;


static char *alignPtr(char *ptr, size_t alignment)
{
	return (char *)(((uintptr_t)ptr + alignment - 1) & ~(alignment - 1));
}


MeshCollider *load(const char *path)
{
	int size = 0;
	void *buffer = asset_load(path, &size);
	assert(buffer && size > (int)sizeof(RawCollisionHeader));

	const RawCollisionHeader *header = (const RawCollisionHeader *)buffer;
	assert(header->tri_count > 0 && header->tri_count <= 0xFFFFu);
	assert(header->vert_count > 0 && header->vert_count <= 0xFFFFu);

	char *data = (char *)(header + 1);

	const uint16_t *indices = (const uint16_t *)data;
	data += header->tri_count * 3 * sizeof(uint16_t);
	data = alignPtr(data, 4);

	const int16_t *packed_normals = (const int16_t *)data;
	data += header->tri_count * 3 * sizeof(int16_t);
	data = alignPtr(data, 4);

	const Vector3 *vertices = (const Vector3 *)data;
	data += header->vert_count * sizeof(Vector3);
	data = alignPtr(data, 4);

	const uint8_t *active_edges = (const uint8_t *)data;

	/* Trips on assets built before the active-edge block: re-import them. */
	assert((char *)active_edges + header->tri_count <= (char *)buffer + size);

	MeshCollider *mesh = (MeshCollider *)malloc(sizeof(MeshCollider));
	mesh->triangle_count = (uint16_t)header->tri_count;
	mesh->vertex_count = (uint16_t)header->vert_count;
	mesh->indices = indices;
	mesh->packed_normals = packed_normals;
	mesh->vertices = vertices;
	mesh->active_edges = active_edges;
	mesh->asset = buffer;

	collision::dynamicAABBTree::init(&mesh->tree);

	for (int32_t t = 0; t < mesh->triangle_count; t++) {
		Triangle triangle;
		getTriangle(mesh, t, &triangle);
		const Vector3 *v = triangle.vertices;

		AABB aabb;
		aabb.min = v[0];
		aabb.max = v[0];
		for (int i = 1; i < 3; i++) {
			if (v[i].x < aabb.min.x) aabb.min.x = v[i].x;
			if (v[i].y < aabb.min.y) aabb.min.y = v[i].y;
			if (v[i].z < aabb.min.z) aabb.min.z = v[i].z;
			if (v[i].x > aabb.max.x) aabb.max.x = v[i].x;
			if (v[i].y > aabb.max.y) aabb.max.y = v[i].y;
			if (v[i].z > aabb.max.z) aabb.max.z = v[i].z;
		}

		collision::dynamicAABBTree::insert(&mesh->tree, aabb, (void *)(intptr_t)t);
	}

	return mesh;
}

void destroy(MeshCollider *mesh)
{
	if (!mesh) return;
	collision::dynamicAABBTree::shutdown(&mesh->tree);
	free(mesh->asset);
	free(mesh);
}

void getTriangle(const MeshCollider *mesh, int32_t index, Triangle *out)
{
	const float scale = 1.0f / 32767.0f;
	const uint16_t *idx = &mesh->indices[index * 3];
	const int16_t *n = &mesh->packed_normals[index * 3];

	out->vertices[0] = mesh->vertices[idx[0]];
	out->vertices[1] = mesh->vertices[idx[1]];
	out->vertices[2] = mesh->vertices[idx[2]];
	out->normal = (Vector3){ n[0] * scale, n[1] * scale, n[2] * scale };
	out->active_edges = mesh->active_edges[index];
}

void queryAABB(const MeshCollider *mesh, void *cb, DynamicAABBTree::QueryCallback callback, AABB aabb)
{
	collision::dynamicAABBTree::queryAABB(&mesh->tree, cb, callback, aabb);
}


typedef struct MeshRaycast {
	const MeshCollider *mesh;
	RaycastData *ray;
	int hit;
} MeshRaycast;

/* Every leaf the ray crosses is a triangle. Shortening the ray on each hit
   leaves the closest one and lets the tree prune what is behind it. */
static int raycastLeaf(void *cb, int32_t id)
{
	MeshRaycast *query = (MeshRaycast *)cb;

	Triangle triangle;
	getTriangle(query->mesh, (int32_t)(intptr_t)collision::dynamicAABBTree::getUserData(&query->mesh->tree, id), &triangle);

	if (triangle::raycast(&triangle, query->ray)) {
		query->ray->t = query->ray->toi;
		query->hit = 1;
	}
	return 1;
}

/* The tree lives in mesh-local space: the ray goes in through the inverse of
   the mesh's world transform and the hit normal is rotated back out. The
   time of impact is a distance along the ray, so it survives as is. */
int raycast(const MeshCollider *mesh, const Transform *world, RaycastData *raycast)
{
	RaycastData local = *raycast;
	local.start = transform::mulVectorTransposed(world, &raycast->start);
	local.dir = matrix3::transformVectorTransposed(&world->rotation, &raycast->dir);

	MeshRaycast query = { .mesh = mesh, .ray = &local, .hit = 0 };
	collision::dynamicAABBTree::queryRay(&mesh->tree, &query, raycastLeaf, &local);

	if (!query.hit) return 0;

	raycast->toi = local.toi;
	raycast->normal = matrix3::transformVector(&world->rotation, &local.normal);
	return 1;
}

}
}
