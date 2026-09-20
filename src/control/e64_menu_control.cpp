/*
	Dispatch only: what each state does with the controller lives in the game's
	state table, in the def's control callback.
*/
#include <math.h>

#include "control/e64_menu_control.h"
#include "game/e64_game.h"
#include "game/e64_game_states.h"
#include "time/e64_time.h"

namespace e64 {

/* A direction given once moves the index once. Kept held, it waits out
   MENU_REPEAT_DELAY and from there steps every MENU_REPEAT_PERIOD, so reaching
   the far end of a list does not mean tapping once per entry. */
#define MENU_REPEAT_DELAY  0.8f
#define MENU_REPEAT_PERIOD 0.125f

/* How far the stick has to stay pushed to count as still held. The edge
   libdragon reports arrives at half the stick's travel, so the repeat reads
   the same line: under it, the input that opened the step is over. */
/* Half the stick's travel, on a reading that comes in from -1 to 1. */
#define STICK_REPEAT_THRESHOLD 0.5f


namespace menu {
namespace control {

typedef enum {

	MENU_DIR_NONE,
	MENU_DIR_UP, MENU_DIR_DOWN, MENU_DIR_LEFT, MENU_DIR_RIGHT,

} MenuDirection;

/* Only one direction repeats at a time. A menu moves by one step, so the last
   direction pressed is the one holding the clock: pressing another takes it
   over, and the wait starts again from there. */
static struct {

	MenuDirection direction;
	float elapsed;
	bool  repeating;

} menuRepeat;

/* Whether this direction hands the menu a step this frame: the frame it is
   pressed, and then once per period for as long as it is kept down. */
static bool getStep(MenuDirection direction, bool pressed, bool held)
{
	if (pressed) {
		menuRepeat.direction = direction;
		menuRepeat.elapsed   = 0.0f;
		menuRepeat.repeating = false;
		return true;
	}

	/* Nobody owns the clock and this direction is still down: it takes it over
	   and waits the delay out again. This is the way back for a direction held
	   through another one: pressing a second direction takes the clock away,
	   and letting that one go leaves the first with no edge left to claim it. */
	if (menuRepeat.direction == MENU_DIR_NONE && held) {
		menuRepeat.direction = direction;
		menuRepeat.elapsed   = 0.0f;
		menuRepeat.repeating = false;
		return false;
	}

	if (menuRepeat.direction != direction) return false;

	if (!held) {
		menuRepeat.direction = MENU_DIR_NONE;
		return false;
	}

	menuRepeat.elapsed += time_get()->delta;

	float wait = menuRepeat.repeating ? MENU_REPEAT_PERIOD : MENU_REPEAT_DELAY;
	if (menuRepeat.elapsed < wait) return false;

	/* What is left over stays on the clock: dropping it would stretch every
	   period by whatever the frame overshot it by. */
	menuRepeat.elapsed  -= wait;
	menuRepeat.repeating = true;
	return true;
}

void read(Controls *controls, const Controller *controller, const ControlBinding *binding)
{
	/* Holding a direction repeats it, but only past half the stick's travel:
	   near the centre it would repeat on the slightest lean. */
	Vector2 stick = controller::getStickNormalized(controller);

	const bool up    = getStep(MENU_DIR_UP,
		controller::isPressed(controller, binding->up)
		|| controller->stick_pressed_y > 0,
		controller::isHeld(controller, binding->up)
		|| stick.y > STICK_REPEAT_THRESHOLD);

	const bool down  = getStep(MENU_DIR_DOWN,
		controller::isPressed(controller, binding->down)
		|| controller->stick_pressed_y < 0,
		controller::isHeld(controller, binding->down)
		|| stick.y < -STICK_REPEAT_THRESHOLD);

	const bool left  = getStep(MENU_DIR_LEFT,
		controller::isPressed(controller, binding->left)
		|| controller->stick_pressed_x < 0,
		controller::isHeld(controller, binding->left)
		|| stick.x < -STICK_REPEAT_THRESHOLD);

	const bool right = getStep(MENU_DIR_RIGHT,
		controller::isPressed(controller, binding->right)
		|| controller->stick_pressed_x > 0,
		controller::isHeld(controller, binding->right)
		|| stick.x > STICK_REPEAT_THRESHOLD);

	*controls = (Controls){
		.confirm   = controller::isPressed(controller, binding->confirm),
		.cancel    = controller::isPressed(controller, binding->cancel),
		.pause     = controller::isPressed(controller, binding->pause),

		.up        = up,
		.down      = down,

		/* How fast, not whether: a button is all the way, the stick is as far
		   as it is pushed, and whichever asks for more wins. */
		.up_held   = fmaxf(controller::isHeld(controller, binding->up),   fmaxf( stick.y, 0.0f)),
		.down_held = fmaxf(controller::isHeld(controller, binding->down), fmaxf(-stick.y, 0.0f)),

		.left      = left,
		.right     = right,

		.tab_left  = controller::isPressed(controller, binding->tab_left),
		.tab_right = controller::isPressed(controller, binding->tab_right),
	};
}

void update(void)
{
	const GameStateDef *def = gameState_get(game_get()->state);
	if (def->control) def->control();
}

}
}

}
