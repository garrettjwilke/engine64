/*
	3D character collision

	How to add primitive shape colliders to scene models.

	What this example adds over example 02:

		colliders built from primitive shapes instead of a mesh
		friction and restitution declared per shape
		a character with three gaits, sprint and stamina
		content split one file per piece

	The room's collision is a triangle mesh built by the importer from the same
	.glb, as in example 02. Primitives are the other way to declare collision:
	measurements written in code, with no asset behind them.

	Content lives one file per piece: room and character under prefabs/, the
	light, the fog and the camera under scene/, the button bindings under
	controls/. What stays here is the placement table and the state that runs
	the frame.

	The primitives are declared here, next to the placements that use them.
*/
#include <libdragon.h>

#include "game/e64_game.h"               /* init, state table, frame step    */
#include "scene3d/e64_scene3d.h"         /* scene declaration and live scene */
#include "entity/e64_entity3d.h"         /* colliders and shapes for a prefab */
#include "viewport/e64_viewport.h"       /* screen modes and the live camera  */
#include "camera/e64_spring_arm.h"       /* reading the arm back for debug    */
#include "player/e64_player.h"           /* the seat a controller drives      */
#include "control/e64_camera_control.h"  /* buttons wired to camera motion    */
#include "control/e64_player_control.h"  /* the controls a state declares     */
#include "time/e64_time.h"               /* frame delta                       */
#include "debug/e64_debug.h"             /* on-screen debug lines             */


/* Declared in the other files of this example. */
extern const Prefab3D room;
extern const Prefab3D character;

extern const CameraDef camera;
extern const LightDef  light;
extern const FogDef    fog;

extern const ControlsDef controls;


/* --- the primitives --------------------------------------------------------
	Collision is declared in two steps, and both are needed. A PhysicsShapeDef
	is one solid: its kind, the measurements that kind takes, and what it does
	on contact. An Entity3DColliderDef is the array of those shapes plus how
	many there are, and that is what the prefab points at. They are separate
	because one body can carry several shapes, each with its own offset, so a
	prefab always takes a collider even when it holds a single shape.

	Shapes are in metres.
*/

/* --- capsule --------------------------------------------------------------*/

/* Half height is the segment between the two caps, so the whole capsule stands
   radius plus half height either side of its centre. Raised by that centre, it
   rests on the floor. */
static const PhysicsShapeDef capsule_shapes[] = {
	{ .type = SHAPE_CAPSULE, .capsule = {
		.tx          = { .position = { 0.0f, 0.0f, 0.90f } },
		.radius      = 0.35f,
		.half_height = 0.55f,
	}},
};

static const Entity3DColliderDef capsule_collider = { capsule_shapes, 1 };

static const Prefab3D capsule = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/capsule.t3dm",
	.collider = &capsule_collider,
};

/* --- box ------------------------------------------------------------------*/

/* Box extents are measured from the centre out, so a one metre cube is half a
   metre on each axis. */
static const PhysicsShapeDef box_shapes[] = {
	{ .type = SHAPE_BOX, .box = {
		.e           = { 0.5f, 0.5f, 0.5f },
	}},
};

static const Entity3DColliderDef box_collider = { box_shapes, 1 };

static const Prefab3D cube = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/cube.t3dm",
	.collider = &box_collider,
};

/* --- sphere ---------------------------------------------------------------*/

static const PhysicsShapeDef sphere_shapes[] = {
	{ .type = SHAPE_SPHERE, .sphere = {
		.radius      = 0.5f,
	}},
};

static const Entity3DColliderDef sphere_collider = { sphere_shapes, 1 };

static const Prefab3D sphere = {

	.type     = PREFAB3D_PROP,
	.model    = "rom:/models/sphere.t3dm",
	.collider = &sphere_collider,
};


/* --- the scene -------------------------------------------------------------*/

/* One row per placement: which prefab, then where it stands. Declaring them
   here is the whole job: the load builds each one in order, registers it in
   the physics and draws it, with nothing else to call. What is left out stays
   zero, and a zero scale means original size. */
static Scene3DPrefab scene_prefabs[] = {

	{ &character, { 0.0f, -6.0f, 0.0f } },

	{ &capsule, {   0.0f, 0.0f, 0.0f } },
	{ &cube,    { -10.0f, 0.0f, 1.0f }, {0}, { 2.0f, 2.0f, 2.0f } },
	{ &sphere,  {  10.0f, 0.0f, 1.0f }, {0}, { 2.0f, 2.0f, 2.0f } },

	{ &room },
};

static Scene3DDef scene = {

	.light  = &light,
	.fog    = &fog,
	.camera = &camera,

	.prefab       = scene_prefabs,
	.prefab_count = sizeof(scene_prefabs) / sizeof(scene_prefabs[0]),
};


/* --- the state -------------------------------------------------------------
	A state is one mode of the game: it carries the scene it draws and the
	function the engine calls every frame. This one is the only mode here,
	and its update is where the physics runs, the body is driven and the
	camera follows it.
*/

enum { GAMEPLAY3D, STATE_COUNT };

static void gameplay3d_update(void)
{
	Viewport *viewport = viewport_get();
	float delta = time_get()->delta;

	player_setCharacter3DControl(PLAYER_1, viewport);
	player_update();
	
	scene3d_updateCharacters(viewport->fb_index);

	cameraControl_update(&viewport->camera, viewport->camera.binding, scene3d_get(), delta);
	viewport_setPerspectiveCamera();

	/* Debug lines have to be rewritten every frame; nothing persists.
	*/
	debugUI_set(0, "STICK walk");
	debugUI_set(1, "A jump");
	debugUI_set(2, "Z sprint");
	debugUI_set(4, "CBUTTONS orbit camera");
	debugUI_set(5, "L R arm length");
	debugUI_set(6, "DPAD fov");

	debugUI_showFPS();
	debugUI_setRight(0, "arm %.1f", cameraSpringArm_getLength(&viewport->camera));
	debugUI_setRight(1, "fov %.1f", viewport->camera.field_of_view);
}

static const GameStateDef states[STATE_COUNT] = {

	[GAMEPLAY3D] = {
		.update        = gameplay3d_update,
		.scene3d       = &scene,

		/* Wired once, after the scene is loaded and before the first update:
		   the player is seated on the body its binding names, and the camera
		   answers to the buttons that name it. */
		.controls      = &controls,

		/* The engine opens no screen by itself, so a state that draws has to
		   name one. */
		.viewport      = SCREEN_320x240,
	},
};


int main()
{
	debug_init_isviewer();
	debug_init_usblog();

	game_init();

	debugUI_init();

	game_start(states, STATE_COUNT, GAMEPLAY3D);

	for (;;) game_runStep();

	game_close();

	return 0;
}
