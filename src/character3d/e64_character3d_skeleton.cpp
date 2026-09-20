#include <assert.h>
#include <fmath.h>

#include "physics/math/e64_quaternion.h"
#include "character3d/e64_character3d_skeleton.h"

namespace e64 {

void skeletonModifiers_add(SkeletonModifiers *modifiers, SkeletonModifierFn apply, void *context)
{
	assert(modifiers->count < SKELETON_MODIFIER_MAX);
	modifiers->modifier[modifiers->count++] = (SkeletonModifier){ apply, context };
}


void skeletonModifiers_apply(SkeletonModifiers *modifiers, T3DSkeleton *skeleton)
{
	for (uint8_t i = 0; i < modifiers->count; i++)
		modifiers->modifier[i].apply(skeleton, modifiers->modifier[i].context);
}


void skeleton_getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation)
{
	uint16_t chain[16];
	int depth = 0;

	uint16_t idx = (uint16_t)bone;
	while (idx != 0xFFFF && depth < 16) {
		chain[depth++] = idx;
		idx = skeleton->skeletonRef->bones[idx].parentIdx;
	}

	*position = (T3DVec3){{ 0.0f, 0.0f, 0.0f }};
	*rotation = (T3DQuat){{ 0.0f, 0.0f, 0.0f, 1.0f }};

	for (int i = depth - 1; i >= 0; i--) {
		const T3DBone *b = &skeleton->bones[chain[i]];

		/* T3DQuat and T3DVec3 are laid out like the math module's types. */
		Vector3 step = quaternion_rotateVector((const Quaternion *)rotation, (const Vector3 *)&b->position);
		position->v[0] += step.x;
		position->v[1] += step.y;
		position->v[2] += step.z;

		T3DQuat next;
		t3d_quat_mul(&next, rotation, (T3DQuat *)&b->rotation);
		*rotation = next;
	}
}

}
