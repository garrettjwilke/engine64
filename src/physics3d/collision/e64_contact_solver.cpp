/*
	Ported from qu3e q3ContactSolver.cpp — altered source, not the original software.

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
	Sequential impulse solver (PGS).
*/
#include "physics3d/collision/e64_contact_solver.h"
#include "physics3d/e64_physics_island.h"
#include "physics3d/e64_rigid_body.h"
#include "physics3d/e64_physics.h"

namespace e64 {
namespace contact {
namespace solver {

static inline float invert_or_zero(float x)
{
	return x != 0.0f ? 1.0f / x : 0.0f;
}


void initialize(Contact::Solver *s, physics::Island *island)
{
	s->island = island;
	s->contact_count = island->contact_count;
	s->contacts = island->contact_states;
	s->enable_friction = island->enable_friction;
}


void shutdown(Contact::Solver *s)
{
	for (int32_t i = 0; i < s->contact_count; ++i) {
		Contact::Solver::ConstraintState *c = s->contacts + i;
		Contact::Constraint *cc = s->island->contacts[i];

		for (int32_t j = 0; j < c->contact_count; ++j) {
			Contact::Point *oc = cc->manifold.contacts + j;
			Contact::Solver::State *cs = c->contacts + j;
			oc->normal_impulse = cs->normal_impulse;
			oc->tangent_impulse[0] = cs->tangent_impulse[0];
			oc->tangent_impulse[1] = cs->tangent_impulse[1];
		}
	}
}


void preSolve(Contact::Solver *s, float dt)
{
	physics::Island::VelocityState *velocities = s->island->velocities;

	for (int32_t i = 0; i < s->contact_count; ++i) {
		Contact::Solver::ConstraintState *cs = s->contacts + i;

		Vector3 vA = velocities[cs->index_a].v;
		Vector3 wA = velocities[cs->index_a].w;
		Vector3 vB = velocities[cs->index_b].v;
		Vector3 wB = velocities[cs->index_b].w;

		/* A static or kinematic side has inv_mass 0 and a zero inverse
		   inertia: every term it contributes is 0 and no impulse can move it.
		   Most contacts in a game have one such side, so that half is
		   skipped rather than multiplied out to nothing. */
		int move_a = cs->mA != 0.0f;
		int move_b = cs->mB != 0.0f;

		for (int32_t j = 0; j < cs->contact_count; ++j) {
			Contact::Solver::State *c = cs->contacts + j;

			float nm = cs->mA + cs->mB;
			float tm[2] = { nm, nm };

			if (move_a) {
				Vector3 raCn = vector3::cross(&c->ra, &cs->normal);
				Vector3 iA_raCn = matrix3::transformVector(&cs->iA, &raCn);
				nm += vector3::dot(&raCn, &iA_raCn);
			}
			if (move_b) {
				Vector3 rbCn = vector3::cross(&c->rb, &cs->normal);
				Vector3 iB_rbCn = matrix3::transformVector(&cs->iB, &rbCn);
				nm += vector3::dot(&rbCn, &iB_rbCn);
			}
			c->normal_mass = invert_or_zero(nm);

			for (int32_t k = 0; k < 2; ++k) {
				if (move_a) {
					Vector3 raCt = vector3::cross(&cs->tangent_vectors[k], &c->ra);
					Vector3 iA_raCt = matrix3::transformVector(&cs->iA, &raCt);
					tm[k] += vector3::dot(&raCt, &iA_raCt);
				}
				if (move_b) {
					Vector3 rbCt = vector3::cross(&cs->tangent_vectors[k], &c->rb);
					Vector3 iB_rbCt = matrix3::transformVector(&cs->iB, &rbCt);
					tm[k] += vector3::dot(&rbCt, &iB_rbCt);
				}
				c->tangent_mass[k] = invert_or_zero(tm[k]);
			}

			float pen_bias = c->penetration + physics::PENETRATION_SLOP;
			if (pen_bias > 0.0f) pen_bias = 0.0f;
			c->bias = -physics::BAUMGARTE * (1.0f / dt) * pen_bias;

			Vector3 P = vector3::scaled(&cs->normal, c->normal_impulse);

			if (s->enable_friction) {
				Vector3 t0 = vector3::scaled(&cs->tangent_vectors[0], c->tangent_impulse[0]);
				Vector3 t1 = vector3::scaled(&cs->tangent_vectors[1], c->tangent_impulse[1]);
				P = vector3::sum(&P, &t0);
				P = vector3::sum(&P, &t1);
			}

			if (move_a) {
				Vector3 P_a = vector3::scaled(&P, cs->mA);
				vA = vector3::difference(&vA, &P_a);
				Vector3 cross_ra_P = vector3::cross(&c->ra, &P);
				Vector3 iA_cross_a = matrix3::transformVector(&cs->iA, &cross_ra_P);
				wA = vector3::difference(&wA, &iA_cross_a);
			}
			if (move_b) {
				Vector3 P_b = vector3::scaled(&P, cs->mB);
				vB = vector3::sum(&vB, &P_b);
				Vector3 cross_rb_P = vector3::cross(&c->rb, &P);
				Vector3 iB_cross_b = matrix3::transformVector(&cs->iB, &cross_rb_P);
				wB = vector3::sum(&wB, &iB_cross_b);
			}

			/* rel = (vB + wB × rb) - vA - wA × ra */
			Vector3 wb_rb = vector3::cross(&wB, &c->rb);
			Vector3 vb_rel = vector3::sum(&vB, &wb_rb);
			Vector3 wa_ra = vector3::cross(&wA, &c->ra);
			Vector3 va_rel = vector3::sum(&vA, &wa_ra);
			Vector3 rel = vector3::difference(&vb_rel, &va_rel);
			float dv = vector3::dot(&rel, &cs->normal);
			if (dv < -1.0f) c->bias += -(cs->restitution) * dv;
		}

		velocities[cs->index_a].v = vA;
		velocities[cs->index_a].w = wA;
		velocities[cs->index_b].v = vB;
		velocities[cs->index_b].w = wB;
	}
}


void solve(Contact::Solver *s)
{
	physics::Island::VelocityState *velocities = s->island->velocities;

	for (int32_t i = 0; i < s->contact_count; ++i) {
		Contact::Solver::ConstraintState *cs = s->contacts + i;

		Vector3 vA = velocities[cs->index_a].v;
		Vector3 wA = velocities[cs->index_a].w;
		Vector3 vB = velocities[cs->index_b].v;
		Vector3 wB = velocities[cs->index_b].w;

		/* Same skip as preSolve: an impulse on a massless side is a no-op. */
		int move_a = cs->mA != 0.0f;
		int move_b = cs->mB != 0.0f;

		for (int32_t j = 0; j < cs->contact_count; ++j) {
			Contact::Solver::State *c = cs->contacts + j;

			/* dv = (vB + wB × rb) - vA - wA × ra */
			Vector3 wb_rb = vector3::cross(&wB, &c->rb);
			Vector3 vb_rel = vector3::sum(&vB, &wb_rb);
			Vector3 wa_ra = vector3::cross(&wA, &c->ra);
			Vector3 va_rel = vector3::sum(&vA, &wa_ra);
			Vector3 dv = vector3::difference(&vb_rel, &va_rel);

			if (s->enable_friction) {
				for (int32_t k = 0; k < 2; ++k) {
					float lambda = -vector3::dot(&dv, &cs->tangent_vectors[k]) * c->tangent_mass[k];
					float max_lambda = cs->friction * c->normal_impulse;
					float old_pt = c->tangent_impulse[k];
					c->tangent_impulse[k] = clampf(old_pt + lambda, -max_lambda, max_lambda);
					lambda = c->tangent_impulse[k] - old_pt;

					Vector3 impulse = vector3::scaled(&cs->tangent_vectors[k], lambda);

					if (move_a) {
						Vector3 imp_a = vector3::scaled(&impulse, cs->mA);
						vA = vector3::difference(&vA, &imp_a);
						Vector3 cross_ra = vector3::cross(&c->ra, &impulse);
						Vector3 iA_cra = matrix3::transformVector(&cs->iA, &cross_ra);
						wA = vector3::difference(&wA, &iA_cra);
					}
					if (move_b) {
						Vector3 imp_b = vector3::scaled(&impulse, cs->mB);
						vB = vector3::sum(&vB, &imp_b);
						Vector3 cross_rb = vector3::cross(&c->rb, &impulse);
						Vector3 iB_crb = matrix3::transformVector(&cs->iB, &cross_rb);
						wB = vector3::sum(&wB, &iB_crb);
					}
				}
			}

			/* Recompute dv after friction. */
			wb_rb = vector3::cross(&wB, &c->rb);
			vb_rel = vector3::sum(&vB, &wb_rb);
			wa_ra = vector3::cross(&wA, &c->ra);
			va_rel = vector3::sum(&vA, &wa_ra);
			dv = vector3::difference(&vb_rel, &va_rel);

			float vn = vector3::dot(&dv, &cs->normal);
			float lambda = c->normal_mass * (-vn + c->bias);
			float temp_pn = c->normal_impulse;
			c->normal_impulse = (temp_pn + lambda > 0.0f) ? (temp_pn + lambda) : 0.0f;
			lambda = c->normal_impulse - temp_pn;

			Vector3 impulse = vector3::scaled(&cs->normal, lambda);

			if (move_a) {
				Vector3 imp_a = vector3::scaled(&impulse, cs->mA);
				vA = vector3::difference(&vA, &imp_a);
				Vector3 cross_ra = vector3::cross(&c->ra, &impulse);
				Vector3 iA_cra = matrix3::transformVector(&cs->iA, &cross_ra);
				wA = vector3::difference(&wA, &iA_cra);
			}
			if (move_b) {
				Vector3 imp_b = vector3::scaled(&impulse, cs->mB);
				vB = vector3::sum(&vB, &imp_b);
				Vector3 cross_rb = vector3::cross(&c->rb, &impulse);
				Vector3 iB_crb = matrix3::transformVector(&cs->iB, &cross_rb);
				wB = vector3::sum(&wB, &iB_crb);
			}
		}

		velocities[cs->index_a].v = vA;
		velocities[cs->index_a].w = wA;
		velocities[cs->index_b].v = vB;
		velocities[cs->index_b].w = wB;
	}
}

}
}
}
