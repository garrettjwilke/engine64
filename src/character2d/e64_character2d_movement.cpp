#include <assert.h>
#include <math.h>
#include <fmath.h>

#include "character2d/e64_character2d.h"

namespace e64 {

namespace character2d {
namespace movement {

/* Which states are ordinary locomotion: the ones the body goes back to when
   an action ends. Falling is not one, so a landing returns to the walk or the
   idle it left the floor with. */
static bool isLocomotionState(uint8_t mode)
{
	return mode == MOVEMENT2D_STATE_IDLE || mode == MOVEMENT2D_STATE_WALKING;
}

void setMode(Movement *movement, uint8_t new_mode)
{
	if (movement->current == new_mode) return;
	movement->current = new_mode;
	if (isLocomotionState(new_mode)) movement->locomotion = new_mode;
}

bool isLocomotion(uint8_t mode)
{
	return isLocomotionState(mode);
}

static void evaluateTransitions(Character2D *character)
{
	Movement *movement = &character->movement;
	if (movement->next == MOVEMENT2D_STATE_NONE) return;
	setMode(movement, movement->next);
	movement->next = MOVEMENT2D_STATE_NONE;
}


static const GaitSettings *getGait(const Character2D *character)
{
	const MovementSettings *settings = character->movement.settings;
	uint8_t gait = character->movement.data.gait;
	if (gait >= settings->gait_count) gait = settings->gait_count - 1;
	return &settings->gait[gait];
}

static float getTargetSpeed(const Character2D *character, uint8_t state)
{
	if (state == MOVEMENT2D_STATE_WALKING) return getGait(character)->target_speed;
	return 0.0f;   /* anything else is asking to stand still */
}

static float getAccelerationRate(const Character2D *character, uint8_t state)
{
	if (state == MOVEMENT2D_STATE_WALKING) return getGait(character)->response_rate;
	return character->movement.settings->idle_response_rate;
}

/* The one axis the stick steers: the direction carries the sign, so the target
   is a signed speed and letting go is a target of zero. */
static void setHorizontalVelocity(Character2D *character, float direction, float target_speed, float response_rate, float dt)
{
	MovementData *data = &character->movement.data;

	float target_vx = target_speed * direction;

	float factor = fm_expf(-response_rate * dt);
	data->velocity.x = data->velocity.x * factor + target_vx * (1.0f - factor);
}

/* Which way the sprite looks. The body faces what it is asked for, not what it
   drifts at: a jump steered back mid-air turns as the stick does, and letting
   go keeps the last facing instead of snapping. */
static void setFacing(Character2D *character, const MovementCommand *cmd)
{
	if (cmd->direction < 0.0f) character->facing_left = true;
	if (cmd->direction > 0.0f) character->facing_left = false;
}

static void updateBody(Character2D *character, float dt)
{
	MovementData *data = &character->movement.data;

	if (data->velocity.x != 0)
		data->horizontal_speed = fabsf(data->velocity.x);

	if (fabsf(data->velocity.x) < CHARACTER2D_LOCOMOTION_MIN_SPEED && data->velocity.y == 0) {
		data->velocity.x = 0;
		data->horizontal_speed = 0;
	}

	character->position.x += data->velocity.x * dt;
	character->position.y += data->velocity.y * dt;
}

/* The crouch that starts a jump runs here, on the ground, over walk or idle:
   the stick still drives the body and holding the button only brakes it. The
   impulse and the switch to the air come together when the crouch ends, so a
   jump is never a state of its own — the air is always a fall. */
static void setChargingJump(Character2D *character, MovementCommand *cmd, float dt)
{
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	if (cmd->jump_triggered) {
		data->jump_initial_velocity = data->velocity;
		data->jump_timer    = 0.0f;
		data->jump_force    = 0.0f;
		cmd->jump_triggered = false;

		/* Snap: no crouch at all. The floor is left on this very frame with
		   the minimum launch, and the rest of the height is added in the air
		   for as long as the button holds. */
		if (settings->jump_mode == JUMP2D_SNAP) {
			data->velocity.x = data->jump_initial_velocity.x * CHARACTER2D_JUMP_LAUNCH_VELOCITY_SCALE;
			data->velocity.y = -settings->jump_base_speed;

			character->movement.next = MOVEMENT2D_STATE_FALLING;
			return;
		}
	}
	else if (data->jump_timer == 0.0f) return;   /* nothing being charged */

	data->jump_timer += dt;
	if (cmd->jump_held) {
		data->jump_force += dt;
		data->velocity.x *= CHARACTER2D_JUMP_HOLD_VELOCITY_SCALE;
	}

	if (data->jump_timer < settings->jump_timer_max) return;

	/* Leaving the floor: the launch keeps a share of the run it came from. */
	data->velocity.x = data->jump_initial_velocity.x * CHARACTER2D_JUMP_LAUNCH_VELOCITY_SCALE;
	data->velocity.y = -data->jump_force * settings->jump_force_multiplier;
	if (data->velocity.y > -settings->jump_base_speed)
		data->velocity.y = -settings->jump_base_speed;

	data->jump_force = 0.0f;
	data->jump_timer = 0.0f;
	character->movement.next = MOVEMENT2D_STATE_FALLING;
}

/* Coyote time for the snap: the body falls the way it always did, but for a
   moment after the edge the button still launches. The charge needs none of
   this — its crouch holds it in locomotion until it lets go. */
bool isCoyoteOpen(const Character2D *character)
{
	return !character->movement.data.is_grounded
	    && character->movement.data.coyote_timer < character->movement.settings->jump_coyote_time;
}

static void setSnappingJump(Character2D *character, MovementCommand *cmd, float dt)
{
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	data->coyote_timer += dt;

	if (!cmd->jump_triggered) return;

	/* An ask the air cannot grant dies here. The control reads the window
	   with the timer of the frame before, so a press on the very edge is
	   taken and then found too late right below; left in the command it
	   would outlive the whole fall and launch the body on the landing. */
	if (settings->jump_mode != JUMP2D_SNAP
	 || data->coyote_timer >= settings->jump_coyote_time) {
		cmd->jump_triggered = false;
		return;
	}

	/* The run it walked off the ledge with is what it launches on: there was
	   no press on the floor to have saved one. */
	data->jump_initial_velocity = data->velocity;
	data->velocity.x *= CHARACTER2D_JUMP_LAUNCH_VELOCITY_SCALE;
	data->velocity.y  = -settings->jump_base_speed;

	/* One launch per edge: the window closes on the jump it granted. */
	data->coyote_timer  = settings->jump_coyote_time;
	cmd->jump_triggered = false;
}

/* A crouch already under way. The edge can run out mid-crouch and the jump is
   not lost for it: the body stays in locomotion and coasts until the crouch
   finishes and launches it as if it had never left the ledge. */
bool isChargingJump(const Character2D *character)
{
	return character->movement.settings->jump_mode == JUMP2D_CHARGE
	    && character->movement.data.jump_timer > 0.0f;
}

static void setLocomotion(Character2D *character, MovementCommand *cmd, float dt)
{
	uint8_t state = character->movement.current;
	setHorizontalVelocity(character, cmd->direction, getTargetSpeed(character, state) * cmd->speed_scale, getAccelerationRate(character, state), dt);
	setChargingJump(character, cmd, dt);
}

/* Gravity and the terminal speed that goes with it, for anything with no floor
   under it. */
static void setGravity(MovementData *data, float gravity_scale, float dt)
{
	data->velocity.y += CHARACTER2D_GRAVITY * gravity_scale * dt;
	if (data->velocity.y > CHARACTER2D_FALL_MAX_SPEED)
		data->velocity.y = CHARACTER2D_FALL_MAX_SPEED;
}

static void setRolling(Character2D *character, MovementCommand *cmd, float dt)
{
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	/* The trigger is the entry mark and is consumed here: the direction is
	   taken once and held until grip, and the timer starts from zero however
	   the previous roll ended. A roll with no push goes the way the body
	   faces. */
	if (cmd->roll_triggered) {
		data->roll_direction = cmd->direction != 0.0f ? cmd->direction : (character->facing_left ? -1.0f : 1.0f);
		data->roll_timer     = 0.0f;
		cmd->roll_triggered  = false;
	}

	/* Rolling off a ledge drops: the state is what holds to the end of the
	   clip, not the ground. */
	if (!data->is_grounded) setGravity(data, 1.0f, dt);

	/* Out to idle, not to the locomotion state held from before the roll: the
	   control runs first next frame and the stick raises it to walk if it is
	   asking for one. */
	if (data->roll_timer >= settings->roll_timer_max) {
		character->movement.next = MOVEMENT2D_STATE_IDLE;
		data->roll_timer = 0.0f;
		return;
	}

	/* Three phases off the one timer: the launch drives the asked direction
	   at the roll's own speed, the spin holds whatever speed it reached along
	   the body's own facing, and the grip hands the steering back to the
	   stick. */
	float direction     = data->roll_direction;
	float target_speed  = settings->roll_target_speed;
	float response_rate = settings->roll_launch_response_rate;

	if (data->roll_timer >= settings->roll_grip_time) {
		direction     = cmd->direction;
		target_speed  = data->horizontal_speed;
		response_rate = settings->roll_grip_response_rate;
	}
	else if (data->roll_timer >= settings->roll_ground_time) {
		direction     = character->facing_left ? -1.0f : 1.0f;
		target_speed  = data->horizontal_speed;
		response_rate = settings->roll_spin_response_rate;
	}

	setHorizontalVelocity(character, direction, target_speed, response_rate, dt);
	data->roll_timer += dt;
}

static void setFalling(Character2D *character, MovementCommand *cmd, float dt)
{
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	setSnappingJump(character, cmd, dt);

	/* Air control decides how much of the speed the stick would give on the
	   ground the air is allowed to reach. At zero it does nothing and the
	   jump keeps the run that launched it. */
	float target_speed = data->horizontal_speed;
	if (settings->air_control > 0.0f) {
		float ground = character->movement.locomotion == MOVEMENT2D_STATE_WALKING
		             ? getTargetSpeed(character, MOVEMENT2D_STATE_WALKING) * cmd->speed_scale
		             : 0.0f;
		target_speed += (ground - target_speed) * settings->air_control;
	}

	setHorizontalVelocity(character, cmd->direction, target_speed,
	                      settings->jump_response_rate * settings->air_control, dt);

	/* Snap: holding the button makes the rise cost less gravity, so how long
	   it is held is how high it goes. Only on the way up — past the top the
	   fall is the fall, and nobody floats down. */
	float gravity_scale = 1.0f;
	if (settings->jump_mode == JUMP2D_SNAP && cmd->jump_held && data->velocity.y < 0.0f)
		gravity_scale = settings->jump_hold_gravity_scale;

	setGravity(data, gravity_scale, dt);
}

static void (*handler[MOVEMENT2D_STATE_COUNT])(Character2D *, MovementCommand *, float) = {
	[MOVEMENT2D_STATE_IDLE]    = setLocomotion,
	[MOVEMENT2D_STATE_WALKING] = setLocomotion,
	[MOVEMENT2D_STATE_ROLLING] = setRolling,
	[MOVEMENT2D_STATE_FALLING] = setFalling,
};

_Static_assert(sizeof(handler) / sizeof(handler[0]) == MOVEMENT2D_STATE_COUNT, "handler must have one entry per character state");

}
}


void Character2D::updateMovement(character2d::MovementCommand *cmd, float dt)
{
	assert(cmd);

	assert(movement.current < character2d::MOVEMENT2D_STATE_COUNT);
	assert(character2d::movement::handler[movement.current] != NULL);

	/* A scaled-down command locks the top gait away: the character stays on
	   the previous one until the scale is back at full. */
	uint8_t gait = cmd->gait;
	if (cmd->speed_scale < 1.0f && gait == movement.settings->gait_count - 1)
		gait = movement.settings->gait_count - 2;
	movement.data.gait = gait;
	movement.next = character2d::MOVEMENT2D_STATE_NONE;

	character2d::movement::handler[movement.current](this, cmd, dt);
	character2d::movement::updateBody(this, dt);
	character2d::physics::collide(this);
	character2d::movement::setFacing(this, cmd);
	character2d::movement::evaluateTransitions(this);
}

}
