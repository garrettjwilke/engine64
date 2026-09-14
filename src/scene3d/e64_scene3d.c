#include <assert.h>
#include <malloc.h>

#include <t3d/t3dmath.h>

#include "shaders/e64_mesh_deform.h"
#include "viewport/e64_viewport.h"
#include "control/e64_camera_control.h"
#include "time/e64_time.h"
#include "scene3d/e64_lighting.h"
#include "scene3d/e64_fog.h"
#include "entity/e64_entity3d.h"
#include "scene3d/e64_scene3d.h"
#include "physics/world/e64_physics_world.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/body/e64_rigid_body.h"
#include "physics/e64_physics_settings.h"
#include "engine/e64_common.h"


static Scene3D scene;
static PhysicsWorld g_physics;

Scene3D        *scene3d_get(void)        { return &scene; }
PhysicsWorld *scene3d_getPhysics(void) { return &g_physics; }
PhysicsWorld *physics_getWorld(void) { return &g_physics; }

void scene3d_load(const Scene3DDef *def)
{
	/* A scene with no light def is left pitch black rather than lit by
	   something the game never asked for. */
	static const LightDef unlit;
	static const FogDef   clear;

	/* The 3D pipeline comes up with the scene that needs it and goes down
	   with it, so a game that never loads one never pays for it. */
	viewport_open3D();

	const LightDef *light = def->light ? def->light : &unlit;
	const FogDef   *fog   = def->fog   ? def->fog   : &clear;

	light_init(light);
	fog_init(fog);

	Camera *camera = &viewport_get()->camera;
	camera_reset(camera);
	if (def->camera) {
		camera->target_field_of_view = def->camera->field_of_view;
		camera->field_of_view        = def->camera->field_of_view;
		camera->near_clipping        = def->camera->near_clipping;
		camera->far_clipping         = def->camera->far_clipping;
		camera->base_near_clipping   = def->camera->near_clipping;
		camera->base_far_clipping    = def->camera->far_clipping;
		camera->auto_clipping        = def->camera->auto_clipping;
	}
	switch (def->camera ? def->camera->type : CAMERA_TYPE_NONE) {
		case CAMERA_TYPE_SPRING_ARM:
			cameraSpringArm_init(camera, &def->camera->spring_arm);
			break;
		case CAMERA_TYPE_NONE:
		case CAMERA_TYPE_COUNT:
			break;
	}

	assert(scene.entity_count == 0);
	scene = (Scene3D){0};

	Vector3 gravity = { 0.0f, 0.0f, -9.8f };
	physicsWorld_init(&g_physics, PHYSICS_TIMESTEP, gravity, PHYSICS_SOLVER_ITERATIONS);
	physicsWorld_setWind(&g_physics, def->wind);

	for (int i = 0; i < def->prefab_count; i++) {
		const Scene3DPrefab *placed = &def->prefab[i];
		const Prefab3D *prefab = placed->prefab;

		Vector3 scale = placed->scale;
		if (scale.x == 0.0f && scale.y == 0.0f && scale.z == 0.0f)
			scale = (Vector3){ 1.0f, 1.0f, 1.0f };

		/* entity.c builds from a flat parameter block: filled here straight
		   from the prefab and its placement, and gone after the load. */
		Entity3DDef entity_def = {
			.model_path  = prefab->model,
			.part          = prefab->part,
			.part_count    = prefab->part_count,
			.part_position = prefab->part_position,
			.sound       = prefab->sound,
			.sound_count = prefab->sound_count,
			.position   = placed->position,
			.rotation   = placed->rotation,
			.scale      = scale,
			.collider   = prefab->collider,
			.cull       = true,
		};

		switch (prefab->type) {
			case PREFAB3D_CHARACTER:
				entity_def.character = prefab->character;
				break;
			case PREFAB3D_PROP:
				entity_def.body = prefab->prop;
				break;
			case PREFAB3D_CLOTH:
				entity_def.cloth = prefab->cloth;
				break;
			case PREFAB3D_WATER:
				entity_def.water = prefab->water;
				break;
		}

		Entity3D *entity = entity3d_create(&entity_def);

		if (entity_def.collider)
			entity3d_attachPhysics(entity, &entity_def, &g_physics);

		if (entity_def.cloth) {
			Cloth *cloth = physicsWorld_createCloth(&g_physics, entity_def.cloth);
			/* The cloth runs in metres, the vertex buffer in render units. */
			if (cloth) {
				cloth->culled = &entity->mesh->culled;
				mesh_setDeform(entity->mesh, cloth->render_position, cloth->normal,
				               NULL, cloth->particle_count, RENDER_SCALE);
			}
		}

		if (entity_def.water) {
			Water *water = water_create(entity_def.water);
			/* Same contract as the cloth: points in metres, buffer in render
			   units. The draw conf is what scrolls the texture layers, so it
			   only works through the per-frame material path. */
			if (water) {
				water->culled = &entity->mesh->culled;
				mesh_setDeform(entity->mesh, water->position, water->normal,
				               water->rgba, water->count, RENDER_SCALE);
				entity->mesh->draw_conf = &water->conf;

				/* The entity's collider is the water's sensor volume: bind
				   them and the bodies inside it start floating. */
				if (entity->body)
					water_bindPhysics(water, entity->body, &g_physics);
			}
		}

		if (entity_def.character) {
			assert(scene.character3d_count < SCENE_MAX_CHARACTERS);
			Character3D *character = character3d_create(entity_def.character, entity);
			scene.character[scene.character3d_count++] = character;

			character3dPhysics_createBody(character, &g_physics);

			const Character3DWeaponsDef *weapons = entity_def.character->weapons_def;
			for (int slot = 0; weapons && slot < WEAPON_SLOT_COUNT; slot++)
				if (weapons->weapon[slot])
					character3d_equipWeapon(character, slot, weapons->weapon[slot]);
		}

		if (!entity_def.character) {
			for (int fb = 0; fb < FB_COUNT; fb++)
				mesh_setMatrix(entity->mesh, &entity->transform, fb);
		}

		scene.entity[scene.entity_count++] = entity;
	}
}

void scene3d_clear(void)
{
	scene = (Scene3D){0};
}

void scene3d_unload(void)
{
	for (int i = 0; i < scene.character3d_count; i++)
		character3d_delete(scene.character[i]);
	for (int i = 0; i < scene.entity_count; i++)
		entity3d_delete(scene.entity[i]);
	water_clear();
	scene3d_clear();
	physicsWorld_shutdown(&g_physics);

	viewport_close3D();
}

/* The characters' half of the frame after physics_update: each one collides
   against the world, hands the outcome to its body, and from there to what
   draws it. Always these four, always in this order, so no game writes them
   out. The list is the scene's, which is why it lives here. */
void scene3d_updateCharacters(uint8_t fb_index)
{
	for (int i = 0; i < scene.character3d_count; i++) {
		Character3D *character = scene.character[i];

		character3dPhysics_collide(character, &g_physics);
		character3dPhysics_syncBody(character);
		entity3d_setTransform(character->entity, &character->body);
		entity3d_setMatrix(character->entity, fb_index);
	}
}

/* The same for everything else in the world. What the solver moved is
   somewhere new by the time the frame gets here, and the matrix it is drawn
   with has to say so. Anything still was placed when the scene loaded and this
   passes over it. The list is the scene's, which is why it lives here. */
void scene3d_updateEntities(uint8_t fb_index)
{
	for (int i = 0; i < scene.entity_count; i++)
		entity3d_setMatrixFromBody(scene.entity[i], fb_index);
}

/* The camera's half of the frame: it reads the buttons the scene declared for
   it, settles where it now belongs around what it is watching, and hands the
   result to the projection. Always these three, always in this order, so no
   game writes them out. */
void scene3d_updateCamera(const Vector3 *target)
{
	Camera *camera = &viewport_get()->camera;

	if (camera->binding)
		cameraControl_update(camera, camera->binding, &scene, time_get()->delta);

	viewport_updateCamera((Vector3 *)target, &scene);
	viewport_setPerspectiveCamera();
}

void scene3d_addEntity(Entity3D *entity)
{
	assert(scene.entity_count < SCENE_MAX_ENTITIES);
	scene.entity[scene.entity_count++] = entity;
}

Character3D *scene3d_getCharacter3D(uint8_t index)
{
	if (index >= scene.character3d_count) return NULL;
	return scene.character[index];
}

void scene3d_setRenderContext(const Scene3D *s, RenderContext *ctx, const Viewport *viewport)
{
	uint8_t fb_index = viewport->fb_index;

	for (int i = 0; i < s->entity_count; i++) {
		Entity3D    *e      = s->entity[i];
		Mesh        *mesh   = e->mesh;
		if (!mesh) continue;
		T3DMat4FP   *matrix = mesh->matrix_buffer ? &mesh->matrix_buffer[fb_index] : NULL;
		T3DSkeleton *skel   = mesh->skeleton;

		/* The mesh culls itself and writes its own flags; here they are
		   only consumed. */
		if (e->cull) {
			mesh_cull(mesh, &viewport->t3d_viewport);
			if (mesh->culled) continue;
		}

		/* Whatever drives this mesh has already moved: fold the new positions
		   into this frame's vertex buffer, then point the segment its recorded
		   display list reads from at that same copy. */
		mesh_updateDeform(mesh, fb_index);
		mesh_bindDeformFrame(mesh, fb_index);

		if (mesh->dl_count == 0) {
			assert(ctx->object_count < RENDER_MAX_3D_ELEMENTS);
			ctx->object[ctx->object_count++] = (Element3D){ NULL, mesh->model, matrix, skel, mesh->draw_conf };
			continue;
		}

		/* A part displaced from the rest of the model carries a matrix of its
		   own; every other one draws with the entity's. */
		for (int part = 0; part < mesh->dl_count; part++) {
			if (!(mesh->visible & (1u << part))) continue;
			if (e->cull && (mesh->part_culled & (1u << part))) continue;

			T3DMat4FP *part_matrix = (T3DMat4FP *)mesh_getPartMatrix(mesh, part, fb_index);

			assert(ctx->object_count < RENDER_MAX_3D_ELEMENTS);
			ctx->object[ctx->object_count++] = (Element3D){ mesh->dl[part], NULL, part_matrix, skel };
		}
	}
}
