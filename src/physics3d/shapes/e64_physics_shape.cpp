/*
	Narrowphase/body dispatchers. Each function builds the world transform
	(body_tx · shape->local) and routes to the concrete shape implementation
	based on shape->type.
*/
#include <stddef.h>

#include "physics3d/shapes/e64_physics_shape.h"
#include "physics3d/collision/e64_mesh_collider.h"

namespace e64 {
namespace physics {
namespace shape {

namespace def {

Transform localTransform(const Transform *tx)
{
	Transform local = *tx;

	if (vector3::squaredMagnitude(&local.rotation.ex) == 0.0f &&
	    vector3::squaredMagnitude(&local.rotation.ey) == 0.0f &&
	    vector3::squaredMagnitude(&local.rotation.ez) == 0.0f)
		local.rotation = matrix3::identity();

	return local;
}

}


bool fromDef(Shape *shape, const Shape::Def *def, Vector3 scale)
{
	const Transform *tx = NULL;

	*shape = (Shape){ .type = def->type };

	switch (def->type) {
		case Shape::SHAPE_BOX:
			shape->box = (Box){ .e = {
				def->box.e.x * scale.x,
				def->box.e.y * scale.y,
				def->box.e.z * scale.z,
			}};
			shape->friction = def->box.friction;
			shape->restitution = def->box.restitution;
			shape->density = def->box.density;
			shape->sensor = def->box.sensor;
			tx = &def->box.tx;
			break;

		/* A sphere cannot be squashed, so a non-uniform scale takes X. */
		case Shape::SHAPE_SPHERE:
			shape->sphere = (Sphere){ .radius = def->sphere.radius * scale.x };
			shape->friction = def->sphere.friction;
			shape->restitution = def->sphere.restitution;
			shape->density = def->sphere.density;
			shape->sensor = def->sphere.sensor;
			tx = &def->sphere.tx;
			break;

		/* The capsule runs along local Z: radius takes X, height takes Z. */
		case Shape::SHAPE_CAPSULE:
			shape->capsule = (Capsule){
				.radius = def->capsule.radius * scale.x,
				.half_height = def->capsule.half_height * scale.z,
			};
			shape->friction = def->capsule.friction;
			shape->restitution = def->capsule.restitution;
			shape->density = def->capsule.density;
			shape->sensor = def->capsule.sensor;
			tx = &def->capsule.tx;
			break;

		/* The mesh comes from an asset, so the def names a file instead of
		   describing a size. Scale is ignored: the triangles are already at
		   the size they were authored. */
		case Shape::SHAPE_MESH:
			shape->mesh = meshCollider::load(def->mesh.path);
			if (shape->mesh == NULL) return false;

			shape->friction = def->mesh.friction;
			shape->restitution = def->mesh.restitution;
			shape->density = 0.0f;
			tx = &def->mesh.tx;
			break;
	}

	shape->local = def::localTransform(tx);
	shape->local.position = (Vector3){
		shape->local.position.x * scale.x,
		shape->local.position.y * scale.y,
		shape->local.position.z * scale.z,
	};

	return true;
}


/* Counterpart of fromDef: only the mesh case allocates, and the asset it
   loaded dies with the shape that asked for it. */
void release(Shape *shape)
{
	if (shape->type != Shape::SHAPE_MESH || shape->mesh == NULL) return;

	meshCollider::destroy(shape->mesh);
	shape->mesh = NULL;
}


int testPoint(const Shape *shape, const Vector3 *p)
{
	const Transform *world = &shape->world;

	switch (shape->type) {
		case Shape::SHAPE_BOX: return box::testPoint (&shape->box, world, p);
		case Shape::SHAPE_SPHERE: return sphere::testPoint (&shape->sphere, world, p);
		case Shape::SHAPE_CAPSULE: return capsule::testPoint(&shape->capsule, world, p);
		case Shape::SHAPE_MESH: break; /* static-only, never on a rigid body */
	}
	return 0;
}


int raycast(const Shape *shape, RaycastData *raycast)
{
	const Transform *world = &shape->world;

	switch (shape->type) {
		case Shape::SHAPE_BOX: return box::raycast (&shape->box, world, raycast);
		case Shape::SHAPE_SPHERE: return sphere::raycast (&shape->sphere, world, raycast);
		case Shape::SHAPE_CAPSULE: return capsule::raycast(&shape->capsule, world, raycast);
		case Shape::SHAPE_MESH: return meshCollider::raycast(shape->mesh, world, raycast);
	}
	return 0;
}


void computeAABB(const Shape *shape, AABB *aabb)
{
	const Transform *world = &shape->world;

	switch (shape->type) {
		case Shape::SHAPE_BOX: box::computeAABB (&shape->box, world, aabb); break;
		case Shape::SHAPE_SPHERE: sphere::computeAABB (&shape->sphere, world, aabb); break;
		case Shape::SHAPE_CAPSULE: capsule::computeAABB(&shape->capsule, world, aabb); break;

		/* The tree's root already bounds every triangle, in mesh-local space.
		   Taken as a box centred on that bound and carried by the mesh's
		   world transform, it is bounded again in the world, rotation
		   included. */
		case Shape::SHAPE_MESH: {
			*aabb = (AABB){ vector3::zero(), vector3::zero() };
			if (shape->mesh == NULL || shape->mesh->tree.root < 0) break;

			AABB root = collision::dynamicAABBTree::getFatAABB(&shape->mesh->tree, shape->mesh->tree.root);
			Vector3 center = vector3::sum(&root.min, &root.max);
			center = vector3::scaled(&center, 0.5f);

			Box bound;
			bound.e = vector3::difference(&root.max, &center);

			Transform bound_tx = *world;
			bound_tx.position = transform::mulVector(world, &center);

			box::computeAABB(&bound, &bound_tx, aabb);
			break;
		}
	}
}


void computeMass(const Shape *shape, MassData *md)
{
	switch (shape->type) {
		case Shape::SHAPE_BOX: box::computeMass (&shape->box, &shape->local, shape->density, md); break;
		case Shape::SHAPE_SPHERE: sphere::computeMass (&shape->sphere, &shape->local, shape->density, md); break;
		case Shape::SHAPE_CAPSULE: capsule::computeMass(&shape->capsule, &shape->local, shape->density, md); break;
		case Shape::SHAPE_MESH: break; /* static-only, never on a rigid body */
	}
}

}
}
}
