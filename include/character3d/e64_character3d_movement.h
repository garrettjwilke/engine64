#ifndef ENGINE64_CHARACTER3D_MOVEMENT_H
#define ENGINE64_CHARACTER3D_MOVEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include <t3d/t3dmath.h>

typedef struct Character3D Character3D;


#define LOCOMOTION_MIN_SPEED 0.05f

#define CHARACTER3D_ROTATION_SNAP_THRESHOLD 1.0f

#define CHARACTER3D_STRAFE_YAW_OFFSET 0.0f

#define CHARACTER3D_ROTATION_MODE_LERP 0
#define CHARACTER3D_ROTATION_MODE_SNAP 1

#define CHARACTER3D_JUMP_HOLD_VELOCITY_SCALE 0.96f
#define CHARACTER3D_JUMP_LAUNCH_VELOCITY_SCALE 0.8f

#define CHARACTER3D_GRAVITY -20.0f
#define CHARACTER3D_FALL_MAX_SPEED -15.0f

#define CHARACTER3D_WATER_DRAG             4.0f    /* vertical, per second, at full submersion */
#define CHARACTER3D_WATER_SINK_MAX_SPEED  -2.0f    /* fully reached at the fraction below */
#define CHARACTER3D_WATER_SINK_LIMIT_FULL  0.8f    /* submersion where the sink limit saturates */

/* Swim entry is the character's own swim equilibrium: sunk past the depth
   its buoyancy holds it at, the water is already carrying it, so it swims.
   A fixed threshold could sit above that depth, and then a body floating at
   its equilibrium never reaches it — it stays falling on the surface.

   Exit trails entry by this factor, the hysteresis that keeps the waves from
   flickering the state at the boundary. Exit also needs footing. */
#define CHARACTER3D_WATER_SWIM_EXIT_SCALE 0.9f

/* Where the body sits while climbing. The clip grips 0.26 m ahead of the
   origin, but holding at that distance buries a capsule of 0.35 radius nine
   centimetres into the rungs, and a body sunk into the ladder reads worse
   than hands closing just short of it. Stand at the radius: the capsule
   comes to rest against the plane of the rungs, where a climber belongs.
   Lower it toward 0.26 to close the hands, raise it to pull clear. */
#define CHARACTER3D_LADDER_STAND_DISTANCE 0.35f

/* How hard the body is pulled onto the ladder's centre line and holding
   distance, per second. Fast enough that the entry snap reads as a grab. */
#define CHARACTER3D_LADDER_ANCHOR_RATE    12.0f

/* How close the ground has to be for a descent to end on it. */
#define CHARACTER3D_LADDER_GROUND_REACH   0.15f

/* Hysteresis on the top of the volume, the same trick the swim thresholds
   use. Leaving happens on losing the volume, so without it the two edges
   are one line: a body standing on the crest has its feet exactly at the
   boundary, and a stick still asking to climb re-grabs the ladder the frame
   after it let go, over and over. Grabbing on has to happen this far below
   the top, which is out of reach of anything already standing on it. Only
   the top edge moves — the foot of the ladder is nowhere near it. */
#define CHARACTER3D_LADDER_ENTER_MARGIN   0.50f

/* Widest angle between the stick and the ladder's facing that still counts
   as asking to climb. Past it the stick is walking past the ladder. */
#define CHARACTER3D_LADDER_ENTER_ANGLE    70.0f

/* Push toward the rungs when the climb runs off the top of the volume, so
   the body steps onto the landing instead of sliding back down the face it
   was hugging. The climbable volume is what decides where the top is: the
   climb ends where the volume does. */
#define CHARACTER3D_LADDER_EXIT_SPEED     1.6f

enum {
	CHARACTER3D_SWIM_GAIT_IDLE,
	CHARACTER3D_SWIM_GAIT_SLOW,
	CHARACTER3D_SWIM_GAIT_FAST,
};


/* How the button becomes height. Charge holds the body down for as long as
   the crouch lasts and launches with what it gathered; snap leaves the floor
   on the press and keeps adding while the button stays down. Zero is charge,
   which is what every character did before the choice existed. */
typedef enum {
	JUMP_CHARGE,
	JUMP_SNAP,
} JumpMode;


/* No jumping state: the crouch that starts a jump runs on the ground, over
   locomotion, and the air is always a fall. */
typedef enum {
	MOVEMENT_STATE_IDLE,
	MOVEMENT_STATE_WALKING,
	MOVEMENT_STATE_ROLLING,
	MOVEMENT_STATE_FALLING,
	MOVEMENT_STATE_SWIMMING,
	MOVEMENT_STATE_CLIMBING,
	MOVEMENT_STATE_COUNT,
	MOVEMENT_STATE_NONE
} MovementState;

/* One gait phase of the WALKING state. How many and their values are up to
   the caller; the order runs from lowest to highest target_speed.

   stick_threshold is how far the stick has to be pushed to reach this gait, as
   a share of its travel. The stick picks the highest gait whose threshold it
   has passed. The first gait needs none: it is the floor any stick out of the
   deadzone already reaches.

   Past the first one, a gait left without a threshold belongs to the sprint
   button and the stick never picks it. Declare it last and the button is what
   reaches it; give it a threshold instead and the stick reaches it like any
   other gait. */
typedef struct {
	float target_speed;
	float response_rate;
	float rotation_response_rate;
	float stick_threshold;
} Character3DGaitSettings;

typedef struct {

	float idle_response_rate;
	float idle_rotation_response_rate;

	const Character3DGaitSettings *gait;
	uint8_t gait_count;

	float roll_target_speed;
	float roll_launch_response_rate;
	float roll_spin_response_rate;
	float roll_grip_response_rate;
	float roll_ground_time;
	float roll_grip_time;
	float roll_timer_max;

	JumpMode jump_mode;

	float jump_response_rate;
	/* What the launch is worth on its own: the whole of it under snap, the
	   floor a short charge cannot go under. */
	float jump_base_speed;

	/* Charge only: the crouch lasts this long, and what it gathered is
	   multiplied into the launch. */
	float jump_force_multiplier;
	float jump_timer_max;

	/* Snap only: the fraction of gravity the rise pays while the button is
	   held. Lower climbs higher; at 1.0 holding does nothing. It only ever
	   applies on the way up, so nobody floats down. */
	float jump_hold_gravity_scale;

	/* Snap only: how long the jump still answers after the floor is gone.
	   Walking off a ledge the body coasts for this long before the fall
	   takes it, and the button launches the whole way. Charge needs none of
	   this: its own crouch is the window. */
	float jump_coyote_time;

	/* How much of the ground's steering the air gets, 0 to 1. At zero the
	   body keeps the heading and the speed it left with, and the stick does
	   nothing until it lands; at one it turns and accelerates in the air
	   exactly as it would on the floor. */
	float air_control;

	float swim_slow_speed;
	float swim_fast_speed;
	float swim_response_rate;

	/* Vertical speed on a ladder and how fast it is reached. The climb clip
	   is timed against the first of them: at full speed it plays at its own
	   pace, and slower while the second is still ramping up to it. */
	float climb_speed;
	float climb_response_rate;

	/* Fake buoyancy: submerged fraction of the capsule where the scaled
	   gravity flips sign. The equilibrium follows the pose the clips are
	   authored at: treading water holds the head at the capsule top,
	   stroking holds the body at its middle, and speed slides the target
	   between the two so the swim rides up to the surface.

	   The swim one doubles as the swim state threshold: sunk past it the
	   water carries the body, so that is where the swim begins, and exit
	   trails it by CHARACTER3D_WATER_SWIM_EXIT_SCALE. */
	float water_equilibrium_idle;
	float water_equilibrium_swim;

} Character3DMovementSettings;

typedef struct {
	float previous_yaw;
	float horizontal_speed;
	float roll_timer;
	Vector3 jump_initial_velocity;
	float jump_force;
	float jump_timer;
	/* Time since the floor was lost, counted only while the coyote window is
	   still open. Reset on every landing. */
	float coyote_timer;
	float roll_yaw;
	bool is_grounded;
	/* Straight down from the feet, written by the collision pass. Negative
	   with no floor within reach: the animation times the landing on it. */
	float floor_distance;
	
	bool in_water;
	float submerged_fraction;   /* 0..1 of the capsule under the surface */

	/* Written by the ladder probe of the collision pass. The anchor is where
	   the body has to stand to reach the rungs: the ladder's centre line,
	   pulled out to the clip's holding distance on the side the body is on. */
	bool  on_ladder;
	float ladder_yaw;      /* body rotation.z that faces the rungs */
	float ladder_anchor_x;
	float ladder_anchor_y;
	float ladder_top;      /* world z of the climbable volume's ceiling */

	uint8_t rotation_mode;
	bool strafe;
	bool strafe_locked;
	float strafe_yaw;
	
	bool aiming;
	bool charging_shoot;
	bool shooting;

	float gait;
} Character3DMovementData;

typedef struct {
	float target_yaw;
	
	bool roll_triggered;

	bool jump_held;
	bool jump_triggered;

	bool strafe;
	bool strafe_locked;
	float strafe_yaw;

	bool aiming;
	bool charging_shoot;
	bool shooting;

	/* Which gait is asked for. A whole number is that gait; between two of
	   them the speed lands between theirs. A character with a single gait has
	   none above it to step to, so the fraction is the share of that one
	   speed being asked for, straight off how far the stick is held. */
	float gait;
	uint8_t swim_gait;   /* CHARACTER3D_SWIM_GAIT_*, from the stick while swimming */

	/* Stick along the ladder: +1 climbs, -1 descends, 0 holds. The stick is
	   read in the ladder's own frame, so pushing at the rungs always climbs
	   whichever way the camera happens to look. */
	float climb;
	bool  climb_release;   /* jump button: let go and drop */
} MovementCommand;

typedef struct Character3DMovement {
	const Character3DMovementSettings *settings;
	Character3DMovementData data;
	uint8_t current;
	uint8_t locomotion;
	uint8_t next;
} Character3DMovement;

void character3d_updateMovement(Character3D *character, MovementCommand *cmd, float dt);
void character3dMovement_setMode(Character3DMovement *movement, uint8_t new_mode);
bool character3dMovement_isLocomotion(uint8_t mode);

/* A crouch under way. The floor probe reads it to leave the body in locomotion
   when the ledge runs out mid-charge, so the jump it was building survives. */
bool character3dMovement_isChargingJump(const Character3D *character);

/* Still inside the coyote window: off the floor, but not for long enough that
   the jump has stopped answering. The control reads it to keep taking the
   button after the ledge. */
bool character3dMovement_isCoyoteOpen(const Character3D *character);

#endif
