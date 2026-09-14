/*
	Where the world is looked at from, and what the player can do about it.

	A scene is four answers: what the world contains and where it stands, how
	it is lit, whether there is fog, and where it is looked at from. The first
	one is the prefabs next door; this file is the last one.

	None of this has a default. A field left out is zero and the camera behaves
	accordingly, so everything it needs is written here.
*/
#include "camera/e64_camera.h"
#include "control/e64_camera_control.h"


/* Naming the buttons is the whole job: the engine reads this every frame and
   turns, pulls back and zooms the camera itself. An action left at BTN_NONE
   simply never happens. */
static const CameraControlBinding camera_binding = {

	.player = PLAYER_1,

	.pan_left  = BTN_C_LEFT,
	.pan_right = BTN_C_RIGHT,
	.tilt_up   = BTN_C_UP,
	.tilt_down = BTN_C_DOWN,

	.distance_in  = BTN_L,
	.distance_out = BTN_R,

	.fov_in    = BTN_D_UP,
	.fov_out   = BTN_D_DOWN,
};

/* A spring arm camera hangs off the end of an arm anchored to a point in the
   world, and swings around that point instead of moving freely. */
const CameraDef camera = {

	.type    = CAMERA_TYPE_SPRING_ARM,
	.binding = &camera_binding,

	/* The lens. Nothing nearer than the near plane or farther than the far one
	   is drawn. The console stores depth with limited precision, so the wider
	   that range, the coarser it gets, and surfaces close to each other start
	   flickering over which one is in front. */
	.field_of_view = 60.0f,
	.near_clipping =  1.0f,
	.far_clipping  = 50.0f,

	.spring_arm = {
		.arm_length    = 5.0f,   /* how far back the camera sits       */
		.side_offset   = 0.0f,     /* pushed off centre, over a shoulder */
		.height_offset = 1.2f,   /* the anchor, raised off the floor   */
		.yaw           = -45.0f,   /* where it starts around the anchor  */
		.pitch         = 15.0f,    /* and how far above it               */

		.settings = {
			/* The arm chases its target instead of snapping to it. A higher
			   response catches up sooner, and the maximum caps how fast it can
			   be swung. Two numbers each: turning and tilting. */
			.response_rate = {  10.0f,  10.0f },
			.max_velocity  = { 120.0f, 100.0f },
			.direction     = {   1.0f,   1.0f },   /* -1 inverts that axis */

			.zoom_response_rate = 6.0f,
			.distance_speed     = 4.0f,   /* pulling in and out */
			.fov_speed          =  30.0f,   /* narrowing the lens */

			/* How far up and down it is allowed to go, so it never ends up
			   under the floor or on its back. */
			.max_pitch =  80.0f,
			.min_pitch = -50.0f,
		},
	},
};
