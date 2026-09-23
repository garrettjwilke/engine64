/*
	Post-animation skeleton edits. The animation writes the base pose; the
	modifier list runs in order on top of it (weapons, physics driven bones,
	aim, IK), each writing local bone TRS + hasChanged. A modifier never
	touches bone matrices and never calls t3d_skeleton_update: the skeleton's
	owner does that once, after the list.
*/
#ifndef ENGINE64_SKELETON_H
#define ENGINE64_SKELETON_H

#include <stdint.h>
#include <t3d/t3dskeleton.h>

namespace e64 {

namespace skeleton {

class Modifiers {
public:

	static constexpr uint8_t MAX = 8;

	typedef void (*Fn)(T3DSkeleton *skeleton, void *context);

	struct Entry {

		Fn apply;
		void *context;

	};

	Entry entry[MAX];
	uint8_t count;

};


namespace modifiers {

void add(Modifiers *modifiers, Modifiers::Fn apply, void *context);
void apply(Modifiers *modifiers, T3DSkeleton *skeleton);

}


/* Model-space pose of a bone, composed from the local TRS chain: current
   frame, unlike bone->matrix which lags one skeleton update behind. */
void getBonePose(const T3DSkeleton *skeleton, int16_t bone, T3DVec3 *position, T3DQuat *rotation);

}

}

#endif
