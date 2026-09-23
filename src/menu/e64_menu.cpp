#include "menu/e64_menu.h"
#include "menu/e64_menu_control.h"
#include "controller/e64_controller.h"

namespace e64 {

namespace menu {

static Menu stack;


int8_t getIndex(void) { return stack.index; }
void setIndex(int8_t index) { stack.index = index; }
uint8_t getDepth(void) { return stack.top; }

int8_t getItemCount(void)
{
	if (stack.top == 0) return 0;
	return stack.frame[stack.top - 1].item_count;
}

void moveIndex(int8_t delta, int8_t max)
{
	stack.index += delta;
	if (stack.index < 0) stack.index = max;
	if (stack.index > max) stack.index = 0;
}

void init(void)
{
	stack.index = 0;
	stack.top = 0;
}

void open(const Menu::Def *def)
{
	if (def == NULL) return;

	/* The push is refused when the stack is full: nothing to write the menu
	   into. */
	if (stack.top >= Menu::STACK_MAX) return;

	if (stack.top > 0)
		stack.frame[stack.top - 1].index = stack.index;

	stack.frame[stack.top++] = (Menu::Frame){
		.def = def,
		.index = 0,
		.item_count = (int8_t)def->item_count,
	};
	stack.index = 0;
}

void back(void)
{
	if (stack.top == 0) return;

	stack.top--;
	stack.index = (stack.top > 0)
		? stack.frame[stack.top - 1].index
		: 0;
}

const Menu::Def *get(void)
{
	if (stack.top == 0) return NULL;
	return stack.frame[stack.top - 1].def;
}

void update(const ControlBinding *binding)
{
	const Menu::Def *def = get();
	if (def == NULL || binding == NULL) return;

	Controls controls;
	control::read(&controls, &controller::get()[binding->player], binding);

	if (def->item_count > 0) {
		if (controls.up) moveIndex(-1, (int8_t)def->item_count - 1);
		if (controls.down) moveIndex( 1, (int8_t)def->item_count - 1);
	}

	/* Confirm ends the step: a press that opens a submenu must not reach the
	   cancel below as well. */
	if (controls.confirm && def->item_count > 0) {

		const Menu::ItemDef *item = &def->item[getIndex()];

		if (item->confirm) item->confirm();
		if (item->submenu) open(item->submenu);
		return;
	}

	if (controls.cancel) {
		/* Deeper than the first level there is always somewhere to go back to;
		   on the first one, closing is the menu's own to decide. */
		if (stack.top > 1) back();
		else if (def->cancel) def->cancel();
	}
}

}

}
