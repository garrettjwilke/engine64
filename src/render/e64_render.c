#include <assert.h>
#include <string.h>

#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dskeleton.h>

#include "scene3d/e64_lighting.h"
#include "scene3d/e64_fog.h"
#include "viewport/e64_viewport.h"
#include "graphics/e64_font.h"
#include "graphics/e64_sprites.h"
#include "graphics/e64_shapes.h"
#include "particles/e64_particles.h"
#include "render/e64_render.h"
#include "scene2d/e64_scene2d.h"
#include "debug/e64_debug.h"
#include "time/e64_time.h"
#include "scene3d/e64_scene3d.h"

#include "game/e64_game.h"


/* The frame's draw list. Filled by the scenes and consumed here, every
   frame; nobody outside sees it. */
static RenderContext render_context;


void renderTransform_init(RenderTransform *t)
{
	*t = (RenderTransform){
		.position = {0.0f, 0.0f, 0.0f},
		.rotation = {0.0f, 0.0f, 0.0f},
		.scale    = {1.0f, 1.0f, 1.0f},
	};
}

void render_initContext(RenderContext *ctx)
{
	/* Only the counts matter: entries are fully written before being read,
	   and zeroing the whole struct wipes the entire 8 KB dcache. */
	ctx->element_count = 0;
	ctx->section_count = 0;
	ctx->object_count  = 0;
}


static void render_start(int *fb_index)
{
	*fb_index = (*fb_index + 1) % FB_COUNT;

	/* Geometry fades toward the fog color, so the background must be it. A
	   2D scene that declares a sky paints that instead. */
	Fog *fog = fog_get();
	color_t clear = fog->enabled ? fog->color : RGBA32(0, 0, 0, 0xFF);

	const Scene2D *scene2d = scene2d_get();
	if (scene2d->def && scene2d->def->background.a) clear = scene2d->def->background;

	viewport_attach();

	/* The 3D pipeline is only up while a 3D scene is, and it starts its own
	   frame: the render state it leaves is the one the clear and everything
	   after it run under, which is why it goes first. */
	if (viewport_has3D()) {
		t3d_frame_start();
		t3d_viewport_attach(&viewport_get()->t3d_viewport);
	}

	viewport_clear(clear);

	/* Only the 3D pipeline writes depth. */
	if (viewport_has3D()) t3d_screen_clear_depth();
}

static void render_end(void)
{
	viewport_detach();
}

/* No transparency a sprite could carry: the RDP is not on a sprite mode. */
#define SPRITE_MODE_UNSET (-1)

/* The whole RDP state a sprite draws under, in one batch: the mode API
   calls between begin and end are programmed as a single change instead of
   one apiece. Uniform fade for textured elements, the stamina wheel way:
   env alpha modulates only the alpha channel while RGB passes TEX0
   untouched, so prim color stays free for tinting.

   A run of sprites sharing a transparency sets this once: a stage's tiles
   are hundreds of elements under the very same state. */
static void render_setSpriteMode(uint8_t transparency)
{
	rdpq_mode_begin();
		rdpq_set_mode_standard();
		rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
		rdpq_mode_alphacompare(1);
		if (transparency) rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,TEX0), (TEX0,0,ENV,0)));
	rdpq_mode_end();

	/* Not a mode call, so it is issued on its own either way. */
	if (transparency) rdpq_set_env_color(RGBA32(0, 0, 0, 255 - transparency));
}

void render(void)
{
	RenderContext *ctx = &render_context;
	Viewport *viewport = viewport_get();

	render_initContext(ctx);
	scene3d_setRenderContext(scene3d_get(), ctx, viewport);
	scene2d_setRenderContext(scene2d_get(), ctx);

	render_start(&viewport->fb_index);

	if (ctx->object_count > 0) {
		light_set(light_get());
		fog_set(fog_get());

		/* A mesh contributes one element per visible part, and every part of
		   the same mesh shares its skeleton and its matrix: a character with
		   three weapons is four elements with identical state. Binding and
		   pushing once per run of equal state, and popping only when it
		   changes, cuts that setup without altering a single draw. */
		const T3DSkeleton *bound  = NULL;
		const T3DMat4FP   *pushed = NULL;

		for (int i = 0; i < ctx->object_count; i++) {
			Element3D *obj = &ctx->object[i];

			if (obj->skeleton && obj->skeleton != bound) {
				t3d_skeleton_use(obj->skeleton);
				bound = obj->skeleton;
			}

			if (obj->matrix != pushed) {
				if (pushed) t3d_matrix_pop(1);
				if (obj->matrix) t3d_matrix_push(obj->matrix);
				pushed = obj->matrix;
			}

			if (obj->dl) {
				rspq_block_run(obj->dl);
				continue;
			}

			T3DModelState state = t3d_model_state_create();
			state.drawConf = obj->conf;
			T3DModelIter it = t3d_model_iter_create(obj->model, T3D_CHUNK_TYPE_OBJECT);
			while (t3d_model_iter_next(&it)) {
				if (!it.object->isVisible) continue;
				t3d_model_draw_material(it.object->material, &state);
				rspq_block_run(it.object->userBlock);
			}
		}

		if (pushed) t3d_matrix_pop(1);
	}

	particles_draw();

	for (int s = 0; s < ctx->section_count; s++) {
		RenderSection *section = &ctx->section[s];

		/* Nothing on screen, nothing on the wire: a fully hidden section
		   must not emit a single command, scissor setup included. */
		bool any_visible = false;
		for (int i = 0; i < section->element_count; i++) {
			if (!ctx->element[section->element_start + i].graphic->is_hidden) {
				any_visible = true;
				break;
			}
		}
		if (!any_visible) continue;

		if (section->has_scissor) {
			rdpq_set_scissor(
				section->scissor_x,
				section->scissor_y,
				section->scissor_x + section->scissor_w,
				section->scissor_y + section->scissor_h);
		}

		/* The sprite state the RDP is already programmed with: the
		   transparency it was set for, or SPRITE_MODE_UNSET when a shape or
		   a text left the mode on something else. Only a change pays. */
		int sprite_mode = SPRITE_MODE_UNSET;

		/* The texture already in TMEM, so a run of elements sharing one
		   sprite uploads it once. Anything that draws its own way leaves it
		   unknown and the next upload is paid again. */
		const sprite_t *loaded = NULL;

		for (int i = 0; i < section->element_count; i++) {
			Element2D *element = &ctx->element[section->element_start + i];
			const Graphic *graphic = element->graphic;
			if (graphic->is_hidden) continue;
			switch (graphic->type) {
				case GRAPHIC_RECTANGLE:
					shape_drawRectangle(&graphic->rectangle, element->position, element->scale);
					sprite_mode = SPRITE_MODE_UNSET;
					loaded      = NULL;
					break;
				case GRAPHIC_TEXT:
					text_draw(&graphic->text, element->position);
					sprite_mode = SPRITE_MODE_UNSET;
					loaded      = NULL;
					break;
				case GRAPHIC_SPRITE:
					if (sprite_mode != graphic->transparency) {
						render_setSpriteMode(graphic->transparency);
						sprite_mode = graphic->transparency;
					}

					if (sprite_isLoadable(&graphic->sprite, element->rotation)) {
						if (graphic->sprite.asset != loaded) {
							sprite_loadTexture(&graphic->sprite);
							loaded = graphic->sprite.asset;
						}
						sprite_drawLoaded(&graphic->sprite, element->position, element->scale);
						break;
					}

					if (graphic->sprite.tiled) sprite_drawTiled(&graphic->sprite, element->position, element->scale);
					else                       sprite_draw(&graphic->sprite, element->position, element->scale, element->rotation);
					loaded = NULL;
					break;
			}
		}

		/* Back to the whole screen, whatever the display mode is now. */
		if (section->has_scissor)
			rdpq_set_scissor(0, 0, display_get_width(), display_get_height());
	}

	debugUI_draw();

	render_end();
}
