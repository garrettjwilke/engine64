/*
	3D scene

	How to build a simple 3D scene, in this case 3 models; a room, a post and a lamp.

	The engine has no world of its own. Everything a scene contains is declared
	as static data and handed over once, and the engine builds the live scene
	from it when the state that owns it is entered:

		a Prefab3D per kind of thing the world contains
		a scene3d::Def listing where each one is placed, plus light, fog and camera
		a Game::State::Def naming that scene, the screen, and an update function
		game::state::start with the state table, then game::runStep once per frame

	Nothing below is allocated or registered by hand. Entering the state loads
	the scene, leaving it frees everything the scene brought.

	The update function also shows how a model built from several objects can
	have each of them drawn or skipped at runtime.
*/
#include <libdragon.h>

#include "game/e64_game.h" /* init, state table, frame step */
#include "game/e64_game_states.h"
#include "scene3d/e64_scene3d.h" /* scene declaration and live scene */
#include "entity/e64_entity3d.h" /* one prefab placed in the world */
#include "viewport/e64_viewport.h" /* screen modes and the live camera */
#include "controller/e64_controller.h" /* controller state and buttons */
#include "camera/e64_camera3d_control.h" /* buttons wired to camera motion */
#include "controller/e64_controls.h" /* handing the controls over */
#include "camera/e64_camera3d.h" /* camera declaration */
#include "camera/e64_spring_arm.h" /* reading the arm back for debug */
#include "time/e64_time.h" /* frame delta */
#include "debug/e64_debug.h" /* on-screen debug lines */


/* --- prefabs ---------------------------------------------------------------
	A Prefab3D declares one kind of content: a model, the type tag that says
	how the engine treats it, and whatever that type needs. It holds no
	transform, so the same prefab can be placed any number of times.

	PREFAB3D_PROP with no .collider and no .prop body is static scenery: drawn,
	never simulated, no collision. Adding physics to a prop is example 02.

	.model is a path into the ROM filesystem. The Makefile compiles the .glb
	files in assets/models into the .t3dm files named here.
*/

/* The smallest prefab there is: a type and a model, drawn as one piece. */
static const e64::Prefab3D room = { .type = e64::prefab3d::PREFAB3D_PROP, .model = "rom:/models/room.t3dm" };

/* lamp_post.glb contains two objects, named "post" and "lamp" in Blender.

   A model is drawn as a single unit unless the prefab lists part names. Each
   name in .part becomes a part that can be drawn or skipped independently at
   runtime, matched against the object names inside the model. Everything the
   list does not name is grouped into one remaining part that is always drawn.
   Up to seven names can be listed.

   .part_position places each named part inside the entity, in the same order
   as the names, and .part_count must equal the number of names listed. A part
   left at zero is drawn exactly where it was modelled.

   Here both objects were modelled at the origin, so the lamp needs an offset
   to sit on top of the post: 5.5012 of post plus 0.75 of lamp radius. The
   point light further down uses the same height. */
#define LAMP_HEIGHT 6.252f

static const char *const lamp_post_parts[] = {
	"post",
	"lamp",
};

static const e64::Vector3 lamp_post_part_positions[] = {
	{ 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, LAMP_HEIGHT },
};

static const e64::Prefab3D lamp_post = {

	.type = e64::prefab3d::PREFAB3D_PROP,
	.model = "rom:/models/lamp_post.t3dm",

	.part = lamp_post_parts,
	.part_position = lamp_post_part_positions,
	.part_count = 2,
};


/* --- the scene -------------------------------------------------------------
	The content of the world, as data. Read once when the state is entered.
*/

/* The scene's entities: one row each, naming a prefab followed by position,
   rotation and scale. The scene is built walking this table in order, and
   the live entities keep that order, so the entity created from row N is
   scene3d::get()->entity[N].

   Fields left out of a row are zero, and a zero scale means original size. */
static e64::scene3d::Entity scene_entities[] = {

	{ &room, { 0.0f, 0.0f, 0.0f } },
	{ &lamp_post, { 0.0f, 0.0f, 0.0f } },
};

/* --- the camera ------------------------------------------------------------*/

/* A spring arm camera hangs at the end of an arm anchored to a target point,
   and orbits that point in yaw and pitch. The game only supplies the point,
   once per frame; everything else happens inside the engine.

   There are no defaults: any field left out is zero. */
static const e64::camera3d::Def camera = {

	.type = e64::camera3d::CAMERA_TYPE_SPRING_ARM,

	.field_of_view = 60.0f, /* vertical lens angle, between 10 and 120 */
	.near_clipping = 1.0f, /* closer than this is not drawn */
	.far_clipping = 50.0f, /* farther than this is not drawn */

	.spring_arm = {
		.arm_length = 6.0f, /* distance from the target, in metres */
		.side_offset = 0.0f, /* shifts the arm sideways, for over-shoulder */
		.yaw = -45.0f, /* starting orbit angle, in degrees */
		.pitch = 15.0f, /* starting elevation, in degrees */
		.height_offset = 3.0f, /* raises the anchor above the target point */

		.settings = {
			/* The arm does not jump to where the controls ask: it accelerates
			   towards it. One value per axis, yaw then pitch. */
			.response_rate = { 10.0f, 10.0f }, /* how hard it accelerates */
			.max_velocity = { 120.0f, 100.0f }, /* degrees per second cap */
			.direction = { 1.0f, 1.0f }, /* -1 inverts that axis */

			.zoom_response_rate = 6.0f, /* how fast the arm settles at a new length */
			.distance_speed = 4.0f, /* metres per second while zooming */
			.fov_speed = 30.0f, /* degrees per second while changing fov */

			.max_pitch = 80.0f, /* how far above the target it can climb */
			.min_pitch = -50.0f, /* and how far below it can drop */
		},
	},
};

/* One button per camera action, and the camera they move. The engine reads
   this binding every frame and applies the motion itself, so the game never
   moves the camera. Any action left unset is BTN_NONE and never triggers. */
static const e64::camera3d::ControlBinding camera_binding = {

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

/* What this state drives with, named in its declaration further down. Each
   binding names the piece it moves, so entering the state is all it takes for
   the engine to wire them: there is nothing to bind by hand. */
static const e64::controls::Def controls = {

	.camera = &camera_binding,
};

/* --- the light -------------------------------------------------------------
	Ambient is the flat colour every surface keeps regardless of what reaches
	it, and the only thing lighting the faces no light hits.

	.source holds seven slots shared by every light type. They are read in
	order and stop at the first empty one, so a scene only pays for what it
	declares.

	A point light radiates in all directions from .position, and .size is the
	radius it reaches. This one sits just under the lamp part placed above.
*/
static const e64::light::Def light = {

	.ambient_color = { 60, 60, 70, 0xFF },

	.source = {
		{ .type = e64::light::LIGHT_POINT,
		  .color = { 255, 245, 220, 0xFF },
		  .point = { .position = {{ 0.0f, 0.0f, LAMP_HEIGHT - 0.42f }},
		  			 .size = 25.0f }},
	},
};

/* Distance fog: a surface blends towards .color the farther it is from the
   camera, untouched up to .near and fully replaced past .far. Both in metres,
   measured along the view axis.

   Keep the range inside the camera's far plane. Fog that saturates past it
   never finishes, and geometry is cut at the plane anyway, so the room's back
   wall would pop out of a haze that never closed.

   Enabling fog also paints the background: the frame is cleared to .color, so
   what fades out in the distance matches what is behind it. With fog off the
   background is black.

   Materials have their own say. One exported with fog disabled ignores what
   the scene declares, so an asset that has to take fog needs it enabled back
   in Blender. */
static const e64::fog::Def fog = {

	.color = { 70, 80, 100, 0xFF },
	.near = 5.0f,
	.far = 45.0f,
	.enabled = true,
};

/* The scene itself: content plus the three things that decide how it looks. */
static e64::scene3d::Def scene = {

	.light = &light,
	.fog = &fog,
	.camera = &camera,

	.entity = scene_entities,
	.entity_count = sizeof(scene_entities) / sizeof(scene_entities[0]),
};


/* --- the state -------------------------------------------------------------
	A game state is one mode of the game: title screen, gameplay, pause,
	credits. Exactly one is current. Each declares the scenes it runs, the
	screen it uses, and the function the engine calls every frame while it is
	current. Switching states unloads everything the old one loaded.

	The engine calls nothing of the game's on its own except the update of the
	current state.
*/

enum { GAMEPLAY3D, STATE_COUNT };

/* The point the camera orbits, moved with the stick. Metres per second, scaled
   by how far the stick is pushed. */
#define CAMERA_TARGET_SPEED 0.1f

static e64::Vector3 camera_target = { 0.0f, 0.0f, 0.0f };

/* Part visibility is set, not toggled, so the game keeps the current value of
   each one. Both parts are visible when the scene loads. */
static bool draw_lamp = true;
static bool draw_post = true;

/* The state update: the only place this game's own code runs. Called once per
   frame while this state is current, after the controllers are polled and the
   physics has stepped, before the frame is drawn.

   It does two things: switches the two parts of the lamp post on and off, and
   walks the point the camera orbits across the floor. */
static void gameplay3d_update(void)
{
	e64::Scene3D *scene3d = e64::scene3d::get();

	/* The controller, already polled by the engine this frame, and how long
	   the previous frame took, in seconds. */
	const e64::Controller *pad = e64::controller::get();
	float delta = e64::time::get()->delta;

	/* Buttons are read directly here, unlike the camera above.

	   Moving a camera is engine work, so it is declared as a binding and the
	   engine does it. There is a binding type per module that moves something:
	   camera, 3D character, 2D character, menu. Toggling a lamp belongs to no
	   module, so there is nothing to bind it to and the game reads the button
	   itself.

	   controller::isPressed is true only on the frame the button goes down,
	   which is what a toggle needs; controller::isHeld would fire every frame.

	   entity[1] is the lamp post because it is the second row of the placement
	   table. Parts are addressed by the name the prefab declared, and setting
	   a name the entity does not have does nothing. */
	if (e64::controller::isPressed(pad, e64::BTN_A)) {
		draw_lamp = !draw_lamp;
		e64::entity3d::setPartVisible(scene3d->entity[1], "lamp", draw_lamp);
	}

	if (e64::controller::isPressed(pad, e64::BTN_B)) {
		draw_post = !draw_post;
		e64::entity3d::setPartVisible(scene3d->entity[1], "post", draw_post);
	}

	/* Raw stick values run from -127 to 127 and never rest at exactly zero, so
	   anything under the deadzone is discarded.

	   The stick vector is then rotated by the camera's angle around the target
	   before being applied, which makes up on the stick mean away from the
	   camera at any orbit angle. A character controller does the same thing. */
	float x = fabsf(pad->input.stick_x) >= STICK_DEADZONE ? pad->input.stick_x : 0.0f;
	float y = fabsf(pad->input.stick_y) >= STICK_DEADZONE ? pad->input.stick_y : 0.0f;

	if (x != 0.0f || y != 0.0f) {
		float angle = e64::deg_to_rad(e64::camera3d::getAngleAround(&e64::viewport::get()->camera, &camera_target));
		float sin_a, cos_a;
		fm_sincosf(angle, &sin_a, &cos_a);

		camera_target.x += (x * cos_a + y * sin_a) * CAMERA_TARGET_SPEED * delta;
		camera_target.y += (y * cos_a - x * sin_a) * CAMERA_TARGET_SPEED * delta;
	}

	/* Hands the camera the point to orbit for this frame. Reading the buttons,
	   accelerating the arm and applying the result all happen inside. Call it
	   once per frame from any state that has a 3D camera. */
	e64::scene3d::updateCamera(&camera_target);

	/* Debug lines: numbered slots down the left, numbered slots down the right
	   under the framerate. Skipped numbers leave blank rows. Each line has to
	   be rewritten every frame; nothing persists. */
	e64::debug::ui::set(0, "A %s lamp", draw_lamp ? "hide" : "show");
	e64::debug::ui::set(1, "B %s post", draw_post ? "hide" : "show");

	e64::debug::ui::set(3, "STICK move camera target");
	e64::debug::ui::set(4, "CBUTTONS orbit camera");
	e64::debug::ui::set(5, "L R arm length");
	e64::debug::ui::set(6, "DPAD fov");

	e64::debug::ui::showFPS();
	e64::debug::ui::setRight(0, "arm %.1f", e64::camera3d::springArm::getLength(&e64::viewport::get()->camera));
	e64::debug::ui::setRight(1, "fov %.1f", e64::viewport::get()->camera.field_of_view);
}

static const e64::Game::State::Def states[STATE_COUNT] = {

	[GAMEPLAY3D] = {
		.update = gameplay3d_update,
		.scene3d = &scene,
		.controls = &controls,

		/* The engine opens no screen by itself, so every state that draws has
		   to name a mode. Entering a state that names the current mode leaves
		   the screen as it is. */
		.viewport = SCREEN_320x240
	},
};


int main()
{
	/* Where printf output goes: the emulator's debug viewer, and USB on a
	   flashcart. Both optional, neither affects the game. */
	debug_init_isviewer();
	debug_init_usblog();

	/* Brings up video, audio, controllers, physics and the ROM filesystem.
	   Runs before anything else the engine offers. */
	e64::game::init();

	e64::debug::ui::init();

	/* Hands over the state table and enters the initial state, which is what
	   loads the scene declared above. Everything after this point is driven by
	   the state that is current. */
	e64::game::state::start(states, STATE_COUNT, GAMEPLAY3D);

	/* One frame per iteration: poll the controllers, step the physics, run the
	   current state's update, draw. */
	for (;;) e64::game::runStep();

	e64::game::close();

	return 0;
}
