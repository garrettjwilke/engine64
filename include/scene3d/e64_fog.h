#ifndef ENGINE64_FOG_H
#define ENGINE64_FOG_H

#include <stdbool.h>
#include <libdragon.h>

/* Distance fog: computed per vertex on the RSP and blended by the RDP.
   Range is in metres along the view axis, and only reaches what the camera
   planes already let through. */

typedef struct {

	color_t color;
	float near;
	float far;
	bool enabled;

} FogDef;

typedef struct {

	color_t color;
	float near;
	float far;
	bool enabled;

} Fog;


Fog* fog_get(void);

void fog_init(const FogDef* def);
void fog_set(Fog* fog);

#endif
