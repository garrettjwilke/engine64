/*
	Ported from qu3e q3Scene.cpp — altered source, not the original software.

	Copyright (c) 2014 Randy Gaul http://www.randygaul.net

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
	     claim that you wrote the original software. If you use this software
	     in a product, an acknowledgment in the product documentation would be
	     appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
	     be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
*/

/*
	The module's own step: the frame's clock over the world the rest of
	physics assembles.
*/
#include <assert.h>
#include <stddef.h>

#include "physics/e64_physics.h"
#include "physics/e64_physics_world.h"
#include "physics/e64_physics_island.h"
#include "physics/e64_rigid_body.h"
#include "physics/e64_cloth.h"
#include "physics/e64_buoyancy.h"
#include "physics/collision/e64_broad_phase.h"
#include "physics/collision/e64_contact.h"
#include "physics/collision/e64_contact_solver.h"
#include "physics/collision/e64_contact_manager.h"

namespace e64 {
namespace physics {

/* One step per rendered frame, on the frame's own clock. The clamp is what
   keeps a hiccup from becoming one huge step: past it the simulation runs in
   slow motion for that frame instead of blowing up the solver. */
void update(World *s, float delta)
{
	if (delta <= 0.0f) return; /* first frame; 1/dt lives in the solver bias */

	s->dt = (delta > MAX_TIMESTEP) ? MAX_TIMESTEP : delta;
	step(s);

	/* Cloths keep the fixed step: a Verlet cloth's look is tuned to its step
	   size, so it steps on its own clock instead of the frame's dt. Its cost
	   per second is constant, so it cannot feed back into the frame time. */
	s->accumulator += delta;

	float ceiling = TIMESTEP * CLOTH_MAX_SUBSTEPS;
	if (s->accumulator > ceiling) s->accumulator = ceiling;

	/* A cloth nobody is looking at holds its pose: skipping its step leaves
	   both Verlet slots alone, so it resumes with the velocity it had. The
	   accumulator is the world's and drains either way, so coming back into
	   view never owes a burst of substeps. */
	while (s->accumulator >= TIMESTEP) {
		for (Cloth *cloth = s->cloth_list; cloth; cloth = cloth->next) {
			if (cloth::isCulled(cloth)) continue;
			cloth->gravity = s->gravity;
			cloth->wind = s->wind;
			cloth::step(cloth, TIMESTEP);
		}
		s->accumulator -= TIMESTEP;
	}

	/* Shown state: previous and current step blended by the leftover
	   fraction, same scheme as the dynamic bones. */
	float t = s->accumulator / TIMESTEP;
	for (Cloth *cloth = s->cloth_list; cloth; cloth = cloth->next)
		if (!cloth::isCulled(cloth)) cloth::blendRenderState(cloth, t);
}


void step(World *s)
{
	if (s->new_shape) {
		collision::broadPhase::updatePairs(&s->contact_manager.broadphase);
		s->new_shape = 0;
	}

	contact::manager::testCollisions(&s->contact_manager);

	/* After the narrowphase, so the sensors' COLLIDING flags are fresh;
	   before the islands, so the forces integrate in this same step. */
	for (int32_t i = 0; i < s->buoyancy_count; i++)
		buoyancy::apply(s, s->buoyancy[i]);

	for (RigidBody *body = s->body_list; body; body = body->next) {
		body->flags &= ~RigidBody::BODY_FLAG_ISLAND;
	}

	/* Reserve stack for island buffers. */
	memory::stack::reserve(&s->stack,
		(uint32_t)(sizeof(RigidBody *) * s->body_count
		         + sizeof(Island::VelocityState) * s->body_count
		         + sizeof(Contact::Constraint *) * s->contact_manager.contact_count
		         + sizeof(Contact::Solver::ConstraintState) * s->contact_manager.contact_count
		         + sizeof(RigidBody *) * s->body_count)
	);

	Island island;
	island.body_capacity = s->body_count;
	island.contact_capacity = s->contact_manager.contact_count;
	island.bodies = (RigidBody **) memory::stack::allocate(&s->stack, (int32_t)(sizeof(RigidBody *) * s->body_count));
	island.velocities = (Island::VelocityState *) memory::stack::allocate(&s->stack, (int32_t)(sizeof(Island::VelocityState) * s->body_count));
	island.contacts = (Contact::Constraint **) memory::stack::allocate(&s->stack, (int32_t)(sizeof(Contact::Constraint *) * island.contact_capacity));
	island.contact_states = (Contact::Solver::ConstraintState *)memory::stack::allocate(&s->stack, (int32_t)(sizeof(Contact::Solver::ConstraintState) * island.contact_capacity));
	island.allow_sleep = s->allow_sleep;
	island.enable_friction = s->enable_friction;
	island.body_count = 0;
	island.contact_count = 0;
	island.dt = s->dt;
	island.gravity = s->gravity;
	island.iterations = s->iterations;

	int32_t stack_size = s->body_count;
	RigidBody **stack = (RigidBody **)memory::stack::allocate(&s->stack, (int32_t)(sizeof(RigidBody *) * stack_size));

	for (RigidBody *seed = s->body_list; seed; seed = seed->next) {
		if (seed->flags & RigidBody::BODY_FLAG_ISLAND) continue;
		if (!(seed->flags & RigidBody::BODY_FLAG_AWAKE)) continue;
		if (seed->flags & RigidBody::BODY_FLAG_STATIC) continue;

		int32_t stack_count = 0;
		stack[stack_count++] = seed;
		island.body_count = 0;
		island.contact_count = 0;

		seed->flags |= RigidBody::BODY_FLAG_ISLAND;

		while (stack_count > 0) {
			RigidBody *body = stack[--stack_count];
			island::addBody(&island, body);

			rigidBody::setToAwake(body);

			if (body->flags & RigidBody::BODY_FLAG_STATIC) continue;

			Contact::Edge *contacts = body->contact_list;
			for (Contact::Edge *edge = contacts; edge; edge = edge->next) {
				Contact::Constraint *contact = edge->constraint;

				if (contact->flags & CONSTRAINT_ISLAND) continue;
				if (!(contact->flags & CONSTRAINT_COLLIDING)) continue;
				if (contact->A->sensor || contact->B->sensor) continue;

				contact->flags |= CONSTRAINT_ISLAND;
				island::addContact(&island, contact);

				RigidBody *other = edge->other;
				if (other->flags & RigidBody::BODY_FLAG_ISLAND) continue;

				assert(stack_count < stack_size);
				stack[stack_count++] = other;
				other->flags |= RigidBody::BODY_FLAG_ISLAND;
			}
		}

		assert(island.body_count != 0);

		island::initialize(&island);
		island::solve(&island);

		/* Reset static island flag so statics can participate in multiple islands. */
		for (int32_t i = 0; i < island.body_count; ++i) {
			RigidBody *body = island.bodies[i];
			if (body->flags & RigidBody::BODY_FLAG_STATIC) body->flags &= ~RigidBody::BODY_FLAG_ISLAND;
		}
	}

	memory::stack::free(&s->stack, stack);
	memory::stack::free(&s->stack, island.contact_states);
	memory::stack::free(&s->stack, island.contacts);
	memory::stack::free(&s->stack, island.velocities);
	memory::stack::free(&s->stack, island.bodies);

	/* Sync broadphase AABBs. A sleeping body did not move, so its proxy is
	   already where it belongs: re-inserting it into the tree every substep is
	   what makes a settled scene keep costing, and settled is the normal case. */
	for (RigidBody *body = s->body_list; body; body = body->next) {
		if (body->flags & RigidBody::BODY_FLAG_STATIC) continue;
		if (!(body->flags & RigidBody::BODY_FLAG_AWAKE)) continue;

		rigidBody::synchronizeProxies(body);
	}

	contact::manager::findNewContacts(&s->contact_manager);

	for (RigidBody *body = s->body_list; body; body = body->next) {
		body->force = vector3::zero();
		body->torque = vector3::zero();
	}

}

}
}
