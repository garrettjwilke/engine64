/*
	The scene's camera: how the world is framed.

	Declared apart from main.c because a camera is reused across scenes more
	often than anything else in them. The scene references it by name, and so
	does the binding that moves it, which is declared with the other controls.

	There are no defaults: any field left out is zero.
*/
#include "camera/e64_camera.h"


/* A spring arm camera hangs at the end of an arm anchored to a target point,
   and orbits that point in yaw and pitch. */
const CameraDef camera = {

	.type    = CAMERA_TYPE_SPRING_ARM,

	/* The lens. Nothing closer than the near plane or farther than the far one
	   is drawn. Depth precision is spread across that range, so keeping it
	   tight is what stops close surfaces from flickering over each other. */
	.field_of_view = 60.0f,   /* vertical lens angle, between 10 and 120 */
	.near_clipping =  1.0f,
	.far_clipping  = 50.0f,

	.spring_arm = {
		.arm_length    = 5.0f,     /* distance from the target, in metres        */
		.side_offset   = 0.0f,     /* shifts the arm sideways, for over-shoulder */
		.height_offset = 1.2f,     /* raises the anchor above the target point   */
		.yaw           = -45.0f,   /* starting orbit angle, in degrees           */
		.pitch         = 15.0f,    /* starting elevation, in degrees             */

		.settings = {
			/* The arm does not jump to where the controls ask: it accelerates
			   towards it. One value per axis, yaw then pitch. */
			.response_rate = {  10.0f,  10.0f },   /* how hard it accelerates */
			.max_velocity  = { 120.0f, 100.0f },   /* degrees per second cap  */
			.direction     = {   1.0f,   1.0f },   /* -1 inverts that axis    */

			.zoom_response_rate = 6.0f,   /* how fast the arm settles at a new length */
			.distance_speed     = 4.0f,   /* metres per second while zooming          */
			.fov_speed          =  30.0f,   /* degrees per second while changing fov  */

			/* How far up and down the orbit is allowed to go, in degrees, so
			   it never ends up under the floor or on its back. */
			.max_pitch =  80.0f,
			.min_pitch = -50.0f,
		},
	},
};
