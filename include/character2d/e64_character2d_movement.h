/*
	The 2D character's movement: what turns a command into a velocity.
	The horizontal axis runs on gaits, the vertical on gravity and a jump, 
	and there is no jumping state: the crouch that starts a jump runs on the
	ground, and the air is always a fall.

	Screen y grows downward, so gravity is added to the velocity and the jump
	is subtracted from it.
*/
#ifndef ENGINE64_CHARACTER2D_MOVEMENT_H
#define ENGINE64_CHARACTER2D_MOVEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "physics/math/e64_vector2.h"

namespace e64 {

typedef struct Character2D Character2D;


/* Under this the body counts as standing still: what the animation reads to
   tell an idle from a walk. Pixels per second. */
#define CHARACTER2D_LOCOMOTION_MIN_SPEED 1.0f

#define CHARACTER2D_JUMP_HOLD_VELOCITY_SCALE   0.96f
#define CHARACTER2D_JUMP_LAUNCH_VELOCITY_SCALE 0.8f

/* Pixels per second squared, positive: down the screen axis. */
#define CHARACTER2D_GRAVITY         900.0f
#define CHARACTER2D_FALL_MAX_SPEED  600.0f


namespace character2d {

/* How the button becomes height. Charge holds the body down for as long as
   the crouch lasts and launches with what it gathered; snap leaves the floor
   on the press and keeps adding while the button stays down. */
typedef enum {
	JUMP2D_CHARGE,
	JUMP2D_SNAP,
} JumpMode;


/* No jumping state: the crouch that starts a jump runs on the ground, over
   locomotion, and the air is always a fall. Whatever mechanic comes next
   comes in here. */
typedef enum {
	MOVEMENT2D_STATE_IDLE,
	MOVEMENT2D_STATE_WALKING,
	MOVEMENT2D_STATE_ROLLING,
	MOVEMENT2D_STATE_FALLING,
	MOVEMENT2D_STATE_COUNT,
	MOVEMENT2D_STATE_NONE
} MovementState;


/* What the control asks of the body this frame. */
typedef struct MovementCommand {

	float direction;        /* -1 left, +1 right, 0 none */

	bool  jump_held;
	bool  jump_triggered;
	bool  roll_triggered;

	uint8_t gait;
	float   speed_scale;    /* 1.0 normal, lower while tired */

} MovementCommand;


/* One gait phase of the WALKING state, walk first and the fastest last. How
   many and their values are up to the caller; the order runs from lowest to
   highest target_speed. Pixels per second, and how fast the current speed is
   pulled toward it, per second. */
typedef struct GaitSettings {

	float target_speed;
	float response_rate;

} GaitSettings;


typedef struct MovementSettings {

	/* What brakes the body when the stick lets go. */
	float idle_response_rate;

	const GaitSettings *gait;
	uint8_t             gait_count;

	/* The roll, three phases off one timer: the launch drives the direction
	   it was asked with at the roll's own speed, the spin holds whatever
	   speed it reached along the body's facing, and the grip hands the
	   steering back to the stick. Pixels per second, seconds. */
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
	   held. Lower climbs higher; at 1.0 holding does nothing. Only on the way
	   up, so nobody floats down. */
	float jump_hold_gravity_scale;

	/* Snap only: how long the jump still answers after the floor is gone. */
	float jump_coyote_time;

	/* How much of the ground's steering the air gets, 0 to 1. */
	float air_control;

} MovementSettings;


typedef struct MovementData {

	Vector2 velocity;        /* pixels per second, y downward */
	float   horizontal_speed;

	Vector2 jump_initial_velocity;
	float   jump_force;
	float   jump_timer;

	float roll_timer;
	float roll_direction;    /* -1 or +1, taken on the trigger and held until grip */

	/* Time since the floor was lost, counted only while the coyote window is
	   still open. Reset on every landing. */
	float coyote_timer;

	/* Written by the physics every frame: whether the feet rest on a floor,
	   where that floor is in pixels, and how far above it the feet are,
	   which the animation reads to start the landing one clip-to-contact
	   early. */
	bool  is_grounded;
	float grounding_height;
	float floor_distance;

	uint8_t gait;

} MovementData;


typedef struct Movement {

	const MovementSettings *settings;
	MovementData            data;

	uint8_t current;
	uint8_t locomotion;
	uint8_t next;

} Movement;


namespace movement {

void setMode(Movement *movement, uint8_t new_mode);
bool isLocomotion(uint8_t mode);

/* A crouch under way: the jump it is building survives the ledge running out
   mid-charge, so the body stays in locomotion until it launches. */
bool isChargingJump(const Character2D *character);

/* Still inside the coyote window: off the floor, but not for long enough that
   the jump has stopped answering. The control reads it to keep taking the
   button after the ledge. */
bool isCoyoteOpen(const Character2D *character);

}
}

}

#endif
