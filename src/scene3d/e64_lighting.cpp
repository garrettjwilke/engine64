#include <stdint.h>
#include <libdragon.h>
#include <rspq_profile.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3dmath.h>

#include "physics/e64_physics.h"
#include "engine/e64_common.h"               /* RENDER_SCALE */
#include "scene3d/e64_lighting.h"

namespace e64 {

static Light light;

Light *light_get(void) { return &light; }


/* A scene declares its lights in metres, where it stands and how far it
   carries, and the hardware measures both against the geometry it is lighting.
   Converting at the load leaves the per frame path untouched. */
void light_init(const LightDef *def)
{
	light = *def;

	for (int i = 0; i < LIGHT_COUNT; i++) {
		if (light.source[i].type == LIGHT_NONE) break;

		if (light.source[i].type == LIGHT_DIRECTIONAL) {
			t3d_vec3_norm(&light.source[i].directional.direction);
			continue;
		}

		for (int axis = 0; axis < 3; axis++)
			light.source[i].point.position.v[axis] *= RENDER_SCALE;

		light.source[i].point.size *= RENDER_SCALE;
	}
}

void light_set(const Light *light)
{
	t3d_light_set_ambient((uint8_t *)&light->ambient_color.r);

	int count = 0;
	for (; count < LIGHT_COUNT; count++) {
		const LightSource *source = &light->source[count];

		switch (source->type) {
			case LIGHT_DIRECTIONAL:
				t3d_light_set_directional(count, (uint8_t *)&source->color.r,
				                          (T3DVec3 *)&source->directional.direction);
				break;
			case LIGHT_POINT:
				t3d_light_set_point(count, (uint8_t *)&source->color.r,
				                    (T3DVec3 *)&source->point.position,
				                    source->point.size, false);
				break;
			case LIGHT_NONE:
				goto done;
		}
	}
done:
	t3d_light_set_count(count);
}

}
