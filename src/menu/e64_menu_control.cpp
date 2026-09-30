/*
	Reading the controller as a menu does: presses, with a held direction
	repeating on a clock. What each state does with it lives in the game's
	state table, in the def's control callback, which the state loop runs.
*/
#include <math.h>

#include "menu/e64_menu_control.h"
#include "time/e64_time.h"

namespace e64 {

namespace menu {
namespace control {

/* A direction given once moves the index once. Kept held, it waits out
   REPEAT_DELAY and from there steps every REPEAT_PERIOD, so reaching the far
   end of a list does not mean tapping once per entry. */
static constexpr float REPEAT_DELAY = 0.8f;
static constexpr float REPEAT_PERIOD = 0.125f;

/* How far the stick has to stay pushed to count as still held. The edge
   libdragon reports arrives at half the stick's travel, so the repeat reads
   the same line: under it, the input that opened the step is over. */
/* Half the stick's travel, on a reading that comes in from -1 to 1. */
static constexpr float STICK_REPEAT_THRESHOLD = 0.5f;


enum Direction {

	DIR_NONE,
	DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT,

};

/* Only one direction repeats at a time. A menu moves by one step, so the last
   direction pressed is the one holding the clock: pressing another takes it
   over, and the wait starts again from there. */
static struct {

	Direction direction;
	float elapsed;
	bool repeating;

} repeat;

/* Whether this direction hands the menu a step this frame: the frame it is
   pressed, and then once per period for as long as it is kept down. */
static bool getStep(Direction direction, bool pressed, bool held)
{
	if (pressed) {
		repeat.direction = direction;
		repeat.elapsed = 0.0f;
		repeat.repeating = false;
		return true;
	}

	/* Nobody owns the clock and this direction is still down: it takes it over
	   and waits the delay out again. This is the way back for a direction held
	   through another one: pressing a second direction takes the clock away,
	   and letting that one go leaves the first with no edge left to claim it. */
	if (repeat.direction == DIR_NONE && held) {
		repeat.direction = direction;
		repeat.elapsed = 0.0f;
		repeat.repeating = false;
		return false;
	}

	if (repeat.direction != direction) return false;

	if (!held) {
		repeat.direction = DIR_NONE;
		return false;
	}

	repeat.elapsed += time::get()->delta;

	float wait = repeat.repeating ? REPEAT_PERIOD : REPEAT_DELAY;
	if (repeat.elapsed < wait) return false;

	/* What is left over stays on the clock: dropping it would stretch every
	   period by whatever the frame overshot it by. */
	repeat.elapsed -= wait;
	repeat.repeating = true;
	return true;
}

void read(Controls *controls, const Controller *controller, const ControlBinding *binding)
{
	/* Holding a direction repeats it, but only past half the stick's travel:
	   near the centre it would repeat on the slightest lean. */
	Vector2 stick = controller::getStickNormalized(controller);

	const bool up = getStep(DIR_UP,
		controller::isPressed(controller, binding->up)
		|| controller->stick_pressed_y > 0,
		controller::isHeld(controller, binding->up)
		|| stick.y > STICK_REPEAT_THRESHOLD);

	const bool down = getStep(DIR_DOWN,
		controller::isPressed(controller, binding->down)
		|| controller->stick_pressed_y < 0,
		controller::isHeld(controller, binding->down)
		|| stick.y < -STICK_REPEAT_THRESHOLD);

	const bool left = getStep(DIR_LEFT,
		controller::isPressed(controller, binding->left)
		|| controller->stick_pressed_x < 0,
		controller::isHeld(controller, binding->left)
		|| stick.x < -STICK_REPEAT_THRESHOLD);

	const bool right = getStep(DIR_RIGHT,
		controller::isPressed(controller, binding->right)
		|| controller->stick_pressed_x > 0,
		controller::isHeld(controller, binding->right)
		|| stick.x > STICK_REPEAT_THRESHOLD);

	*controls = (Controls){
		.confirm = controller::isPressed(controller, binding->confirm),
		.cancel = controller::isPressed(controller, binding->cancel),
		.pause = controller::isPressed(controller, binding->pause),

		.up = up,
		.down = down,

		/* How fast, not whether: a button is all the way, the stick is as far
		   as it is pushed, and whichever asks for more wins. */
		.up_held = fmaxf(controller::isHeld(controller, binding->up), fmaxf( stick.y, 0.0f)),
		.down_held = fmaxf(controller::isHeld(controller, binding->down), fmaxf(-stick.y, 0.0f)),

		.left = left,
		.right = right,

		.tab_left = controller::isPressed(controller, binding->tab_left),
		.tab_right = controller::isPressed(controller, binding->tab_right),
	};
}

}
}

}
