#ifndef ENGINE64_MESH_H
#define ENGINE64_MESH_H

#include <libdragon.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dskeleton.h>

#include "render/e64_render.h"
#include "physics/math/e64_vector2.h"
#include "physics/math/e64_vector3.h"
#include "physics/math/e64_quaternion.h"

namespace e64 {

struct MeshDeform;

/* Parts are selected through one bitmask, and that is what caps them. */
#define MESH_MAX_PARTS 8

/* The two lists a prefab declares its parts with, written inline where they
   are used so the game never has to name a type or spell out a literal. */
#define MESH_PARTS(...)          (const char *const[]){ __VA_ARGS__ }
#define MESH_PART_POSITIONS(...) (const Vector3[])   { __VA_ARGS__ }

typedef struct Mesh {
	rspq_block_t **dl;            /* one block per part */
	uint8_t        dl_count;
	uint8_t        visible;       /* bitmask: parts to render */

	/* Object path (dl_count == 0): the model's objects grouped by material.
	   object_order lists them sorted by material, object_material says which
	   material block each position draws under, and userBlock of each object
	   holds its geometry alone. A material is uploaded once per mesh and
	   its visible objects run right after it. */
	rspq_block_t **material_block;   /* one per distinct material, NULL for none */
	uint8_t        material_count;
	T3DObject    **object_order;
	uint8_t       *object_material;
	uint16_t       object_count;
	bool           material_vertex_fx; /* some material sets a vertex FX: reset after the mesh */

	/* The names the parts were recorded from, one per named part. Part 0 has
	   no name: it is everything the list did not claim. */
	const char **part_name;
	uint8_t      part_count;

	/* The model object each named part was recorded from, so a part can be
	   cut from the frame by its own box. NULL for a name the model has no
	   object for, which is never cut. */
	const T3DObject **part_object;

	/* Bitmask, numbered like the visibility one: the parts the frustum threw
	   out this frame. */
	uint8_t part_culled;

	/* A named part can be drawn away from the rest of the model. Its offset is
	   held in the entity's own space and turned into a matrix of its own every
	   frame, one per framebuffer. Both stay NULL until a part is actually
	   offset, and a part left at zero keeps using the entity's matrix.
	   Indexed by named part, so part 1 is the first entry. */
	RenderTransform *part_offset;
	T3DMat4FP       *part_matrix;

	T3DMat4FP     *matrix_buffer; /* NULL = matrix baked in dl (static mesh) */
	T3DModel      *model;
	T3DSkeleton   *skeleton;      /* NULL = static mesh (set by character3d_create) */

	/* Where the vertices come from when something else drives them. The
	   binding lives in its own module, so the mesh only needs to know it is
	   there. NULL = vertices come straight from the model. */
	struct MeshDeform *deform;

	/* Scroll of the two texture tiles, s/t in texels, added on top of the
	   file's own translate each frame after the recorded material runs: the
	   material stays in its block and only SET_TILE_SIZE is reissued. NULL
	   for everything that does not scroll. */
	const Vector2 *texture_scroll;

	/* Model-space box of the whole mesh, taken once. */
	int16_t local_min[3];
	int16_t local_max[3];

	/* Where the entity stands, in render units, as last handed to the
	   matrix. The frustum is moved into the model's space with these, so
	   every box stays the one the file wrote. */
	Vector3    scale;
	Quaternion rotation;
	Vector3    position;
	bool       culled;
} Mesh;


void mesh_initBounds(Mesh *mesh);

/* Brings the frustum into the model's space and tests the file's own boxes
   against it: culled for the whole mesh, part_culled per named part, and
   isVisible per model object for meshes drawn through the object path,
   through the model's BVH when the file carries one. */
void mesh_cull(Mesh *mesh, const T3DViewport *viewport);

void mesh_setMatrix(Mesh *mesh, const RenderTransform *transform, uint8_t fb_index);

/* Same, but from a simulated body: position in metres and a quaternion, which
   is what a tumbling body actually has. */
void mesh_setMatrixFromBody(Mesh *mesh, const Vector3 *position, const Quaternion *rotation,
                            const Vector3 *scale, uint8_t fb_index);

/* Records part 0 (every object not in the list) plus one part per named
   object, in list order. Pass the skeleton segment placeholder as matrices
   for skinned models, NULL for static ones. The parts share one visibility
   bitmask, so a model can declare at most MESH_MAX_PARTS - 1 names. */
void mesh_recordParts(Mesh *mesh, const char *const *names, uint8_t count, const T3DMat4FP *matrices);

/* Part index for a recorded name, 0 when the mesh has no part by that name.
   Part 0 is the unnamed remainder, so it never comes back from a lookup. */
uint8_t mesh_findPart(const Mesh *mesh, const char *name);

/* Turns one recorded part on or off. Everything the parts left over sits in
   part 0, which is on unless something turns it off too. */
void mesh_setPartVisible(Mesh *mesh, uint8_t part, bool visible);

bool mesh_isPartVisible(const Mesh *mesh, uint8_t part);

/* Moves one named part away from the rest of the model, in the entity's own
   space: position in render units, rotation in degrees, and a zero scale left
   as original size. The part is given matrices of its own the first time it is
   offset, and follows the entity from then on. */
void mesh_setPartOffset(Mesh *mesh, uint8_t part, const RenderTransform *offset);

/* The offset a part is currently drawn with, NULL when it has none. */
const RenderTransform *mesh_getPartOffset(const Mesh *mesh, uint8_t part);

/* The matrix a part has to be drawn with: its own when it is offset, and the
   mesh's otherwise. Part 0 always draws with the mesh's. */
const T3DMat4FP *mesh_getPartMatrix(const Mesh *mesh, uint8_t part, uint8_t fb_index);

/* Object path: one block per distinct material with the material complete,
   and one block per object with its geometry alone, in the object's own
   userBlock. Objects are ordered by material so render uploads each material
   once per mesh and draws its visible objects after it. */
void mesh_recordObjects(Mesh *mesh);

/* Hands the vertices over to an external set of points, matched by rest
   position. `scale` converts source units to render units. Pass source_normal
   to have the shading follow the deformation, and source_rgba to drive the
   vertex colors too; NULL keeps the model's own. */
bool mesh_setDeform(Mesh *mesh, const Vector3 *source, const Vector3 *source_normal,
                    const uint8_t *source_rgba, uint16_t source_count, float scale);

/* Pushes the current source positions and normals into the vertex buffer.
   No-op when the mesh is not deformed. */
void mesh_updateDeform(Mesh *mesh, uint8_t fb_index);

/* Points the deformed mesh at this frame's vertices. Call right before its
   display list runs. No-op when the mesh is not deformed. */
void mesh_bindDeformFrame(Mesh *mesh, uint8_t fb_index);

}

#endif
