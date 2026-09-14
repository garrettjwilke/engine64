/*
	Distance haze. Off here; example 03 turns it on.

	A material has its own say in this: one exported with fog disabled ignores
	whatever the scene declares, so a new asset that has to take fog needs it
	enabled back in Blender.
*/
#include "scene3d/e64_fog.h"


const FogDef fog = { .enabled = false };
