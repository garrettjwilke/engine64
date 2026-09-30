/*
	The interface on screen: layers of widgets in screen space, drawn after
	the scenes on a pass of their own. A widget is a graphic and where it is
	placed; nothing of the world (camera, parallax, body) reaches it. One
	animation plays at a time over it, so the engine keeps a single player.
*/
#ifndef ENGINE64_UI_H
#define ENGINE64_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "math/e64_vector2.h"
#include "graphics/e64_graphic.h"
#include "render/e64_render.h"
#include "ui/e64_ui_animation.h"

namespace e64 {

namespace ui {

constexpr int MAX_LAYER = 8;
constexpr int MAX_WIDGET = 32;
constexpr int MAX_FONT = 8;


/* A widget as declared: what it draws with and where. The load builds the
   live widget of the same index. A zero scale means identity. */
typedef struct WidgetDef {

	Vector2 position;
	Vector2 scale;
	float rotation;
	const Graphic *graphic;

} WidgetDef;

/* A layer groups what it draws under one order and one scissor. */
typedef struct Layer {

	const WidgetDef *widget;
	uint8_t widget_count;

	bool has_scissor;
	float scissor_x;
	float scissor_y;
	float scissor_w;
	float scissor_h;

} Layer;

typedef struct Def {

	const Layer *layer;
	uint8_t layer_count;

} Def;

}


/* The live widget: the graphic is its own copy, with the sprite loaded;
   whoever animates it writes here. */
class Widget {
public:

	Vector2 position;
	Vector2 scale;
	float rotation;

	Graphic graphic;

};


/* The live interface: the widgets flat, in placement order, with where each
   layer's begin. */
typedef struct UI {

	const ui::Def *def;

	Widget widget[ui::MAX_WIDGET];
	uint8_t widget_count;

	/* The font ids its texts name, each loaded once for the interface. */
	uint8_t font[ui::MAX_FONT];
	uint8_t font_count;

	uint8_t layer_start[ui::MAX_LAYER];

} UI;


namespace ui {

UI *get(void);

void load(const Def *def);
void unload(void);

/* A layer's widget by its placement index in the definition. */
Widget *getWidget(UI *ui, uint8_t layer, uint8_t index);

void play(const UIAnimation *animation, bool reversed);

/* True while an animation plays: controls gate their input on this, and the
   state machinery waits on it before switching. */
bool isTransitioning(void);

/* Advances what is playing and applies the animation that runs every frame,
   the one the live lookups live in. NULL for none. */
void update(const UIAnimation *idle);

/* Pushes one section per layer into the frame's list, after whatever the
   scenes left there, with the layer's scissor and one Render::Element2D per
   widget in it, in screen pixels of the current mode. */
void setRenderContext(const UI *ui, Render::Context *ctx);

}

}

#endif
