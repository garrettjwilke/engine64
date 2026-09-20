
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

namespace e64 {

/* A stage contributes one entry per visible tile: a full screen of 16 px
   cells is 300 of them per layer. */
#define RENDER_MAX_2D_ELEMENTS 1024
/* One entry per visible model object, not per entity: a prop with four
   materials is four of them, so this has to clear the scene's object count. */
#define RENDER_MAX_3D_PIECES     256
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

struct Mesh;

/* One visible model object: its geometry block sits in the object, its
   material in the mesh module's table. The frame groups pieces by material
   and draws each material once, ahead of every piece that uses it. */
typedef struct {

	uint8_t            material;   /* table id, MESH_MATERIAL_NONE for none */
	const T3DObject   *object;
	const struct Mesh *mesh;       /* for the texture scroll */
	T3DMat4FP         *matrix;
	T3DSkeleton       *skeleton;

} RenderPiece;

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

	RenderPiece   piece[RENDER_MAX_3D_PIECES];
	uint16_t      piece_count;

} RenderContext;


void renderTransform_init(RenderTransform *t);

void render_initContext(RenderContext *ctx);

/* Draws the frame: hands its context to each scene to fill, then paints it.
   A scene that is not loaded pushes nothing, so whatever is up is what
   shows. Reaches the scenes and the viewport itself. */
void render(void);


}

#endif
