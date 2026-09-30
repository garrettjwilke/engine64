#include <math.h>

#include "physics3d/e64_spring.h"
#include "math/e64_verlet.h"

namespace e64 {
namespace spring {

void reset(Spring *spring)
{
	*spring = (Spring){};
}


Vector3 update(Spring *spring, const Spring::Settings *settings, Vector3 anchor, float dt)
{
	if (!spring->primed) {
		spring->position = anchor;
		spring->previous = anchor;
		spring->primed = true;
	}

	Vector3 to_anchor = vector3::difference(&anchor, &spring->position);
	Vector3 accel = vector3::scaled(&to_anchor, settings->stiffness);
	vector3::add(&accel, &settings->gravity);
	float retain = expf(-settings->damping * dt);

	verlet::integrate(&spring->position, &spring->previous, accel, retain, dt);

	/* Clamp to the radius: the guarantee that it never detaches. */
	Vector3 offset = vector3::difference(&spring->position, &anchor);
	float dist_sq = vector3::squaredMagnitude(&offset);
	float max_sq = settings->max_offset * settings->max_offset;

	if (dist_sq > max_sq && dist_sq > 0.0f) {
		vector3::scale(&offset, settings->max_offset / sqrtf(dist_sq));
		spring->position = vector3::sum(&anchor, &offset);
	}

	return offset;
}

}
}
