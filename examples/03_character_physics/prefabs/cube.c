/*
	A one metre cube. Physics units are metres, so the model that measures 100
	units on screen is 1.0 here, and the box is half of that on each side.
*/
#include "prefab/e64_prefab3d.h"


static const PhysicsShapeDef cube_shapes[] = {
	{ .type = SHAPE_BOX, .box = {
		.e           = { 0.5f, 0.5f, 0.5f },
		.friction    = 0.8f,
		.restitution = 0.1f,
	}},
};

static const Entity3DColliderDef cube_collider = { cube_shapes, 1 };

const Prefab3D cube = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/cube.t3dm",
	.collider = &cube_collider,
};
