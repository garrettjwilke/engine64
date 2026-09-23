/*
	Menus, as data. A menu is a list of items and what each one does; the
	engine holds the cursor, moves it with the buttons the binding names,
	opens and closes levels, and calls the item that was confirmed.

	What an item does is the game's: the function it names is the game's own,
	and it is the only place a menu decides anything.
*/
#ifndef ENGINE64_MENU_H
#define ENGINE64_MENU_H

#include <stdint.h>

#include "controller/e64_controller.h"

namespace e64 {

/* The menu stack: where the cursor stands on each open level, and the menu
   that level is showing. Opening a submenu pushes a level and remembers the
   one below. */
class Menu {

public:

	/* How deep submenus can nest before open is ignored. */
	static constexpr uint8_t STACK_MAX = 8;

	struct Def;

	/* One line of a menu. Both fields are optional: an item with neither is
	   a label the cursor still stops on. */
	struct ItemDef {

		void (*confirm)(void);

		/* Opened on top of this menu when the item is confirmed, with the
		   cursor of this level kept underneath. Cancel comes back to it. */
		const Def *submenu;

	};

	struct Def {

		const ItemDef *item;
		uint8_t item_count;

		/* Cancel on the first level, where there is nothing to go back to:
		   this is how a menu closes itself. NULL leaves it where it is. */
		void (*cancel)(void);

	};

	struct Frame {

		const Def *def;
		int8_t index;
		int8_t item_count;

	};

	Frame frame[STACK_MAX];
	uint8_t top;
	int8_t index;

};


namespace menu {

typedef struct ControlBinding {

	/* Whose controller moves the cursor. There is one cursor, so the game
	   names the player here, the way it does for the camera. */
	PlayerID player;

	/* Which menu these buttons put up and walk through. */
	const Menu::Def *menu;

	ButtonID confirm;
	ButtonID cancel;
	ButtonID pause;
	ButtonID up;
	ButtonID down;
	ButtonID left;
	ButtonID right;
	ButtonID tab_left;
	ButtonID tab_right;

} ControlBinding;


typedef struct Controls {

	bool confirm;
	bool cancel;
	bool pause;
	bool up;
	bool down;
	float up_held;
	float down_held;
	bool left;
	bool right;
	bool tab_left;
	bool tab_right;

} Controls;


/* Empties the stack: the menu this state opens starts from nothing. */
void init(void);

/* Shows this menu, on top of whatever is open. */
void open(const Menu::Def *def);

/* Closes the level on top and goes back to the one under it, cursor and all. */
void back(void);

/* The menu the cursor is on, NULL while nothing is open. */
const Menu::Def *get(void);

int8_t getIndex(void);
void setIndex(int8_t index);
void moveIndex(int8_t delta, int8_t max);

int8_t getItemCount(void);
uint8_t getDepth(void);

/* One step of the open menu, off the buttons this binding names: the cursor
   moves, a confirmed item runs and opens what it names, and cancel closes the
   level it is on. Call it once per frame from the state that runs the menu. */
void update(const ControlBinding *binding);

}

}

#endif
