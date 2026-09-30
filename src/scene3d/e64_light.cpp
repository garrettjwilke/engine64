#include <stdint.h>
#include <libdragon.h>
#include <rspq_profile.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dmath.h>

#include "physics3d/e64_physics.h"
#include "engine/e64_common.h" /* RENDER_SCALE */
#include "scene3d/e64_light.h"

namespace e64 {
namespace light {

static Light l;

Light *get(void) { return &l; }


/* A scene declares its lights in metres, where it stands and how far it
   carries, and the hardware measures both against the geometry it is lighting.
   Converting at the load leaves the per frame path untouched. */
void init(const Def *def)
{
	l = *def;

	for (int i = 0; i < COUNT; i++) {
		if (l.source[i].type == LIGHT_NONE) break;

		if (l.source[i].type == LIGHT_DIRECTIONAL) {
			t3d_vec3_norm(&l.source[i].directional.direction);
			continue;
		}

		for (int axis = 0; axis < 3; axis++)
			l.source[i].point.position.v[axis] *= RENDER_SCALE;

		l.source[i].point.size *= RENDER_SCALE;
	}
}

void set(const Light *source)
{
	t3d_light_set_ambient((uint8_t *)&source->ambient_color.r);

	int count = 0;
	for (; count < COUNT; count++) {
		const Source *s = &source->source[count];

		switch (s->type) {
			case LIGHT_DIRECTIONAL:
				t3d_light_set_directional(count, (uint8_t *)&s->color.r,
				                          (T3DVec3 *)&s->directional.direction);
				break;
			case LIGHT_POINT:
				t3d_light_set_point(count, (uint8_t *)&s->color.r,
				                    (T3DVec3 *)&s->point.position,
				                    s->point.size, false);
				break;
			case LIGHT_NONE:
				goto done;
		}
	}
done:
	t3d_light_set_count(count);
}

}
}
