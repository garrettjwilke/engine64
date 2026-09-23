#include <libdragon.h>
#include <t3d/t3d.h>

#include "scene3d/e64_fog.h"
#include "engine/e64_common.h"

namespace e64 {
namespace fog {

static Fog f;

Fog *get(void) { return &f; }


void init(const Def *def)
{
	f.color = def->color;
	f.near = def->near;
	f.far = def->far;
	f.enabled = def->enabled;
}

void set(Fog *source)
{
	if (!source->enabled) {
		t3d_fog_set_enabled(false);
		return;
	}

	rdpq_mode_fog(RDPQ_FOG_STANDARD);
	rdpq_set_fog_color(source->color);

	/* Declared in metres, like everything else in a scene, and applied in
	   render units: the same conversion the camera planes get. */
	t3d_fog_set_range(source->near * RENDER_SCALE, source->far * RENDER_SCALE);
	t3d_fog_set_enabled(true);
}

}
}
