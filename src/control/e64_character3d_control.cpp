#include <math.h>
#include <fmath.h>

#include "entity/e64_entity3d.h"
#include "control/e64_character3d_control.h"

namespace e64 {

namespace character3d {
namespace control {

static void setJump(Character3D *character, MovementCommand *cmd, const Controls *actions)
{
	Movement *movement = &character->movement;

	/* The button never changes the state: it only asks for a jump. The crouch
	   runs on the ground and the movement switches to the air on the impulse.
	   The coyote window counts as ground for this: the ledge is gone but the
	   ask is still taken. A ladder is not: the climb keeps the body off the
	   floor with the coyote timer frozen, so the window would read as open
	   for the whole climb. There the button means let go, never jump. */
	if (actions->jump
	    && movement->current != MOVEMENT_STATE_CLIMBING
	    && (character3d::movement::isLocomotion(movement->current)
	        || character3d::movement::isCoyoteOpen(character))
	    && movement->data.jump_timer == 0.0f) {
		cmd->jump_held      = true;
		cmd->jump_triggered = true;
	} else if (actions->jump_held) {
		return;
	} else {
		cmd->jump_held = false;
	}
}

static void setRoll(Character3D *character, MovementCommand *cmd, const Controls *actions)
{
	Movement *movement = &character->movement;

	/* Not while a jump is being charged: the crouch owns the body until it
	   takes off. */
	if (actions->roll && character3d::movement::isLocomotion(movement->current)
	    && movement->current != MOVEMENT_STATE_IDLE
	    && movement->data.jump_timer == 0.0f) {
		cmd->roll_triggered = true;
		character3d::movement::setMode(movement, MOVEMENT_STATE_ROLLING);
	}
}

/* Only with both feet in ordinary locomotion: never mid-air, mid-roll, in
   the water or on a ladder, and not while a jump crouch owns the body. */
static void setWeaponSwitch(Character3D *character, const Controls *actions)
{
	Movement *movement = &character->movement;

	if (!character3d::movement::isLocomotion(movement->current)
	    || movement->data.jump_timer != 0.0f) return;

	if (actions->weapon_next) weapon::cycle(character, +1);
	if (actions->weapon_prev) weapon::cycle(character, -1);
}

static void setLocomotionWithStick(Character3D *character, MovementCommand *cmd, const Controls *actions, float camera_angle_around)
{
	Movement *movement = &character->movement;
	float stick_magnitude = 0;

	/* The deadzone was already taken out: centred, the stick reads zero. */
	if (actions->stick_x != 0.0f || actions->stick_y != 0.0f) {
		Vector2 stick   = {actions->stick_x, actions->stick_y};
		stick_magnitude = vector2_magnitude(&stick);
		cmd->target_yaw = rad_to_deg(fm_atan2f(actions->stick_x, -actions->stick_y) - deg_to_rad(camera_angle_around));
	}

	/* Swimming keeps its state; the stick only picks the swim gait. */
	if (movement->current == MOVEMENT_STATE_SWIMMING) {
		if (stick_magnitude == 0)  cmd->swim_gait = CHARACTER3D_SWIM_GAIT_IDLE;
		else if (actions->sprint)  cmd->swim_gait = CHARACTER3D_SWIM_GAIT_FAST;
		else                       cmd->swim_gait = CHARACTER3D_SWIM_GAIT_SLOW;
		return;
	}

	uint8_t mode = (stick_magnitude == 0) ? MOVEMENT_STATE_IDLE : MOVEMENT_STATE_WALKING;

	/* An action owns the current state and its gait until it ends: the stick
	   only picks the state it goes back to. */
	if (!character3d::movement::isLocomotion(movement->current)) {
		movement->locomotion = mode;
		return;
	}

	character3d::movement::setMode(movement, mode);
	if (mode == MOVEMENT_STATE_IDLE) return;

	const MovementSettings *settings = movement->settings;
	uint8_t last_gait = settings->gait_count - 1;

	/* A single gait has none above it to step to: how far the stick is held is
	   the share of that one speed being asked for, so the body walks anywhere
	   between a crawl and its top speed. */
	if (last_gait == 0) {
		cmd->gait = (stick_magnitude < 1.0f) ? stick_magnitude : 1.0f;
		return;
	}

	/* The sprint button owns the last gait. Otherwise the stick reaches for the
	   highest gait it has pushed past, skipping the ones declared without a
	   threshold: those are the button's own. The first gait is never measured,
	   it is the floor a stick out of the deadzone already reaches. */
	bool sprinting = actions->sprint && !actions->aim;
	uint8_t reach  = sprinting ? last_gait : 0;

	if (!sprinting)
		for (uint8_t i = 1; i <= last_gait; i++)
			if (settings->gait[i].stick_threshold > 0.0f
			 && stick_magnitude >= settings->gait[i].stick_threshold) reach = i;

	cmd->gait = (float)reach;
}

/* The aim button holds the drawn weapon at the ready; the shoot button on top
   charges the shot. The release edge already travels in the actions, left for
   the shot itself. */
static void setAiming(Character3D *character, MovementCommand *cmd, const Controls *actions)
{
	const WeaponDef *drawn = weapon::drawn(character);
	bool charges = drawn && drawn->shoot_mode == SHOOT_CHARGE;

	cmd->aiming = drawn && actions->aim
		&& character3d::movement::isLocomotion(character->movement.current);
	cmd->charging_shoot = cmd->aiming && charges && actions->shoot;
}

static void setStrafe(Character3D *character, MovementCommand *cmd, const Controls *actions, float camera_angle_around)
{
	/* The air keeps the strafe: a jump out of it must not turn the body
	   toward its run, it faces the camera until it lands. */
	uint8_t current = character->movement.current;
	cmd->strafe     = actions->aim
	               && (character3d::movement::isLocomotion(current) || current == MOVEMENT_STATE_FALLING);
	cmd->strafe_yaw = angle_wrap(camera_angle_around + 180.0f + CHARACTER3D_STRAFE_YAW_OFFSET);
}

/* A ladder is asked for with the stick, never a button: pushing at the rungs
   climbs and pulling away from them descends. Both are read against the
   ladder's own facing, so which one the stick means never depends on where
   the camera happens to be. Only the release is a button, and it is the one
   that jumps everywhere else. */
static void setClimb(Character3D *character, MovementCommand *cmd, const Controls *actions)
{
	Movement *movement = &character->movement;

	cmd->climb         = 0.0f;
	cmd->climb_release = false;

	bool climbing = movement->current == MOVEMENT_STATE_CLIMBING;
	if (!climbing && !movement->data.on_ladder) return;

	if (climbing && actions->jump) {
		cmd->climb_release = true;
		return;
	}

	/* Already on it: the stick is read raw, up climbs and down descends. A
	   ladder is the one place the camera must not get a vote — it swings
	   around the body as it rises, and a heading built off it would turn
	   the same push into a climb or a drop depending on where it ended up. */
	if (climbing) {
		/* The reading is normalised and its deadzone is already gone, so any
		   value left standing is a real ask. */
		if (actions->stick_y > 0.0f) cmd->climb =  1.0f;
		if (actions->stick_y < 0.0f) cmd->climb = -1.0f;
		return;
	}

	if (actions->stick_x == 0.0f && actions->stick_y == 0.0f) return;

	/* Grabbing on is the opposite case: walking at a ladder is what asks
	   for it, so the entry is the camera-relative heading measured against
	   the ladder's facing. target_yaw is the stick already in world space
	   and a body's rotation is the negative of the heading it walks, which
	   is what brings the two into one frame to be compared. */
	float alignment = fm_cosf(deg_to_rad(cmd->target_yaw + movement->data.ladder_yaw));

	if (alignment >= fm_cosf(deg_to_rad(CHARACTER3D_LADDER_ENTER_ANGLE))) cmd->climb = 1.0f;
}

void read(Controls *controls, const ControlBinding *binding)
{
	const Controller *controller = &controller::get()[binding->player];

	*controls = (Controls){
		.jump       = controller::isPressed(controller, binding->jump),
		.jump_held  = controller::isHeld(controller, binding->jump),
		.roll       = controller::isPressed(controller, binding->roll),
		.sprint     = controller::isHeld(controller, binding->sprint),
		.aim        = controller::isHeld(controller, binding->aim),
		.shoot          = controller::isHeld(controller, binding->shoot),
		.shoot_released = controller::isReleased(controller, binding->shoot),
		.weapon_next    = controller::isPressed(controller, binding->weapon_next),
		.weapon_prev    = controller::isPressed(controller, binding->weapon_prev),
		/* Normalised here so everything downstream works in -1 to 1 and the
		   deadzone is taken out once, in the one place that reads the pad. */
		.stick_x = controller::getStickNormalized(controller).x,
		.stick_y = controller::getStickNormalized(controller).y,
	};
}

void update(Character3D *character, MovementCommand *cmd, const Controls *actions, float camera_angle_around)
{
	setWeaponSwitch(character, actions);
	setRoll(character, cmd, actions);
	setJump(character, cmd, actions);
	setStrafe(character, cmd, actions, camera_angle_around);
	setAiming(character, cmd, actions);
	setLocomotionWithStick(character, cmd, actions, camera_angle_around);

	/* After the stick: the climb is read off the heading it just wrote. */
	setClimb(character, cmd, actions);
}

}
}

}
