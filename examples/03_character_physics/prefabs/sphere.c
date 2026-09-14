/*
	A one metre ball. Rounded, so the character slides off it instead of
	standing on top: the counterpart of the cube's flat faces.
*/
#include "prefab/e64_prefab3d.h"


static const PhysicsShapeDef sphere_shapes[] = {
	{ .type = SHAPE_SPHERE, .sphere = {
		.radius      = 0.5f,
		.friction    = 0.8f,
		.restitution = 0.1f,
	}},
};

static const Entity3DColliderDef sphere_collider = { sphere_shapes, 1 };

const Prefab3D sphere = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/sphere.t3dm",
	.collider = &sphere_collider,
};
