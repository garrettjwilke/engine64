#include <math.h>
#include <libdragon.h>

#include "controller/e64_controller.h"
#include "math/e64_math.h"

namespace e64 {

namespace controller {

static Controller controller[CONTROLLER_COUNT];

Controller *get(void) { return controller; }


static void getInputs(Controller *controller, joypad_port_t port)
{
	controller->connected = joypad_is_connected(port);

	controller->pressed = joypad_get_buttons_pressed(port);
	controller->held = joypad_get_buttons_held(port);
	controller->released = joypad_get_buttons_released(port);
	controller->input = joypad_get_inputs(port);

	controller->stick_pressed_x = joypad_get_axis_pressed(port, JOYPAD_AXIS_STICK_X);
	controller->stick_pressed_y = joypad_get_axis_pressed(port, JOYPAD_AXIS_STICK_Y);
}


/* The pad keeps three sets of buttons every frame: the ones that went down,
   the ones staying down, and the ones that came up. This reads a button out of
   whichever set it is handed. */
static bool readButton(const joypad_buttons_t *button, ButtonID id)
{
	switch (id) {
		case BTN_A: return button->a;
		case BTN_B: return button->b;
		case BTN_Z: return button->z;
		case BTN_START: return button->start;
		case BTN_D_UP: return button->d_up;
		case BTN_D_DOWN: return button->d_down;
		case BTN_D_LEFT: return button->d_left;
		case BTN_D_RIGHT: return button->d_right;
		case BTN_C_UP: return button->c_up;
		case BTN_C_DOWN: return button->c_down;
		case BTN_C_LEFT: return button->c_left;
		case BTN_C_RIGHT: return button->c_right;
		case BTN_L: return button->l;
		case BTN_R: return button->r;
		default: return false;
	}
}

/* The frame a button goes down. Held, it answers once and stops. */
bool isPressed(const Controller *pad, ButtonID id)
{
	return readButton(&pad->pressed, id);
}

/* Every frame a button stays down, the first one included. */
bool isHeld(const Controller *pad, ButtonID id)
{
	return readButton(&pad->held, id);
}

/* The frame a button comes back up. */
bool isReleased(const Controller *pad, ButtonID id)
{
	return readButton(&pad->released, id);
}

/* The analog stick as the hardware reports it: each axis from -127 to 127,
   whatever the controller's own travel happens to be. */
Vector2 getStick(const Controller *controller)
{
	return (Vector2){ (float)controller->input.stick_x, (float)controller->input.stick_y };
}

/* The C stick, same thing. An N64 controller has no C stick and reads zero
   here; its C buttons are read as buttons. */
Vector2 getCStick(const Controller *controller)
{
	return (Vector2){ (float)controller->input.cstick_x, (float)controller->input.cstick_y };
}

/* Brings a raw reading to -1 to 1. Under the deadzone the stick reads as
   centred, since it never rests at exactly zero, and past its own travel it
   stops at the end instead of going over. */
static Vector2 normalizeStick(Vector2 stick, float range)
{
	if (fabsf(stick.x) < STICK_DEADZONE) stick.x = 0.0f;
	if (fabsf(stick.y) < STICK_DEADZONE) stick.y = 0.0f;

	stick.x = clampf(stick.x / range, -1.0f, 1.0f);
	stick.y = clampf(stick.y / range, -1.0f, 1.0f);

	return stick;
}

/* Which way a normalised reading points, one axis at a time: -1, 0 or 1. How
   far it went is dropped, which is what a menu wants. */
static Vector2 stickDirection(Vector2 stick)
{
	return (Vector2){
		stick.x == 0.0f ? 0.0f : (stick.x < 0.0f ? -1.0f : 1.0f),
		stick.y == 0.0f ? 0.0f : (stick.y < 0.0f ? -1.0f : 1.0f),
	};
}

Vector2 getStickNormalized(const Controller *controller)
{
	return normalizeStick(getStick(controller), STICK_RANGE);
}

Vector2 getCStickNormalized(const Controller *controller)
{
	return normalizeStick(getCStick(controller), CSTICK_RANGE);
}

Vector2 getStickDirection(const Controller *controller)
{
	return stickDirection(getStickNormalized(controller));
}

Vector2 getCStickDirection(const Controller *controller)
{
	return stickDirection(getCStickNormalized(controller));
}

void start(void)
{
	for (int i = 0; i < CONTROLLER_COUNT; i++)
		controller[i] = (Controller){};
}

void poll(void)
{
	joypad_poll();
	for (int i = 0; i < CONTROLLER_COUNT; i++)
		getInputs(&controller[i], (joypad_port_t)i);
}

}

}
