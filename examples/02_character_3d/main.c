/*
	3D character

	A body in a room, on the pad: the stick walks it, A jumps, and the camera
	trails it.

	A character is its settings and nothing else — the engine brings no movement
	of its own — so what is declared below is the whole of how this one moves.

	A single gait is declared, which is the case where the speed follows the
	stick instead of stepping between gaits. The room carries a collision mesh
	because a body falls through anything that has none; collision and the
	solver are the subject of the next example.
*/
#include <libdragon.h>

#include "game/e64_game.h"               /* startup, states, the frame loop  */
#include "scene3d/e64_scene3d.h"         /* the 3D scene and what it holds   */
#include "entity/e64_entity3d.h"         /* one thing placed in the world    */
#include "viewport/e64_viewport.h"       /* what the camera ends up drawing  */
#include "player/e64_player.h"           /* who is holding the pad           */
#include "control/e64_controller.h"      /* the pad as the hardware reads it */
#include "control/e64_camera_control.h"  /* buttons wired to camera motion   */
#include "control/e64_character3d_control.h"  /* buttons wired to a body     */
#include "control/e64_player_control.h"  /* handing the pad to the character */
#include "camera/e64_spring_arm.h"       /* reading the arm back for debug   */
#include "time/e64_time.h"               /* how long the last frame took     */
#include "debug/e64_debug.h"             /* the on-screen frame counter      */


/* Three of the scene's four answers live in scene/, one per file. The fourth,
   what the world holds and where, is the placement table further down. */
extern const LightDef  light;
extern const FogDef    fog;
extern const CameraDef camera;


/* --- the room --------------------------------------------------------------
	Floor and walls, plus the triangle mesh the body stands on. Both come from
	the same .glb: the model importer writes what is drawn, the collision
	importer what is collided against, and the Makefile asks for the second one
	by naming it under assets_collision.
*/

static const PhysicsShapeDef room_shapes[] = {

	{ .type = SHAPE_MESH, .mesh = { .path = "rom:/collision/room.collision" }},
};

static const Entity3DColliderDef room_collider = { room_shapes, 1 };

static const Prefab3D room = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/room.t3dm",
	.collider = &room_collider,
};


/* --- the character ---------------------------------------------------------
	An upright capsule. The numbers are in metres: a scene is written in the
	units the physics runs in.
*/

/* A button per action, for one player's pad. What this body cannot do is left
   out and reads as never pressed. */
static const Character3DControlBinding character3d_binding = {

	.player = PLAYER_1,
	.jump = BTN_A,
};

/* A gait is a speed the body settles into, with its own rates for reaching it
   and for turning. Declared alone it has no step above it to reach, so how far
   the stick is held is the share of this speed being asked for: the body walks
   anywhere between a crawl and four metres a second. */
static const Character3DGaitSettings gait[] = {

	{ .target_speed = 7.0f, .response_rate = 8.0f, .rotation_response_rate = 12.0f },
};

static const Character3DMovementSettings movement = {

	.idle_response_rate          = 12.0f,
	.idle_rotation_response_rate = 10.0f,

	.gait       = gait,
	.gait_count = E64_ARRAY_COUNT(gait),

	/* Snap: the body leaves the floor the frame the button goes down, and while
	   it stays down the rise pays less gravity, so a tap and a held press reach
	   different heights. The other mode crouches first and launches on release.
	*/
	.jump_mode               = JUMP_SNAP,
	.jump_response_rate      = 10.0f,
	.jump_base_speed         =  9.0f,
	.jump_hold_gravity_scale =  0.7f,

	/* What the stick is worth in the air against what it is worth on the
	   ground. At zero the jump keeps the run that launched it. */
	.air_control = 0.8f,
};

/* The capsule the body collides with, upright, in metres. */
static const Character3DColliderSettings collider = {

	.radius = 0.35f,
	.height = 1.80f,
};

/* The character: every set of settings it is built from. */
static const Character3DDef character3d_def = {

	.movement_settings = &movement,
	.collider_settings = &collider
};

/* What a scene places: the model and the character built with it. No position,
   so the same one can be placed as many times as wanted. */
static const Prefab3D character = {

	.type      = PREFAB3D_CHARACTER,
	.model     = "rom:/models/capsule.t3dm",
	.character = &character3d_def,
};



/* --- the scene -------------------------------------------------------------
	Which prefab stands where. A prefab says what a thing is and carries no
	position, so placing the same one twice is another row.

	They are built in the order written, and that order is the index each one
	keeps in the live scene. A field left out of a row is zero, and a zero
	scale means the size it was modelled at.
*/

static Scene3DPrefab scene_prefabs[] = {

	{ &room },
	{ &character, { 0.0f, 0.0f, 0.0f } },
};

static Scene3DDef scene = {

	.light  = &light,
	.fog    = &fog,
	.camera = &camera,

	.prefab       = scene_prefabs,
	.prefab_count = E64_ARRAY_COUNT(scene_prefabs),
};


/* --- the state -------------------------------------------------------------
	A state is one mode of the game: the title screen, the match, the pause.
	One runs at a time, and it carries the scene it draws and the function the
	engine calls every frame.
*/

enum { GAME_STATE_EXAMPLE, STATE_COUNT };

/* Run once the scene is built, which is the first moment there is a body to
   hand over. Characters are created in placement order and this is the only
   one, so it is the one at index 0. */
static void GameStateExample_bindCharacter(void)
{
	player_setCharacter3D(scene3d_getCharacter3D(0), &character3d_binding);
}

static void GameStateExample_update(void)
{
	Viewport *viewport = viewport_get();

	/* Two halves of the same step: the first turns this frame's buttons into
	   what the body is being asked to do, the second is where it answers. */
	player_setCharacter3DControl(PLAYER_1, viewport);
	player_update();

	/* A character is not placed by the solver: it collides itself against the
	   world and writes its own matrix, which is what this does. */
	scene3d_updateCharacters(viewport->fb_index);

	/* The camera was declared with its buttons, and its binding names the
	   player whose body it follows. */
	cameraControl_update(&viewport->camera, viewport->camera.binding, scene3d_get(), time_get()->delta);
	viewport_setPerspectiveCamera();

	debugUI_set(0, "STICK walk");
	debugUI_set(1, "A jump");
	debugUI_set(2, "CBUTTONS orbit camera");
	debugUI_set(3, "L R arm length");
	debugUI_set(4, "DPAD fov");

	debugUI_showFPS();
	debugUI_setRight(0, "arm %d", (int)cameraSpringArm_getLength(&viewport->camera));
	debugUI_setRight(1, "fov %d", (int)viewport->camera.field_of_view);
}

static const GameStateDef states[STATE_COUNT] = {

	[GAME_STATE_EXAMPLE] = {
		.update        = GameStateExample_update,
		.bindCharacter = GameStateExample_bindCharacter,
		.scene3d       = &scene,

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
