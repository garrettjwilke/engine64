/*
	Triangle primitive for static collision meshes. Vertices gathered by index
	from a CollisionMesh, normal precomputed.
*/
#ifndef ENGINE64_TRIANGLE_H
#define ENGINE64_TRIANGLE_H

#include <stdint.h>

#include "math/e64_vector3.h"
#include "physics3d/geometry/e64_raycast.h"

namespace e64 {

typedef struct Triangle {
	Vector3 vertices[3];
	Vector3 normal;
	uint8_t active_edges; /* bit 0 = v0v1, bit 1 = v1v2, bit 2 = v2v0; baked by the importer */
} Triangle;


namespace triangle {

int raycast(const Triangle *t, RaycastData *raycast);

/* The point of the triangle nearest to a point in space: inside the face it
   is the projection, outside it is on the edge or the vertex the point falls
   past. */
Vector3 closestToPoint(const Triangle *t, const Vector3 *point);

}

}

#endif
