#ifndef ENGINE64_MENU_CONTROL_H
#define ENGINE64_MENU_CONTROL_H

#include "e64_controller.h"

/* The menu module reads this binding, so naming its declaration is as far as
   this file can go towards it. */
typedef struct MenuDef MenuDef;

typedef struct MenuControlBinding {

	/* Whose controller moves the cursor. There is one cursor, so the game
	   names the player here, the way it does for the camera. */
	PlayerID player;

	/* Which menu these buttons put up and walk through. */
	const MenuDef *menu;

	ButtonID confirm;
	ButtonID cancel;
	ButtonID pause;
	ButtonID up;
	ButtonID down;
	ButtonID left;
	ButtonID right;
	ButtonID tab_left;
	ButtonID tab_right;

} MenuControlBinding;


typedef struct MenuControls {

	bool confirm;
	bool cancel;
	bool pause;
	bool up;
	bool down;
	/* How hard the direction is pushed, 0 to 1: a button answers 1 while it
	   is down, the stick answers its own travel. What rolls on this rolls at
	   the speed it is asked for. */
	float up_held;
	float down_held;
	bool left;
	bool right;
	bool tab_left;
	bool tab_right;

} MenuControls;


void menuControls_map(MenuControls *controls, const Controller *controller, const MenuControlBinding *binding);
void menuControl_update(void);

#endif
