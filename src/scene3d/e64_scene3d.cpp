#include <assert.h>
#include <malloc.h>

#include <t3d/t3dmath.h>

#include "model/e64_mesh_deform.h"
#include "viewport/e64_viewport.h"
#include "camera/e64_camera3d_control.h"
#include "controller/e64_controls.h"
#include "player/e64_player.h"
#include "time/e64_time.h"
#include "scene3d/e64_light.h"
#include "scene3d/e64_fog.h"
#include "entity/e64_entity3d.h"
#include "scene3d/e64_scene3d.h"
#include "physics/e64_physics_world.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/e64_rigid_body.h"
#include "physics/e64_physics.h"
#include "engine/e64_common.h"

namespace e64 {

static Scene3D scene;
static physics::World g_physics;

namespace scene3d {

Scene3D *get(void) { return &scene; }
physics::World *getPhysics(void) { return &g_physics; }

void load(const Def *def, const controls::Def *controls)
{
	const character3d::ControlBinding *binding = controls ? controls->character3d : NULL;

	/* A scene with no light def is left pitch black rather than lit by
	   something the game never asked for. */
	static const light::Def unlit = {};
	static const fog::Def no_fog = {};

	/* The 3D pipeline comes up with the scene that needs it and goes down
	   with it, so a game that never loads one never pays for it. */
	viewport::open3D();

	const light::Def *light_def = def->light ? def->light : &unlit;
	const fog::Def *fog_def = def->fog ? def->fog : &no_fog;

	light::init(light_def);
	fog::init(fog_def);

	Camera *camera = &viewport::get()->camera;
	camera3d::reset(camera);
	if (def->camera) {
		camera->target_field_of_view = def->camera->field_of_view;
		camera->field_of_view = def->camera->field_of_view;
		camera->near_clipping = def->camera->near_clipping;
		camera->far_clipping = def->camera->far_clipping;
		camera->base_near_clipping = def->camera->near_clipping;
		camera->base_far_clipping = def->camera->far_clipping;
		camera->auto_clipping = def->camera->auto_clipping;
	}
	switch (def->camera ? def->camera->type : camera3d::CAMERA_TYPE_NONE) {
		case camera3d::CAMERA_TYPE_SPRING_ARM:
			camera3d::springArm::init(camera, &def->camera->spring_arm);
			break;
		case camera3d::CAMERA_TYPE_NONE:
		case camera3d::CAMERA_TYPE_COUNT:
			break;
	}
	camera->binding = controls ? controls->camera : NULL;

	assert(scene.entity_count == 0);
	scene = (Scene3D){};

	Vector3 gravity = { 0.0f, 0.0f, -9.8f };
	physics::world::init(&g_physics, physics::TIMESTEP, gravity, physics::SOLVER_ITERATIONS);
	physics::world::setWind(&g_physics, def->wind);

	for (int i = 0; i < def->entity_count; i++) {
		const Entity *placed = &def->entity[i];
		const Prefab3D *prefab = placed->prefab;

		Vector3 scale = placed->scale;
		if (scale.x == 0.0f && scale.y == 0.0f && scale.z == 0.0f)
			scale = (Vector3){ 1.0f, 1.0f, 1.0f };

		/* entity.c builds from a flat parameter block: filled here straight
		   from the prefab and its placement, and gone after the load. */
		entity3d::Def entity_def = {
			.model_path = prefab->model,
			.part = prefab->part,
			.part_count = prefab->part_count,
			.part_position = prefab->part_position,
			.sound = prefab->sound,
			.sound_count = prefab->sound_count,
			.position = placed->position,
			.rotation = placed->rotation,
			.scale = scale,
			.collider = prefab->collider,
			.cull = true,
		};

		switch (prefab->type) {
			case prefab3d::PREFAB3D_CHARACTER:
				entity_def.character = prefab->character;
				break;
			case prefab3d::PREFAB3D_PROP:
				entity_def.body = prefab->prop;
				break;
			case prefab3d::PREFAB3D_CLOTH:
				entity_def.cloth = prefab->cloth;
				break;
			case prefab3d::PREFAB3D_WATER:
				entity_def.water = prefab->water;
				break;
		}

		Entity3D *entity = entity3d::create(&entity_def);

		if (entity_def.collider)
			entity3d::attachPhysics(entity, &entity_def, &g_physics);

		if (entity_def.cloth) {
			Cloth *cloth = physics::world::createCloth(&g_physics, entity_def.cloth);
			/* The cloth runs in metres, the vertex buffer in render units. */
			if (cloth) {
				cloth->culled = &entity->mesh->culled;
				mesh::deform::set(entity->mesh, cloth->render_position, cloth->normal,
				                  NULL, cloth->particle_count, RENDER_SCALE);
			}
		}

		if (entity_def.water) {
			Water *water = water::create(entity_def.water);
			/* Same contract as the cloth: points in metres, buffer in render
			   units. The offsets are what scroll the texture layers; render
			   reads them after the recorded material. */
			if (water) {
				water->culled = &entity->mesh->culled;
				mesh::deform::set(entity->mesh, water->position, water->normal,
				                  water->rgba, water->count, RENDER_SCALE);
				entity->mesh->texture_scroll = water->offset;

				/* The entity's collider is the water's sensor volume: bind
				   them and the bodies inside it start floating. */
				if (entity->body)
					water::bindPhysics(water, entity->body, &g_physics);
			}
		}

		if (entity_def.character) {
			assert(scene.character3d_count < MAX_CHARACTERS);
			Character3D *character = character3d::create(entity_def.character, entity);
			scene.character[scene.character3d_count++] = character;

			character3d::physics::createBody(character, &g_physics);

			const character3d::WeaponsDef *weapons = entity_def.character->weapons_def;
			for (int slot = 0; weapons && slot < character3d::WEAPON_SLOT_COUNT; slot++)
				if (weapons->weapon[slot])
					character3d::weapon::equip(character, slot, weapons->weapon[slot]);

			/* The binding names this placement: its player takes the body. */
			if (binding && binding->character == placed)
				player::setCharacter3D(character, binding);
		}

		if (!entity_def.character) {
			for (int fb = 0; fb < Viewport::FB_COUNT; fb++)
				mesh::setMatrix(entity->mesh, &entity->transform, fb);
		}

		scene.entity[scene.entity_count++] = entity;
	}
}

void clear(void)
{
	scene = (Scene3D){};
}

void unload(void)
{
	for (int i = 0; i < scene.character3d_count; i++)
		character3d::destroy(scene.character[i]);
	for (int i = 0; i < scene.entity_count; i++)
		entity3d::destroy(scene.entity[i]);
	water::clear();
	clear();
	physics::world::shutdown(&g_physics);

	viewport::close3D();
}

/* The characters' half of the frame after physics::update: each one collides
   against the world, hands the outcome to its body, and from there to what
   draws it. Always these four, always in this order, so no game writes them
   out. The list is the scene's, which is why it lives here. */
void updateCharacters(uint8_t fb_index)
{
	for (int i = 0; i < scene.character3d_count; i++) {
		Character3D *character = scene.character[i];

		character3d::physics::collide(character, &g_physics);
		character3d::physics::syncBody(character);
		entity3d::setTransform(character->entity, &character->body);
		entity3d::setMatrix(character->entity, fb_index);
	}
}

/* The same for everything else in the world. What the solver moved is
   somewhere new by the time the frame gets here, and the matrix it is drawn
   with has to say so. Anything still was placed when the scene loaded and this
   passes over it. The list is the scene's, which is why it lives here. */
void updateEntities(uint8_t fb_index)
{
	for (int i = 0; i < scene.entity_count; i++)
		entity3d::setMatrixFromBody(scene.entity[i], fb_index);
}

/* The camera's half of the frame: it reads the buttons the scene declared for
   it, settles where it now belongs around what it is watching, and hands the
   result to the projection. Always these three, always in this order, so no
   game writes them out. */
void updateCamera(const Vector3 *target)
{
	Camera *camera = &viewport::get()->camera;

	if (camera->binding)
		camera3d::control::update(camera, camera->binding, &scene, time::get()->delta);

	viewport::updateCamera((Vector3 *)target, &scene);
	viewport::setPerspectiveCamera();
}

void addEntity(Entity3D *entity)
{
	assert(scene.entity_count < MAX_ENTITIES);
	scene.entity[scene.entity_count++] = entity;
}

Character3D *getCharacter3D(uint8_t index)
{
	if (index >= scene.character3d_count) return NULL;
	return scene.character[index];
}

void setRenderContext(const Scene3D *s, Render::Context *ctx, const Viewport *viewport)
{
	uint8_t fb_index = viewport->fb_index;

	for (int i = 0; i < s->entity_count; i++) {
		Entity3D *e = s->entity[i];
		Mesh *mesh = e->mesh;
		if (!mesh) continue;
		T3DSkeleton *skel = mesh->skeleton;

		/* The mesh culls itself and writes its own flags; here they are
		   only consumed. */
		if (e->cull) {
			mesh::cull(mesh, &viewport->t3d_viewport);
			if (mesh->culled) continue;
		}

		/* Whatever drives this mesh has already moved: fold the new positions
		   into this frame's vertex buffer, then point the segment its recorded
		   display list reads from at that same copy. */
		mesh::deform::update(mesh, fb_index);
		mesh::deform::bindFrame(mesh, fb_index);

		/* One element per object still on: its part turned on, and not cut
		   by the frustum when the entity culls. A part displaced from the
		   rest of the model carries a matrix of its own; every other object
		   draws with the entity's. */
		for (uint16_t o = 0; o < mesh->object_count; o++) {
			uint8_t part = mesh->object_part[o];
			if (!(mesh->visible & (1u << part))) continue;
			if (e->cull && !mesh->object[o]->isVisible) continue;

			assert(ctx->element3d_count < Render::MAX_3D_ELEMENTS);
			ctx->element3d[ctx->element3d_count++] = (Render::Element3D){
				.material = mesh->object_material[o],
				.object = mesh->object[o],
				.mesh = mesh,
				.matrix = (T3DMat4FP *)mesh::part::getMatrix(mesh, part, fb_index),
				.skeleton = skel,
			};
		}
	}
}

}
}
