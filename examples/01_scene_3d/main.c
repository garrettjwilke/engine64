/*
	3D scene

	Start here. The smallest world engine64 can draw: a room, a lamp post
	standing in it, and a camera to look around with.

	The engine draws through scenes. There are two kinds, 2D and 3D, and the 3D
	one is what this example covers. A 3D scene is four answers:

		what the world contains, and where it stands
		how it is lit
		whether there is fog
		where it is looked at from

	All four are plain data, written below and handed over once.

	It also covers how a model made of several objects can have any of them
	shown and hidden on demand.
*/
#include <libdragon.h>

#include "game/e64_game.h"               /* startup, states, the frame loop  */
#include "scene3d/e64_scene3d.h"         /* the 3D scene and what it holds   */
#include "entity/e64_entity3d.h"         /* one thing placed in the world    */
#include "viewport/e64_viewport.h"       /* what the camera ends up drawing  */
#include "control/e64_controller.h"      /* the pad as the hardware reads it */
#include "control/e64_camera_control.h"  /* buttons wired to camera motion   */
#include "camera/e64_camera.h"           /* where the camera is looking      */
#include "camera/e64_spring_arm.h"       /* reading the arm back for debug   */
#include "time/e64_time.h"               /* how long the last frame took     */
#include "debug/e64_debug.h"             /* the on-screen frame counter      */


/* --- prefabs ---------------------------------------------------------------
	A prefab is one kind of thing the world can contain: a model, plus
	whatever that kind needs to work. It carries no position, which is why the
	same prefab can be placed as many times as wanted.

	Both of these are props: drawn and nothing else. Neither declares a
	collider or a body. Example 02 gives props both.

	The .model path is a file in the ROM's filesystem. The Makefile builds it
	from the .glb left in assets/models.
*/

/* The shortest a prefab gets: a kind and a model, drawn whole. */
static const Prefab3D room = { .type = PREFAB3D_PROP, .model = "rom:/models/room.t3dm" };

/* lamp_post.glb holds two objects, named "post" and "lamp" in Blender.

   Listing a name in .part cuts that object out as a part the game can show and
   hide on its own. Everything left unlisted stays together as one more part,
   always drawn. Seven names is the cap.

   .part_position gives each of those names a position of its own, in the same
   order, and .part_count has to match how many names were listed. A position
   left at zero draws the part where it was modelled and reserves no extra matrix.

   Both objects were modelled at the origin. The post reaches 5.5012 metres and
   the lamp has a radius of 0.75, so their sum is what leaves the lamp resting
   on the tip. The light below is declared at that same height. */
#define LAMP_HEIGHT 6.252f

static const Prefab3D lamp_post = {

	.type          = PREFAB3D_PROP,
	.model         = "rom:/models/lamp_post.t3dm",

	.part = MESH_PARTS(
		"post",
		"lamp"
	),

	.part_position = MESH_PART_POSITIONS(
		{ 0.0f, 0.0f, 0.0f         },
		{ 0.0f, 0.0f, LAMP_HEIGHT  }
	),

	.part_count = 2,
};


/* --- the scene -------------------------------------------------------------
	Everything the world is made of, as data. The engine reads it once, when
	the state opens, and builds the world from it.
*/

/* One row per thing placed: which prefab, then its position, and a rotation
   after that if it needs one. Loading the scene builds each row in order.

   Anything left out of a row is zero, and a zero scale means original size. */
static Scene3DPrefab scene_prefabs[] = {

	{ &room,      { 0.0f, 0.0f, 0.0f } },
	{ &lamp_post, { 0.0f, 0.0f, 0.0f } },
};

/* --- the camera ------------------------------------------------------------*/

/* One button per action. The engine reads this every frame and moves the
   camera itself. An action left at BTN_NONE never happens. */
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

/* A spring arm camera sits on the end of an arm anchored to a point and swings
   around it. Nothing here has a default: a field left out is zero. */
static const CameraDef camera = {

	.type    = CAMERA_TYPE_SPRING_ARM,
	.binding = &camera_binding,

	.field_of_view = 60.0f,   /* how wide the lens is            */
	.near_clipping =  1.0f,   /* nothing nearer than this drawn  */
	.far_clipping  = 50.0f,   /* nothing farther than this drawn */

	.spring_arm = {
		.arm_length    = 6.0f,   /* how far back the camera sits       */
		.side_offset   = 0.0f,   /* pushed off centre, over a shoulder */
		.height_offset = 3.0f,   /* the anchor, raised off the floor   */
		.yaw           = -45.0f,   /* where it starts around the anchor  */
		.pitch         = 15.0f,    /* and how far above it               */

		.settings = {
			/* The arm chases its target instead of snapping to it. One number
			   for turning, one for tilting. */
			.response_rate = {  10.0f,  10.0f },   /* how fast it catches up */
			.max_velocity  = { 120.0f, 100.0f },   /* and how fast it swings */
			.direction     = {   1.0f,   1.0f },   /* -1 inverts that axis   */

			.zoom_response_rate = 6.0f,
			.distance_speed     = 4.0f,   /* pulling in and out */
			.fov_speed          =  30.0f,   /* narrowing the lens */

			.max_pitch =  80.0f,   /* never lands on its back    */
			.min_pitch = -50.0f,   /* never goes under the floor */
		},
	},
};

/* --- the light -------------------------------------------------------------
	Ambient is the colour a surface keeps where nothing shines on it, and the
	only reason the dark side of an object is not black.

	.source holds seven slots, shared between directional and point lights.
	They are read in order and cut at the first empty one.

	A point light shines from its position in every direction, and .size is how
	far it carries. This one sits at the height the lamp was put at.
*/
static const LightDef light = {

	.ambient_color = { 60, 60, 70, 0xFF },

	.source = {
		{ .type  = LIGHT_POINT,
		  .color = { 255, 245, 220, 0xFF },
		  .point = { .position = {{ 0.0f, 0.0f, LAMP_HEIGHT - 0.42f }},
		  			 .size = 25.0f }},
	},
};

/* Distance haze. Off here; example 02 turns it on. A material exported with
   fog disabled ignores this. */
static const FogDef fog = { .enabled = false };

/* The four answers together. This is the scene. */
static Scene3DDef scene = {

	.light  = &light,
	.fog    = &fog,
	.camera = &camera,

	.prefab       = scene_prefabs,
	.prefab_count = sizeof(scene_prefabs) / sizeof(scene_prefabs[0]),
};


/* --- the state -------------------------------------------------------------
	A state is one mode of the game: the title screen, the match, the pause,
	the credits. One runs at a time. Each carries the scene it draws, the
	sprites and fonts it needs, and the function the engine calls every frame.
	Leaving a state frees all of that, and the next one loads its own.

	The engine takes this table at startup, and from then on the only thing it
	calls on its own is the update of whichever state is current.
*/

enum { GAME_STATE_EXAMPLE, STATE_COUNT };

/* The point the camera swings around, walked by the stick. The speed is in
   metres per second, times whatever the stick reads. */
#define CAMERA_TARGET_SPEED 0.1f

static Vector3 camera_target = { 0.0f, 0.0f, 0.0f };

/* Whether each object of the lamp post is being drawn. A button only says "the
   other one now", so the game has to remember where it left them. Both start
   true, which is how the world comes up. */
static bool draw_lamp = true;
static bool draw_post = true;

/* The state's update: the one place the game's own code runs. The engine calls
   it once per frame for as long as this state is in play.

   This one does two things: shows and hides the two objects of the lamp post,
   and walks the point the camera watches around the floor. */
static void GameStateExample_update(void)
{
	Scene3D *scene3d = scene3d_get();

	/* The pad, already polled by the engine, and how long the last frame took.
	   Everything below reads from these two. */
	const Controller *pad = controller_get();
	float delta = time_get()->delta;

	/* From here down the pad is read button by button, and that is the
	   difference with the camera.

	   Moving a camera is something the engine does, so it takes a binding.
	   There is one of those for the camera, one for a 3D character, one for a
	   2D one and one for menus, each binding the actions that module has.

	   Declaring an action of the game's own is on the to do list. Until then,
	   turning a lamp off has nothing to bind to and the game reads the pad
	   itself.

	   .pressed is the frame a button goes down. Held, it reads once and stops,
	   which is what a switch wants.

	   The world is built in the order of the placement table, so the lamp post
	   written second is entity 1. */
	if (button_isPressed(pad, BTN_A)) {
		draw_lamp = !draw_lamp;
		entity3d_setPartVisible(scene3d->entity[1], "lamp", draw_lamp);
	}

	if (button_isPressed(pad, BTN_B)) {
		draw_post = !draw_post;
		entity3d_setPartVisible(scene3d->entity[1], "post", draw_post);
	}

	/* Under the deadzone the stick reads as centred: it never rests at exactly
	   zero.

	   The push is turned by the camera's angle before it is applied, so up on
	   the stick is always away from the viewer. It is what a character does to
	   walk. */
	float x = fabsf(pad->input.stick_x) >= STICK_DEADZONE ? pad->input.stick_x : 0.0f;
	float y = fabsf(pad->input.stick_y) >= STICK_DEADZONE ? pad->input.stick_y : 0.0f;

	if (x != 0.0f || y != 0.0f) {
		float angle = deg_to_rad(camera_getAngleAround(&viewport_get()->camera, &camera_target));
		float sin_a, cos_a;
		fm_sincosf(angle, &sin_a, &cos_a);

		camera_target.x += (x * cos_a + y * sin_a) * CAMERA_TARGET_SPEED * delta;
		camera_target.y += (y * cos_a - x * sin_a) * CAMERA_TARGET_SPEED * delta;
	}

	/* The only thing about the camera the game decides: what it watches. The
	   buttons were declared with it, up in the scene. */
	scene3d_updateCamera(&camera_target);

	/* The debug overlay: free lines down the left, free lines down the right
	   under the framerate. The engine only ever puts the rate there. */
	debugUI_set(0, "A %s lamp", draw_lamp ? "hide" : "show");
	debugUI_set(1, "B %s post", draw_post ? "hide" : "show");

	debugUI_set(3, "STICK move camera target");
	debugUI_set(4, "CBUTTONS orbit camera");
	debugUI_set(5, "L R arm length");
	debugUI_set(6, "DPAD fov");

	debugUI_showFPS();
	debugUI_setRight(0, "arm %d", (int)cameraSpringArm_getLength(&viewport_get()->camera));
	debugUI_setRight(1, "fov %d", (int)viewport_get()->camera.field_of_view);
}

static const GameStateDef states[STATE_COUNT] = {

	[GAME_STATE_EXAMPLE] = {
		.update     = GameStateExample_update,
		.scene3d    = &scene,

		/* The engine opens no screen by itself, so a state that draws has to
		   name one. */
		.viewport   = SCREEN_320x240,

		/* A state can be drawn on top of another one, which is how a pause
		   keeps the game visible behind it. This one stands alone. */
		.overlay_of = GAME_STATE_NONE,
	},
};


int main()
{
	/* Where printed output goes: the emulator's viewer, and the USB cable on a
	   flashcart. Neither is needed to run. */
	debug_init_isviewer();
	debug_init_usblog();

	/* Video, audio, the pads, the physics, the ROM's filesystem: everything
	   that has to be up before a game can run. */
	game_init();

	debugUI_init();

	/* Hand over the whole game and say which state opens. Opening that state
	   is what builds the scene written above. */
	game_start(states, STATE_COUNT, GAME_STATE_EXAMPLE);

	/* One frame per turn, forever: read the pads, step the physics, run the
	   current state's update, draw. */
	for (;;) game_runStep();

	game_close();

	return 0;
}
