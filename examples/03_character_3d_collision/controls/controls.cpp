/*
	What each button does, one binding per thing that moves.

	A binding is a mapping plus a target: the player whose controller is read,
	the piece it moves, and a button per action the module offers. The engine
	reads them when the state is entered and again every frame, so nothing here
	is called from the game.

	An action left out is BTN_NONE and never fires. The stick is not bound in
	either of them: it always drives the body, and the camera takes its angle
	from where it already is.
*/
#include "controller/e64_controls.h"

#include "camera/e64_camera3d.h"
#include "scene3d/e64_scene3d.h"


extern const e64::camera3d::Def camera;
extern e64::scene3d::Entity scene_entities[];


/* Naming the player here is also what the camera follows: it tracks the body
   seated in that slot, with no target passed in from the game. */
extern const e64::camera3d::ControlBinding camera_binding = {

	.player = e64::PLAYER_1,
	.camera = &camera,

	.pan_left = e64::BTN_C_LEFT,
	.pan_right = e64::BTN_C_RIGHT,
	.tilt_up = e64::BTN_C_UP,
	.tilt_down = e64::BTN_C_DOWN,

	.distance_in = e64::BTN_L,
	.distance_out = e64::BTN_R,

	.fov_in = e64::BTN_D_UP,
	.fov_out = e64::BTN_D_DOWN,
};

/* The scene entity it drives is what seats the player: the scene builds a
   body from that row, and that body is the one this controller moves. The
   character is the first row of the entity table in main.cpp.

   Only what this body can do. Aiming, shooting and weapon switching are left
   out, so those buttons read as never pressed. */
extern const e64::character3d::ControlBinding character3d_binding = {

	.player = e64::PLAYER_1,
	.character = &scene_entities[0],

	.jump = e64::BTN_A,
	.sprint = e64::BTN_Z,
};

/* The state names this, and the engine wires both when it is entered. */
extern const e64::controls::Def controls = {

	.camera = &camera_binding,
	.character3d = &character3d_binding,
};
