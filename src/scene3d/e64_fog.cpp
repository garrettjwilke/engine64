#include <libdragon.h>
#include <t3d/t3d.h>

#include "scene3d/e64_fog.h"
#include "engine/e64_common.h"


static Fog fog;

Fog* fog_get(void) { return &fog; }


void fog_init(const FogDef* def)
{
	fog.color   = def->color;
	fog.near    = def->near;
	fog.far     = def->far;
	fog.enabled = def->enabled;
}

void fog_set(Fog* fog)
{
	if (!fog->enabled) {
		t3d_fog_set_enabled(false);
		return;
	}

	rdpq_mode_fog(RDPQ_FOG_STANDARD);
	rdpq_set_fog_color(fog->color);

	/* Declared in metres, like everything else in a scene, and applied in
	   render units: the same conversion the camera planes get. */
	t3d_fog_set_range(fog->near * RENDER_SCALE, fog->far * RENDER_SCALE);
	t3d_fog_set_enabled(true);
}
