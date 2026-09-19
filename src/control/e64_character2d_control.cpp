#include <math.h>

#include "control/e64_character2d_control.h"


static void character2dControl_setJump(Character2D *character, Movement2DCommand *cmd, const Character2DControls *actions)
{
	Character2DMovement *movement = &character->movement;

	/* The button never changes the state: it only asks for a jump. The crouch
	   runs on the ground and the movement switches to the air on the impulse.
	   The coyote window counts as ground for this: the ledge is gone but the
	   ask is still taken. */
	if (actions->jump
	    && (character2dMovement_isLocomotion(movement->current)
	        || character2dMovement_isCoyoteOpen(character))
	    && movement->data.jump_timer == 0.0f) {
		cmd->jump_held      = true;
		cmd->jump_triggered = true;
	} else if (actions->jump_held) {
		return;
	} else {
		cmd->jump_held = false;
	}
}

/* Only from a walk or a run, and not while a jump is being charged: the
   crouch owns the body until it takes off. */
static void character2dControl_setRoll(Character2D *character, Movement2DCommand *cmd, const Character2DControls *actions)
{
	Character2DMovement *movement = &character->movement;

	if (actions->roll && character2dMovement_isLocomotion(movement->current)
	 && movement->current != MOVEMENT2D_STATE_IDLE
	 && movement->data.jump_timer == 0.0f) {
		cmd->roll_triggered = true;
		character2dMovement_setMode(movement, MOVEMENT2D_STATE_ROLLING);
	}
}

/* The one axis, off the stick or the d-pad. The pad is all or nothing, so it
   asks for the full push: pressing left on it is the same as leaning the
   stick all the way. Both at once cancel, the way two opposite pushes do. */
static float character2dControl_getDirection(const Character2DControls *actions, float *magnitude)
{
	if (actions->left != actions->right) {
		*magnitude = JOYPAD_RANGE_N64_STICK_MAX;
		return actions->left ? -1.0f : 1.0f;
	}

	if (fabsf(actions->stick_x) >= STICK_DEADZONE) {
		*magnitude = fabsf(actions->stick_x);
		return actions->stick_x < 0.0f ? -1.0f : 1.0f;
	}

	*magnitude = 0.0f;
	return 0.0f;
}

static void character2dControl_setLocomotionWithStick(Character2D *character, Movement2DCommand *cmd, const Character2DControls *actions)
{
	Character2DMovement *movement = &character->movement;

	float magnitude = 0.0f;
	cmd->direction  = character2dControl_getDirection(actions, &magnitude);

	uint8_t mode = (magnitude == 0.0f) ? MOVEMENT2D_STATE_IDLE : MOVEMENT2D_STATE_WALKING;

	/* An action owns the current state and its gait until it ends: the stick
	   only picks the state it goes back to. */
	if (!character2dMovement_isLocomotion(movement->current)) {
		movement->locomotion = mode;
		return;
	}

	character2dMovement_setMode(movement, mode);
	if (mode == MOVEMENT2D_STATE_IDLE) return;

	const Character2DMovementSettings *settings = movement->settings;
	uint8_t last_gait = settings->gait_count - 1;

	if (magnitude <= PLAYER2D_STICK_WALK_THRESHOLD)
		cmd->gait = 0;
	else if (actions->sprint)
		cmd->gait = last_gait;
	else
		cmd->gait = (last_gait > 1) ? 1 : last_gait;
}

void character2dControls_read(Character2DControls *controls, const Character2DControlBinding *binding)
{
	const Controller *controller = &controller_get()[binding->player];

	*controls = (Character2DControls){
		.jump      = button_isPressed(controller, binding->jump),
		.jump_held = button_isHeld(controller, binding->jump),
		.roll      = button_isPressed(controller, binding->roll),
		.sprint    = button_isHeld(controller, binding->sprint),
		.left      = button_isHeld(controller, binding->left),
		.right     = button_isHeld(controller, binding->right),
		.stick_x   = (float)controller->input.stick_x,
	};
}

void character2dControl_update(Character2D *character, Movement2DCommand *cmd, const Character2DControls *actions)
{
	/* No stats on the 2D body to tire it: the command always asks for the
	   full speed. */
	cmd->speed_scale = 1.0f;

	character2dControl_setJump(character, cmd, actions);
	character2dControl_setRoll(character, cmd, actions);
	character2dControl_setLocomotionWithStick(character, cmd, actions);
}
