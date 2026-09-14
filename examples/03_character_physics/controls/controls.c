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
#include "control/e64_player_control.h"

#include "camera/e64_camera.h"
#include "prefab/e64_prefab3d.h"


extern const CameraDef camera;
extern const Prefab3D  character;


/* Naming the player here is also what the camera follows: it tracks the body
   seated in that slot, with no target passed in from the game. */
const CameraControlBinding camera_binding = {

	.player = PLAYER_1,
	.camera = &camera,

	.pan_left  = BTN_C_LEFT,
	.pan_right = BTN_C_RIGHT,
	.tilt_up   = BTN_C_UP,
	.tilt_down = BTN_C_DOWN,

	.distance_in  = BTN_L,
	.distance_out = BTN_R,

	.fov_in    = BTN_D_UP,
	.fov_out   = BTN_D_DOWN,
};

/* The prefab it drives is what seats the player: the scene builds one body
   from it, and that body is the one this controller moves.

   Only what this body can do. Aiming, shooting and weapon switching are left
   out, so those buttons read as never pressed. */
const Character3DControlBinding character3d_binding = {

	.player    = PLAYER_1,
	.character = &character,

	.jump   = BTN_A,
	.sprint = BTN_Z,
};

/* The state names this, and the engine wires both when it is entered. */
const ControlsDef controls = {

	.camera      = &camera_binding,
	.character3d = &character3d_binding,
};
