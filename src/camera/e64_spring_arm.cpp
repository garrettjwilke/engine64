#include <math.h>
#include <fmath.h>

#include "physics/math/e64_math_common.h"
#include "camera/e64_camera.h"
#include "camera/e64_spring_arm.h"

namespace e64 {

namespace camera {
namespace springArm {

float getPitch(const Camera *camera)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return 0.0f;
	return camera->spring_arm.data.pitch;
}

float getYaw(const Camera *camera)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return 0.0f;
	return camera->spring_arm.data.yaw;
}

float getLength(const Camera *camera)
{
	if (camera->type != CAMERA_TYPE_SPRING_ARM) return 0.0f;
	return camera->spring_arm.data.arm_length;
}


static void setVelocity(Camera *camera, float dt)
{
	SpringArmData *data = &camera->spring_arm.data;
	const SpringArmSettings *settings = &camera->spring_arm.settings;

	float factor_x = fm_expf(-settings->response_rate.x * dt);
	float factor_y = fm_expf(-settings->response_rate.y * dt);
	data->velocity.x = data->velocity.x * factor_x + data->target_velocity.x * (1.0f - factor_x);
	data->velocity.y = data->velocity.y * factor_y + data->target_velocity.y * (1.0f - factor_y);
}


static void setPosition(Camera *camera, Vector3 *center, float dt)
{
	SpringArmData *data = &camera->spring_arm.data;
	const SpringArmSettings *settings = &camera->spring_arm.settings;

	data->pitch += data->velocity.y * dt;
	data->yaw   += data->velocity.x * dt;

	data->yaw = angle_wrap(data->yaw);

	if (data->pitch > settings->max_pitch) data->pitch = settings->max_pitch;
	if (data->pitch < settings->min_pitch) data->pitch = settings->min_pitch;

	float yaw   = deg_to_rad(data->yaw);
	float pitch = deg_to_rad(data->pitch);

	float sin_yaw, cos_yaw, sin_pitch, cos_pitch;
	fm_sincosf(yaw,   &sin_yaw,   &cos_yaw);
	fm_sincosf(pitch, &sin_pitch, &cos_pitch);

	/* forward points from the camera toward the pivot; right is its horizontal perpendicular */
	Vector3 forward = { cos_pitch * sin_yaw, cos_pitch * cos_yaw, -sin_pitch };
	Vector3 right   = { cos_yaw, -sin_yaw, 0.0f };

	Vector3 pivot = { center->x, center->y, center->z + data->height_offset };

	camera->position.x = pivot.x - forward.x * data->arm_length + right.x * data->side_offset;
	camera->position.y = pivot.y - forward.y * data->arm_length + right.y * data->side_offset;
	camera->position.z = pivot.z - forward.z * data->arm_length;

	camera->target.x = pivot.x + right.x * data->side_offset;
	camera->target.y = pivot.y + right.y * data->side_offset;
	camera->target.z = pivot.z;
}


void init(Camera *camera, const SpringArmDef *def)
{
	camera->type = CAMERA_TYPE_SPRING_ARM;
	camera->spring_arm.settings = def->settings;
	camera->spring_arm.data     = (SpringArmData){
		.target_arm_length  = def->arm_length,
		.arm_length         = def->arm_length,
		.target_side_offset = def->side_offset,
		.side_offset        = def->side_offset,
		.yaw               = def->yaw,
		.pitch             = def->pitch,
		.height_offset      = def->height_offset,
	};
}


void update(Camera *camera, Vector3 *center, float dt)
{
	setVelocity(camera, dt);
	setPosition(camera, center, dt);
}

}
}

}
