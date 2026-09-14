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

#include "control/e64_menu_control.h"

/* How deep submenus can nest before open is ignored. */
#define MENU_STACK_MAX 8

/* One line of a menu. Both fields are optional: an item with neither is a
   label the cursor still stops on. */
typedef struct MenuItemDef {

	void (*confirm)(void);

	/* Opened on top of this menu when the item is confirmed, with the cursor
	   of this level kept underneath. Cancel comes back to it. */
	const MenuDef *submenu;

} MenuItemDef;


struct MenuDef {

	const MenuItemDef *item;
	uint8_t            item_count;

	/* Cancel on the first level, where there is nothing to go back to: this is
	   how a menu closes itself. NULL leaves it where it is. */
	void (*cancel)(void);
};


/* Where the cursor stands on one level, and the menu that level is showing.
   Opening a submenu pushes a level and remembers the one below. */
typedef struct MenuStackFrame {

	const MenuDef *def;
	int8_t         index;
	int8_t         item_count;

} MenuStackFrame;

typedef struct {

	MenuStackFrame frame[MENU_STACK_MAX];
	uint8_t        top;
	int8_t         index;

} MenuStack;


/* Empties the stack: the menu this state opens starts from nothing. */
void menuStack_init(void);
void menuStack_open(int8_t item_count);
void menuStack_back(void);

int8_t menuStack_getIndex(void);
void   menuStack_setIndex(int8_t index);
void   menuStack_moveIndex(int8_t delta, int8_t max);

int8_t  menuStack_getItemCount(void);
uint8_t menuStack_getDepth(void);


/* Shows this menu, on top of whatever is open. */
void menu_open(const MenuDef *def);

/* Closes the level on top and goes back to the one under it, cursor and all. */
void menu_back(void);

/* The menu the cursor is on, NULL while nothing is open. */
const MenuDef *menu_get(void);

/* One step of the open menu, off the buttons this binding names: the cursor
   moves, a confirmed item runs and opens what it names, and cancel closes the
   level it is on. Call it once per frame from the state that runs the menu. */
void menu_update(const MenuControlBinding *binding);

#endif
