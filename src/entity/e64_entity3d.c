#include <assert.h>
#include <stdlib.h>
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>

#include "entity/e64_entity3d.h"
#include "resource/e64_resource.h"
#include "shaders/e64_mesh_deform.h"
#include "character3d/e64_character3d_animation.h"
#include "viewport/e64_viewport.h"
#include "physics/math/e64_math_common.h"
#include "physics/world/e64_physics_world.h"


void entity3d_init(Entity3D *entity, const Entity3DDef *def)
{
	*entity = (Entity3D){0};
	renderTransform_init(&entity->transform);
	entity->transform.position = def->position;
	entity->transform.rotation = def->rotation;
	entity->transform.scale    = def->scale;
	entity->cull               = def->cull;

}

Entity3D *entity3d_create(const Entity3DDef *def)
{
	Entity3D *entity = malloc(sizeof(Entity3D));
	assert(entity);
	entity3d_init(entity, def);

	/* The sounds open with the entity. A looping one is the object's own
	   noise: it starts here, from where the object stands, and stops when
	   the entity goes. */
	if (def->sound_count) {
		entity->sound = malloc(def->sound_count * sizeof(Sound));
		assert(entity->sound);
		for (int i = 0; i < def->sound_count; i++) {
			entity->sound[i] = sound_load(def->sound[i]);
			if (def->sound[i]->loop)
				sound_play(&entity->sound[i], &entity->transform.position, 1.0f, 0.0f);
		}
		entity->sound_count = def->sound_count;
	}

	/* A sound placed alone has nothing to draw. */
	if (!def->model_path) return entity;

	entity->mesh = malloc(sizeof(Mesh));
	assert(entity->mesh);
	entity->mesh->model = resource_load(def->model_path, RESOURCE_MODEL, NULL);
	entity->mesh->matrix_buffer = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);
	assert(entity->mesh->matrix_buffer);
	t3d_mat4fp_identity(entity->mesh->matrix_buffer);

	entity->mesh->skeleton  = NULL;
	entity->mesh->deform    = NULL;
	entity->mesh->draw_conf = NULL;
	mesh_initBounds(entity->mesh);

	if (def->character) {
		entity->mesh->dl = NULL;   /* character3d_create builds the skinned parts */
		entity->mesh->dl_count = 0;
		entity->mesh->visible  = 0;
	} else if (def->cloth) {
		entity->mesh->dl = malloc(sizeof(rspq_block_t *));
		assert(entity->mesh->dl);
		rspq_block_begin();
		t3d_model_draw(entity->mesh->model);
		entity->mesh->dl[0]    = rspq_block_end();
		entity->mesh->dl_count = 1;
		entity->mesh->visible  = 1;
	} else if (def->part_count) {
		/* The model is static, so its parts record with no skeleton segment,
		   and every one of them starts on screen: a prop shows whole until the
		   game decides to hide something. */
		mesh_recordParts(entity->mesh, def->part, def->part_count, NULL);
		entity->mesh->visible = (uint8_t)((1u << entity->mesh->dl_count) - 1);

		/* A part declared away from where it was modelled gets its offset here,
		   so it is already in place the first time it is drawn. */
		for (int i = 0; def->part_position && i < def->part_count; i++) {
			const Vector3 *position = &def->part_position[i];
			if (position->x == 0.0f && position->y == 0.0f && position->z == 0.0f)
				continue;

			RenderTransform offset = { .position = *position, .scale = { 1.0f, 1.0f, 1.0f } };
			mesh_setPartOffset(entity->mesh, 1 + i, &offset);
		}
	} else {
		mesh_recordObjects(entity->mesh);
	}

	return entity;
}

void entity3d_setPartVisible(Entity3D *entity, const char *name, bool visible)
{
	if (!entity->mesh) return;

	uint8_t part = mesh_findPart(entity->mesh, name);
	if (!part) return;

	mesh_setPartVisible(entity->mesh, part, visible);
}

void entity3d_setPartOffset(Entity3D *entity, const char *name, const RenderTransform *offset)
{
	if (!entity->mesh) return;

	uint8_t part = mesh_findPart(entity->mesh, name);
	if (!part) return;

	mesh_setPartOffset(entity->mesh, part, offset);
}

void entity3d_delete(Entity3D *entity)
{
	if (entity->mesh) {
		for (int i = 0; i < entity->mesh->dl_count; i++)
			rspq_block_free(entity->mesh->dl[i]);
		free(entity->mesh->dl);
		if (entity->mesh->deform) {
			meshDeform_delete(entity->mesh->deform);
			free(entity->mesh->deform);
		}
		free(entity->mesh->bound);
		free(entity->mesh->part_name);
		free(entity->mesh->part_bound);
		free(entity->mesh->part_offset);
		if (entity->mesh->part_matrix) free_uncached(entity->mesh->part_matrix);
		free_uncached(entity->mesh->matrix_buffer);
		resource_unload(entity->mesh->model);
		free(entity->mesh);
	}

	for (int i = 0; i < entity->sound_count; i++)
		sound_unload(&entity->sound[i]);
	free(entity->sound);

	free(entity);
}

void entity3d_playSound(const Entity3D *entity, uint8_t trigger, const Vector3 *position, float volume_scale)
{
	/* The candidates are the entity's sounds tagged with this trigger; the
	   loops are already playing and never fire. */
	uint8_t candidate[SOUND_MAX_EMITTERS];
	uint8_t count = 0;

	for (uint8_t i = 0; i < entity->sound_count && count < SOUND_MAX_EMITTERS; i++) {
		const SoundDef *def = entity->sound[i].def;
		if (def->loop || def->trigger != trigger) continue;
		candidate[count++] = i;
	}
	if (count == 0) return;

	Vector3 here;
	if (position == NULL) {
		if (entity->body) here = entity->body->tx.position;
		else               here = entity->transform.position;
		position = &here;
	}

	sound_play(&entity->sound[candidate[rand() % count]], position, volume_scale, 0.0f);
}

void entity3d_setTransform(Entity3D *entity, const KinematicBody *body)
{
	entity->transform.position = body->position;
	entity->transform.rotation = body->rotation;
}

void entity3d_setMatrix(Entity3D *entity, uint8_t fb_index)
{
	mesh_setMatrix(entity->mesh, &entity->transform, fb_index);
}

/* For entities the solver moves: their placement lives in the body, not in the
   render transform, and a tumbling body needs its quaternion rather than the
   euler angles the transform carries. No-op for anything else. */
void entity3d_setMatrixFromBody(Entity3D *entity, uint8_t fb_index)
{
	if (entity->body == NULL || !(entity->body->flags & BODY_FLAG_DYNAMIC)) return;

	mesh_setMatrixFromBody(entity->mesh, &entity->body->tx.position, &entity->body->q,
	                       &entity->transform.scale, fb_index);
}


/* World transform of a static collider: entity position in metres plus the
   entity rotation built with the same euler function the renderer uses, so
   collision and visuals always match. */
Transform entity3d_colliderTransform(const Entity3DDef *def)
{
	T3DMat4 mat;
	t3d_mat4_from_srt_euler(&mat,
		(float[3]){1.0f, 1.0f, 1.0f},
		(float[3]){deg_to_rad(def->rotation.x), deg_to_rad(def->rotation.y), deg_to_rad(def->rotation.z)},
		(float[3]){0.0f, 0.0f, 0.0f});

	return (Transform){
		.rotation = {
			.ex = { mat.m[0][0], mat.m[0][1], mat.m[0][2] },
			.ey = { mat.m[1][0], mat.m[1][1], mat.m[1][2] },
			.ez = { mat.m[2][0], mat.m[2][1], mat.m[2][2] },
		},
		.position = def->position,
	};
}

/* Copies the body-def override bits on top of the freshly-initialised body
   (position, orientation) and then attaches the entity's shape to it. */
RigidBody *entity3d_attachPhysics(Entity3D *entity, const Entity3DDef *def, PhysicsWorld *world)
{
	RigidBodyDef body_def;
	rigidBodyDef_init(&body_def);

	if (def->body) {
		/* The user-supplied RigidBodyDef describes only the body properties,
		   not the world position. Position/rotation come from the entity. */
		body_def.body_type       = def->body->body_type;
		body_def.gravity_scale   = def->body->gravity_scale;
		body_def.layers          = def->body->layers ? def->body->layers : 1;
		body_def.linear_damping  = def->body->linear_damping;
		body_def.angular_damping = def->body->angular_damping;
		body_def.allow_sleep     = def->body->allow_sleep;
		body_def.awake           = def->body->awake;
		body_def.active          = def->body->active;
		body_def.lock_axis_x     = def->body->lock_axis_x;
		body_def.lock_axis_y     = def->body->lock_axis_y;
		body_def.lock_axis_z     = def->body->lock_axis_z;
	}

	/* Position in metres and rotation through the renderer's euler convention,
	   both from the collider transform: a rotated entity collides the way it
	   renders. */
	Transform collider = entity3d_colliderTransform(def);
	body_def.position = collider.position;

	Quaternion rotation = quaternion_fromMatrix3(&collider.rotation);
	quaternion_toAxisAngle(&rotation, &body_def.axis, &body_def.angle);

	/* Identity degenerates to a zero axis; any axis stands for no rotation. */
	if (vector3_squaredMagnitude(&body_def.axis) == 0.0f)
		body_def.axis = (Vector3){ 0.0f, 0.0f, 1.0f };

	RigidBody *body = physicsWorld_createBody(world, &body_def);
	body->owner   = entity;
	entity->body  = body;

	if (def->collider) {
		for (uint8_t i = 0; i < def->collider->count; i++)
			rigidBody_addShape(body, &def->collider->shape[i], def->scale);
	}

	return body;
}

