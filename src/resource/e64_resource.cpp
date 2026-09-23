#include <assert.h>
#include <string.h>
#include <libdragon.h>
#include <t3d/t3dmodel.h>

#include "resource/e64_resource.h"

namespace e64 {

namespace resource {

/* The table. Loads and unloads come in any order, so a slot is free when its
   path is NULL rather than past a count. */
static Resource table[Resource::MAX];


static void *open(const char *path, Resource::Type type, const void *parms)
{
	switch (type) {
		case Resource::MODEL: return t3d_model_load(path);
		case Resource::SPRITE: return sprite_load(path);
		case Resource::FONT: return rdpq_font_load(path);
		case Resource::WAVE: return wav64_load(path, (wav64_loadparms_t *)parms);
	}
	return NULL;
}

static void close(Resource *resource)
{
	switch (resource->type) {
		case Resource::MODEL: t3d_model_free((T3DModel *)resource->data); break;
		case Resource::SPRITE: sprite_free((sprite_t *)resource->data); break;
		case Resource::FONT: rdpq_font_free((rdpq_font_t *)resource->data); break;
		case Resource::WAVE: wav64_close((wav64_t *)resource->data); break;
	}
}


void *load(const char *path, Resource::Type type, const void *parms)
{
	assert(path);

	Resource *free_slot = NULL;

	for (int i = 0; i < Resource::MAX; i++) {
		Resource *resource = &table[i];

		if (resource->path == NULL) {
			if (free_slot == NULL) free_slot = resource;
			continue;
		}

		if (strcmp(resource->path, path) == 0) {
			resource->users++;
			return resource->data;
		}
	}

	assert(free_slot);

	free_slot->data = open(path, type, parms);
	assert(free_slot->data);

	free_slot->path = path;
	free_slot->type = type;
	free_slot->users = 1;

	return free_slot->data;
}

void unload(void *data)
{
	if (data == NULL) return;

	for (int i = 0; i < Resource::MAX; i++) {
		Resource *resource = &table[i];

		if (resource->data != data) continue;

		if (--resource->users == 0) {
			close(resource);
			resource->path = NULL;
			resource->data = NULL;
		}
		return;
	}

	/* A pointer nobody handed out. */
	assert(false);
}

}

}
