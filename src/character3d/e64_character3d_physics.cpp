/*
	Capsule collision response for the kinematic body, against static geometry.

	Depenetration and floor detection ported from Godot: the recovery step of
	GodotSpace3D::test_body_motion (modules/godot_physics_3d/godot_space_3d.cpp)
	and the contact classification of Character3DBody3D::_set_collision_direction
	(scene/3d/physics/character3d_body_3d.cpp). No swept motion: the body moves
	the full step first and is recovered out of penetration here.
*/
#include <math.h>
#include <fmath.h>
#include <stdint.h>

#include "character3d/e64_character3d.h"
#include "physics/math/e64_math_common.h"
#include "physics/math/e64_math_functions.h"
#include "physics/collision/e64_collision.h"
#include "physics/collision/e64_collision_mesh.h"


#define CHARACTER3D_MAX_CONTACTS           16      /* contacts kept per recovery pass */
#define CHARACTER3D_MAX_TRIANGLES          20      /* triangle candidates per query */
#define CHARACTER3D_RECOVERY_ATTEMPTS      4       /* Godot: recover_attempts */
#define CHARACTER3D_RECOVERY_MARGIN        0.001f  /* Godot: default safe margin */
#define CHARACTER3D_MIN_CONTACT_DEPTH      (CHARACTER3D_RECOVERY_MARGIN * 0.05f)  /* Godot: TEST_MOTION_MIN_CONTACT_DEPTH_FACTOR */
#define CHARACTER3D_RECOVERY_FACTOR        0.4f    /* Godot: fraction of the depth recovered per pass */
#define CHARACTER3D_FLOOR_SNAP_LENGTH      0.15f    /* downward probe, metres */
#define CHARACTER3D_FALL_PROBE_LENGTH      8.0f     /* how far down the landing is looked for */
/* Walkable limit of 50 degrees plus Godot's FLOOR_ANGLE_THRESHOLD of 0.01
   rad, as a cosine: floor is decided on the cosine, never the angle, because
   acosf is newlib with no fast counterpart. The argument is constant, so gcc
   folds the cosf at compile time. */
#define CHARACTER3D_FLOOR_MAX_SLOPE_COS    cosf(PI / 180 * 50.0f + 0.01f)


void character3dCollider_init(Character3DCollider *collider, float radius, float half_height)
{
	collider->shape.radius      = radius;
	collider->shape.half_height = half_height;
	transform_init(&collider->world);
}

void character3dCollider_setVertical(Character3DCollider *collider, const Vector3 *position)
{
	collider->world.position = (Vector3){
		position->x,
		position->y,
		position->z + collider->shape.radius + collider->shape.half_height,
	};
}


/* Registers the kinematic body in the world. Infinite mass and no gravity: the
   solver reads it to push rigid bodies and never writes back. It must not
   sleep, or the boxes resting on it would miss the moment it starts moving. */
void character3dPhysics_createBody(Character3D *character, PhysicsWorld *world)
{
	RigidBodyDef def;
	rigidBodyDef_init(&def);

	def.body_type   = BODY_KINEMATIC;
	def.position    = character->collider.world.position;
	def.axis        = (Vector3){ 0.0f, 0.0f, 1.0f };
	def.angle       = 0.0f;
	def.allow_sleep = 0;

	RigidBody *rigid = physicsWorld_createBody(world, &def);
	if (rigid == NULL) return;

	rigid->owner = character->entity;

	CapsuleDef shape = {
		.radius      = character->collider.shape.radius,
		.half_height = character->collider.shape.half_height,
		.friction    = 0.5f,
		.restitution = 0.0f,
		.density     = 0.0f,
	};
	transform_init(&shape.tx);

	rigidBody_addCapsule(rigid, &shape);
	character->body.rigid = rigid;
}


/* Hands the frame's outcome to the world. Velocity matters as much as the
   position: it is what tells a contact how hard the character is pushing. */
void character3dPhysics_syncBody(Character3D *character)
{
	RigidBody *rigid = character->body.rigid;
	if (rigid == NULL) return;

	rigidBody_setTransformPositionYaw(rigid, character->collider.world.position,
	                                  deg_to_rad(character->body.rotation.z));
	rigidBody_setLinearVelocity(rigid, character->body.velocity);
	rigidBody_setToAwake(rigid);
}


/* Contact gathering: every touching manifold of the frame, not just one. */

typedef struct Character3DContact {
	Vector3 normal;   /* unit, from the surface toward the character */
	float   depth;    /* positive penetration */
} Character3DContact;

typedef struct Character3DCollisionState {
	bool    floor;
	bool    wall;
	bool    ceiling;
	Vector3 wall_normal;   /* deepest wall contact, toward the character */
	float   wall_depth;
} Character3DCollisionState;

/* Only mesh and count are set up before a query: count gates the triangle
   array, whose entries are written before they are read. Zeroing the whole
   struct would cost a memset of the array on every call. */
typedef struct TriangleQuery {
	const CollisionMesh *mesh;
	int32_t triangle[CHARACTER3D_MAX_TRIANGLES];
	int     count;
} TriangleQuery;

static int character3dPhysics_collectTriangle(void *cb, int32_t id)
{
	TriangleQuery *query = (TriangleQuery *)cb;
	if (query->count >= CHARACTER3D_MAX_TRIANGLES) return 0;
	query->triangle[query->count++] = (int32_t)(intptr_t)dynamicAABBTree_getUserData(&query->mesh->tree, id);
	return 1;
}

/* The manifold normal points from the capsule toward the surface and its
   penetration is dist - radius, negative when touching. The contact keeps the
   opposite convention: normal toward the character, positive depth. Every
   collision function hands over a unit normal, with a fallback for the
   degenerate case, so it is only flipped here, never renormalized. */
static int character3dPhysics_appendContact(Character3DContact *contacts, int count, const ContactManifold *m)
{
	if (count >= CHARACTER3D_MAX_CONTACTS) return count;

	float depth = -m->contacts[0].penetration;
	if (depth <= 0.0f) return count;

	contacts[count].normal = vector3_inverted(&m->normal);
	contacts[count].depth  = depth;
	return count + 1;
}

/* All touching triangles of one mesh. The tree lives in mesh-local space:
   the capsule and the velocity hint are brought into it through the inverse
   of the mesh's world transform, and each contact normal is rotated back out
   to world space before it is kept. */
static int character3dPhysics_collectMeshContacts(const Character3DCollider *collider,
                                                const CollisionMesh *mesh, const Transform *tx,
                                                const Vector3 *velocity,
                                                Character3DContact *contacts, int count)
{
	Transform local          = transform_productTransposed(tx, &collider->world);
	Vector3   local_velocity = matrix3_transformVectorTransposed(&tx->rotation, velocity);

	AABB aabb;
	capsule_computeAABB(&collider->shape, &local, &aabb);

	/* libdragon builds with -ftrivial-auto-var-init=pattern, which fills every
	   local without an initializer on entry. The large ones of this hot path
	   opt out: each is written before it is read, and the fill alone was a
	   memset of several KB per frame. */
	TriangleQuery query __attribute__((uninitialized));
	query.mesh  = mesh;
	query.count = 0;
	collisionMesh_queryAABB(mesh, &query, character3dPhysics_collectTriangle, aabb);

	for (int i = 0; i < query.count; i++) {
		Triangle triangle __attribute__((uninitialized));
		collisionMesh_getTriangle(mesh, query.triangle[i], &triangle);

		/* contact_count gates the manifold: normal and contacts[0] are written
		   by the collision function whenever it sets a contact. */
		ContactManifold m __attribute__((uninitialized));
		m.contact_count = 0;
		capsuleToTriangle(&m, &collider->shape, &local, &triangle);
		if (!m.contact_count) continue;

		/* Contact point on the triangle: the manifold stores the point on the
		   capsule surface, the triangle sits penetration further along the
		   normal. Everything here is mesh-local, velocity included. */
		Vector3 tri_point = m.contacts[0].position;
		vector3_addScaledVector(&tri_point, &m.normal, m.contacts[0].penetration);

		m.normal = collision_fixTriangleNormal(&triangle, &tri_point, &m.normal, &local_velocity);
		m.normal = matrix3_transformVector(&tx->rotation, &m.normal);

		count = character3dPhysics_appendContact(contacts, count, &m);
	}

	return count;
}

/* Walks the world's bodies instead of a copied list: their shapes are stored
   relative to their body, so each one is composed into world space here
   before it is tested. Obstacles are the static geometry plus every other
   kinematic body — the capsule another character keeps in the world. The
   solver cannot resolve two kinematic bodies against each other, so the
   controllers do it: each one walks out of the other. Dynamic bodies stay
   out, they are pushed by the solver instead. */
static int character3dPhysics_collectContacts(const Character3DCollider *collider,
                                            const PhysicsWorld *world,
                                            const RigidBody *self,
                                            const Vector3 *velocity,
                                            Character3DContact *contacts)
{
	int count = 0;

	for (const RigidBody *body = world->body_list; body; body = body->next) {
		if (body == self) continue;
		if (!(body->flags & (BODY_FLAG_STATIC | BODY_FLAG_KINEMATIC))) continue;

		for (const PhysicsShape *shape = body->shapes; shape; shape = shape->next) {
			if (shape->sensor) continue;   /* volumes (water), not obstacles */

			const Transform *tx = &shape->world;
			ContactManifold m __attribute__((uninitialized));
			m.contact_count = 0;

			switch (shape->type) {
				case SHAPE_MESH:
					count = character3dPhysics_collectMeshContacts(collider, shape->mesh, tx, velocity, contacts, count);
					continue;
				case SHAPE_BOX:
					capsuleToStaticBox(&m, &collider->shape, &collider->world, &shape->box, tx);
					break;
				case SHAPE_SPHERE:
					capsuleToStaticSphere(&m, &collider->shape, &collider->world, &shape->sphere, tx);
					break;
				case SHAPE_CAPSULE:
					capsuleToStaticCapsule(&m, &collider->shape, &collider->world, &shape->capsule, tx);
					break;
			}
			if (!m.contact_count) continue;

			count = character3dPhysics_appendContact(contacts, count, &m);
		}
	}

	return count;
}


/* Port of Character3DBody3D::_set_collision_direction: every contact of the
   pass is classified by its angle against the up axis — floor, ceiling or
   wall — and the frame state accumulates them. Floor detection can never be
   masked by a wall contact. */
static void character3dPhysics_classifyContacts(const Character3DContact *contacts, int count, Character3DCollisionState *state)
{
	bool was_wall   = state->wall;
	bool pass_floor = false;
	bool pass_wall  = false;

	int     wall_collision_count = 0;
	Vector3 combined_wall_normal = { 0.0f, 0.0f, 0.0f };
	Vector3 tmp_wall_col         = { 0.0f, 0.0f, 0.0f };

	for (int i = count - 1; i >= 0; i--) {
		const Character3DContact *c = &contacts[i];

		/* dot(normal, up) == normal.z; angle <= limit is cosine >= its cosine */
		if (c->normal.z >= CHARACTER3D_FLOOR_MAX_SLOPE_COS) {
			pass_floor   = true;
			state->floor = true;
			continue;
		}

		if (-c->normal.z >= CHARACTER3D_FLOOR_MAX_SLOPE_COS) {
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

		/* Collect normal for calculating average. */
		Vector3 d = vector3_difference(&c->normal, &tmp_wall_col);
		if (vector3_dot(&d, &d) > 1.0e-6f) {
			tmp_wall_col = c->normal;
			vector3_add(&combined_wall_normal, &c->normal);
			wall_collision_count++;
		}
	}

	/* Two steep walls can add up to walkable support (a wedge): their combined
	   normal points up within the floor limit even though neither does. */
	if (pass_wall && wall_collision_count > 1 && !pass_floor) {
		float magnitude = vector3_magnitude(&combined_wall_normal);
		if (magnitude > 1.0e-6f && combined_wall_normal.z >= CHARACTER3D_FLOOR_MAX_SLOPE_COS * magnitude) {
			state->floor = true;
			state->wall  = was_wall;
		}
	}
}

/* Port of test_body_motion STEP 1 (free body if stuck): one recovery vector
   accumulated over every contact and applied whole, up to four attempts. Each
   contact's depth is re-measured against the motion accumulated so far in the
   pass, so stacked contacts on the same plane do not over-correct. */
static void character3dPhysics_recover(Character3D *character, const PhysicsWorld *world, Character3DCollisionState *state)
{
	int recover_attempts = CHARACTER3D_RECOVERY_ATTEMPTS;

	do {
		Character3DContact contacts[CHARACTER3D_MAX_CONTACTS] __attribute__((uninitialized));
		int count = character3dPhysics_collectContacts(&character->collider, world, character->body.rigid,
		                                             &character->body.velocity, contacts);
		if (!count) break;

		character3dPhysics_classifyContacts(contacts, count, state);

		Vector3 recover_motion = { 0.0f, 0.0f, 0.0f };
		for (int i = 0; i < count; i++) {
			float depth = contacts[i].depth - vector3_dot(&contacts[i].normal, &recover_motion);
			if (depth > CHARACTER3D_MIN_CONTACT_DEPTH + 1.0e-5f)
				vector3_addScaledVector(&recover_motion, &contacts[i].normal, (depth - CHARACTER3D_MIN_CONTACT_DEPTH) * CHARACTER3D_RECOVERY_FACTOR);
		}

		if (vector3_dot(&recover_motion, &recover_motion) == 0.0f) break;

		vector3_add(&character->body.position, &recover_motion);
		character3dCollider_setVertical(&character->collider, &character->body.position);
	} while (--recover_attempts);
}


/* Velocity responses, once per frame from the classified state. */

static void character3dCollision_setGroundResponse(Character3D *character)
{
	KinematicBody *body = &character->body;
	Character3DMovementData *data = &character->movement.data;

	/* Moving up (jump takeoff) — touching the ground must not cancel it. */
	if (body->velocity.z > 0.0f) return;

	data->is_grounded = 1;
	body->acceleration.z = 0.0f;
	body->velocity.z = 0.0f;

	/* The floor is back: the next ledge gets its own coyote window. */
	data->coyote_timer = 0.0f;
	if (character->movement.current == MOVEMENT_STATE_FALLING)
		character3dMovement_setMode(&character->movement, character->movement.locomotion);
}

static void character3dCollision_setCeilingResponse(Character3D *character)
{
	KinematicBody *body = &character->body;
	if (body->velocity.z > 0.0f) body->velocity.z = 0.0f;
}

/* Slide: remove the velocity component pushing into the wall, keep the rest.
   On the floor only the horizontal part of the wall normal is used — edge
   normals of the mesh carry a vertical component, and letting it through
   injects upward velocity that kills the floor detection. */
static void character3dCollision_setWallResponse(Character3D *character, const Character3DCollisionState *state, bool was_on_floor)
{
	KinematicBody *body = &character->body;

	if (was_on_floor || character->movement.data.is_grounded) {
		Vector3 n = state->wall_normal;
		n.z = 0.0f;

		float magnitude = vector3_magnitude(&n);
		if (magnitude < 1.0e-6f) return;
		vector3_scale(&n, 1.0f / magnitude);

		float t = body->velocity.x * n.x + body->velocity.y * n.y;
		if (t < 0.0f) {
			body->velocity.x -= t * n.x;
			body->velocity.y -= t * n.y;
		}
	}
	else {
		float t = vector3_dot(&body->velocity, &state->wall_normal);
		if (t < 0.0f) vector3_addScaledVector(&body->velocity, &state->wall_normal, -t);
	}
}

static void character3dCollision_respond(Character3D *character, const Character3DCollisionState *state, bool was_on_floor)
{
	if (state->floor)   character3dCollision_setGroundResponse(character);
	if (state->ceiling) character3dCollision_setCeilingResponse(character);
	if (state->wall)    character3dCollision_setWallResponse(character, state, was_on_floor);
}


/* Floor query: the capsule's bottom sphere swept down by the snap
   length. Answers whether there is walkable floor under the character,
   how deep the probe sinks into it and with which normal. Contacts whose
   normal is steeper than the walkable limit (walls) are ignored. */

typedef struct FloorProbe {
	int     found;
	float   penetration;   /* deepest floor contact, along its normal */
	Vector3 normal;        /* from the floor toward the character */
} FloorProbe;

/* closest: nearest point of the surface to the sphere center, world space. */
static void floorProbe_consider(FloorProbe *probe, const Vector3 *center, float radius, const Vector3 *closest)
{
	Vector3 d     = vector3_difference(center, closest);
	float   dist2 = vector3_dot(&d, &d);
	if (dist2 > radius * radius) return;

	float dist = sqrtf(dist2);
	Vector3 normal = (dist > 1.0e-6f)
		? vector3_scaled(&d, 1.0f / dist)
		: vector3_create(0.0f, 0.0f, 1.0f);

	/* Walkable floor only. */
	if (normal.z < CHARACTER3D_FLOOR_MAX_SLOPE_COS) return;

	float penetration = radius - dist;
	if (!probe->found || penetration > probe->penetration) {
		probe->found       = 1;
		probe->penetration = penetration;
		probe->normal      = normal;
	}
}

static void character3dPhysics_probeFloor(const PhysicsWorld *world, const Vector3 *center, float radius, FloorProbe *probe)
{
	/* found gates every other field: they are written together on a hit. */
	probe->found = 0;

	for (const RigidBody *body = world->body_list; body; body = body->next) {
	if (!(body->flags & BODY_FLAG_STATIC)) continue;

	for (const PhysicsShape *shape = body->shapes; shape; shape = shape->next) {
		if (shape->sensor) continue;   /* volumes (water), not floor */

		const Transform *tx = &shape->world;

		switch (shape->type) {
			case SHAPE_MESH: {
				/* The tree is mesh-local: the sphere goes in through the
				   inverse transform, the closest point comes back out. */
				Vector3 local_center = transform_mulVectorTransposed(tx, center);

				AABB aabb = {
					{ local_center.x - radius, local_center.y - radius, local_center.z - radius },
					{ local_center.x + radius, local_center.y + radius, local_center.z + radius },
				};

				TriangleQuery query __attribute__((uninitialized));
				query.mesh  = shape->mesh;
				query.count = 0;
				collisionMesh_queryAABB(shape->mesh, &query, character3dPhysics_collectTriangle, aabb);

				for (int t = 0; t < query.count; t++) {
					Triangle triangle __attribute__((uninitialized));
					collisionMesh_getTriangle(shape->mesh, query.triangle[t], &triangle);
					Vector3 closest_local = triangle_closestToPoint(&triangle.vertices[0], &triangle.vertices[1], &triangle.vertices[2], &local_center);
					Vector3 closest       = transform_mulVector(tx, &closest_local);
					floorProbe_consider(probe, center, radius, &closest);
				}
				break;
			}
			case SHAPE_BOX: {
				Vector3 local_center = transform_mulVectorTransposed(tx, center);

				Vector3 e = shape->box.e;
				AABB box_local = { { -e.x, -e.y, -e.z }, { e.x, e.y, e.z } };
				Vector3 closest_local = aabb_closestToPoint(&box_local, &local_center);
				Vector3 closest = transform_mulVector(tx, &closest_local);
				floorProbe_consider(probe, center, radius, &closest);
				break;
			}
			case SHAPE_SPHERE: {
				const Vector3 *pos = &tx->position;
				Vector3 d = vector3_difference(center, pos);
				Vector3 dir = vector3_normalized(&d);
				Vector3 closest = *pos;
				vector3_addScaledVector(&closest, &dir, shape->sphere.radius);
				floorProbe_consider(probe, center, radius, &closest);
				break;
			}
			case SHAPE_CAPSULE: {
				Vector3 a, b;
				capsule_getSegment(&shape->capsule, tx, &a, &b);
				Vector3 on_seg = segment_closestToPoint(&a, &b, center);
				Vector3 d = vector3_difference(center, &on_seg);
				Vector3 dir = vector3_normalized(&d);
				Vector3 closest = on_seg;
				vector3_addScaledVector(&closest, &dir, shape->capsule.radius);
				floorProbe_consider(probe, center, radius, &closest);
				break;
			}
		}
	}
	}
}

/* How far the floor is straight below the feet, so the animation can start the
   landing exactly one clip-to-contact away from it. Negative with nothing
   within reach. Sensors are skipped: the water is not a floor to land on. */
static float character3dPhysics_floorDistance(const Character3D *character, const PhysicsWorld *world)
{
	Vector3 down = { 0.0f, 0.0f, -1.0f };

	RaycastData ray;
	raycast_set(&ray, &character->body.position, &down, CHARACTER3D_FALL_PROBE_LENGTH);

	float distance = -1.0f;

	for (const RigidBody *body = world->body_list; body; body = body->next) {
		if (!(body->flags & BODY_FLAG_STATIC)) continue;

		for (const PhysicsShape *shape = body->shapes; shape; shape = shape->next) {
			if (shape->sensor) continue;
			if (!physicsShape_raycast(shape, &ray)) continue;

			/* Closest wins: the ray is shortened so the rest is behind it. */
			distance = ray.toi;
			ray.t    = ray.toi;
		}
	}

	return distance;
}

static void character3dPhysics_findFloor(const Character3D *character, const PhysicsWorld *world, FloorProbe *probe)
{
	float radius = character->collider.shape.radius;

	/* Bottom-sphere center, swept down by the snap length. */
	Vector3 center = character->body.position;
	center.z += radius - CHARACTER3D_FLOOR_SNAP_LENGTH;

	character3dPhysics_probeFloor(world, &center, radius, probe);
}

/* Godot's _snap_on_floor conditions: only when the character was on the
   floor, is not on it now, and is not moving up. Vertical-only correction,
   fed by the bottom-sphere floor probe. */
static void character3dPhysics_snapToFloor(Character3D *character, const FloorProbe *floor,
                                         bool was_on_floor, bool velocity_facing_up)
{
	if (character->movement.data.is_grounded || !was_on_floor || velocity_facing_up) return;
	if (!floor->found) return;

	float drop = CHARACTER3D_FLOOR_SNAP_LENGTH - floor->penetration / floor->normal.z;
	if (drop > 0.0f) character->body.position.z -= drop;

	character3dCollider_setVertical(&character->collider, &character->body.position);
	character3dCollision_setGroundResponse(character);
}

/* Ladder probe: the climbable volume the feet are inside, resolved into the
   frame the climb needs — where to hold on, which way to face, where the
   bottom is.

   The volume's local Y is the face normal and its local X runs along the
   rungs, so the hold is the body brought onto the centre plane (local X at
   zero) and pushed back out to the distance the climb clip grips at. Boxes
   only — the frame is the point of the volume, and a sphere has no face to
   climb. */
static void character3dPhysics_probeLadder(Character3D *character, const PhysicsWorld *world)
{
	Character3DMovementData *data = &character->movement.data;

	data->on_ladder = false;

	const Vector3 *feet = &character->body.position;

	for (const RigidBody *body = world->body_list; body; body = body->next) {
		if (!(body->flags & BODY_FLAG_STATIC)) continue;

		for (const PhysicsShape *shape = body->shapes; shape; shape = shape->next) {
			if (shape->sensor != SENSOR_CLIMBABLE) continue;
			if (shape->type   != SHAPE_BOX)        continue;
			if (!physicsShape_testPoint(shape, feet)) continue;

			const Transform *tx    = &shape->world;
			Vector3          local = transform_mulVectorTransposed(tx, feet);

			/* Only the -Y face is climbable; the back of a ladder is its
			   back. Turn the placement 180 to serve the other side. */
			if (local.y > 0.0f) continue;

			Vector3 hold   = { 0.0f, -CHARACTER3D_LADDER_STAND_DISTANCE, local.z };
			Vector3 anchor = transform_mulVector(tx, &hold);

			/* ex/ey/ez are the local axes in world space, so ey is the face
			   normal. The heading is built the way the movement reads one off
			   a velocity, so the conventions cannot drift apart. */
			Vector3 into = tx->rotation.ey;

			/* The ceiling, for the entry margin to measure down from. Only
			   yaw ever turns a ladder, so the box's local up is the world's
			   and its half-height is the whole distance. */
			Vector3 ceiling = { 0.0f, 0.0f, shape->box.e.z };

			data->on_ladder       = true;
			data->ladder_anchor_x = anchor.x;
			data->ladder_anchor_y = anchor.y;
			data->ladder_yaw      = rad_to_deg(atan2f(-into.x, -into.y));
			data->ladder_top      = transform_mulVector(tx, &ceiling).z;
			return;
		}
	}
}

/* Water probe: fraction of the capsule under the surface, from the world's
   buoyancy volumes. The character is kinematic, so it never shows up in a
   sensor's contact list — it asks the volumes directly. Read by the movement
   code on the next frame, which is where the fake buoyancy lives. */
static void character3dPhysics_probeWater(Character3D *character, const PhysicsWorld *world)
{
	Character3DMovementData *data = &character->movement.data;

	data->in_water = false;
	data->submerged_fraction = 0.0f;

	Vector3 feet = character->body.position;

	for (int32_t i = 0; i < world->buoyancy_count; i++) {
		const BuoyancyVolume *volume = world->buoyancy[i];

		for (const PhysicsShape *shape = volume->body->shapes; shape; shape = shape->next) {
			if (!shape->sensor) continue;
			if (!physicsShape_testPoint(shape, &feet)) continue;

			float surface = volume->surface_height(volume->surface, feet.x, feet.y);
			float height  = 2.0f * (character->collider.shape.half_height + character->collider.shape.radius);

			float fraction = (surface - feet.z) / height;
			if (fraction <= 0.0f) continue;

			data->in_water = true;
			data->submerged_fraction = (fraction > 1.0f) ? 1.0f : fraction;
			return;
		}
	}
}

void character3dPhysics_collide(Character3D *character, const PhysicsWorld *world)
{
	Character3DMovementData *data = &character->movement.data;

	bool was_on_floor       = data->is_grounded;
	bool velocity_facing_up = character->body.velocity.z > 0.0f;
	data->is_grounded = 0;

	character3dCollider_setVertical(&character->collider, &character->body.position);

	character3dPhysics_probeWater(character, world);
	character3dPhysics_probeLadder(character, world);

	/* Climbing owns the body: the hands grip closer than the capsule radius,
	   so a recovery pass would shove it off the rungs it is holding, and a
	   floor snap would stand it on the first one it passes. The climb state
	   is bounded by its own volume instead.

	   The one thing still worth measuring is how far the ground is, which is
	   what tells a descent it has arrived. */
	if (character->movement.current == MOVEMENT_STATE_CLIMBING) {
		data->floor_distance = character3dPhysics_floorDistance(character, world);
		return;
	}

	Character3DCollisionState state = { .wall_depth = -1.0f };
	character3dPhysics_recover(character, world, &state);
	character3dCollision_respond(character, &state, was_on_floor);

	FloorProbe floor;
	character3dPhysics_findFloor(character, world, &floor);

	character3dPhysics_snapToFloor(character, &floor, was_on_floor, velocity_facing_up);

	/* Nothing walkable under the probe and no contact resolved as floor.
	   A contact already classified as floor outranks the probe: pressed against
	   a wall the probe can miss the floor it is standing on, and falling on that
	   alone puts the character back on the ground the next frame, one frame at a
	   time, forever.

	   Only locomotion falls: a roll runs to the end of its own clip, ground
	   under it or not. A crouch under way holds it too — the ledge ran out
	   mid-charge and the jump is not lost for it, so the body coasts in
	   locomotion until the crouch launches it. */
	if (!floor.found && !data->is_grounded
	 && character3dMovement_isLocomotion(character->movement.current)
	 && !character3dMovement_isChargingJump(character))
		character3dMovement_setMode(&character->movement, MOVEMENT_STATE_FALLING);

	/* Only worth measuring off the ground, and only on the way down: it feeds
	   the landing, and a rising body has nothing to time yet. */
	data->floor_distance = (!data->is_grounded && character->body.velocity.z < 0.0f)
		? character3dPhysics_floorDistance(character, world)
		: -1.0f;
}
