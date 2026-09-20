/*
	Follows the target the way Godot's Camera2D does: the drag margins decide
	where the view has to be, the world's edges push it back inside, and the
	smoothing decides how fast it gets there. Which of the last two runs first
	is the caller's to pick — met before the smoothing the view eases into the
	border, met after it stops dead on it.

	The screen it works against is the display mode in force, asked for every
	time: the mode changes while the game runs, and a size taken once would
	centre the view on a screen that is no longer there.
*/
#include <math.h>
#include <fmath.h>
#include <libdragon.h>

#include "camera/e64_camera2d.h"
#include "viewport/e64_viewport.h"

namespace e64 {

namespace camera2d {

static float clampZoom(float zoom)
{
	if (zoom <= 0.0f) return 1.0f;
	if (zoom < CAMERA2D_ZOOM_MIN) return CAMERA2D_ZOOM_MIN;
	if (zoom > CAMERA2D_ZOOM_MAX) return CAMERA2D_ZOOM_MAX;
	return zoom;
}

/* Half the screen in world pixels: the higher the zoom, the less world fits.
   Kept with the zoom so drawing never divides. */
static void setExtent(Camera2D *camera)
{
	Vector2 scale = viewport_getScale();

	camera->extent.x = display_get_width()  * 0.5f / (camera->zoom * scale.x);
	camera->extent.y = display_get_height() * 0.5f / (camera->zoom * scale.y);
}

void setZoom(Camera2D *camera, float zoom)
{
	camera->zoom = clampZoom(zoom);
	setExtent(camera);
}

void init(Camera2D *camera, const Def *def)
{
	*camera = (Camera2D){
		.type          = def->type,
		.anchor        = def->anchor,
		.position      = def->position,
		.offset        = def->offset,
		.limit_enabled = def->limit_enabled,
		.rotate        = def->rotate,
		.settings      = def->follow,
	};

	for (int i = 0; i < CAMERA2D_SIDE_COUNT; i++)
		camera->limit[i] = def->limit[i];

	setZoom(camera, def->zoom);

	camera->data.target_position = def->position;
}

/* Where the target is allowed to sit before the view answers. With the drag
   on, the camera is dragged only once the target passes the margin; with it
   off, the offset places the camera against those same margins, which is what
   a look ahead writes. */
static float setDrag(float current, float target, float extent,
                     float margin_low, float margin_high,
                     bool drag, float offset)
{
	if (drag) {
		float low  = target + extent * margin_low;
		float high = target - extent * margin_high;
		if (current > low)  current = low;
		if (current < high) current = high;
		return current;
	}

	float margin = (offset < 0.0f) ? margin_high : margin_low;
	return target + extent * margin * offset;
}

/* The world's edges, in world pixels. A world narrower than the screen has no
   room to push into: the view centres on what there is. */
static float setLimit(float position, float extent, float low, float high)
{
	if (low > high - extent * 2.0f) return (low + high) * 0.5f;
	if (position - extent < low)    return low  + extent;
	if (position + extent > high)   return high - extent;
	return position;
}

static void setLimits(Camera2D *camera, Vector2 *position)
{
	if (!camera->limit_enabled) return;

	position->x = setLimit(position->x, camera->extent.x,
	                       camera->limit[CAMERA2D_SIDE_LEFT],
	                       camera->limit[CAMERA2D_SIDE_RIGHT]);
	position->y = setLimit(position->y, camera->extent.y,
	                       camera->limit[CAMERA2D_SIDE_TOP],
	                       camera->limit[CAMERA2D_SIDE_BOTTOM]);
}

void update(Camera2D *camera, Vector2 target, float facing, float dt)
{
	if (camera->type == CAMERA2D_TYPE_NONE) return;

	FollowSettings *settings = &camera->settings;
	FollowData     *data     = &camera->data;

	/* The look ahead turns with the body: the offset carries the sign, so a
	   change of direction slides the lead across instead of jumping it. */
	float ahead_x = settings->drag_offset_x * facing;

	data->target_position.x = setDrag(data->target_position.x, target.x, camera->extent.x,
	                                  settings->drag_margin[CAMERA2D_SIDE_LEFT],
	                                  settings->drag_margin[CAMERA2D_SIDE_RIGHT],
	                                  settings->drag_horizontal, ahead_x);

	data->target_position.y = setDrag(data->target_position.y, target.y, camera->extent.y,
	                                  settings->drag_margin[CAMERA2D_SIDE_TOP],
	                                  settings->drag_margin[CAMERA2D_SIDE_BOTTOM],
	                                  settings->drag_vertical, settings->drag_offset_y);

	/* Before the smoothing: the view eases into the border rather than
	   stopping on it. */
	if (settings->limit_smoothing) setLimits(camera, &data->target_position);

	/* The first frame plants the view: easing in from wherever the camera was
	   declared would sweep the whole world once. */
	if (!data->settled) {
		camera->position = data->target_position;
		data->settled    = true;
	}
	else if (settings->position_smoothing_speed > 0.0f) {
		float factor = fm_expf(-settings->position_smoothing_speed * dt);
		camera->position.x = camera->position.x * factor + data->target_position.x * (1.0f - factor);
		camera->position.y = camera->position.y * factor + data->target_position.y * (1.0f - factor);
	}
	else camera->position = data->target_position;

	/* After the smoothing: the border is a wall the view never crosses. */
	if (!settings->limit_smoothing) setLimits(camera, &camera->position);

	if (!camera->rotate) return;

	if (settings->rotation_smoothing_speed > 0.0f) {
		float factor = fm_expf(-settings->rotation_smoothing_speed * dt);
		camera->rotation = camera->rotation * factor + data->target_rotation * (1.0f - factor);
	}
	else camera->rotation = data->target_rotation;
}

Vector2 toScreen(const Camera2D *camera, Vector2 position, float parallax)
{
	if (camera->type == CAMERA2D_TYPE_NONE) return position;

	/* What the thing takes of the camera's movement is all the parallax is:
	   at zero the view never moves under it and it stays where it was placed. */
	Vector2 view = vector2_scaled(&camera->position, parallax);

	/* World pixels become screen pixels here: a mode whose columns are
	   split in two lays two of them down for every one across. */
	Vector2 scale = viewport_getScale();

	Vector2 screen = {
		(position.x - view.x) * camera->zoom * scale.x + camera->offset.x,
		(position.y - view.y) * camera->zoom * scale.y + camera->offset.y,
	};

	if (camera->anchor == CAMERA2D_ANCHOR_CENTER) {
		screen.x += display_get_width()  * 0.5f;
		screen.y += display_get_height() * 0.5f;
	}

	return screen;
}

Vector2 toWorld(const Camera2D *camera, Vector2 screen)
{
	if (camera->type == CAMERA2D_TYPE_NONE) return screen;

	if (camera->anchor == CAMERA2D_ANCHOR_CENTER) {
		screen.x -= display_get_width()  * 0.5f;
		screen.y -= display_get_height() * 0.5f;
	}

	return (Vector2){
		(screen.x - camera->offset.x) / camera->zoom + camera->position.x,
		(screen.y - camera->offset.y) / camera->zoom + camera->position.y,
	};
}

}

}
