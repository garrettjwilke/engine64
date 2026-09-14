/*
	Dispatch only: what each state does with the controller lives in the game's
	state table, in the def's control callback.
*/
#include <math.h>

#include "control/e64_menu_control.h"
#include "game/e64_game.h"
#include "game/e64_game_states.h"
#include "time/e64_time.h"


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
static bool direction_getStep(MenuDirection direction, bool pressed, bool held)
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

void menuControls_map(MenuControls *controls, const Controller *controller, const MenuControlBinding *binding)
{
	/* Holding a direction repeats it, but only past half the stick's travel:
	   near the centre it would repeat on the slightest lean. */
	Vector2 stick = controller_getStickNormalized(controller);

	const bool up    = direction_getStep(MENU_DIR_UP,
		button_isPressed(controller, binding->up)
		|| controller->stick_pressed_y > 0,
		button_isHeld(controller, binding->up)
		|| stick.y > STICK_REPEAT_THRESHOLD);

	const bool down  = direction_getStep(MENU_DIR_DOWN,
		button_isPressed(controller, binding->down)
		|| controller->stick_pressed_y < 0,
		button_isHeld(controller, binding->down)
		|| stick.y < -STICK_REPEAT_THRESHOLD);

	const bool left  = direction_getStep(MENU_DIR_LEFT,
		button_isPressed(controller, binding->left)
		|| controller->stick_pressed_x < 0,
		button_isHeld(controller, binding->left)
		|| stick.x < -STICK_REPEAT_THRESHOLD);

	const bool right = direction_getStep(MENU_DIR_RIGHT,
		button_isPressed(controller, binding->right)
		|| controller->stick_pressed_x > 0,
		button_isHeld(controller, binding->right)
		|| stick.x > STICK_REPEAT_THRESHOLD);

	*controls = (MenuControls){
		.confirm   = button_isPressed(controller, binding->confirm),
		.cancel    = button_isPressed(controller, binding->cancel),
		.pause     = button_isPressed(controller, binding->pause),

		.up        = up,
		.down      = down,
		.left      = left,
		.right     = right,

		/* How fast, not whether: a button is all the way, the stick is as far
		   as it is pushed, and whichever asks for more wins. */
		.up_held   = fmaxf(button_isHeld(controller, binding->up),   fmaxf( stick.y, 0.0f)),
		.down_held = fmaxf(button_isHeld(controller, binding->down), fmaxf(-stick.y, 0.0f)),

		.tab_left  = button_isPressed(controller, binding->tab_left),
		.tab_right = button_isPressed(controller, binding->tab_right),
	};
}

void menuControl_update(void)
{
	const GameStateDef *def = gameState_get(game_get()->state);
	if (def->control) def->control();
}
