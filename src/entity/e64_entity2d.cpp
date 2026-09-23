#include <assert.h>
#include <malloc.h>

#include "entity/e64_entity2d.h"
#include "resource/e64_resource.h"

namespace e64 {

namespace entity2d {

Entity2D *create(const Def *def)
{
	assert(def && def->graphic);

	Entity2D *entity = (Entity2D *)malloc(sizeof(Entity2D));
	assert(entity);

	Graphic *graphic = (Graphic *)malloc(sizeof(Graphic));
	assert(graphic);

	/* The definition names the file; the live graphic gets it loaded. */
	*graphic = *def->graphic;
	if (graphic->type == Graphic::SPRITE) {
		graphic->sprite.asset = (sprite_t *)resource::load(graphic->sprite.path, Resource::SPRITE, NULL);
		assert(graphic->sprite.asset);
	}

	bool unit_scale = def->scale.x == 0.0f && def->scale.y == 0.0f;

	*entity = (Entity2D){
		.position = def->position,
		.scale = unit_scale ? (Vector2){ 1.0f, 1.0f } : def->scale,
		.rotation = def->rotation,
		.graphic = graphic,
	};

	/* The sounds open with the entity. A looping one starts here and stops
	   when the entity goes. Flat: a 2D scene has no world to place it in. */
	if (def->sound_count) {
		static const Vector3 flat = {};

		entity->sound = (Sound *)malloc(def->sound_count * sizeof(Sound));
		assert(entity->sound);
		for (int i = 0; i < def->sound_count; i++) {
			entity->sound[i] = sound::load(def->sound[i]);
			if (def->sound[i]->loop)
				sound::play(&entity->sound[i], &flat, 1.0f, 0.0f);
		}
		entity->sound_count = def->sound_count;
	}

	return entity;
}

void destroy(Entity2D *entity)
{
	if (!entity) return;

	for (int i = 0; i < entity->sound_count; i++)
		sound::unload(&entity->sound[i]);
	free(entity->sound);

	if (entity->graphic->type == Graphic::SPRITE)
		resource::unload(entity->graphic->sprite.asset);

	free(entity->graphic);
	free(entity);
}

}

}
