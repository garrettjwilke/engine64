
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

	/* The model's own box only holds what was modelled inside it, so a model
	   with a part drawn somewhere else cannot be cut by it: that part would go
	   down with a box it was never in. Those go straight to the per part test
	   below, which is the only one that knows where the pieces ended up. */
	mesh->culled = mesh->part_offset
	             ? false
	             : !t3d_frustum_vs_aabb_s16(&frustum, mesh->local_min, mesh->local_max);
	if (mesh->culled) return;

	/* A model recorded in parts cuts each named part against its own box, so
	   two pieces of one model come and go on their own. Part 0 is everything
	   left unnamed and rides the whole-model box tested above. */
	if (mesh->dl_count != 0) {
		mesh->part_culled = 0;

		if (mesh->part_offset
		 && !t3d_frustum_vs_aabb_s16(&frustum, mesh->local_min, mesh->local_max))
			mesh->part_culled |= 1u;

		for (int i = 0; i < mesh->part_count; i++) {
			const T3DObject *object = mesh->part_object[i];
			if (!object) continue;

			/* A part drawn away from the rest of the model brings the frustum
			   into its own space, or it would be cut by where it was modelled
			   instead of where it ends up. */
			T3DFrustum        part_frustum;
			const T3DFrustum *test = &frustum;
			if (mesh->part_offset) {
				Vector3    scale, position;
				Quaternion rotation;
				mesh_composePart(mesh, &mesh->part_offset[i], &scale, &rotation, &position);
				mesh_localFrustum(&part_frustum, &viewport->viewFrustum, &scale, &rotation, &position);
				test = &part_frustum;
			}

			if (!t3d_frustum_vs_aabb_s16(test, object->aabbMin, object->aabbMax))
				mesh->part_culled |= (uint8_t)(1u << (1 + i));
		}
		return;
	}

	/* Object path. A file with a BVH walks its boxes by node and marks only
	   what it reaches, so everything starts off; without one, each object is
	   tested on its own. */
	const T3DBvh *bvh = t3d_model_bvh_get(mesh->model);
	if (bvh) {
		for (uint16_t i = 0; i < mesh->object_count; i++) mesh->object_order[i]->isVisible = false;
		t3d_model_bvh_query_frustum(bvh, &frustum);
		return;
	}

	for (uint16_t i = 0; i < mesh->object_count; i++) {
		T3DObject *object = mesh->object_order[i];
		object->isVisible = t3d_frustum_vs_aabb_s16(&frustum, object->aabbMin, object->aabbMax);
	}
}


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
	   and the display lists were recorded before that with the old absolute
	   ones. Record them again so the draw goes through the segment. */
	if (mesh->dl_count > 0) {
		for (int i = 0; i < mesh->dl_count; i++) rspq_block_free(mesh->dl[i]);

		rspq_block_begin();
		t3d_model_draw(mesh->model);
		mesh->dl[0]    = rspq_block_end();
		mesh->dl_count = 1;
	} else {
		/* Object path: the blocks live per object instead. */
		T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
		while (t3d_model_iter_next(&it)) {
			if (it.object->userBlock) rspq_block_free(it.object->userBlock);
			rspq_block_begin();
			t3d_model_draw_object(it.object, NULL);
			it.object->userBlock = rspq_block_end();
		}
	}

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


/* Part recording: objects named in the list get their own part (dl),
   every other object lands together in part 0. NULL name = part 0. */
typedef struct {
	const char *const *names;
	uint8_t count;
	const char *name;
} MeshPartFilter;

static bool mesh_filterPart(void *user, const T3DObject *obj)
{
	const MeshPartFilter *filter = (const MeshPartFilter *)user;

	if (filter->name) return obj->name && strcmp(obj->name, filter->name) == 0;

	for (int i = 0; i < filter->count; i++)
		if (obj->name && strcmp(obj->name, filter->names[i]) == 0) return false;
	return true;
}

static rspq_block_t *mesh_recordPart(Mesh *mesh, MeshPartFilter *filter, const T3DMat4FP *matrices)
{
	rspq_block_begin();
	t3d_model_draw_custom(mesh->model, (T3DModelDrawConf){
		.userData = filter,
		.filterCb = mesh_filterPart,
		.matrices = matrices,
	});
	return rspq_block_end();
}

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

void mesh_recordObjects(Mesh *mesh)
{
	uint16_t count = 0;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) count++;

	mesh->object_count       = count;
	mesh->object_order       = (T3DObject **)malloc(sizeof(T3DObject *) * count);
	mesh->object_material    = (uint8_t *)malloc(count);
	mesh->material_block     = (rspq_block_t **)malloc(sizeof(rspq_block_t *) * count);
	mesh->material_count     = 0;
	mesh->material_vertex_fx = false;
	assert(mesh->object_order && mesh->object_material && mesh->material_block);

	/* The distinct materials, in order of first appearance. The file shares
	   one material chunk between the objects that use it, so the pointer
	   tells them apart. */
	T3DMaterial *material[count];

	it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) {
		rspq_block_begin();
		t3d_model_draw_object(it.object, NULL);
		it.object->userBlock = rspq_block_end();

		uint8_t m = 0;
		while (m < mesh->material_count && material[m] != it.object->material) m++;
		if (m < mesh->material_count) continue;

		assert(mesh->material_count < 255);
		material[mesh->material_count++] = it.object->material;
	}

	/* One block per material, complete. Then the objects, material by
	   material, so the ones sharing a material sit together. */
	uint16_t position = 0;
	for (uint8_t m = 0; m < mesh->material_count; m++) {
		mesh->material_block[m] = NULL;

		if (material[m]) {
			T3DModelState state = mesh_fullMaterialState();
			rspq_block_begin();
			t3d_model_draw_material(material[m], &state);
			mesh->material_block[m] = rspq_block_end();

			if (material[m]->vertexFxFunc != T3D_VERTEX_FX_NONE)
				mesh->material_vertex_fx = true;
		}

		it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
		while (t3d_model_iter_next(&it)) {
			if (it.object->material != material[m]) continue;
			mesh->object_order[position]    = it.object;
			mesh->object_material[position] = m;
			position++;
		}
	}

	mesh->dl          = NULL;
	mesh->dl_count    = 0;
	mesh->visible     = 1;
	mesh->part_name   = NULL;
	mesh->part_count  = 0;
	mesh->part_object = NULL;
	mesh->part_culled = 0;
	mesh->part_offset = NULL;
	mesh->part_matrix = NULL;
}

void mesh_recordParts(Mesh *mesh, const char *const *names, uint8_t count, const T3DMat4FP *matrices)
{
	assert(1 + count <= MESH_MAX_PARTS);

	MeshPartFilter filter = { .names = names, .count = count };

	/* The names are kept so a part can be found by the name it was declared
	   with; only the pointers are copied, the strings stay where they are. */
	mesh->part_count  = count;
	mesh->part_offset = NULL;
	mesh->part_matrix = NULL;
	mesh->part_culled = 0;

	/* Parts draw through their own blocks: nothing of the object path. */
	mesh->material_block     = NULL;
	mesh->material_count     = 0;
	mesh->object_order       = NULL;
	mesh->object_material    = NULL;
	mesh->object_count       = 0;
	mesh->material_vertex_fx = false;
	mesh->part_name   = count ? (const char **)malloc(sizeof(char *) * count) : NULL;
	mesh->part_object = count ? (const T3DObject **)calloc(count, sizeof(T3DObject *)) : NULL;

	for (int i = 0; i < count; i++) mesh->part_name[i] = names[i];

	/* Each named part keeps the object it was cut from, whose box is the one
	   it is culled by. */
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) {
		for (int i = 0; i < count; i++)
			if (it.object->name && strcmp(it.object->name, names[i]) == 0)
				mesh->part_object[i] = it.object;
	}

	mesh->dl_count = 1 + count;
	mesh->dl = (rspq_block_t **)malloc(sizeof(rspq_block_t *) * mesh->dl_count);
	assert(mesh->dl);

	filter.name = NULL;
	mesh->dl[0] = mesh_recordPart(mesh, &filter, matrices);

	for (int i = 0; i < count; i++) {
		filter.name = names[i];
		mesh->dl[1 + i] = mesh_recordPart(mesh, &filter, matrices);
	}

	mesh->visible = 1;
}

uint8_t mesh_findPart(const Mesh *mesh, const char *name)
{
	for (int i = 0; i < mesh->part_count; i++)
		if (strcmp(mesh->part_name[i], name) == 0) return 1 + i;
	return 0;
}

void mesh_setPartVisible(Mesh *mesh, uint8_t part, bool visible)
{
	assert(part < mesh->dl_count);

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

}
