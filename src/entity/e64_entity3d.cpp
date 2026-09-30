#include <assert.h>
#include <stdlib.h>
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>

#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d_animation.h"
#include "viewport/e64_viewport.h"
#include "math/e64_math.h"
#include "physics3d/e64_physics_world.h"

namespace e64 {

namespace entity3d {

void init(Entity3D *entity, const Def *def)
{
	*entity = (Entity3D){};
	render::transform::init(&entity->transform);
	entity->transform.position = def->position;
	entity->transform.rotation = def->rotation;
	entity->transform.scale = def->scale;
	entity->cull = def->cull;

}

Entity3D *create(const Def *def)
{
	Entity3D *entity = (Entity3D *)malloc(sizeof(Entity3D));
	assert(entity);
	init(entity, def);

	/* The sounds open with the entity. A looping one is the object's own
	   noise: it starts here, from where the object stands, and stops when
	   the entity goes. */
	if (def->sound_count) {
		entity->sound = (Sound *)malloc(def->sound_count * sizeof(Sound));
		assert(entity->sound);
		for (int i = 0; i < def->sound_count; i++) {
			entity->sound[i] = sound::load(def->sound[i]);
			if (def->sound[i]->loop)
				sound::play(&entity->sound[i], &entity->transform.position, 1.0f, 0.0f);
		}
		entity->sound_count = def->sound_count;
	}

	/* A sound placed alone has nothing to draw. */
	if (def->mesh)
		entity->mesh = mesh::create(def->mesh);

	return entity;
}

void setPartVisible(Entity3D *entity, const char *name, bool visible)
{
	if (!entity->mesh) return;

	uint8_t part = mesh::part::find(entity->mesh, name);
	if (!part) return;

	mesh::part::setVisible(entity->mesh, part, visible);
}

void setPartOffset(Entity3D *entity, const char *name, const Render::Transform *offset)
{
	if (!entity->mesh) return;

	uint8_t part = mesh::part::find(entity->mesh, name);
	if (!part) return;

	mesh::part::setOffset(entity->mesh, part, offset);
}

void destroy(Entity3D *entity)
{
	if (entity->mesh)
		mesh::destroy(entity->mesh);

	for (int i = 0; i < entity->sound_count; i++)
		sound::unload(&entity->sound[i]);
	free(entity->sound);

	free(entity);
}

void playSound(const Entity3D *entity, uint8_t trigger, const Vector3 *position, float volume_scale)
{
	/* The candidates are the entity's sounds tagged with this trigger; the
	   loops are already playing and never fire. */
	uint8_t candidate[Sound::MAX_EMITTERS];
	uint8_t count = 0;

	for (uint8_t i = 0; i < entity->sound_count && count < Sound::MAX_EMITTERS; i++) {
		const Sound::Def *def = entity->sound[i].def;
		if (def->loop || def->trigger != trigger) continue;
		candidate[count++] = i;
	}
	if (count == 0) return;

	Vector3 here;
	if (position == NULL) {
		if (entity->body) here = entity->body->tx.position;
		else here = entity->transform.position;
		position = &here;
	}

	sound::play(&entity->sound[candidate[rand() % count]], position, volume_scale, 0.0f);
}

void setTransform(Entity3D *entity, const character3d::KinematicBody *body)
{
	entity->transform.position = body->position;
	entity->transform.rotation = body->rotation;
}

void setMatrix(Entity3D *entity, uint8_t fb_index)
{
	mesh::setMatrix(entity->mesh, &entity->transform, fb_index);
}

/* For entities the solver moves: their placement lives in the body, not in the
   render transform, and a tumbling body needs its quaternion rather than the
   euler angles the transform carries. No-op for anything else. */
void setMatrixFromBody(Entity3D *entity, uint8_t fb_index)
{
	if (entity->body == NULL || !(entity->body->flags & RigidBody::BODY_FLAG_DYNAMIC)) return;

	mesh::setMatrixFromBody(entity->mesh, &entity->body->tx.position, &entity->body->q,
	                        &entity->transform.scale, fb_index);
}


/* World transform of a static collider: entity position in metres plus the
   entity rotation built with the same euler function the renderer uses, so
   collision and visuals always match. */
Transform colliderTransform(const Def *def)
{
	T3DMat4 mat;
	t3d_mat4_from_srt_euler(&mat,
		(float[3]){1.0f, 1.0f, 1.0f},
		(float[3]){deg_to_rad(def->rotation.x), deg_to_rad(def->rotation.y), deg_to_rad(def->rotation.z)},
		(float[3]){0.0f, 0.0f, 0.0f});

	return (Transform){
		.position = def->position,
		.rotation = {
			.ex = { mat.m[0][0], mat.m[0][1], mat.m[0][2] },
			.ey = { mat.m[1][0], mat.m[1][1], mat.m[1][2] },
			.ez = { mat.m[2][0], mat.m[2][1], mat.m[2][2] },
		},
	};
}

/* Copies the body-def override bits on top of the freshly-initialised body
   (position, orientation) and then attaches the entity's shape to it. */
RigidBody *attachPhysics(Entity3D *entity, const Def *def, physics::World *world)
{
	RigidBody::Def body_def;
	rigidBody::def::init(&body_def);

	if (def->body) {
		/* The user-supplied RigidBody::Def describes only the body properties,
		   not the world position. Position/rotation come from the entity. */
		body_def.body_type = def->body->body_type;
		body_def.gravity_scale = def->body->gravity_scale;
		body_def.layers = def->body->layers ? def->body->layers : 1;
		body_def.linear_damping = def->body->linear_damping;
		body_def.angular_damping = def->body->angular_damping;
		body_def.allow_sleep = def->body->allow_sleep;
		body_def.awake = def->body->awake;
		body_def.active = def->body->active;
		body_def.lock_axis_x = def->body->lock_axis_x;
		body_def.lock_axis_y = def->body->lock_axis_y;
		body_def.lock_axis_z = def->body->lock_axis_z;
	}

	/* Position in metres and rotation through the renderer's euler convention,
	   both from the collider transform: a rotated entity collides the way it
	   renders. */
	Transform collider = colliderTransform(def);
	body_def.position = collider.position;

	Quaternion rotation = quaternion::fromMatrix3(&collider.rotation);
	quaternion::toAxisAngle(&rotation, &body_def.axis, &body_def.angle);

	/* Identity degenerates to a zero axis; any axis stands for no rotation. */
	if (vector3::squaredMagnitude(&body_def.axis) == 0.0f)
		body_def.axis = (Vector3){ 0.0f, 0.0f, 1.0f };

	RigidBody *body = physics::world::createBody(world, &body_def);
	body->owner = entity;
	entity->body = body;

	if (def->collider) {
		for (uint8_t i = 0; i < def->collider->count; i++)
			rigidBody::addShape(body, &def->collider->shape[i], def->scale);
	}

	return body;
}

}

}
