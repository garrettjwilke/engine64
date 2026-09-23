#include "sound/e64_prop_sound.h"
#include "entity/e64_entity3d.h"
#include "shaders/e64_water.h"
#include "physics/collision/e64_contact.h"
#include "physics/e64_rigid_body.h"
#include "physics/e64_physics_world.h"
#include "math/e64_math.h"

namespace e64 {

namespace propSound {

/* The measure of a hit is the impulse the solver spent stopping it, over the
   body's mass: the speed it killed, in m/s — one scale for every weight.
   Below the floor it is resting jitter and stays silent. */
static constexpr float COLLISION_SPEED_MIN = 0.4f;
static constexpr float COLLISION_SPEED_MAX = 3.0f;
static constexpr float COLLISION_VOLUME_MIN = 0.1f;
static constexpr float COLLISION_VOLUME_MAX = 1.0f;

/* The plunge is the vertical speed on the frame the body meets the water:
   rolling in barely whispers, a fall from the deck slaps. No cutoff floor —
   entering slowly still wets. */
static constexpr float PLUNGE_SPEED_MIN = 0.5f;
static constexpr float PLUNGE_SPEED_MAX = 6.0f;
static constexpr float PLUNGE_VOLUME_MIN = 0.1f;
static constexpr float PLUNGE_VOLUME_MAX = 0.4f;


static void collision(const RigidBody *body, float impulse)
{
	float speed = impulse * body->inv_mass;
	if (speed < COLLISION_SPEED_MIN) return;

	float t = (speed - COLLISION_SPEED_MIN)
	        / (COLLISION_SPEED_MAX - COLLISION_SPEED_MIN);
	if (t > 1.0f) t = 1.0f;

	float volume = COLLISION_VOLUME_MIN
	             + t * (COLLISION_VOLUME_MAX - COLLISION_VOLUME_MIN);

	entity3d::playSound((const Entity3D *)body->owner, TRIGGER_COLLISION, NULL, volume);
}

/* The sound is the surface's, the place and the volume are the body's that
   entered. */
static void waterEntry(const RigidBody *body, const Water *water)
{
	float plunge_speed = -body->linear_velocity.z;

	float t = (plunge_speed - PLUNGE_SPEED_MIN)
	        / (PLUNGE_SPEED_MAX - PLUNGE_SPEED_MIN);
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	float volume = PLUNGE_VOLUME_MIN
	             + t * (PLUNGE_VOLUME_MAX - PLUNGE_VOLUME_MIN);

	entity3d::playSound((const Entity3D *)water->volume.body->owner, TRIGGER_WATER_ENTRY, &body->tx.position, volume);
}

void update(physics::World *world)
{
	/* The constraint's first colliding frame, tracked by the engine itself,
	   so no state lives here. A solid contact is a hit; a sensor one is a
	   body meeting a water volume. */
	for (const Contact::Constraint *c = world->contact_manager.contact_list; c; c = c->next) {
		if (!(c->flags & CONSTRAINT_COLLIDING)) continue;
		if ( c->flags & CONSTRAINT_WAS_COLLIDING) continue;

		if (c->manifold.sensor) {
			const Water *water_a = water::getBoundSurface(c->body_a);
			const Water *water_b = water::getBoundSurface(c->body_b);

			if (water_a) waterEntry(c->body_b, water_a);
			else if (water_b) waterEntry(c->body_a, water_b);
			continue;
		}

		float impulse = 0.0f;
		for (int32_t i = 0; i < c->manifold.contact_count; i++)
			impulse += c->manifold.contacts[i].normal_impulse;

		bool a_hits = (c->body_a->flags & RigidBody::BODY_FLAG_DYNAMIC) && c->body_a->owner;
		bool b_hits = (c->body_b->flags & RigidBody::BODY_FLAG_DYNAMIC) && c->body_b->owner;

		/* When both sides would sound, the heavier body owns the hit: one
		   thud per contact, not two stacked. */
		if (a_hits && b_hits) {
			if (c->body_a->mass >= c->body_b->mass) b_hits = false;
			else a_hits = false;
		}

		if (a_hits) collision(c->body_a, impulse);
		if (b_hits) collision(c->body_b, impulse);
	}
}

}

}
