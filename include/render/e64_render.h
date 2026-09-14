
#ifndef ENGINE64_RENDER_H
#define ENGINE64_RENDER_H

#include <stdbool.h>

#include <libdragon.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>
#include "graphics/e64_shapes.h"
#include "graphics/e64_graphic.h"
#include "physics/math/e64_vector3.h"
#include "viewport/e64_viewport.h"

/* A stage contributes one entry per visible tile: a full screen of 16 px
   cells is 300 of them per layer. */
#define RENDER_MAX_2D_ELEMENTS 1024
/* One entry per visible mesh part, not per entity: a skinned character alone
   contributes several, so this has to clear the scene's entity budget. */
#define RENDER_MAX_3D_ELEMENTS    64
#define RENDER_MAX_SECTIONS        8

typedef struct Scene3D          Scene3D;
typedef struct Scene2D         Scene2D;

typedef struct RenderTransform {
	Vector3 position;
	Vector3 rotation;
	Vector3 scale;
} RenderTransform;

typedef struct {

	const Graphic *graphic;

	Vector2 position;
	Vector2 scale;
	float   rotation;

} Element2D;

typedef struct {

	rspq_block_t *dl;      /* NULL: draw model's visible objects instead */
	T3DModel     *model;
	T3DMat4FP    *matrix;
	T3DSkeleton  *skeleton;
	T3DModelDrawConf *conf; /* optional, object path only: per-frame tile/texture hooks */

} Element3D;

typedef struct {

	/* 16 bits: a stage puts hundreds of tiles in one section. */
	uint16_t element_start;
	uint16_t element_count;

	bool    has_scissor;
	float   scissor_x;
	float   scissor_y;
	float   scissor_w;
	float   scissor_h;

} RenderSection;

typedef struct RenderContext {

	Element2D     element[RENDER_MAX_2D_ELEMENTS];
	uint16_t      element_count;

	RenderSection section[RENDER_MAX_SECTIONS];
	uint8_t       section_count;

	Element3D     object[RENDER_MAX_3D_ELEMENTS];
	uint8_t       object_count;

} RenderContext;


void renderTransform_init(RenderTransform *t);

void render_initContext(RenderContext *ctx);

/* Draws the frame: hands its context to each scene to fill, then paints it.
   A scene that is not loaded pushes nothing, so whatever is up is what
   shows. Reaches the scenes and the viewport itself. */
void render(void);


#endif
