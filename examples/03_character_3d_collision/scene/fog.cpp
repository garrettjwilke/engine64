/*
	Distance fog: surfaces blend towards .color between .near and .far, both in
	metres along the view axis, and the background is cleared to that same
	colour. Example 01 covers what each field does.

	Range picked against the camera's 50 metre far plane, so the haze closes
	before geometry is cut.

	Every material in this example has fog enabled on the Blender side, which is
	what lets the scene decide the range for all of them.
*/
#include "scene3d/e64_fog.h"


extern const FogDef fog = {

	.color   = { 70, 80, 100, 0xFF },
	.near    = 15.0f,
	.far     = 45.0f,
	.enabled = true,
};
