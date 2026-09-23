/*
	Camera-pitch aim bend, split evenly along a spine chain.

	The delta is never added in a bone's own frame: the aiming pose keeps the
	torso half-turned at whatever angle the clip authored, so a local axis is
	tilted with it. The bend is built once about the world's horizontal side
	axis and conjugated into each bone's frame through the rotation the chain
	actually carries this frame, which keeps it pure pitch under any twist.
*/
#ifndef ENGINE64_CHARACTER3D_AIM_H
#define ENGINE64_CHARACTER3D_AIM_H

#include <stdint.h>
#include <t3d/t3dskeleton.h>

namespace e64 {

class Character3D;
typedef struct Camera Camera;

#define CHARACTER3D_AIM_MAX_BONES 4


namespace character3d {

typedef struct AimingSettings {

	const char *const *bone; /* spine chain, root to tip */
	uint8_t count;
	float pitch_scale; /* spine degrees per camera degree, sign included */

} AimingSettings;

/* Resolved once at create: the names above become indices so the bend never
   searches the skeleton by string, and the scale rides along so the bend has
   everything it needs without reaching back into the def. */
typedef struct Aiming {

	int16_t bone[CHARACTER3D_AIM_MAX_BONES];
	uint8_t count;
	float pitch_scale;

} Aiming;


namespace aim {

void init(Character3D *character, const AimingSettings *settings);

/* skeleton::Modifiers::Fn; context is the Character3D. Weighted by the aim blend,
   so the torso straightens on its own when the mode fades. */
void apply(T3DSkeleton *skeleton, void *context);

}

}

}

#endif
