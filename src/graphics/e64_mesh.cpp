
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <t3d/t3dmodel.h>

#include "graphics/e64_mesh.h"
#include "shaders/e64_mesh_deform.h"
#include "engine/e64_common.h"
#include "physics/math/e64_math_common.h"
#include "physics/math/e64_quaternion.h"

namespace e64 {

/* ------------------------------------------------------------------------ */
/* Material table                                                            */
/* ------------------------------------------------------------------------ */

/* One recorded block per distinct material, whatever model it came from.
   Every object using it holds one user; the block goes when the last one
   leaves. A material is the RDP state its block emits, so two materials are
   the same when every field that reaches the block is the same. */
typedef struct {
	const T3DMaterial *material;   /* the first one registered, NULL = free */
	rspq_block_t      *block;
	uint16_t           users;
	bool               vertex_fx;
	bool               deferred;
} MaterialEntry;

static MaterialEntry material_table[MESH_MAX_MATERIALS];

/* A material state that matches nothing, so a material recorded against it
   emits every setting it carries. t3d's own fresh state starts at zero and
   skips a value equal to zero (vertex FX none, draw flags none), which in a
   block of its own would make the material inherit whatever ran before. */
static T3DModelState mesh_fullMaterialState(void)
{
	T3DModelState state = t3d_model_state_create();
	state.lastRenderFlags = 0xFFFFFFFF;
	state.lastVertFXFunc  = 0xFF;
	state.lastTextureHashA = 0xFFFFFFFF;
	state.lastTextureHashB = 0xFFFFFFFF;
	state.lastPrimColor  = RGBA32(1, 2, 3, 4);
	state.lastEnvColor   = RGBA32(1, 2, 3, 4);
	state.lastBlendColor = RGBA32(1, 2, 3, 4);
	return state;
}

/* The texture as the block uploads it: which one (the hash of its ROM path,
   the same identity t3d caches the sprite by) and how its tile is set. The
   path string and the sprite pointer say nothing the hash does not. */
static bool material_textureEquals(const T3DMaterialTexture *a, const T3DMaterialTexture *b)
{
	return a->texReference == b->texReference
	    && a->textureHash  == b->textureHash
	    && a->texWidth     == b->texWidth
	    && a->texHeight    == b->texHeight
	    && memcmp(&a->s, &b->s, sizeof(T3DMaterialAxis)) == 0
	    && memcmp(&a->t, &b->t, sizeof(T3DMaterialAxis)) == 0;
}

static bool material_equals(const T3DMaterial *a, const T3DMaterial *b)
{
	return a->colorCombiner  == b->colorCombiner
	    && a->otherModeValue == b->otherModeValue
	    && a->otherModeMask  == b->otherModeMask
	    && a->blendMode      == b->blendMode
	    && a->renderFlags    == b->renderFlags
	    && a->fogMode        == b->fogMode
	    && a->setColorFlags  == b->setColorFlags
	    && a->vertexFxFunc   == b->vertexFxFunc
	    && color_to_packed32(a->primColor)  == color_to_packed32(b->primColor)
	    && color_to_packed32(a->envColor)   == color_to_packed32(b->envColor)
	    && color_to_packed32(a->blendColor) == color_to_packed32(b->blendColor)
	    && material_textureEquals(&a->textureA, &b->textureA)
	    && material_textureEquals(&a->textureB, &b->textureB);
}

/* The id of a material, recording it the first time it is seen. The pointer
   is tried first: objects of one file share their material chunk, and every
   entity of one model shares the file. */
static uint8_t material_register(const T3DMaterial *material)
{
	if (!material) return MESH_MATERIAL_NONE;

	MaterialEntry *free_slot = NULL;

	for (uint8_t i = 0; i < MESH_MAX_MATERIALS; i++) {
		MaterialEntry *entry = &material_table[i];
		if (!entry->material) {
			if (!free_slot) free_slot = entry;
			continue;
		}
		if (entry->material == material) {
			entry->users++;
			return i;
		}
	}

	for (uint8_t i = 0; i < MESH_MAX_MATERIALS; i++) {
		MaterialEntry *entry = &material_table[i];
		if (entry->material && material_equals(entry->material, material)) {
			entry->users++;
			return i;
		}
	}

	assert(free_slot);

	T3DModelState state = mesh_fullMaterialState();
	rspq_block_begin();
	t3d_model_draw_material((T3DMaterial *)material, &state);
	free_slot->block = rspq_block_end();

	free_slot->material  = material;
	free_slot->users     = 1;
	free_slot->vertex_fx = material->vertexFxFunc != T3D_VERTEX_FX_NONE;

	/* Alpha blending reads what is already in the framebuffer, a decal reads
	   the depth of the surface under it: both need their scene order. The
	   exporter sorts a file's objects that way; across entities the render
	   has to. */
	free_slot->deferred = material->blendMode != 0
	                   || (material->otherModeValue & SOM_ZMODE_MASK) == SOM_ZMODE_DECAL;

	return (uint8_t)(free_slot - material_table);
}

static void material_release(uint8_t id)
{
	if (id == MESH_MATERIAL_NONE) return;

	MaterialEntry *entry = &material_table[id];
	assert(entry->users > 0);

	if (--entry->users > 0) return;

	rspq_block_free(entry->block);
	entry->block    = NULL;
	entry->material = NULL;
}

rspq_block_t *mesh_materialBlock(uint8_t id)
{
	return id == MESH_MATERIAL_NONE ? NULL : material_table[id].block;
}

bool mesh_materialHasVertexFx(uint8_t id)
{
	return id != MESH_MATERIAL_NONE && material_table[id].vertex_fx;
}

bool mesh_materialIsDeferred(uint8_t id)
{
	return id != MESH_MATERIAL_NONE && material_table[id].deferred;
}


/* ------------------------------------------------------------------------ */
/* Bounds and culling                                                        */
/* ------------------------------------------------------------------------ */

/* A skinned model's boxes are written per bone, so none of them says where the
   mesh ends up. Two things bound that for good: a vertex never leaves its own
   bone by more than the model box reaches, and a bone never leaves the root by
   more than its chain is long. The box that holds both holds every pose the
   rig can take, so it is measured once and never again. */
static void mesh_skinnedBound(Mesh *mesh, const T3DChunkSkeleton *skeleton)
{
	float reach = 0.0f;
	for (int i = 0; i < 3; i++) {
		float lo = -mesh->model->aabbMin[i];
		float hi =  mesh->model->aabbMax[i];
		if (lo > reach) reach = lo;
		if (hi > reach) reach = hi;
	}

	float chain[skeleton->boneCount];
	float longest = 0.0f;

	for (int b = 0; b < skeleton->boneCount; b++) {
		const T3DChunkBone *bone = &skeleton->bones[b];
		T3DVec3 p = bone->position;

		chain[b] = sqrtf(p.v[0]*p.v[0] + p.v[1]*p.v[1] + p.v[2]*p.v[2]);
		if (bone->parentIdx < b) chain[b] += chain[bone->parentIdx];
		if (chain[b] > longest) longest = chain[b];
	}

	int16_t half = (int16_t)(longest + reach);
	for (int i = 0; i < 3; i++) {
		mesh->local_min[i] = -half;
		mesh->local_max[i] =  half;
	}
}

void mesh_initBounds(Mesh *mesh)
{
	mesh->scale    = (Vector3){ 1.0f, 1.0f, 1.0f };
	mesh->rotation = quaternion_identity();
	mesh->position = (Vector3){ 0.0f, 0.0f, 0.0f };
	mesh->culled   = false;

	const T3DChunkSkeleton *skeleton = t3d_model_get_skeleton(mesh->model);
	if (skeleton) {
		mesh_skinnedBound(mesh, skeleton);
		return;
	}

	for (int i = 0; i < 3; i++) {
		mesh->local_min[i] = mesh->model->aabbMin[i];
		mesh->local_max[i] = mesh->model->aabbMax[i];
	}
}

/* The RSP matrix of a placement, straight in fixed point. */
static void mesh_writeMatrix(T3DMat4FP *out, const Vector3 *scale, const Quaternion *rotation, const Vector3 *position)
{
	t3d_mat4fp_from_srt(
		out,
		(float[3]){ scale->x, scale->y, scale->z },
		(float[4]){ rotation->x, rotation->y, rotation->z, rotation->w },
		(float[3]){ position->x, position->y, position->z }
	);
}

/* An offset part's placement: the entity's with the offset applied inside it.
   Scales multiply per axis and rotations compose, which equals the entity's
   matrix times the offset's while the entity's scale is uniform or the offset
   carries no rotation; a prop scaled per axis with a rotated part would need
   the full product. */
static void mesh_composePart(const Mesh *mesh, const RenderTransform *offset,
                             Vector3 *scale, Quaternion *rotation, Vector3 *position)
{
	Vector3 local = {
		mesh->scale.x * offset->position.x * RENDER_SCALE,
		mesh->scale.y * offset->position.y * RENDER_SCALE,
		mesh->scale.z * offset->position.z * RENDER_SCALE,
	};
	Vector3    moved = quaternion_rotateVector(&mesh->rotation, &local);
	Quaternion turn  = quaternion_fromEuler(deg_to_rad(offset->rotation.x), deg_to_rad(offset->rotation.y), deg_to_rad(offset->rotation.z));

	*scale    = (Vector3){ mesh->scale.x * offset->scale.x, mesh->scale.y * offset->scale.y, mesh->scale.z * offset->scale.z };
	*rotation = quaternion_product(&mesh->rotation, &turn);
	*position = (Vector3){ mesh->position.x + moved.x, mesh->position.y + moved.y, mesh->position.z + moved.z };
}

/* The frustum in the space of a placement. A world plane n·x + w, for a point
   placed by scale S, rotation R and translation t, reads (S·Rᵀn)·x + (w + n·t)
   on the model's own coordinates. The box test only looks at the sign, so the
   planes need no renormalising. */
static void mesh_localFrustum(T3DFrustum *out, const T3DFrustum *world,
                              const Vector3 *scale, const Quaternion *rotation, const Vector3 *position)
{
	Quaternion inverse = { -rotation->x, -rotation->y, -rotation->z, rotation->w };

	for (int i = 0; i < 6; i++) {
		const float *plane = world->planes[i].v;
		Vector3 n = { plane[0], plane[1], plane[2] };
		Vector3 r = quaternion_rotateVector(&inverse, &n);

		out->planes[i].v[0] = r.x * scale->x;
		out->planes[i].v[1] = r.y * scale->y;
		out->planes[i].v[2] = r.z * scale->z;
		out->planes[i].v[3] = plane[3] + n.x * position->x + n.y * position->y + n.z * position->z;
	}
}

void mesh_cull(Mesh *mesh, const T3DViewport *viewport)
{
	T3DFrustum frustum;
	mesh_localFrustum(&frustum, &viewport->viewFrustum, &mesh->scale, &mesh->rotation, &mesh->position);

	bool in_box = t3d_frustum_vs_aabb_s16(&frustum, mesh->local_min, mesh->local_max);

	/* The model's own box only holds what was modelled inside it, so a model
	   with a part drawn somewhere else cannot be cut by it: that part would go
	   down with a box it was never in. Those go to the per object test below,
	   which is the only one that knows where the pieces ended up. */
	mesh->culled = mesh->part_offset ? false : !in_box;
	if (mesh->culled) return;

	/* A static file with a BVH, drawn where it was modelled, walks its boxes
	   by node and marks only what it reaches, so everything starts off. */
	const T3DBvh *bvh = mesh->skeleton ? NULL : t3d_model_bvh_get(mesh->model);
	if (bvh && !mesh->part_offset) {
		for (uint16_t i = 0; i < mesh->object_count; i++) mesh->object[i]->isVisible = false;
		t3d_model_bvh_query_frustum(bvh, &frustum);
		return;
	}

	/* A part drawn away from the rest of the model brings the frustum into
	   its own space, or it would be cut by where it was modelled instead of
	   where it ends up. Part 0 stays where the model is. */
	T3DFrustum        part_frustum[MESH_MAX_PARTS];
	const T3DFrustum *test[MESH_MAX_PARTS];

	test[0] = &frustum;
	for (int p = 1; p <= mesh->part_count; p++) {
		test[p] = &frustum;
		if (!mesh->part_offset) continue;

		Vector3    scale, position;
		Quaternion rotation;
		mesh_composePart(mesh, &mesh->part_offset[p - 1], &scale, &rotation, &position);
		mesh_localFrustum(&part_frustum[p], &viewport->viewFrustum, &scale, &rotation, &position);
		test[p] = &part_frustum[p];
	}

	for (uint16_t i = 0; i < mesh->object_count; i++) {
		T3DObject *object = mesh->object[i];
		uint8_t    part   = mesh->object_part[i];

		/* A skinned body's object boxes are written per bone and say nothing
		   about the pose: it rides the whole-model box tested above. Its named
		   parts keep their own box, where the file modelled them. */
		if (mesh->skeleton && part == 0) {
			object->isVisible = in_box;
			continue;
		}

		object->isVisible = t3d_frustum_vs_aabb_s16(test[part], object->aabbMin, object->aabbMax);
	}
}


/* ------------------------------------------------------------------------ */
/* Placement                                                                 */
/* ------------------------------------------------------------------------ */

/* An offset part is drawn with the entity's placement carrying its own on
   top, so it follows whatever the entity does and keeps its displacement
   inside it. */
static void mesh_setPartMatrices(Mesh *mesh, uint8_t fb_index)
{
	if (!mesh->part_matrix) return;

	for (int i = 0; i < mesh->part_count; i++) {
		Vector3    scale, position;
		Quaternion rotation;
		mesh_composePart(mesh, &mesh->part_offset[i], &scale, &rotation, &position);
		mesh_writeMatrix(&mesh->part_matrix[i * FB_COUNT + fb_index], &scale, &rotation, &position);
	}
}


/* A simulated body tumbles, and euler angles cannot describe that without
   picking an order and losing the tumble at the poles. The body already keeps
   a quaternion, so it goes straight to the matrix. */
void mesh_setMatrixFromBody(Mesh *mesh, const Vector3 *position, const Quaternion *rotation,
                            const Vector3 *scale, uint8_t fb_index)
{
	mesh->scale    = *scale;
	mesh->rotation = *rotation;
	mesh->position = (Vector3){ position->x * RENDER_SCALE, position->y * RENDER_SCALE, position->z * RENDER_SCALE };

	mesh_writeMatrix(&mesh->matrix_buffer[fb_index], &mesh->scale, &mesh->rotation, &mesh->position);
	mesh_setPartMatrices(mesh, fb_index);
}


/* The placement is kept as scale, rotation and translation: the RSP matrix
   is written from it, and the culling moves the frustum with it. */
void mesh_setMatrix(Mesh *mesh, const RenderTransform *transform, uint8_t fb_index)
{
	mesh->scale    = transform->scale;
	mesh->rotation = quaternion_fromEuler(deg_to_rad(transform->rotation.x), deg_to_rad(transform->rotation.y), deg_to_rad(transform->rotation.z));
	mesh->position = (Vector3){ transform->position.x * RENDER_SCALE, transform->position.y * RENDER_SCALE, transform->position.z * RENDER_SCALE };

	mesh_writeMatrix(&mesh->matrix_buffer[fb_index], &mesh->scale, &mesh->rotation, &mesh->position);
	mesh_setPartMatrices(mesh, fb_index);
}


/* ------------------------------------------------------------------------ */
/* Recording                                                                 */
/* ------------------------------------------------------------------------ */

/* Every object gets a block with its geometry alone: vertices, bone matrix
   pushes, triangles. No material inside, so the block runs under whatever
   material the render put up before it. The block hangs off the object,
   which belongs to the model: entities of one model share it, the first one
   records it and t3d_model_free releases it with the model. */
static void mesh_recordGeometry(Mesh *mesh)
{
	for (uint16_t i = 0; i < mesh->object_count; i++) {
		if (mesh->object[i]->userBlock) continue;
		rspq_block_begin();
		t3d_model_draw_object(mesh->object[i], mesh->bone_matrices);
		mesh->object[i]->userBlock = rspq_block_end();
	}
}

void mesh_record(Mesh *mesh, const char *const *names, uint8_t count, const T3DMat4FP *matrices)
{
	assert(1 + count <= MESH_MAX_PARTS);

	uint16_t total = 0;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) total++;

	mesh->object_count    = total;
	mesh->object          = (T3DObject **)malloc(sizeof(T3DObject *) * total);
	mesh->object_material = (uint8_t *)malloc(total);
	mesh->object_part     = (uint8_t *)malloc(total);
	mesh->bone_matrices   = matrices;
	assert(mesh->object && mesh->object_material && mesh->object_part);

	/* The names are kept so a part can be found by the name it was declared
	   with; only the pointers are copied, the strings stay where they are. */
	mesh->part_count  = count;
	mesh->part_name   = count ? (const char **)malloc(sizeof(char *) * count) : NULL;
	mesh->part_offset = NULL;
	mesh->part_matrix = NULL;
	for (int i = 0; i < count; i++) mesh->part_name[i] = names[i];

	/* Each object: its material through the table, its part by name, part 0
	   for everything the list did not claim. */
	uint16_t n = 0;
	it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) {
		mesh->object[n]          = it.object;
		mesh->object_material[n] = material_register(it.object->material);
		mesh->object_part[n]     = 0;

		for (int p = 0; p < count; p++)
			if (it.object->name && strcmp(it.object->name, names[p]) == 0)
				mesh->object_part[n] = (uint8_t)(1 + p);
		n++;
	}

	mesh_recordGeometry(mesh);

	/* Part 0 starts on screen; the named parts are the caller's to turn on. */
	mesh->visible = 1;
}

/* The geometry blocks stay with the model, which other entities may still
   be drawing; only the materials are given back. */
void mesh_release(Mesh *mesh)
{
	for (uint16_t i = 0; i < mesh->object_count; i++)
		material_release(mesh->object_material[i]);

	free(mesh->object);
	free(mesh->object_material);
	free(mesh->object_part);
	free(mesh->part_name);
	free(mesh->part_offset);
	if (mesh->part_matrix) free_uncached(mesh->part_matrix);

	mesh->object          = NULL;
	mesh->object_material = NULL;
	mesh->object_part     = NULL;
	mesh->object_count    = 0;
	mesh->part_name       = NULL;
	mesh->part_count      = 0;
	mesh->part_offset     = NULL;
	mesh->part_matrix     = NULL;
}


/* ------------------------------------------------------------------------ */
/* Parts                                                                     */
/* ------------------------------------------------------------------------ */

uint8_t mesh_findPart(const Mesh *mesh, const char *name)
{
	for (int i = 0; i < mesh->part_count; i++)
		if (strcmp(mesh->part_name[i], name) == 0) return 1 + i;
	return 0;
}

void mesh_setPartVisible(Mesh *mesh, uint8_t part, bool visible)
{
	assert(part <= mesh->part_count);

	if (visible) mesh->visible |=  (uint8_t)(1u << part);
	else         mesh->visible &= (uint8_t)~(1u << part);
}

bool mesh_isPartVisible(const Mesh *mesh, uint8_t part)
{
	return (mesh->visible & (1u << part)) != 0;
}

void mesh_setPartOffset(Mesh *mesh, uint8_t part, const RenderTransform *offset)
{
	assert(part > 0 && part <= mesh->part_count);

	/* Nothing is reserved until a part is actually moved: a model whose parts
	   only ever show and hide pays for none of this. */
	if (!mesh->part_offset) {
		mesh->part_offset = (RenderTransform *)malloc(sizeof(RenderTransform) * mesh->part_count);
		assert(mesh->part_offset);

		/* The parts not being moved have to come out where they already were,
		   so they start at no offset and original size, not at zero. */
		for (int i = 0; i < mesh->part_count; i++)
			mesh->part_offset[i] = (RenderTransform){ .scale = { 1.0f, 1.0f, 1.0f } };

		mesh->part_matrix = (T3DMat4FP *)malloc_uncached(sizeof(T3DMat4FP) * mesh->part_count * FB_COUNT);
		assert(mesh->part_matrix);
		for (int i = 0; i < mesh->part_count * FB_COUNT; i++)
			t3d_mat4fp_identity(&mesh->part_matrix[i]);
	}

	mesh->part_offset[part - 1] = *offset;
}

const RenderTransform *mesh_getPartOffset(const Mesh *mesh, uint8_t part)
{
	if (!mesh->part_offset || part == 0 || part > mesh->part_count) return NULL;
	return &mesh->part_offset[part - 1];
}

const T3DMat4FP *mesh_getPartMatrix(const Mesh *mesh, uint8_t part, uint8_t fb_index)
{
	if (!mesh->part_matrix || part == 0 || part > mesh->part_count)
		return mesh->matrix_buffer ? &mesh->matrix_buffer[fb_index] : NULL;

	return &mesh->part_matrix[(part - 1) * FB_COUNT + fb_index];
}


/* ------------------------------------------------------------------------ */
/* Deform                                                                    */
/* ------------------------------------------------------------------------ */

bool mesh_setDeform(Mesh *mesh, const Vector3 *source, const Vector3 *source_normal,
                    const uint8_t *source_rgba, uint16_t source_count, float scale)
{
	MeshDeform *deform = (MeshDeform *)malloc(sizeof(MeshDeform));
	assert(deform);

	if (!meshDeform_bind(deform, mesh->model, source, source_normal, source_count, scale)) {
		free(deform);
		return false;
	}
	deform->source_rgba = source_rgba;

	/* The binding rewrote the model's vertex addresses as segment references,
	   and the geometry blocks were recorded before that with the old absolute
	   ones. Record them again so the draw goes through the segment. The
	   materials are untouched: they hold no vertex address. */
	for (uint16_t i = 0; i < mesh->object_count; i++) {
		if (mesh->object[i]->userBlock) rspq_block_free(mesh->object[i]->userBlock);
		mesh->object[i]->userBlock = NULL;
	}
	mesh_recordGeometry(mesh);

	mesh->deform = deform;
	return true;
}


void mesh_updateDeform(Mesh *mesh, uint8_t fb_index)
{
	if (mesh->deform) meshDeform_apply(mesh->deform, fb_index);
}

/* Separate from the update because it has to run at draw time, in front of the
   display list, not when the vertices are written. */
void mesh_bindDeformFrame(Mesh *mesh, uint8_t fb_index)
{
	if (mesh->deform) meshDeform_bindFrame(mesh->deform, fb_index);
}

}
