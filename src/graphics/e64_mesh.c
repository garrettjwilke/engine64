
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <t3d/t3dmodel.h>

#include "graphics/e64_mesh.h"
#include "shaders/e64_mesh_deform.h"
#include "engine/e64_common.h"
#include "physics/math/e64_math_common.h"
#include "physics/math/e64_quaternion.h"


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
	uint8_t count = 0;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) count++;

	mesh->bound_count = 1 + count;
	mesh->bound = calloc(mesh->bound_count, sizeof(MeshBound));
	assert(mesh->bound);
	mesh->culled = false;

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

/* Places a model-space box in the world without walking its corners: the
   centre goes through the matrix, and the half-extent through its absolute
   value, which is the axis-aligned box that still contains it after any
   rotation. */
static void mesh_placeBound(MeshBound *bound, const int16_t *min, const int16_t *max, const T3DMat4 *m)
{
	float centre[3], extent[3];
	for (int i = 0; i < 3; i++) {
		centre[i] = (max[i] + min[i]) * 0.5f;
		extent[i] = (max[i] - min[i]) * 0.5f;
	}

	for (int i = 0; i < 3; i++) {
		float c = m->m[3][i];
		float e = 0.0f;

		for (int k = 0; k < 3; k++) {
			c += centre[k] * m->m[k][i];
			e += extent[k] * fabsf(m->m[k][i]);
		}

		bound->min.v[i] = c - e;
		bound->max.v[i] = c + e;
	}
}

static bool mesh_getBoundMatrix(const Mesh *mesh, uint8_t bound, const T3DMat4 *entity_matrix, T3DMat4 *world);

static void mesh_updateBounds(Mesh *mesh, const T3DMat4 *matrix)
{
	if (mesh->bound == NULL) return;

	mesh_placeBound(&mesh->bound[0], mesh->local_min, mesh->local_max, matrix);

	/* A part drawn away from the rest of the model gets its box placed with
	   its own matrix, or it would be cut from the frame by where it was
	   modelled instead of where it ends up. */
	uint8_t i = 1;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it) && i < mesh->bound_count) {
		T3DMat4 world;
		const T3DMat4 *m = mesh_getBoundMatrix(mesh, i, matrix, &world) ? &world : matrix;

		mesh_placeBound(&mesh->bound[i++], it.object->aabbMin, it.object->aabbMax, m);
	}
}

void mesh_cull(Mesh *mesh, const T3DViewport *viewport)
{
	/* The model's own box only holds what was modelled inside it, so a model
	   with a part drawn somewhere else cannot be cut by it: that part would go
	   down with a box it was never in. Those go straight to the per part test
	   below, which is the only one that knows where the pieces ended up. */
	mesh->culled = mesh->part_offset
	             ? false
	             : !t3d_frustum_vs_aabb(&viewport->viewFrustum,
	                                    &mesh->bound[0].min, &mesh->bound[0].max);
	if (mesh->culled) return;

	/* A model recorded in parts cuts each named part against its own box, so
	   two pieces of one model come and go on their own. Part 0 is everything
	   left unnamed and rides the whole-model box tested above. */
	if (mesh->dl_count != 0) {
		mesh->part_culled = 0;

		if (mesh->part_offset
		 && !t3d_frustum_vs_aabb(&viewport->viewFrustum,
		                         &mesh->bound[0].min, &mesh->bound[0].max))
			mesh->part_culled |= 1u;

		for (int i = 0; i < mesh->part_count; i++) {
			uint8_t b = mesh->part_bound[i];
			if (b == 0) continue;

			if (!t3d_frustum_vs_aabb(&viewport->viewFrustum,
			                         &mesh->bound[b].min, &mesh->bound[b].max))
				mesh->part_culled |= (uint8_t)(1u << (1 + i));
		}
		return;
	}

	uint8_t b = 1;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it) && b < mesh->bound_count) {
		it.object->isVisible = t3d_frustum_vs_aabb(&viewport->viewFrustum,
		                                           &mesh->bound[b].min, &mesh->bound[b].max);
		b++;
	}
}


/* An offset part is drawn with the entity's matrix carrying its own on top, so
   it follows whatever the entity does and keeps its displacement inside it. */
static void mesh_setPartMatrices(Mesh *mesh, const T3DMat4 *entity_matrix, uint8_t fb_index)
{
	if (!mesh->part_matrix) return;

	for (int i = 0; i < mesh->part_count; i++) {
		const RenderTransform *offset = &mesh->part_offset[i];

		T3DMat4 local, world;
		t3d_mat4_from_srt_euler(
			&local,
			(float[3]){ offset->scale.x, offset->scale.y, offset->scale.z },
			(float[3]){ deg_to_rad(offset->rotation.x), deg_to_rad(offset->rotation.y), deg_to_rad(offset->rotation.z) },
			(float[3]){ offset->position.x * RENDER_SCALE, offset->position.y * RENDER_SCALE, offset->position.z * RENDER_SCALE }
		);

		t3d_mat4_mul(&world, entity_matrix, &local);
		t3d_mat4_to_fixed_3x4(&mesh->part_matrix[i * FB_COUNT + fb_index], &world);
	}
}

/* The matrix a part's box has to be placed with: its own when it is offset,
   and the model's for everything else. Returns false when this box belongs to
   no part, which is the common case. */
static bool mesh_getBoundMatrix(const Mesh *mesh, uint8_t bound, const T3DMat4 *entity_matrix, T3DMat4 *world)
{
	if (!mesh->part_offset || !mesh->part_bound) return false;

	for (int i = 0; i < mesh->part_count; i++) {
		if (mesh->part_bound[i] != bound) continue;

		const RenderTransform *offset = &mesh->part_offset[i];

		T3DMat4 local;
		t3d_mat4_from_srt_euler(
			&local,
			(float[3]){ offset->scale.x, offset->scale.y, offset->scale.z },
			(float[3]){ deg_to_rad(offset->rotation.x), deg_to_rad(offset->rotation.y), deg_to_rad(offset->rotation.z) },
			(float[3]){ offset->position.x * RENDER_SCALE, offset->position.y * RENDER_SCALE, offset->position.z * RENDER_SCALE }
		);

		t3d_mat4_mul(world, entity_matrix, &local);
		return true;
	}

	return false;
}


/* A simulated body tumbles, and euler angles cannot describe that without
   picking an order and losing the tumble at the poles. The body already keeps
   a quaternion, so it goes straight to the matrix. */
void mesh_setMatrixFromBody(Mesh *mesh, const Vector3 *position, const Quaternion *rotation,
                            const Vector3 *scale, uint8_t fb_index)
{
	T3DMat4 matrix;

	t3d_mat4_from_srt(
		&matrix,
		(float[3]){ scale->x, scale->y, scale->z },
		(float[4]){ rotation->x, rotation->y, rotation->z, rotation->w },
		(float[3]){ position->x * RENDER_SCALE, position->y * RENDER_SCALE, position->z * RENDER_SCALE }
	);

	t3d_mat4_to_fixed_3x4(&mesh->matrix_buffer[fb_index], &matrix);
	mesh_setPartMatrices(mesh, &matrix, fb_index);
	mesh_updateBounds(mesh, &matrix);
}


/* The matrix is built in float because that is what moves the culling bounds
   into the world; the fixed-point one is only for the RSP. */
void mesh_setMatrix(Mesh *mesh, const RenderTransform *transform, uint8_t fb_index)
{
	T3DMat4 matrix;

	t3d_mat4_from_srt_euler(

		&matrix,

		(float[3]){transform->scale.x,         transform->scale.y,         transform->scale.z},
		(float[3]){deg_to_rad(transform->rotation.x), deg_to_rad(transform->rotation.y), deg_to_rad(transform->rotation.z)},
		(float[3]){transform->position.x * RENDER_SCALE, transform->position.y * RENDER_SCALE, transform->position.z * RENDER_SCALE}
	);

	t3d_mat4_to_fixed_3x4(&mesh->matrix_buffer[fb_index], &matrix);
	mesh_setPartMatrices(mesh, &matrix, fb_index);
	mesh_updateBounds(mesh, &matrix);
}


bool mesh_setDeform(Mesh *mesh, const Vector3 *source, const Vector3 *source_normal,
                    const uint8_t *source_rgba, uint16_t source_count, float scale)
{
	MeshDeform *deform = malloc(sizeof(MeshDeform));
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
	const MeshPartFilter *filter = user;

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

void mesh_recordObjects(Mesh *mesh)
{
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it)) {
		rspq_block_begin();
		t3d_model_draw_object(it.object, NULL);
		it.object->userBlock = rspq_block_end();
	}

	mesh->dl          = NULL;
	mesh->dl_count    = 0;
	mesh->visible     = 1;
	mesh->part_name   = NULL;
	mesh->part_count  = 0;
	mesh->part_bound  = NULL;
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
	mesh->part_name   = count ? malloc(sizeof(char *) * count) : NULL;
	mesh->part_bound  = count ? calloc(count, sizeof(uint8_t)) : NULL;

	for (int i = 0; i < count; i++) mesh->part_name[i] = names[i];

	/* The boxes are kept in the model's own object order, so each part is
	   matched to the one its object left behind. */
	uint8_t b = 1;
	T3DModelIter it = t3d_model_iter_create(mesh->model, T3D_CHUNK_TYPE_OBJECT);
	while (t3d_model_iter_next(&it) && b < mesh->bound_count) {
		for (int i = 0; i < count; i++)
			if (it.object->name && strcmp(it.object->name, names[i]) == 0)
				mesh->part_bound[i] = b;
		b++;
	}

	mesh->dl_count = 1 + count;
	mesh->dl = malloc(sizeof(rspq_block_t *) * mesh->dl_count);
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
		mesh->part_offset = malloc(sizeof(RenderTransform) * mesh->part_count);
		assert(mesh->part_offset);

		/* The parts not being moved have to come out where they already were,
		   so they start at no offset and original size, not at zero. */
		for (int i = 0; i < mesh->part_count; i++)
			mesh->part_offset[i] = (RenderTransform){ .scale = { 1.0f, 1.0f, 1.0f } };

		mesh->part_matrix = malloc_uncached(sizeof(T3DMat4FP) * mesh->part_count * FB_COUNT);
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
