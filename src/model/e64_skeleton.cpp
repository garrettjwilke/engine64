#include <assert.h>
#include <fmath.h>

#include "math/e64_quaternion.h"
#include "model/e64_skeleton.h"

namespace e64 {

namespace skeleton {

namespace modifiers {

void add(Modifiers *modifiers, Modifiers::Fn apply, void *context)
{
	assert(modifiers->count < Modifiers::MAX);
	modifiers->entry[modifiers->count++] = (Modifiers::Entry){ apply, context };
}


void apply(Modifiers *modifiers, T3DSkeleton *skeleton)
{
	for (uint8_t i = 0; i < modifiers->count; i++)
		modifiers->entry[i].apply(skeleton, modifiers->entry[i].context);
}

}


void getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation)
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
		Vector3 step = quaternion::rotateVector((const Quaternion *)rotation, (const Vector3 *)&b->position);
		position->v[0] += step.x;
		position->v[1] += step.y;
		position->v[2] += step.z;

		T3DQuat next;
		t3d_quat_mul(&next, rotation, (T3DQuat *)&b->rotation);
		*rotation = next;
	}
}

}

}
