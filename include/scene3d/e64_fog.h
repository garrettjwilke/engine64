#ifndef ENGINE64_FOG_H
#define ENGINE64_FOG_H

#include <stdbool.h>
#include <libdragon.h>

namespace e64 {

/* Distance fog: computed per vertex on the RSP and blended by the RDP.
   Range is in metres along the view axis, and only reaches what the camera
   planes already let through. */

namespace fog {

typedef struct {

	color_t color;
	float near;
	float far;
	bool enabled;

} Def;

}

typedef struct Fog {

	color_t color;
	float near;
	float far;
	bool enabled;

} Fog;


namespace fog {

Fog *get(void);

void init(const Def *def);
void set(Fog *state);

}

}

#endif
