/*
	Ported from qu3e q3Island.cpp — altered source, not the original software.

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
	Integrate velocities, run the solver, integrate positions, manage sleep.
*/
#include <assert.h>
#include <float.h>

#include "physics/e64_physics_island.h"
#include "physics/e64_physics.h"
#include "physics/e64_rigid_body.h"
#include "physics/collision/e64_contact.h"
#include "physics/collision/e64_contact_solver.h"

namespace e64 {
namespace physics {
namespace island {

void addBody(Island *island, RigidBody *body)
{
	assert(island->body_count < island->body_capacity);
	body->island_index = island->body_count;
	island->bodies[island->body_count++] = body;
}


void addContact(Island *island, Contact::Constraint *contact)
{
	assert(island->contact_count < island->contact_capacity);
	island->contacts[island->contact_count++] = contact;
}


void initialize(Island *island)
{
	for (int32_t i = 0; i < island->contact_count; ++i) {
		Contact::Constraint *cc = island->contacts[i];
		Contact::Solver::ConstraintState *c = island->contact_states + i;

		c->center_a = cc->body_a->world_center;
		c->center_b = cc->body_b->world_center;
		c->iA = cc->body_a->inv_inertia_world;
		c->iB = cc->body_b->inv_inertia_world;
		c->mA = cc->body_a->inv_mass;
		c->mB = cc->body_b->inv_mass;
		c->restitution = cc->restitution;
		c->friction = cc->friction;
		c->index_a = cc->body_a->island_index;
		c->index_b = cc->body_b->island_index;
		c->normal = cc->manifold.normal;
		c->tangent_vectors[0] = cc->manifold.tangent_vectors[0];
		c->tangent_vectors[1] = cc->manifold.tangent_vectors[1];
		c->contact_count = cc->manifold.contact_count;

		for (int32_t j = 0; j < c->contact_count; ++j) {
			Contact::Solver::State *s = c->contacts + j;
			Contact::Point *cp = cc->manifold.contacts + j;
			s->ra = vector3::difference(&cp->position, &c->center_a);
			s->rb = vector3::difference(&cp->position, &c->center_b);
			s->penetration = cp->penetration;
			s->normal_impulse = cp->normal_impulse;
			s->tangent_impulse[0] = cp->tangent_impulse[0];
			s->tangent_impulse[1] = cp->tangent_impulse[1];
		}
	}
}


void solve(Island *island)
{
	/* Integrate forces into velocity. */
	for (int32_t i = 0; i < island->body_count; ++i) {
		RigidBody *body = island->bodies[i];
		Island::VelocityState *v = island->velocities + i;

		if (body->flags & RigidBody::BODY_FLAG_DYNAMIC) {
			Vector3 gravity_force = vector3::scaled(&island->gravity, body->gravity_scale);
			rigidBody::applyLinearForce(body, gravity_force);

			/* iW = R · iModel · Rᵀ. */
			Matrix3 r = body->tx.rotation;
			Matrix3 rt = matrix3::transposed(&r);
			Matrix3 tmp = matrix3::product(&r, &body->inv_inertia_model);
			body->inv_inertia_world = matrix3::product(&tmp, &rt);

			Vector3 f_dt = vector3::scaled(&body->force, body->inv_mass * island->dt);
			body->linear_velocity = vector3::sum(&body->linear_velocity, &f_dt);

			Vector3 iw_torque = matrix3::transformVector(&body->inv_inertia_world, &body->torque);
			Vector3 t_dt = vector3::scaled(&iw_torque, island->dt);
			body->angular_velocity = vector3::sum(&body->angular_velocity, &t_dt);

			/* Pade damping. */
			float lin_d = 1.0f / (1.0f + island->dt * body->linear_damping);
			float ang_d = 1.0f / (1.0f + island->dt * body->angular_damping);
			body->linear_velocity = vector3::scaled(&body->linear_velocity, lin_d);
			body->angular_velocity = vector3::scaled(&body->angular_velocity, ang_d);
		}

		v->v = body->linear_velocity;
		v->w = body->angular_velocity;
	}

	/* Contact solver. */
	Contact::Solver solver;
	contact::solver::initialize(&solver, island);
	contact::solver::preSolve(&solver, island->dt);

	for (int32_t i = 0; i < island->iterations; ++i) {
		contact::solver::solve(&solver);
	}

	contact::solver::shutdown(&solver);

	/* Integrate positions. */
	for (int32_t i = 0; i < island->body_count; ++i) {
		RigidBody *body = island->bodies[i];
		Island::VelocityState *v = island->velocities + i;

		if (body->flags & RigidBody::BODY_FLAG_STATIC) continue;

		/* A kinematic body is placed by whoever owns it, not by the solver: it
		   carries a velocity so contacts know how hard it pushes, but
		   integrating that velocity here would advance it a second time. */
		if (body->flags & RigidBody::BODY_FLAG_KINEMATIC) continue;

		body->linear_velocity = v->v;
		body->angular_velocity = v->w;

		Vector3 pos_delta = vector3::scaled(&body->linear_velocity, island->dt);
		body->world_center = vector3::sum(&body->world_center, &pos_delta);

		/* integrate normalizes on its own way out. */
		quaternion::integrate(&body->q, &body->angular_velocity, island->dt);
		body->tx.rotation = quaternion::toMatrix3(&body->q);
	}

	/* Sleep management. */
	if (island->allow_sleep) {
		float min_sleep = FLT_MAX;
		for (int32_t i = 0; i < island->body_count; ++i) {
			RigidBody *body = island->bodies[i];
			if (body->flags & RigidBody::BODY_FLAG_STATIC) continue;

			/* Kinematics move on someone else's schedule, so their stillness
			   says nothing about whether the island can rest. */
			if (body->flags & RigidBody::BODY_FLAG_KINEMATIC) continue;

			float sqr_lin = vector3::dot(&body->linear_velocity, &body->linear_velocity);
			float sqr_ang = vector3::dot(&body->angular_velocity, &body->angular_velocity);

			if (sqr_lin > SLEEP_LINEAR || sqr_ang > SLEEP_ANGULAR) {
				min_sleep = 0.0f;
				body->sleep_time = 0.0f;
			} else {
				body->sleep_time += island->dt;
				if (body->sleep_time < min_sleep) min_sleep = body->sleep_time;
			}
		}

		if (min_sleep > SLEEP_TIME) {
			for (int32_t i = 0; i < island->body_count; ++i) {
				/* Asleep it would stop seeding islands, and the bodies resting
				   against it would never learn that it started moving again. */
				if (island->bodies[i]->flags & RigidBody::BODY_FLAG_KINEMATIC) continue;

				rigidBody::setToSleep(island->bodies[i]);
			}
		}
	}
}

}
}
}
