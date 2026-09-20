/*
	Port of character3d::physics to the plane, which is itself Godot's
	test_body_motion recovery and CharacterBody3D::_set_collision_direction.
	Everything runs in Vector3 with z at zero, so the capsule against a cell
	is aabb_closestToSegment and the floor probe is aabb_closestToPoint, the
	same distance code the 3D body uses against boxes.
*/
#include <math.h>
#include <stdint.h>

#include "character2d/e64_character2d.h"
#include "stage2d/e64_stage2d.h"
#include "physics/math/e64_math_common.h"
#include "physics/math/e64_math_functions.h"
#include "physics/geometry/e64_aabb.h"

namespace e64 {

#define CHARACTER2D_MAX_CONTACTS           16      /* contacts kept per recovery pass */
#define CHARACTER2D_RECOVERY_ATTEMPTS      4       /* Godot: recover_attempts */
#define CHARACTER2D_RECOVERY_MARGIN        0.05f   /* Godot's safe margin, in pixels */
#define CHARACTER2D_MIN_CONTACT_DEPTH      (CHARACTER2D_RECOVERY_MARGIN * 0.05f)  /* Godot: TEST_MOTION_MIN_CONTACT_DEPTH_FACTOR */
#define CHARACTER2D_RECOVERY_FACTOR        0.4f    /* Godot: fraction of the depth recovered per pass */
#define CHARACTER2D_FLOOR_SNAP_LENGTH      2.0f    /* downward probe, pixels */
#define CHARACTER2D_FALL_PROBE_CELLS       6       /* how far down the landing is looked for */
/* Walkable limit of 50 degrees plus Godot's FLOOR_ANGLE_THRESHOLD of 0.01
   rad, as a cosine: floor is decided on the cosine, never the angle. The
   argument is constant, so gcc folds the cosf at compile time. */
#define CHARACTER2D_FLOOR_MAX_SLOPE_COS    cosf(PI / 180 * 50.0f + 0.01f)


namespace character2d {
namespace physics {

typedef struct Contact {
	Vector3 normal;   /* from the cell toward the body */
	float   depth;
} Contact;

typedef struct CollisionState {
	bool    floor;
	bool    wall;
	bool    ceiling;
	Vector3 wall_normal;
	float   wall_depth;
} CollisionState;


/* --- the capsule ------------------------------------------------------------ */

/* The inner segment of the capsule, feet at position: from the bottom
   sphere's centre up to the top sphere's centre. */
static void getSegment(const Character2D *character, Vector3 *a, Vector3 *b)
{
	const ColliderSettings *collider = character->def->collider_settings;
	float r = collider->radius;

	*a = (Vector3){ character->position.x, character->position.y - r, 0.0f };
	*b = (Vector3){ character->position.x, character->position.y - collider->height + r, 0.0f };
}

/* The box of a cell of the stage, a slab through z so the 3D distance code
   never leaves the plane. */
static AABB cellBox(const Stage2D *stage, int32_t x, int32_t y)
{
	Vector2 origin = stage->entity->position;
	float   x0 = origin.x + x * stage->cell_width;
	float   y0 = origin.y + y * stage->cell_height;
	return (AABB){
		{ x0, y0, -1.0f },
		{ x0 + stage->cell_width, y0 + stage->cell_height, 1.0f },
	};
}

/* The cells a world box reaches, clamped to nothing: isSolid answers open
   outside the map. */
static void cellRange(const Stage2D *stage, float min_x, float min_y, float max_x, float max_y,
                      int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
	Vector2 origin = stage->entity->position;
	*x0 = (int32_t)floorf((min_x - origin.x) / stage->cell_width);
	*x1 = (int32_t)floorf((max_x - origin.x) / stage->cell_width);
	*y0 = (int32_t)floorf((min_y - origin.y) / stage->cell_height);
	*y1 = (int32_t)floorf((max_y - origin.y) / stage->cell_height);
}


/* --- contacts ---------------------------------------------------------------- */

/* Every solid cell the capsule overlaps, as a contact: the closest point of
   the cell to the segment, the normal from it toward the segment, the depth
   under the radius. A segment run through by the cell has no direction to
   give, and is pushed up. */
static int collectContacts(const Character2D *character, Contact *contacts)
{
	const Stage2D *stage = character->stage;
	float radius = character->def->collider_settings->radius;

	Vector3 a, b;
	getSegment(character, &a, &b);

	float reach = radius + CHARACTER2D_RECOVERY_MARGIN;
	int32_t x0, y0, x1, y1;
	cellRange(stage, a.x - reach, b.y - reach, a.x + reach, a.y + reach, &x0, &y0, &x1, &y1);

	int count = 0;
	for (int32_t y = y0; y <= y1 && count < CHARACTER2D_MAX_CONTACTS; y++) {
		for (int32_t x = x0; x <= x1 && count < CHARACTER2D_MAX_CONTACTS; x++) {
			if (!stage2d_isSolid(stage, x, y)) continue;

			AABB    box     = cellBox(stage, x, y);
			Vector3 on_box  = aabb_closestToSegment(&box, &a, &b);
			Vector3 on_seg  = segment_closestToPoint(&a, &b, &on_box);
			Vector3 d       = vector3_difference(&on_seg, &on_box);
			float   dist2   = vector3_dot(&d, &d);
			if (dist2 > reach * reach) continue;

			float dist = sqrtf(dist2);
			contacts[count].normal = (dist > 1.0e-6f) ? vector3_scaled(&d, 1.0f / dist) : vector3_create(0.0f, -1.0f, 0.0f);
			contacts[count].depth  = radius - dist;
			count++;
		}
	}
	return count;
}

/* Port of CharacterBody3D::_set_collision_direction: every contact of the
   pass is classified by its angle against the up axis, floor, ceiling or
   wall, and the frame state accumulates them. Up is -y here. */
static void classifyContacts(const Contact *contacts, int count, CollisionState *state)
{
	bool was_wall   = state->wall;
	bool pass_floor = false;
	bool pass_wall  = false;

	int     wall_collision_count = 0;
	Vector3 combined_wall_normal = { 0.0f, 0.0f, 0.0f };
	Vector3 tmp_wall_col         = { 0.0f, 0.0f, 0.0f };

	for (int i = count - 1; i >= 0; i--) {
		const Contact *c = &contacts[i];

		/* dot(normal, up) == -normal.y; angle <= limit is cosine >= its cosine */
		if (-c->normal.y >= CHARACTER2D_FLOOR_MAX_SLOPE_COS) {
			pass_floor   = true;
			state->floor = true;
			continue;
		}

		if (c->normal.y >= CHARACTER2D_FLOOR_MAX_SLOPE_COS) {
			state->ceiling = true;
			continue;
		}

		/* Collision is wall by default. */
		pass_wall   = true;
		state->wall = true;

		if (c->depth > state->wall_depth) {
			state->wall_depth  = c->depth;
			state->wall_normal = c->normal;
		}

		Vector3 d = vector3_difference(&c->normal, &tmp_wall_col);
		if (vector3_dot(&d, &d) > 1.0e-6f) {
			tmp_wall_col = c->normal;
			vector3_add(&combined_wall_normal, &c->normal);
			wall_collision_count++;
		}
	}

	/* Two steep walls can add up to walkable support (a wedge). */
	if (pass_wall && wall_collision_count > 1 && !pass_floor) {
		float magnitude = vector3_magnitude(&combined_wall_normal);
		if (magnitude > 1.0e-6f && -combined_wall_normal.y >= CHARACTER2D_FLOOR_MAX_SLOPE_COS * magnitude) {
			state->floor = true;
			state->wall  = was_wall;
		}
	}
}

/* Port of test_body_motion STEP 1 (free body if stuck): one recovery vector
   accumulated over every contact and applied whole, up to four attempts. */
static void recover(Character2D *character, CollisionState *state)
{
	int recover_attempts = CHARACTER2D_RECOVERY_ATTEMPTS;

	do {
		Contact contacts[CHARACTER2D_MAX_CONTACTS] __attribute__((uninitialized));
		int count = collectContacts(character, contacts);
		if (!count) break;

		classifyContacts(contacts, count, state);

		Vector3 recover_motion = { 0.0f, 0.0f, 0.0f };
		for (int i = 0; i < count; i++) {
			float depth = contacts[i].depth - vector3_dot(&contacts[i].normal, &recover_motion);
			if (depth > CHARACTER2D_MIN_CONTACT_DEPTH + 1.0e-5f)
				vector3_addScaledVector(&recover_motion, &contacts[i].normal, (depth - CHARACTER2D_MIN_CONTACT_DEPTH) * CHARACTER2D_RECOVERY_FACTOR);
		}

		if (vector3_dot(&recover_motion, &recover_motion) == 0.0f) break;

		character->position.x += recover_motion.x;
		character->position.y += recover_motion.y;
	} while (--recover_attempts);
}


/* --- responses --------------------------------------------------------------- */

static void setGroundResponse(Character2D *character)
{
	MovementData *data = &character->movement.data;

	/* Moving up (jump takeoff): touching the ground must not cancel it. */
	if (data->velocity.y < 0.0f) return;

	data->is_grounded = true;
	data->velocity.y  = 0.0f;

	/* The floor is back: the next ledge gets its own coyote window. */
	data->coyote_timer = 0.0f;
	if (character->movement.current == MOVEMENT2D_STATE_FALLING)
		movement::setMode(&character->movement, character->movement.locomotion);
}

static void setCeilingResponse(Character2D *character)
{
	MovementData *data = &character->movement.data;
	if (data->velocity.y < 0.0f) data->velocity.y = 0.0f;
}

/* Slide: remove the velocity component pushing into the wall, keep the
   rest. On the floor only the horizontal part of the wall normal is used,
   so a corner does not inject upward velocity that kills the floor. */
static void setWallResponse(Character2D *character, const CollisionState *state, bool was_on_floor)
{
	MovementData *data = &character->movement.data;

	if (was_on_floor || data->is_grounded) {
		float nx = state->wall_normal.x;
		if (fabsf(nx) < 1.0e-6f) return;
		nx = nx < 0.0f ? -1.0f : 1.0f;

		float t = data->velocity.x * nx;
		if (t < 0.0f) data->velocity.x -= t * nx;
	}
	else {
		float t = data->velocity.x * state->wall_normal.x + data->velocity.y * state->wall_normal.y;
		if (t < 0.0f) {
			data->velocity.x -= t * state->wall_normal.x;
			data->velocity.y -= t * state->wall_normal.y;
		}
	}
}

static void respond(Character2D *character, const CollisionState *state, bool was_on_floor)
{
	if (state->floor)   setGroundResponse(character);
	if (state->ceiling) setCeilingResponse(character);
	if (state->wall)    setWallResponse(character, state, was_on_floor);
}


/* --- floor ------------------------------------------------------------------- */

/* Floor query: the capsule's bottom sphere swept down by the snap length.
   Answers whether there is walkable floor under the body, how deep the probe
   sinks into it and with which normal. */
typedef struct FloorProbe {
	bool    found;
	float   penetration;
	Vector3 normal;
} FloorProbe;

static void floorProbe_consider(FloorProbe *probe, const Vector3 *center, float radius, const Vector3 *closest)
{
	Vector3 d     = vector3_difference(center, closest);
	float   dist2 = vector3_dot(&d, &d);
	if (dist2 > radius * radius) return;

	float dist = sqrtf(dist2);
	Vector3 normal = (dist > 1.0e-6f)
		? vector3_scaled(&d, 1.0f / dist)
		: vector3_create(0.0f, -1.0f, 0.0f);

	/* Walkable floor only. */
	if (-normal.y < CHARACTER2D_FLOOR_MAX_SLOPE_COS) return;

	float penetration = radius - dist;
	if (!probe->found || penetration > probe->penetration) {
		probe->found       = true;
		probe->penetration = penetration;
		probe->normal      = normal;
	}
}

static void findFloor(const Character2D *character, FloorProbe *probe)
{
	const Stage2D *stage = character->stage;
	float radius = character->def->collider_settings->radius;

	/* Bottom-sphere centre, swept down by the snap length. */
	Vector3 center = { character->position.x, character->position.y - radius + CHARACTER2D_FLOOR_SNAP_LENGTH, 0.0f };

	/* Every field written up front: found gates the others, and the
	   compiler cannot see that across the inlining. */
	probe->found       = false;
	probe->penetration = 0.0f;
	probe->normal      = vector3_create(0.0f, -1.0f, 0.0f);

	int32_t x0, y0, x1, y1;
	cellRange(stage, center.x - radius, center.y - radius, center.x + radius, center.y + radius, &x0, &y0, &x1, &y1);

	for (int32_t y = y0; y <= y1; y++)
		for (int32_t x = x0; x <= x1; x++) {
			if (!stage2d_isSolid(stage, x, y)) continue;
			AABB    box     = cellBox(stage, x, y);
			Vector3 closest = aabb_closestToPoint(&box, &center);
			floorProbe_consider(probe, &center, radius, &closest);
		}
}

/* How far the floor is straight below the feet, so the animation can start
   the landing exactly one clip-to-contact away from it. Negative with
   nothing within reach. */
static float floorDistance(const Character2D *character)
{
	const Stage2D *stage = character->stage;
	Vector2 origin = stage->entity->position;

	int32_t col = (int32_t)floorf((character->position.x - origin.x) / stage->cell_width);
	int32_t row = (int32_t)floorf((character->position.y - origin.y) / stage->cell_height);

	for (int32_t y = row; y <= row + CHARACTER2D_FALL_PROBE_CELLS; y++) {
		if (!stage2d_isSolid(stage, col, y)) continue;
		float top = origin.y + y * stage->cell_height;
		if (top < character->position.y) continue;   /* the cell the feet are already in */
		return top - character->position.y;
	}
	return -1.0f;
}

/* Godot's _snap_on_floor conditions: only when the body was on the floor,
   is not on it now, and is not moving up. Vertical-only correction. */
static void snapToFloor(Character2D *character, const FloorProbe *floor, bool was_on_floor, bool velocity_facing_up)
{
	if (character->movement.data.is_grounded || !was_on_floor || velocity_facing_up) return;
	if (!floor->found) return;

	float drop = CHARACTER2D_FLOOR_SNAP_LENGTH - floor->penetration / -floor->normal.y;
	if (drop > 0.0f) character->position.y += drop;

	setGroundResponse(character);
}


void collide(Character2D *character)
{
	MovementData *data = &character->movement.data;

	/* No stage, nothing to stand on or walk into. */
	if (!character->stage) return;

	bool was_on_floor       = data->is_grounded;
	bool velocity_facing_up = data->velocity.y < 0.0f;
	data->is_grounded = false;

	CollisionState state = { .wall_depth = -1.0f };
	recover(character, &state);
	respond(character, &state, was_on_floor);

	FloorProbe floor __attribute__((uninitialized));
	findFloor(character, &floor);

	snapToFloor(character, &floor, was_on_floor, velocity_facing_up);

	/* Nothing walkable under the probe and no contact resolved as floor: a
	   fall. Only locomotion falls: a roll runs to the end of its own clip,
	   and a crouch under way holds the body until it launches. */
	if (!floor.found && !data->is_grounded
	 && movement::isLocomotion(character->movement.current)
	 && !movement::isChargingJump(character))
		movement::setMode(&character->movement, MOVEMENT2D_STATE_FALLING);

	if (data->is_grounded) data->grounding_height = character->position.y;

	/* Only worth measuring off the ground, and only on the way down: it
	   feeds the landing, and a rising body has nothing to time yet. */
	data->floor_distance = (!data->is_grounded && data->velocity.y > 0.0f)
		? floorDistance(character)
		: -1.0f;
}

}
}

}
