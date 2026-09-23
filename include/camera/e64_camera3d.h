#ifndef ENGINE64_CAMERA3D_H
#define ENGINE64_CAMERA3D_H

#include <stdbool.h>

#include "math/e64_vector3.h"
#include "e64_spring_arm.h"

namespace e64 {

namespace camera3d {

typedef enum {

	CAMERA_TYPE_SPRING_ARM,
	CAMERA_TYPE_COUNT,
	CAMERA_TYPE_NONE,

} Type;


typedef struct {

	Type type;

	float field_of_view;
	float near_clipping;
	float far_clipping;

	/* Refits near/far every frame to the boxes the frustum sees: nearest
	   visible corner in, farthest out — the depth-range fit shadow cascades
	   use. Off keeps the fixed planes above; they also stand in while
	   nothing is visible. */
	bool auto_clipping;

	union {
		SpringArmDef spring_arm;
	};

} Def;

struct ControlBinding;

}

/* Sane lens bounds, in degrees. Below the minimum the projection's
   cotangent blows past what the RSP's 16.16 matrices can hold (it crashed
   the fixed-point cast); above the maximum the image is unusable anyway. */
#define CAMERA_FOV_MIN 10.0f
#define CAMERA_FOV_MAX 120.0f


typedef struct Camera {

	Vector3 position;
	Vector3 target;

	/* Same split as the arm: the stick writes the target, the aim adds its
	   offset, and the lens chases the sum. */
	float target_field_of_view;
	float field_of_view;
	float near_clipping;
	float far_clipping;

	/* The def's planes, kept apart: the clipping method moves the live
	   ones starting from these. */
	float base_near_clipping;
	float base_far_clipping;

	bool auto_clipping; /* the clipping method refits the planes each frame */

	/* The buttons it answers to, straight from the scene's declaration. */
	const camera3d::ControlBinding *binding;

	/* view target transition: the outgoing center is frozen at switch time, so
	   the old target moving afterwards cannot disturb the blend */
	Vector3 blend_from;
	float blend_elapsed;
	float blend_duration;

	camera3d::Type type;

	union {
		struct {
			camera3d::SpringArmSettings settings;
			camera3d::SpringArmData data;
		} spring_arm;
	};

} Camera;


struct Scene3D;

namespace camera3d {

void init(Camera *camera);
void reset(Camera *camera);
void update(Camera *camera, Vector3 *center, const struct Scene3D *scene, float dt);
void setViewTarget(Camera *camera, const Vector3 *from, float duration);
float getAngleAround(const Camera *camera, const Vector3 *point);
float getPitch(const Camera *camera);
Vector3 getRight(const Camera *camera);
void fitClipping(Camera *camera, const struct Scene3D *scene);

}

}

#endif
