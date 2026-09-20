#include <math.h>
#include <fmath.h>

#include "physics/math/e64_math_common.h"
#include "physics/math/e64_math_functions.h"
#include "camera/e64_camera.h"
#include "camera/e64_spring_arm.h"
#include "control/e64_camera_control.h"
#include "player/e64_player.h"

namespace e64 {

namespace camera {
namespace control {

/* x and y arrive normalized: how hard the camera is being pushed, whatever
   the game read to get there. */
static void setSpringArmInput(Camera *camera, float x, float y)
{
	SpringArmData *data = &camera->spring_arm.data;
	const SpringArmSettings *settings = &camera->spring_arm.settings;

	data->target_velocity.x = x * settings->max_velocity.x * settings->direction.x;
	data->target_velocity.y = y * settings->max_velocity.y * settings->direction.y;
}


static void (*handler[CAMERA_TYPE_COUNT])(Camera *, float, float) = {
	[CAMERA_TYPE_SPRING_ARM] = setSpringArmInput,
};


static void setInput(Camera *camera, float x, float y)
{
	if (camera->type == CAMERA_TYPE_NONE) return;
	handler[camera->type](camera, x, y);
}

/* The aiming pose: in over the shoulder, narrower view, slower swing. It only
   ever adds its offsets to the targets, so what the player set is still there
   when the aim lets go. Runs at the tail of the update, after the control
   wrote the velocity this scales. */
static void setAiming(Camera *camera, bool aiming, float dt)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return;

	const SpringArmSettings *settings = &camera->spring_arm.settings;
	SpringArmData *data = &camera->spring_arm.data;

	float arm  = data->target_arm_length;
	float fov  = camera->target_field_of_view;
	float side = data->target_side_offset;

	if (aiming) {
		arm  += settings->aim_arm_length;
		fov  += settings->aim_field_of_view;
		side += settings->aim_side_offset;
	}

	setDistance(camera, arm, dt);
	setFieldOfView(camera, fov, dt);
	setSideOffset(camera, side, dt);

	/* The swing was already written this frame: aiming asks for a share of
	   what the stick pushed, not for a different push. */
	if (aiming) {
		data->velocity.x *= settings->aim_velocity_scale;
		data->velocity.y *= settings->aim_velocity_scale;
	}
}


/* Opposite binds cancel out, so holding both leaves the camera still.

   The C stick takes over each axis it is pushed on. An N64 controller has none
   and reads zero, so the buttons are the whole push and the camera swings at
   one speed. A GameCube one reports how far its stick went, and the camera
   moves as fast as it is pushed. */
void update(Camera *camera, const ControlBinding *binding,
            const struct Scene3D *scene, float dt)
{
	const Controller *controller = &controller::get()[binding->player];

	Vector2 push = controller::getCStickNormalized(controller);

	if (push.x == 0.0f)
		push.x = controller::isHeld(controller, binding->pan_right)
		       - controller::isHeld(controller, binding->pan_left);

	if (push.y == 0.0f)
		push.y = controller::isHeld(controller, binding->tilt_up)
		       - controller::isHeld(controller, binding->tilt_down);

	setInput(camera, push.x, push.y);

	if (camera->type == CAMERA_TYPE_SPRING_ARM) {
		const SpringArmSettings *settings = &camera->spring_arm.settings;

		float distance = controller::isHeld(controller, binding->distance_out)
		               - controller::isHeld(controller, binding->distance_in);

		float fov = controller::isHeld(controller, binding->fov_out)
		          - controller::isHeld(controller, binding->fov_in);

		/* The stick moves what the arm is asked for, never where it is: the
		   aim rides on top of this and the two never fight over one field. */
		camera->spring_arm.data.target_arm_length += distance * settings->distance_speed * dt;
		camera->target_field_of_view              += fov      * settings->fov_speed      * dt;

		camera->target_field_of_view = clampf(camera->target_field_of_view,
		                                      CAMERA_FOV_MIN, CAMERA_FOV_MAX);

		camera->spring_arm.data.target_arm_length = clampf(
			camera->spring_arm.data.target_arm_length,
			SPRING_ARM_MIN_LENGTH, SPRING_ARM_MAX_LENGTH);
	}

	/* The binding names the player, so the camera knows what to follow on its
	   own. A seat with nobody in it leaves the camera where it was: a game
	   that frames something else calls viewport_updateCamera instead. */
	const Player *player = &player::get()[binding->player];

	/* The body it follows is also the body that aims, so the pose needs no
	   call of its own. */
	setAiming(camera,
		player->type == player::CHARACTER_3D && player->character3d.character
		&& player->character3d.character->movement.data.aiming, dt);

	if (player->entity)
		camera::update(camera, &player->entity->transform.position, scene, dt);
}


void setDistance(Camera *camera, float distance, float dt)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return;

	SpringArmData *data = &camera->spring_arm.data;
	float rate = camera->spring_arm.settings.zoom_response_rate;

	data->arm_length = lerpf(data->arm_length, distance, 1.0f - fm_expf(-rate * dt));
}


void setFieldOfView(Camera *camera, float field_of_view, float dt)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return;

	float rate = camera->spring_arm.settings.zoom_response_rate;

	/* The aim's offset rides on top of the target, so the bound goes here,
	   on the final ask, not only on the stick's side. */
	field_of_view = clampf(field_of_view, CAMERA_FOV_MIN, CAMERA_FOV_MAX);

	camera->field_of_view = lerpf(camera->field_of_view, field_of_view, 1.0f - fm_expf(-rate * dt));
}


void setSideOffset(Camera *camera, float side_offset, float dt)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return;

	SpringArmData *data = &camera->spring_arm.data;
	float rate = camera->spring_arm.settings.zoom_response_rate;

	data->side_offset = lerpf(data->side_offset, side_offset, 1.0f - fm_expf(-rate * dt));
}

}
}

}
