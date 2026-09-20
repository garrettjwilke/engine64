#include <fmath.h>

#include "physics/math/e64_math_common.h"
#include "camera/e64_camera.h"
#include "camera/e64_spring_arm.h"

namespace e64 {

namespace camera {

static void (*handler[CAMERA_TYPE_COUNT])(Camera *, Vector3 *, float) = {
	[CAMERA_TYPE_SPRING_ARM] = springArm::update,
};

void init(Camera *camera)
{
	/* Lens and placement come from the scene's camera::Def; position and target
	   are one unit apart so the view matrix is not degenerate before the first
	   update places them. */
	*camera = (Camera){
		.position = { 0.0f, 1.0f, 0.0f },
		.target   = { 0.0f, 0.0f, 0.0f },
		.type     = CAMERA_TYPE_NONE,
	};
}

void reset(Camera *camera)
{
	init(camera);
}

/* horizontal yaw of the view direction, in the yaw convention of the engine;
   measured from the view (position -> target) so lateral offsets like the
   shoulder cancel out instead of skewing the angle */
float getAngleAround(const Camera *camera, const Vector3 *point)
{
	(void)point;

	float dx = camera->target.x - camera->position.x;
	float dy = camera->target.y - camera->position.y;

	if (dx == 0.0f && dy == 0.0f) return 0.0f;

	return rad_to_deg(fm_atan2f(dx, dy));
}

/* The camera's right hand side on the ground plane. */
Vector3 getRight(const Camera *camera)
{
	float dx = camera->target.x - camera->position.x;
	float dy = camera->target.y - camera->position.y;

	float length = sqrtf(dx * dx + dy * dy);
	if (length < 1e-4f) return vector3_create(0.0f, -1.0f, 0.0f);

	/* Z is up, so right is forward crossed with it. */
	return vector3_create(dy / length, -dx / length, 0.0f);
}


/* Starts a transition to whatever center the camera is fed next, gliding out of
   the given point. Passing a duration of zero cuts straight to the new target. */
void setViewTarget(Camera *camera, const Vector3 *from, float duration)
{
	camera->blend_from     = *from;
	camera->blend_elapsed  = 0.0f;
	camera->blend_duration = duration;
}

/* The near runs at a fifth of the arm, floored at a metre (the engine is
   in centimetres): the cut zone ends at 20% of the way to the pivot, so it
   never reaches the pieces in view. The far rides the arm whole, so the
   back plane keeps its authored distance past the pivot at any zoom. */
void fitClipping(Camera *camera, const struct Scene3D *scene)
{
	if (!camera->auto_clipping) return;
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return;

	float arm  = camera->spring_arm.data.arm_length;
	float near = arm * 0.2f;

	camera->near_clipping = near > camera->base_near_clipping
	                      ? near : camera->base_near_clipping;
	camera->far_clipping  = arm + camera->base_far_clipping;
}

void update(Camera *camera, Vector3 *center, const struct Scene3D *scene, float dt)
{
	if (camera->type == CAMERA_TYPE_NONE) return;

	Vector3 blended;

	if (camera->blend_elapsed < camera->blend_duration) {
		camera->blend_elapsed += dt;

		float t = camera->blend_elapsed / camera->blend_duration;
		if (t > 1.0f) t = 1.0f;

		float alpha = ease_cubic_in_out(t);

		blended.x = lerpf(camera->blend_from.x, center->x, alpha);
		blended.y = lerpf(camera->blend_from.y, center->y, alpha);
		blended.z = lerpf(camera->blend_from.z, center->z, alpha);

		center = &blended;
	}

	handler[camera->type](camera, center, dt);

	/* The flags read here are the previous frame's cull: the planes trail
	   the visibility by one frame. */
	fitClipping(camera, scene);
}

}

}
