#include "menu/e64_menu.h"
#include "control/e64_controller.h"


static MenuStack menuStack;


int8_t  menuStack_getIndex(void)         { return menuStack.index; }
void    menuStack_setIndex(int8_t index) { menuStack.index = index; }
uint8_t menuStack_getDepth(void)         { return menuStack.top; }

int8_t menuStack_getItemCount(void)
{
	if (menuStack.top == 0) return 0;
	return menuStack.frame[menuStack.top - 1].item_count;
}

void menuStack_moveIndex(int8_t delta, int8_t max)
{
	menuStack.index += delta;
	if (menuStack.index < 0)   menuStack.index = max;
	if (menuStack.index > max) menuStack.index = 0;
}

void menuStack_init(void)
{
	menuStack.index = 0;
	menuStack.top   = 0;
}

void menuStack_open(int8_t item_count)
{
	if (menuStack.top >= MENU_STACK_MAX) return;

	if (menuStack.top > 0)
		menuStack.frame[menuStack.top - 1].index = menuStack.index;

	menuStack.frame[menuStack.top++] = (MenuStackFrame){
		.index      = 0,
		.item_count = item_count,
	};
	menuStack.index = 0;
}

void menuStack_back(void)
{
	if (menuStack.top == 0) return;

	menuStack.top--;
	menuStack.index = (menuStack.top > 0)
		? menuStack.frame[menuStack.top - 1].index
		: 0;
}


void menu_open(const MenuDef *def)
{
	if (def == NULL) return;

	menuStack_open((int8_t)def->item_count);

	/* The push can be refused when the stack is full, and then there is no
	   frame of ours to write the menu into. */
	if (menuStack.top > 0) menuStack.frame[menuStack.top - 1].def = def;
}

void menu_back(void) { menuStack_back(); }

const MenuDef *menu_get(void)
{
	if (menuStack.top == 0) return NULL;
	return menuStack.frame[menuStack.top - 1].def;
}

void menu_update(const MenuControlBinding *binding)
{
	const MenuDef *def = menu_get();
	if (def == NULL || binding == NULL) return;

	MenuControls controls;
	menuControls_map(&controls, &controller_get()[binding->player], binding);

	if (def->item_count > 0) {
		if (controls.up)   menuStack_moveIndex(-1, (int8_t)def->item_count - 1);
		if (controls.down) menuStack_moveIndex( 1, (int8_t)def->item_count - 1);
	}

	/* Confirm ends the step: a press that opens a submenu must not reach the
	   cancel below as well. */
	if (controls.confirm && def->item_count > 0) {

		const MenuItemDef *item = &def->item[menuStack_getIndex()];

		if (item->confirm) item->confirm();
		if (item->submenu) menu_open(item->submenu);
		return;
	}

	if (controls.cancel) {
		/* Deeper than the first level there is always somewhere to go back to;
		   on the first one, closing is the menu's own to decide. */
		if (menuStack.top > 1)   menu_back();
		else if (def->cancel)    def->cancel();
	}
}
