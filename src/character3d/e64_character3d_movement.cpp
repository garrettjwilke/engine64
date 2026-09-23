#include <assert.h>
#include <math.h>
#include <fmath.h>

#include "character3d/e64_character3d.h"
#include "math/e64_math.h"

namespace e64 {

namespace character3d {
namespace movement {

static const bool updates_locomotion[MOVEMENT_STATE_COUNT] = {
	[MOVEMENT_STATE_IDLE] = true,
	[MOVEMENT_STATE_WALKING] = true,
	[MOVEMENT_STATE_ROLLING] = false,
	[MOVEMENT_STATE_FALLING] = false,
	[MOVEMENT_STATE_SWIMMING] = false,
	[MOVEMENT_STATE_CLIMBING] = false,
};

void setMode(Movement *movement, uint8_t new_mode)
{
	if (movement->current == new_mode) return;
	movement->current = new_mode;
	if (updates_locomotion[new_mode]) movement->locomotion = new_mode;
}

bool isLocomotion(uint8_t mode)
{
	return updates_locomotion[mode];
}

static void evaluateTransitions(Character3D *character)
{
	Movement *movement = &character->movement;
	if (movement->next == MOVEMENT_STATE_NONE) return;
	setMode(movement, movement->next);
	movement->next = MOVEMENT_STATE_NONE;
}


static const GaitSettings *getGait(const Character3D *character)
{
	const MovementSettings *settings = character->movement.settings;
	uint8_t gait = (uint8_t)character->movement.data.gait;
	if (gait >= settings->gait_count) gait = settings->gait_count - 1;
	return &settings->gait[gait];
}

/* Tired caps the body at a share of what it asks for. The stats belong to the
   character, so the movement reads them where they live. */
static float getSpeedScale(const Character3D *character)
{
	return character->stats.tired ? character->stats.settings->tired_speed_scale : 1.0f;
}

static float getTargetSpeed(const Character3D *character, uint8_t state)
{
	/* Anything but walking is asking to stand still. */
	if (state != MOVEMENT_STATE_WALKING) return 0.0f;

	float speed = getGait(character)->target_speed;

	/* A single gait has none above it to step to: the fraction it carries is
	   the share of that one speed the stick is asking for. */
	if (character->movement.settings->gait_count == 1) speed *= character->movement.data.gait;
	return speed;
}

static float getAccelerationRate(const Character3D *character, uint8_t state)
{
	if (state == MOVEMENT_STATE_WALKING) return getGait(character)->response_rate;
	return character->movement.settings->idle_response_rate;
}

static float getRotationAccelerationRate(const Character3D *character, uint8_t state)
{
	if (state == MOVEMENT_STATE_WALKING) return getGait(character)->rotation_response_rate;
	return character->movement.settings->idle_rotation_response_rate;
}

static void setHorizontalVelocity(Character3D *character, float yaw, float target_speed, float response_rate, float dt)
{
	KinematicBody *body = &character->body;

	float s, c;
	fm_sincosf(deg_to_rad(yaw), &s, &c);
	float target_vx = target_speed * s;
	float target_vy = target_speed * -c;

	float factor = fm_expf(-response_rate * dt);
	body->velocity.x = body->velocity.x * factor + target_vx * (1.0f - factor);
	body->velocity.y = body->velocity.y * factor + target_vy * (1.0f - factor);
}

/*
static void setHorizontalVelocity(Character3D *character, float yaw, float target_speed, float response_rate, float dt)
{
	KinematicBody *body = &character->body;

	float target_vx = target_speed *  fm_sinf(deg_to_rad(yaw));
	float target_vy = target_speed * -fm_cosf(deg_to_rad(yaw));

	body->acceleration.x = response_rate * (target_vx - body->velocity.x);
	body->acceleration.y = response_rate * (target_vy - body->velocity.y);

	body->velocity.x += body->acceleration.x * dt;
	body->velocity.y += body->acceleration.y * dt;
}
*/

static void setRotation(Character3D *character, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;

	/* Climbing already set the facing from the ladder, and its velocity is
	   the pull onto the anchor: read as a heading it would spin the body. */
	if (character->movement.current == MOVEMENT_STATE_CLIMBING) return;

	bool moving = body->velocity.x != 0 || body->velocity.y != 0;

	if (moving) {
		Vector2 horizontal_velocity = {body->velocity.x, body->velocity.y};
		data->horizontal_speed = vector2::magnitude(&horizontal_velocity);
	}

	float facing_yaw;
	if (data->strafe) {
		/* With the weapon up the body keeps tracking the camera even planted;
		   the plain strafe leaves a standstill facing free. */
		if (!moving && !data->aiming && !data->charging_shoot) return;
		facing_yaw = data->strafe_yaw;
	} else {
		if (!moving) return;
		facing_yaw = rad_to_deg(fm_atan2f(-body->velocity.x, -body->velocity.y));

		if (data->rotation_mode == CHARACTER3D_ROTATION_MODE_SNAP) {
			body->rotation.z = angle_wrap(facing_yaw);
			return;
		}
	}

	const float current_yaw = body->rotation.z;
	const float target_yaw = angle_wrap_relative(facing_yaw, current_yaw);

	if (fabsf(target_yaw - current_yaw) <= CHARACTER3D_ROTATION_SNAP_THRESHOLD) {
		body->rotation.z = target_yaw;
		return;
	}

	uint8_t state = character->movement.current;
	if (state == MOVEMENT_STATE_ROLLING || state == MOVEMENT_STATE_FALLING)
		state = character->movement.locomotion;
	float response_rate = getRotationAccelerationRate(character, state);
	float factor = fm_expf(-response_rate * dt);
	body->rotation.z = angle_wrap(current_yaw * factor + target_yaw * (1.0f - factor));
}

static void updateBody(Character3D *character, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;

	data->previous_yaw = body->rotation.z;

	/* A ladder standing in water is still a ladder: the climb owns the
	   vertical, so the buoyancy must not bob the body off its rungs. */
	if (data->in_water && character->movement.current != MOVEMENT_STATE_CLIMBING) {
		const MovementSettings *settings = character->movement.settings;

		/* Stroking raises the equilibrium so the swim pose meets the surface. */
		float stroke = (settings->swim_slow_speed > 0.0f)
			? data->horizontal_speed / settings->swim_slow_speed : 0.0f;
		if (stroke > 1.0f) stroke = 1.0f;

		float equilibrium = settings->water_equilibrium_idle
			+ (settings->water_equilibrium_swim - settings->water_equilibrium_idle) * stroke;

		/* Gravity scaled by how far the capsule sits from the equilibrium
		   depth: deeper than it, the push turns upward. The drag grows with
		   the submerged body, so a dive keeps its momentum through the
		   surface and brakes progressively on the way down. */
		float buoyant = CHARACTER3D_GRAVITY * (1.0f - data->submerged_fraction / equilibrium);
		body->velocity.z += buoyant * dt;
		body->velocity.z /= (1.0f + CHARACTER3D_WATER_DRAG * data->submerged_fraction * dt);

		/* Progressive sink limit: none at the splash so a dive keeps its
		   momentum, full past the saturation fraction — the pool is barely
		   deeper than the capsule, so the brake cannot wait for the drag. */
		float t = data->submerged_fraction / CHARACTER3D_WATER_SINK_LIMIT_FULL;
		if (t > 1.0f) t = 1.0f;
		float limit = CHARACTER3D_FALL_MAX_SPEED + (CHARACTER3D_WATER_SINK_MAX_SPEED - CHARACTER3D_FALL_MAX_SPEED) * t;
		if (body->velocity.z < limit) body->velocity.z = limit;
	}
	else if (body->acceleration.z)
		body->velocity.z += body->acceleration.z * dt;

	if (fabsf(body->velocity.x) < LOCOMOTION_MIN_SPEED && fabsf(body->velocity.y) < LOCOMOTION_MIN_SPEED && body->velocity.z == 0) {
		body->velocity.x = 0;
		body->velocity.y = 0;
		data->horizontal_speed = 0;
	}

	if (body->velocity.x != 0 || body->velocity.y != 0 || body->velocity.z != 0)
		vector3::addScaledVector(&body->position, &body->velocity, dt);

	setRotation(character, dt);
}

/* The crouch that starts a jump runs here, on the ground, over walk or idle:
   the stick still drives the body and holding the button only brakes it. The
   impulse and the switch to the air come together when the crouch ends, so a
   jump is never a state of its own — the air is always a fall. */
static void setChargingJump(Character3D *character, MovementCommand *cmd, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	if (cmd->jump_triggered) {
		data->jump_initial_velocity = body->velocity;
		data->jump_timer = 0.0f;
		data->jump_force = 0.0f;
		cmd->jump_triggered = false;

		/* Snap: no crouch at all. The floor is left on this very frame with
		   the minimum launch, and the rest of the height is added in the air
		   for as long as the button holds. */
		if (settings->jump_mode == JUMP_SNAP) {
			body->velocity = data->jump_initial_velocity;
			vector3::scale(&body->velocity, CHARACTER3D_JUMP_LAUNCH_VELOCITY_SCALE);
			body->velocity.z = settings->jump_base_speed;

			character->movement.next = MOVEMENT_STATE_FALLING;
			return;
		}
	}
	else if (data->jump_timer == 0.0f) return; /* nothing being charged */

	data->jump_timer += dt;
	if (cmd->jump_held) {
		data->jump_force += dt;
		vector3::scale(&body->velocity, CHARACTER3D_JUMP_HOLD_VELOCITY_SCALE);
	}

	if (data->jump_timer < settings->jump_timer_max) return;

	/* Leaving the floor: the launch keeps a share of the run it came from. */
	body->velocity = data->jump_initial_velocity;
	vector3::scale(&body->velocity, CHARACTER3D_JUMP_LAUNCH_VELOCITY_SCALE);
	body->velocity.z = data->jump_force * settings->jump_force_multiplier;
	if (body->velocity.z < settings->jump_base_speed)
		body->velocity.z = settings->jump_base_speed;

	data->jump_force = 0.0f;
	data->jump_timer = 0.0f;
	character->movement.next = MOVEMENT_STATE_FALLING;
}

/* Coyote time for the snap: the body falls the way it always did, but for a
   moment after the edge the button still launches. The charge needs none of
   this — its crouch holds it in locomotion until it lets go. */
bool isCoyoteOpen(const Character3D *character)
{
	return !character->movement.data.is_grounded
	    && character->movement.data.coyote_timer < character->movement.settings->jump_coyote_time;
}

static void setSnappingJump(Character3D *character, MovementCommand *cmd, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	data->coyote_timer += dt;

	if (settings->jump_mode != JUMP_SNAP
	 || !cmd->jump_triggered
	 || data->coyote_timer >= settings->jump_coyote_time) return;

	/* The run it walked off the ledge with is what it launches on: there was
	   no press on the floor to have saved one. */
	data->jump_initial_velocity = body->velocity;
	vector3::scale(&body->velocity, CHARACTER3D_JUMP_LAUNCH_VELOCITY_SCALE);
	body->velocity.z = settings->jump_base_speed;

	/* One launch per edge: the window closes on the jump it granted. */
	data->coyote_timer = settings->jump_coyote_time;
	cmd->jump_triggered = false;
}

/* A crouch already under way. The edge can run out mid-crouch and the jump is
   not lost for it: the body stays in locomotion and coasts, keeping the speed
   it had and taking no gravity, until the crouch finishes and launches it as
   if it had never left the ledge. */
bool isChargingJump(const Character3D *character)
{
	return character->movement.settings->jump_mode == JUMP_CHARGE
	    && character->movement.data.jump_timer > 0.0f;
}

static void setLocomotion(Character3D *character, MovementCommand *cmd, float dt)
{
	uint8_t state = character->movement.current;
	float target = getTargetSpeed(character, state) * getSpeedScale(character);

	setHorizontalVelocity(character, cmd->target_yaw, target, getAccelerationRate(character, state), dt);
	setChargingJump(character, cmd, dt);
}

/* Gravity and the terminal speed that goes with it, for anything with no floor
   under it. The collision pass clears the acceleration again on landing. */
static void setGravity(KinematicBody *body, float gravity_scale)
{
	body->acceleration.z = CHARACTER3D_GRAVITY * gravity_scale;
	if (body->velocity.z < CHARACTER3D_FALL_MAX_SPEED)
		body->velocity.z = CHARACTER3D_FALL_MAX_SPEED;
}

static void setRolling(Character3D *character, MovementCommand *cmd, float dt)
{
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	/* The trigger is the entry mark and is consumed here: the stick yaw is
	   taken once and held until grip, and the timer starts from zero however
	   the previous roll ended — a ledge can end one before its own last phase. */
	if (cmd->roll_triggered) {
		data->roll_yaw = cmd->target_yaw;
		data->roll_timer = 0.0f;
		cmd->roll_triggered = false;
	}

	/* Rolling off a ledge drops: the state is what holds to the end of the
	   clip, not the ground. */
	if (!data->is_grounded) setGravity(&character->body, 1.0f);

	/* Out to idle, not to the locomotion state held from before the roll: the
	   control runs first next frame and the stick raises it to walk if it is
	   asking for one, the same way the floor probe turns it into a fall. */
	if (data->roll_timer >= settings->roll_timer_max) {
		character->movement.next = MOVEMENT_STATE_IDLE;
		data->roll_timer = 0.0f;
		return;
	}

	/* Three phases off the one timer: the launch drives the stick yaw at the
	   roll's own speed, the spin holds whatever speed it reached along the
	   body's own facing, and the grip hands the steering back to the stick. */
	float yaw = data->roll_yaw;
	float target_speed = settings->roll_target_speed;
	float response_rate = settings->roll_launch_response_rate;

	if (data->roll_timer >= settings->roll_grip_time) {
		yaw = cmd->target_yaw;
		target_speed = data->horizontal_speed;
		response_rate = settings->roll_grip_response_rate;
	}
	else if (data->roll_timer >= settings->roll_ground_time) {
		yaw = -character->body.rotation.z;
		target_speed = data->horizontal_speed;
		response_rate = settings->roll_spin_response_rate;
	}

	setHorizontalVelocity(character, yaw, target_speed, response_rate, dt);
	data->roll_timer += dt;
}

static void setFalling(Character3D *character, MovementCommand *cmd, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	data->is_grounded = 0;

	setSnappingJump(character, cmd, dt);

	/* Air control decides both halves at once: how far the heading can be
	   pulled toward the stick, and how much the speed can move toward what
	   the same stick would give on the ground. At zero neither moves and the
	   jump keeps the run that launched it. */
	float target_speed = data->horizontal_speed;
	if (settings->air_control > 0.0f) {
		float ground = character->movement.locomotion == MOVEMENT_STATE_WALKING
		             ? getTargetSpeed(character, MOVEMENT_STATE_WALKING) * getSpeedScale(character)
		             : 0.0f;
		target_speed += (ground - target_speed) * settings->air_control;
	}

	setHorizontalVelocity(character, cmd->target_yaw, target_speed,
	                      settings->jump_response_rate * settings->air_control, dt);

	/* Snap: holding the button makes the rise cost less gravity, so how long
	   it is held is how high it goes. Only on the way up — past the top the
	   fall is the fall, and nobody floats down. */
	float gravity_scale = 1.0f;
	if (settings->jump_mode == JUMP_SNAP && cmd->jump_held && body->velocity.z > 0.0f)
		gravity_scale = settings->jump_hold_gravity_scale;

	setGravity(body, gravity_scale);
}

/* The vertical is not touched here: the fake buoyancy in updateBody floats,
   bobs and damps the capsule on its own. The stick only swims horizontally. */
static void setSwimming(Character3D *character, MovementCommand *cmd, float dt)
{
	const MovementSettings *settings = character->movement.settings;
	KinematicBody *body = &character->body;

	/* Tired locks the fast stroke away, same as the top gait on land. */
	uint8_t swim_gait = cmd->swim_gait;
	if (character->stats.tired && swim_gait == CHARACTER3D_SWIM_GAIT_FAST)
		swim_gait = CHARACTER3D_SWIM_GAIT_SLOW;

	float target = 0.0f;
	if (swim_gait == CHARACTER3D_SWIM_GAIT_SLOW) target = settings->swim_slow_speed;
	if (swim_gait == CHARACTER3D_SWIM_GAIT_FAST) target = settings->swim_fast_speed;

	setHorizontalVelocity(character, cmd->target_yaw, target * getSpeedScale(character), settings->swim_response_rate, dt);
	body->acceleration.z = 0.0f;
}

/* On a ladder the state owns the body outright: no gravity, and the stick
   drives the vertical instead of a run. The horizontal is spent entirely on
   pulling the body onto the anchor the probe wrote, so an approach from the
   side slides into the ladder's centre line rather than climbing thin air. */
static void setClimbing(Character3D *character, MovementCommand *cmd, float dt)
{
	KinematicBody *body = &character->body;
	MovementData *data = &character->movement.data;
	const MovementSettings *settings = character->movement.settings;

	body->acceleration.z = 0.0f;

	float target = cmd->climb * settings->climb_speed * getSpeedScale(character);
	float factor = fm_expf(-settings->climb_response_rate * dt);
	body->velocity.z = body->velocity.z * factor + target * (1.0f - factor);

	/* The anchor is a position, not a target speed: the pull is written as
	   the velocity that closes the gap left this frame, so it dies out on
	   arrival instead of circling it. */
	float pull = (1.0f - fm_expf(-CHARACTER3D_LADDER_ANCHOR_RATE * dt)) / dt;
	body->velocity.x = (data->ladder_anchor_x - body->position.x) * pull;
	body->velocity.y = (data->ladder_anchor_y - body->position.y) * pull;

	data->horizontal_speed = 0.0f;
	data->is_grounded = 0;

	/* The rungs are what the body faces, whatever it faced walking in. */
	body->rotation.z = data->ladder_yaw;
	data->rotation_mode = CHARACTER3D_ROTATION_MODE_SNAP;
}

/* Ladder entry and exit, from the climbable probe of the physics pass.
   Entering asks the stick to be pushing at the rungs, so walking past a
   ladder never snatches the body onto it.

   Every exit but the release is a loss of contact: the volume is what says
   where climbing is possible, and running off its top while still climbing
   is the body cresting the ladder. That exit gets a push toward the rungs so
   it lands on the ledge instead of peeling back off the face. */
static void evaluateLadder(Character3D *character, MovementCommand *cmd)
{
	Movement *movement = &character->movement;
	MovementData *data = &movement->data;
	KinematicBody *body = &character->body;

	if (movement->current != MOVEMENT_STATE_CLIMBING) {
		if (!data->on_ladder || cmd->climb <= 0.0f) return;
		if (movement->next != MOVEMENT_STATE_NONE) return;

		/* Swimming counts: a ladder in a pool is how the body gets out. */
		if (!isLocomotion(movement->current)
		 && movement->current != MOVEMENT_STATE_FALLING
		 && movement->current != MOVEMENT_STATE_SWIMMING) return;

		/* Not from the crest: standing on top of the ladder leaves the feet
		   right at the volume's ceiling, and without this margin the climb
		   that just ended there grabs straight back on. */
		if (body->position.z > data->ladder_top - CHARACTER3D_LADDER_ENTER_MARGIN) return;

		/* Grabbing on kills whatever vertical the body arrived with: a fall
		   caught by a ladder would otherwise ride its own speed down past
		   the rungs while the climb slowly talks it out of it. */
		body->velocity.z = 0.0f;
		movement->next = MOVEMENT_STATE_CLIMBING;
		return;
	}

	if (cmd->climb_release) {
		/* The release is the jump button: whatever ask it left in the command
		   dies here, or the fall would carry it to the floor and launch it
		   on the landing. */
		cmd->jump_triggered = false;
		cmd->jump_held = false;
		movement->next = MOVEMENT_STATE_FALLING;
		return;
	}

	if (!data->on_ladder) {
		/* Off the top with the stick still climbing: step onto the landing.
		   The push is horizontal and the fall carries it from there, so the
		   body arrives with the landing clip it already has.

		   The heading is commandeered along with the velocity. The fall
		   steers toward whatever the stick last asked for, and on a ladder
		   the stick means up, not a direction — left to itself the body
		   would crest the top and immediately walk off wherever the camera
		   happened to be pointing, which mostly means back into the rungs
		   it just left. It holds this heading until the stick is moved. */
		if (cmd->climb > 0.0f) {
			float s, c;
			fm_sincosf(deg_to_rad(-data->ladder_yaw), &s, &c);
			body->velocity.x = CHARACTER3D_LADDER_EXIT_SPEED * s;
			body->velocity.y = CHARACTER3D_LADDER_EXIT_SPEED * -c;
			body->velocity.z = 0.0f;
			data->horizontal_speed = CHARACTER3D_LADDER_EXIT_SPEED;
			cmd->target_yaw = -data->ladder_yaw;
		}
		movement->next = MOVEMENT_STATE_FALLING;
		return;
	}

	/* Back at the foot with the stick pushing down: the ground itself is
	   what says so, not the volume — its floor sits below the stair's, so
	   that someone standing at the bottom is inside it to begin with. */
	if (cmd->climb < 0.0f && data->floor_distance >= 0.0f
	 && data->floor_distance <= CHARACTER3D_LADDER_GROUND_REACH)
		movement->next = movement->locomotion;
}

/* Swim entry and exit, from the water probe of the physics pass. Two separate
   questions, never one: the volume says whether there is water, and the floor
   says whether it can be walked. A body inside the water with nothing under
   its feet swims; one standing on the bottom wades, however deep the water
   reaches on it.

   How far the body sinks is not asked here on purpose. It moves with the
   buoyancy, which moves with the speed, and a state read off it changes when
   the stick does: standing still in a shallow pool would drop the swim, and
   pushing the stick would take it back, over and over. */
static void evaluateWater(Character3D *character)
{
	Movement *movement = &character->movement;
	const MovementData *data = &movement->data;
	uint8_t current = movement->current;

	if (current != MOVEMENT_STATE_SWIMMING) {
		bool can_enter = isLocomotion(current)
		              || current == MOVEMENT_STATE_FALLING
		              || current == MOVEMENT_STATE_ROLLING;

		if (can_enter && data->in_water && !data->is_grounded)
			movement->next = MOVEMENT_STATE_SWIMMING;
		return;
	}

	/* Out of the water, or standing on the bottom of it. Airborne, the floor
	   probe of the physics pass turns it into falling on its own. */
	if (!data->in_water || data->is_grounded)
		movement->next = movement->locomotion;
}

static void (*handler[MOVEMENT_STATE_COUNT])(Character3D *, MovementCommand *, float) = {
	[MOVEMENT_STATE_IDLE] = setLocomotion,
	[MOVEMENT_STATE_WALKING] = setLocomotion,
	[MOVEMENT_STATE_ROLLING] = setRolling,
	[MOVEMENT_STATE_FALLING] = setFalling,
	[MOVEMENT_STATE_SWIMMING] = setSwimming,
	[MOVEMENT_STATE_CLIMBING] = setClimbing,
};

_Static_assert(sizeof(handler) / sizeof(handler[0]) == MOVEMENT_STATE_COUNT, "movement::handler must have one entry per character state");

}
}

void Character3D::updateMovement(character3d::MovementCommand *cmd, float dt)
{
	assert(cmd);

	assert(movement.current < character3d::MOVEMENT_STATE_COUNT);
	assert(character3d::movement::handler[movement.current] != NULL);

	movement.data.rotation_mode = CHARACTER3D_ROTATION_MODE_LERP;
	movement.data.strafe = cmd->strafe;
	movement.data.strafe_locked = cmd->strafe_locked;
	movement.data.strafe_yaw = cmd->strafe_yaw;
	movement.data.aiming = cmd->aiming;
	movement.data.charging_shoot = cmd->charging_shoot;
	movement.data.shooting = cmd->shooting;

	/* Tired locks the top gait away: the character stays on the previous one
	   until the stamina is back. */
	float gait = cmd->gait;
	uint8_t last = movement.settings->gait_count - 1;
	if (stats.tired && last > 0 && gait > (float)(last - 1))
		gait = (float)(last - 1);
	movement.data.gait = gait;
	movement.next = character3d::MOVEMENT_STATE_NONE;

	character3d::movement::handler[movement.current](this, cmd, dt);
	character3d::movement::updateBody(this, dt);
	character3d::movement::evaluateWater(this);
	character3d::movement::evaluateLadder(this, cmd);
	character3d::movement::evaluateTransitions(this);
}

}
