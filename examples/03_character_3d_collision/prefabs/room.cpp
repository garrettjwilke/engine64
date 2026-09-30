/*
	The room: floor, walls, the raised platform and the mound. Its collision is
	the triangle mesh the importer builds from the same .glb, so what you see
	is what the character walks on.
*/
#include "prefab/e64_prefab3d.h"


static const e64::physics::Shape::Def room_shapes[] = {
	{ .type = e64::physics::Shape::SHAPE_MESH, .mesh = {
		.path = "rom:/collision/room.collision",
		.friction = 0.9f,
		.restitution = 0.1f,
	}},
};

static const e64::collider::Def room_collider = { room_shapes, 1 };

/* No body: a prop without one is static, which is what a room is. */
extern const e64::Prefab3D room = {

	.type = e64::prefab3d::PREFAB3D_PROP,
	.mesh = { .model = "rom:/models/room.t3dm" },
	.collider = &room_collider,
};
