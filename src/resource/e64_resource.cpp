#include <assert.h>
#include <string.h>
#include <libdragon.h>
#include <t3d/t3dmodel.h>

#include "resource/e64_resource.h"


typedef struct {

	const char  *path;    /* NULL = free slot */
	void        *data;
	ResourceType type;
	uint8_t      users;

} Resource;


/* The table. Loads and unloads come in any order, so a slot is free when its
   path is NULL rather than past a count. */
static Resource resource_table[RESOURCE_MAX];


static void *resource_open(const char *path, ResourceType type, const void *parms)
{
	switch (type) {
		case RESOURCE_MODEL:  return t3d_model_load(path);
		case RESOURCE_SPRITE: return sprite_load(path);
		case RESOURCE_FONT:   return rdpq_font_load(path);
		case RESOURCE_WAVE:   return wav64_load(path, (wav64_loadparms_t *)parms);
	}
	return NULL;
}

static void resource_close(Resource *resource)
{
	switch (resource->type) {
		case RESOURCE_MODEL:  t3d_model_free((T3DModel *)resource->data);      break;
		case RESOURCE_SPRITE: sprite_free((sprite_t *)resource->data);         break;
		case RESOURCE_FONT:   rdpq_font_free((rdpq_font_t *)resource->data);   break;
		case RESOURCE_WAVE:   wav64_close((wav64_t *)resource->data);          break;
	}
}


void *resource_load(const char *path, ResourceType type, const void *parms)
{
	assert(path);

	Resource *free_slot = NULL;

	for (int i = 0; i < RESOURCE_MAX; i++) {
		Resource *resource = &resource_table[i];

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

	free_slot->data = resource_open(path, type, parms);
	assert(free_slot->data);

	free_slot->path  = path;
	free_slot->type  = type;
	free_slot->users = 1;

	return free_slot->data;
}

void resource_unload(void *data)
{
	if (data == NULL) return;

	for (int i = 0; i < RESOURCE_MAX; i++) {
		Resource *resource = &resource_table[i];

		if (resource->data != data) continue;

		if (--resource->users == 0) {
			resource_close(resource);
			resource->path = NULL;
			resource->data = NULL;
		}
		return;
	}

	/* A pointer nobody handed out. */
	assert(false);
}
