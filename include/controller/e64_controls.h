#ifndef ENGINE64_CONTROLS_H
#define ENGINE64_CONTROLS_H

#include "camera/e64_camera3d_control.h"
#include "character3d/e64_character3d_control.h"
#include "character2d/e64_character2d_control.h"
#include "menu/e64_menu_control.h"
#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"

namespace e64 {

/* Everything a state drives with. Each binding names the player it reads and
   the piece it moves; the scene load takes them and seats the player on the
   body it builds from the placement the binding names, and hands the camera
   its binding. */
namespace controls {

typedef struct Def {

	const camera3d::ControlBinding *camera;
	const character3d::ControlBinding *character3d;
	const character2d::ControlBinding *character2d;
	const menu::ControlBinding *menu;

} Def;

}

}

#endif
