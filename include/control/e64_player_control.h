#ifndef ENGINE64_PLAYER_CONTROL_H
#define ENGINE64_PLAYER_CONTROL_H

#include "e64_character3d_control.h"
#include "e64_character2d_control.h"
#include "e64_camera_control.h"
#include "e64_menu_control.h"
#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"

typedef struct Viewport Viewport;
typedef struct Player   Player;


/* Everything this game drives with, handed over once before the first state,
   the way the state table itself is. Each binding names what it moves, so
   from here on the engine seats the players, points the camera and puts the
   menu up by itself, every time a scene is loaded. */
typedef struct ControlsDef {

	const CameraControlBinding      *camera;
	const Character3DControlBinding *character3d;
	const Character2DControlBinding *character2d;
	const MenuControlBinding        *menu;

} ControlsDef;

/* Wires what the bindings name against what this scene declared: the seat goes
   to the character built from the prefab they point at, and the camera answers
   if it is the one they point at. Called by the engine when a state is loaded,
   with the controls that state declared. */
void controls_bind(const ControlsDef *controls, const Scene3DDef *scene);

/* The same for the 2D scene, whose characters are spread across its layers. */
void controls_bind2D(const ControlsDef *controls, const Scene2DDef *scene);

/* Reads the buttons this player was seated with and turns them into its
   command for this frame, aimed by the camera. */
void player_setCharacter3DControl(PlayerID id, Viewport *viewport);

/* The same, for a seat driving a 2D body: the screen's own axis needs no
   camera to aim by. */
void player_setCharacter2DControl(PlayerID id);

#endif
