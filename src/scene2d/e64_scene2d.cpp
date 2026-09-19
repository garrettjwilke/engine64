/*
	The 2D scene, built from its definition the way the 3D one is: the def
	declares layers of placed prefabs, the load turns each placement into an
	entity of the single live scene, and from then on whoever animates
	writes there.
*/
#include <assert.h>
#include <malloc.h>

#include "scene2d/e64_scene2d.h"
#include "viewport/e64_viewport.h"


static Scene2D scene2d;


Scene2D *scene2d_get(void) { return &scene2d; }

/* A font is shared by every text that names it: loaded the first time the
   scene meets the id, freed once at unload. */
static void scene2d_loadFont(uint8_t id)
{
	for (int i = 0; i < scene2d.font_count; i++)
		if (scene2d.font[i] == id) return;

	assert(scene2d.font_count < SCENE2D_MAX_FONT);
	font_loadAsset(id);
	scene2d.font[scene2d.font_count++] = id;
}

void scene2d_load(const Scene2DDef *def)
{
	assert(def && def->layer_count <= SCENE2D_MAX_LAYER);

	/* A scene may be loaded over another (an overlay taking the screen):
	   what the one leaving created goes first. */
	scene2d_unload();

	scene2d.def = def;

	/* A scene with nothing but widgets declares no camera: the zeroed one is
	   of no type and moves nothing. */
	if (def->camera) camera2d_init(&scene2d.camera, def->camera);

	for (int i = 0; i < def->layer_count; i++) {
		const Scene2DLayer *layer = &def->layer[i];

		scene2d.layer_start[i] = scene2d.entity_count;

		for (int p = 0; p < layer->prefab_count; p++) {
			const Scene2DPrefab *placed = &layer->prefab[p];
			const Prefab2D      *prefab = placed->prefab;

			if (prefab->graphic.type == GRAPHIC_TEXT)
				scene2d_loadFont(prefab->graphic.text.font);

			/* Filled from the prefab and its placement, the way the 3D load
			   fills its Entity3DDef, and gone after the load. */
			Entity2DDef entity_def = {
				.graphic     = &prefab->graphic,
				.sound       = prefab->sound,
				.sound_count = prefab->sound_count,
				.position = placed->position,
				.scale    = placed->scale,
				.rotation = placed->rotation,
			};

			assert(scene2d.entity_count < SCENE2D_MAX_ENTITY);
			Entity2D *entity = entity2d_create(&entity_def);
			scene2d.entity[scene2d.entity_count++] = entity;

			if (prefab->type == PREFAB2D_CHARACTER) {
				assert(scene2d.character2d_count < SCENE2D_MAX_CHARACTER);
				scene2d.character[scene2d.character2d_count++] = character2d_create(prefab->character, entity);
			}

			/* The stage draws through its own elements; the entity only
			   says where it stands. */
			if (prefab->type == PREFAB2D_STAGE) {
				assert(scene2d.stage_count < SCENE2D_MAX_STAGE);
				entity->graphic->is_hidden = true;
				scene2d.stage[scene2d.stage_count++] = stage2d_create(prefab->stage, entity);
			}
		}
	}

	/* The bodies collide with the last stage placed: the level, drawn over
	   whatever backdrops came before it. */
	if (scene2d.stage_count)
		for (int i = 0; i < scene2d.character2d_count; i++)
			scene2d.character[i]->stage = scene2d.stage[scene2d.stage_count - 1];
}

void scene2d_unload(void)
{
	for (int i = 0; i < scene2d.character2d_count; i++)
		character2d_delete(scene2d.character[i]);

	for (int i = 0; i < scene2d.stage_count; i++)
		stage2d_delete(scene2d.stage[i]);

	for (int i = 0; i < scene2d.entity_count; i++)
		entity2d_delete(scene2d.entity[i]);

	for (int i = 0; i < scene2d.font_count; i++)
		font_unloadAsset(scene2d.font[i]);

	scene2d = (Scene2D){};
}

void scene2d_updateCharacters(float dt)
{
	for (int i = 0; i < scene2d.character2d_count; i++)
		character2d_update(scene2d.character[i], dt);
}

void scene2d_updateCamera(const Character2D *character, float dt)
{
	if (!character) return;

	camera2d_update(&scene2d.camera, character->position,
	                character->facing_left ? -1.0f : 1.0f, dt);
}

Entity2D *scene2d_getEntity(Scene2D *scene, uint8_t layer, uint8_t prefab)
{
	assert(scene->def && layer < scene->def->layer_count);
	assert(prefab < scene->def->layer[layer].prefab_count);

	return scene->entity[scene->layer_start[layer] + prefab];
}

Character2D *scene2d_getCharacter2D(uint8_t index)
{
	if (index >= scene2d.character2d_count) return NULL;
	return scene2d.character[index];
}

Stage2D *scene2d_getStage(uint8_t index)
{
	if (index >= scene2d.stage_count) return NULL;
	return scene2d.stage[index];
}

void scene2d_setRenderContext(const Scene2D *scene, RenderContext *ctx)
{
	if (!scene->def) return;

	/* The stages first, one section for all of them: the world the layers
	   of prefabs are drawn over. */
	if (scene->stage_count) {
		assert(ctx->section_count < RENDER_MAX_SECTIONS);
		RenderSection *section = &ctx->section[ctx->section_count++];
		*section = (RenderSection){ .element_start = ctx->element_count };

		for (int i = 0; i < scene->stage_count; i++)
			stage2d_setRenderContext(scene->stage[i], &scene->camera, ctx);

		section->element_count = ctx->element_count - section->element_start;
	}

	for (int i = 0; i < scene->def->layer_count; i++) {
		const Scene2DLayer *layer = &scene->def->layer[i];

		assert(ctx->section_count < RENDER_MAX_SECTIONS);
		RenderSection *section = &ctx->section[ctx->section_count++];
		section->element_start = ctx->element_count;

		for (int p = 0; p < layer->prefab_count; p++) {
			const Entity2D *entity = scene->entity[scene->layer_start[i] + p];

			/* A stage's entity only says where the stage stands; the stage
			   itself went above. */
			if (layer->prefab[p].prefab->type == PREFAB2D_STAGE) continue;

			/* Where the entity stands is world; what the camera takes of it
			   is the prefab's parallax, and a widget's zero leaves the
			   position it was placed at untouched. */
			float parallax = layer->prefab[p].prefab->parallax;

			/* Under a camera the zoom scales what is drawn as well as where
			   it lands; a scene with no camera draws at the placed size. On
			   top of it goes what a world pixel measures on this screen. */
			float   zoom  = scene->camera.type == CAMERA2D_TYPE_NONE ? 1.0f : scene->camera.zoom;
			Vector2 scale = viewport_getScale();

			/* Whole screen pixels, like the stage's tiles, so a sprite
			   standing on one never lands between texels. Rounded here and
			   nowhere else: the world position keeps its fractions, and a
			   second rounding before the camera would hold the body still
			   for a frame and move it two the next. */
			Vector2 screen = camera2d_toScreen(&scene->camera, entity->position, parallax);

			assert(ctx->element_count < RENDER_MAX_2D_ELEMENTS);
			ctx->element[ctx->element_count++] = (Element2D){
				.graphic  = entity->graphic,
				.position = { floorf(screen.x), floorf(screen.y) },
				.scale    = { entity->scale.x * zoom * scale.x, entity->scale.y * zoom * scale.y },
				.rotation = entity->rotation,
			};
		}

		section->element_count = ctx->element_count - section->element_start;
		section->has_scissor   = layer->has_scissor;
		section->scissor_x     = layer->scissor_x;
		section->scissor_y     = layer->scissor_y;
		section->scissor_w     = layer->scissor_w;
		section->scissor_h     = layer->scissor_h;
	}
}
