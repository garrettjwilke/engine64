#include <malloc.h>
#include <string.h>

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>

#include "shaders/e64_water.h"
#include "physics/collision/e64_mesh_collider.h"
#include "physics/e64_physics_world.h"

namespace e64 {
namespace water {

/* Fresh water, and a drag that settles a bobbing crate in a few swings. */
static constexpr float DEFAULT_DENSITY = 1000.0f;
static constexpr float DEFAULT_LINEAR_DRAG = 2.5f;
static constexpr float DEFAULT_ANGULAR_DRAG = 1.5f;


static Water pool[Water::MAX_SURFACES];
static uint8_t pool_count;


Water *create(const Water::Def *def)
{
	if (pool_count >= Water::MAX_SURFACES) return NULL;

	MeshCollider *mesh = meshCollider::load(def->mesh_path);
	if (mesh == NULL) return NULL;

	Water *water = &pool[pool_count];
	*water = (Water){ .def = *def };

	water->count = mesh->vertex_count;
	water->position = (Vector3 *)malloc(sizeof(Vector3) * water->count * 3);
	water->rgba = (uint8_t *)malloc(4 * water->count);
	if (water->position == NULL || water->rgba == NULL) {
		free(water->position);
		free(water->rgba);
		meshCollider::destroy(mesh);
		*water = (Water){};
		return NULL;
	}
	water->normal = water->position + water->count;
	water->rest = water->normal + water->count;

	/* The mesh is scaffolding, same as the cloth: it seeds the points and
	   nothing keeps a reference to it afterwards. */
	memcpy(water->rest, mesh->vertices, sizeof(Vector3) * water->count);
	meshCollider::destroy(mesh);

	/* An unset tint would paint the water black; white leaves the caustics. */
	if (water->def.color[0] == 0 && water->def.color[1] == 0 && water->def.color[2] == 0)
		water->def.color[0] = water->def.color[1] = water->def.color[2] = 255;

	if (water->def.density == 0.0f) water->def.density = DEFAULT_DENSITY;
	if (water->def.linear_drag == 0.0f) water->def.linear_drag = DEFAULT_LINEAR_DRAG;
	if (water->def.angular_drag == 0.0f) water->def.angular_drag = DEFAULT_ANGULAR_DRAG;

	/* The plane is authored flat; the average irons out export noise. */
	for (uint16_t i = 0; i < water->count; i++)
		water->base_z += water->rest[i].z;
	water->base_z /= (float)water->count;

	for (uint16_t i = 0; i < water->count; i++) {
		water->position[i] = water->rest[i];
		water->normal[i] = (Vector3){ 0.0f, 0.0f, 1.0f };
		memcpy(&water->rgba[i * 4], (uint8_t[]){ water->def.color[0], water->def.color[1],
		                                         water->def.color[2], 0xFF }, 4);
	}

	for (uint8_t w = 0; w < water->def.wave_count; w++) {
		Vector3 dir = { water->def.wave[w].direction_x, water->def.wave[w].direction_y, 0.0f };
		vector3::normalize(&dir);
		water->def.wave[w].direction_x = dir.x;
		water->def.wave[w].direction_y = dir.y;
		water->amplitude_sum += water->def.wave[w].amplitude;
	}

	pool_count++;
	return water;
}

static void scroll(Vector2 *offset, const float speed[2], float wrap, float delta)
{
	offset->x += speed[0] * delta;
	offset->y += speed[1] * delta;

	/* Folded into [0, wrap): a negative translate overflows the fixed-point
	   tile coordinates, and fm_fmodf keeps the sign of its operand. */
	if (wrap > 0.0f) {
		offset->x = fm_fmodf(offset->x, wrap);
		offset->y = fm_fmodf(offset->y, wrap);
		if (offset->x < 0.0f) offset->x += wrap;
		if (offset->y < 0.0f) offset->y += wrap;
	}
}

static void waves(Water *water)
{
	const Water::Def *def = &water->def;

	for (uint16_t i = 0; i < water->count; i++) {
		const Vector3 *rest = &water->rest[i];
		float height = 0.0f;
		float slope_x = 0.0f;
		float slope_y = 0.0f;

		for (uint8_t w = 0; w < def->wave_count; w++) {
			const Water::Wave *wave = &def->wave[w];

			float phase = (wave->direction_x * rest->x + wave->direction_y * rest->y)
			            * wave->frequency + water->time * wave->speed;

			float s, c;
			fm_sincosf(phase, &s, &c);

			height += wave->amplitude * s;
			slope_x += wave->amplitude * wave->frequency * wave->direction_x * c;
			slope_y += wave->amplitude * wave->frequency * wave->direction_y * c;
		}

		water->position[i].z = rest->z + height;

		/* Normal of z = h(x,y) is (-dh/dx, -dh/dy, 1). */
		Vector3 normal = { -slope_x, -slope_y, 1.0f };
		water->normal[i] = vector3::normalized(&normal);

		/* Crests lighter, troughs darker, the shading of the example's lava. */
		float bright = 0.75f;
		if (water->amplitude_sum > 0.0f)
			bright += 0.25f * (height / water->amplitude_sum);

		uint8_t *rgba = &water->rgba[i * 4];
		rgba[0] = (uint8_t)(water->def.color[0] * bright);
		rgba[1] = (uint8_t)(water->def.color[1] * bright);
		rgba[2] = (uint8_t)(water->def.color[2] * bright);
	}
}

void update(float delta)
{
	for (uint8_t i = 0; i < pool_count; i++) {
		Water *water = &pool[i];

		water->time += delta;
		scroll(&water->offset[0], water->def.scroll_a, water->def.wrap_a, delta);
		scroll(&water->offset[1], water->def.scroll_b, water->def.wrap_b, delta);

		if (water->culled && *water->culled) continue;

		waves(water);
	}
}

/* Same sum as waves, at one arbitrary point instead of the mesh's: the
   buoyancy samples ask here, so a body floats on the exact surface the
   player sees. Waves ride on the rest height, which lives in mesh space;
   the cached placement pulls the world query in and lifts the result out. */
float getSurfaceHeight(const Water *water, float x, float y)
{
	const Water::Def *def = &water->def;
	float local_x = x - water->placement.x;
	float local_y = y - water->placement.y;
	float height = water->placement.z + water->base_z;

	for (uint8_t w = 0; w < def->wave_count; w++) {
		const Water::Wave *wave = &def->wave[w];

		float phase = (wave->direction_x * local_x + wave->direction_y * local_y)
		            * wave->frequency + water->time * wave->speed;
		height += wave->amplitude * fm_sinf(phase);
	}

	return height;
}

static float volumeSurfaceHeight(const void *surface, float x, float y)
{
	return getSurfaceHeight((const Water *)surface, x, y);
}

void bindPhysics(Water *water, RigidBody *body, physics::World *world)
{
	/* The body is static: its placement is settled for good at bind time. */
	water->placement = rigidBody::getTransform(body).position;

	water->volume = (buoyancy::Volume){
		.body = body,
		.density = water->def.density,
		.linear_drag = water->def.linear_drag,
		.angular_drag = water->def.angular_drag,
		.surface_height = volumeSurfaceHeight,
		.surface = water,
	};

	physics::world::addBuoyancy(world, &water->volume);
}

Water *getBoundSurface(const RigidBody *body)
{
	for (uint8_t i = 0; i < pool_count; i++)
		if (pool[i].volume.body == body) return &pool[i];
	return NULL;
}

void clear(void)
{
	for (uint8_t i = 0; i < pool_count; i++) {
		free(pool[i].position);
		free(pool[i].rgba);
		pool[i] = (Water){};
	}
	pool_count = 0;
}

}
}
