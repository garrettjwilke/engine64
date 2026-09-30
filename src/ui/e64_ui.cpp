/*
	The interface, built from its definition the way the scenes are: the def
	declares layers of placed widgets, the load turns each placement into a
	live widget, and from then on whoever animates writes there.
*/
#include <assert.h>
#include <math.h>

#include "ui/e64_ui.h"
#include "resource/e64_resource.h"
#include "viewport/e64_viewport.h"
#include "time/e64_time.h"

namespace e64 {

namespace ui {

static UI live;

static UIAnimation::Player player;


UI *get(void) { return &live; }

/* A font is shared by every text that names it: loaded the first time the
   interface meets the id, freed once at unload. */
static void loadFont(uint8_t id)
{
	for (int i = 0; i < live.font_count; i++)
		if (live.font[i] == id) return;

	assert(live.font_count < MAX_FONT);
	font::loadAsset(id);
	live.font[live.font_count++] = id;
}

void load(const Def *def)
{
	assert(def && def->layer_count <= MAX_LAYER);

	/* An interface may be loaded over another (an overlay taking the
	   screen): what the one leaving created goes first. */
	unload();

	live.def = def;

	for (int i = 0; i < def->layer_count; i++) {
		const Layer *layer = &def->layer[i];

		live.layer_start[i] = live.widget_count;

		for (int p = 0; p < layer->widget_count; p++) {
			const WidgetDef *placed = &layer->widget[p];

			assert(live.widget_count < MAX_WIDGET);
			Widget *widget = &live.widget[live.widget_count++];

			bool unit_scale = placed->scale.x == 0.0f && placed->scale.y == 0.0f;

			widget->position = placed->position;
			widget->scale = unit_scale ? (Vector2){ 1.0f, 1.0f } : placed->scale;
			widget->rotation = placed->rotation;

			/* The definition names the file; the live graphic gets it
			   loaded. */
			widget->graphic = *placed->graphic;

			if (widget->graphic.type == Graphic::SPRITE) {
				widget->graphic.sprite.asset = (sprite_t *)resource::load(widget->graphic.sprite.path, Resource::SPRITE, NULL);
				assert(widget->graphic.sprite.asset);
			}

			if (widget->graphic.type == Graphic::TEXT)
				loadFont(widget->graphic.text.font);
		}
	}
}

void unload(void)
{
	for (int i = 0; i < live.widget_count; i++)
		if (live.widget[i].graphic.type == Graphic::SPRITE)
			resource::unload(live.widget[i].graphic.sprite.asset);

	for (int i = 0; i < live.font_count; i++)
		font::unloadAsset(live.font[i]);

	live = (UI){};
	player = (UIAnimation::Player){};
}

Widget *getWidget(UI *ui, uint8_t layer, uint8_t index)
{
	assert(ui->def && layer < ui->def->layer_count);
	assert(index < ui->def->layer[layer].widget_count);

	return &ui->widget[ui->layer_start[layer] + index];
}


void play(const UIAnimation *animation, bool reversed)
{
	uiAnimation::player::start(&player, &live, animation, UIAnimation::PLAY_ONCE, reversed);
}

bool isTransitioning(void)
{
	return player.is_active;
}

void update(const UIAnimation *idle)
{
	uiAnimation::player::update(&player, &live, time::get()->delta);

	if (idle) uiAnimation::apply(&live, idle, 0.0f);
}


void setRenderContext(const UI *ui, Render::Context *ctx)
{
	if (!ui->def) return;

	/* A widget is placed in the pixels of the base screen; a mode whose
	   columns are split in two lays two of them down for every one across,
	   for where it lands as much as for how big it is drawn. */
	Vector2 scale = viewport::getScale();

	for (int i = 0; i < ui->def->layer_count; i++) {
		const Layer *layer = &ui->def->layer[i];

		assert(ctx->section_count < Render::MAX_SECTIONS);
		Render::Section *section = &ctx->section[ctx->section_count++];
		section->element_start = ctx->element2d_count;

		for (int p = 0; p < layer->widget_count; p++) {
			const Widget *widget = &ui->widget[ui->layer_start[i] + p];

			/* Whole screen pixels: a text or a sprite landing between texels
			   comes out blurred. */
			assert(ctx->element2d_count < Render::MAX_2D_ELEMENTS);
			ctx->element2d[ctx->element2d_count++] = (Render::Element2D){
				.graphic = &widget->graphic,
				.position = { floorf(widget->position.x * scale.x), floorf(widget->position.y * scale.y) },
				.scale = { widget->scale.x * scale.x, widget->scale.y * scale.y },
				.rotation = widget->rotation,
			};
		}

		section->element_count = ctx->element2d_count - section->element_start;
		section->has_scissor = layer->has_scissor;
		section->scissor_x = layer->scissor_x * scale.x;
		section->scissor_y = layer->scissor_y * scale.y;
		section->scissor_w = layer->scissor_w * scale.x;
		section->scissor_h = layer->scissor_h * scale.y;
	}
}

}

}
