/*
	Ported from qu3e q3ContactManager.cpp — altered source, not the original software.

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
	Maintains the list of active Contact::Constraint, drives broadphase pair
	generation, and refreshes contacts each step.
*/
#include <stddef.h>

#include "physics/collision/e64_contact_manager.h"
#include "physics/e64_rigid_body.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/geometry/e64_half_space.h" /* vector3::computeBasis */

namespace e64 {
namespace contact {
namespace manager {

void init(Contact::Manager *m, memory::Stack *stack)
{
	m->stack = stack;
	/* A Contact::Constraint carries a whole manifold, so it is 624 bytes: the
	   qu3e page of 256 asks for 156 KB in one malloc, which fragments the heap
	   badly on a console with 4 MB. 32 keeps a page at 20 KB and the allocator
	   simply adds another when a busy scene needs it. */
	memory::paged::init(&m->allocator, (int32_t)sizeof(Contact::Constraint), 32);
	collision::broadPhase::init(&m->broadphase, m);
	m->contact_list = NULL;
	m->contact_count = 0;
	m->contact_listener = NULL;
}


void shutdown(Contact::Manager *m)
{
	collision::broadPhase::shutdown(&m->broadphase);
	memory::paged::shutdown(&m->allocator);
	m->contact_list = NULL;
	m->contact_count = 0;
}


void addContact(Contact::Manager *m, physics::Shape *A, physics::Shape *B)
{
	RigidBody *body_a = A->body;
	RigidBody *body_b = B->body;
	if (!rigidBody::canCollide(body_a, body_b)) return;

	/* Dedup. */
	Contact::Edge *edge = body_a->contact_list;
	while (edge) {
		if (edge->other == body_b) {
			physics::Shape *shape_a = edge->constraint->A;
			physics::Shape *shape_b = edge->constraint->B;
			if (A == shape_a && B == shape_b) return;
		}
		edge = edge->next;
	}

	Contact::Constraint *contact = (Contact::Constraint *)memory::paged::allocate(&m->allocator);
	contact->A = A;
	contact->B = B;
	contact->body_a = A->body;
	contact->body_b = B->body;
	manifold::setPair(&contact->manifold, A, B);
	contact->flags = 0;
	contact->friction = mixFriction(A, B);
	contact->restitution = mixRestitution(A, B);
	contact->manifold.contact_count = 0;

	/* The allocator hands out raw malloc memory, and testCollisions runs
	   computeBasis on this normal whether or not the pair touched. Left as it
	   came, a fresh page from a previous scene makes it garbage — which is why
	   the first run survives and the second one does not. */
	contact->manifold.normal = (Vector3){ 0.0f, 0.0f, 1.0f };
	contact->manifold.tangent_vectors[0] = (Vector3){ 1.0f, 0.0f, 0.0f };
	contact->manifold.tangent_vectors[1] = (Vector3){ 0.0f, 1.0f, 0.0f };

	for (int32_t i = 0; i < 8; ++i) contact->manifold.contacts[i].warm_started = 0;

	contact->prev = NULL;
	contact->next = m->contact_list;
	if (m->contact_list) m->contact_list->prev = contact;
	m->contact_list = contact;

	contact->edge_a.constraint = contact;
	contact->edge_a.other = body_b;
	contact->edge_a.prev = NULL;
	contact->edge_a.next = body_a->contact_list;
	if (body_a->contact_list) body_a->contact_list->prev = &contact->edge_a;
	body_a->contact_list = &contact->edge_a;

	contact->edge_b.constraint = contact;
	contact->edge_b.other = body_a;
	contact->edge_b.prev = NULL;
	contact->edge_b.next = body_b->contact_list;
	if (body_b->contact_list) body_b->contact_list->prev = &contact->edge_b;
	body_b->contact_list = &contact->edge_b;

	rigidBody::setToAwake(body_a);
	rigidBody::setToAwake(body_b);

	++m->contact_count;
}


void findNewContacts(Contact::Manager *m)
{
	collision::broadPhase::updatePairs(&m->broadphase);
}


void removeContact(Contact::Manager *m, Contact::Constraint *contact)
{
	RigidBody *A = contact->body_a;
	RigidBody *B = contact->body_b;

	/* Remove from A. */
	if (contact->edge_a.prev) contact->edge_a.prev->next = contact->edge_a.next;
	if (contact->edge_a.next) contact->edge_a.next->prev = contact->edge_a.prev;
	if (&contact->edge_a == A->contact_list) A->contact_list = contact->edge_a.next;

	/* Remove from B. */
	if (contact->edge_b.prev) contact->edge_b.prev->next = contact->edge_b.next;
	if (contact->edge_b.next) contact->edge_b.next->prev = contact->edge_b.prev;
	if (&contact->edge_b == B->contact_list) B->contact_list = contact->edge_b.next;

	rigidBody::setToAwake(A);
	rigidBody::setToAwake(B);

	if (contact->prev) contact->prev->next = contact->next;
	if (contact->next) contact->next->prev = contact->prev;
	if (contact == m->contact_list) m->contact_list = contact->next;

	--m->contact_count;

	memory::paged::free(&m->allocator, contact);
}


void removeContactsFromBody(Contact::Manager *m, RigidBody *body)
{
	Contact::Edge *edge = body->contact_list;
	while (edge) {
		Contact::Edge *next = edge->next;
		removeContact(m, edge->constraint);
		edge = next;
	}
}


void removeFromBroadphase(Contact::Manager *m, RigidBody *body)
{
	physics::Shape *shape = body->shapes;
	while (shape) {
		collision::broadPhase::removeShape(&m->broadphase, shape);
		shape = shape->next;
	}
}


void testCollisions(Contact::Manager *m)
{
	Contact::Constraint *constraint = m->contact_list;

	while (constraint) {
		physics::Shape *A = constraint->A;
		physics::Shape *B = constraint->B;
		RigidBody *body_a = A->body;
		RigidBody *body_b = B->body;

		constraint->flags &= ~CONSTRAINT_ISLAND;

		if (!rigidBody::isAwake(body_a) && !rigidBody::isAwake(body_b)) {
			constraint = constraint->next;
			continue;
		}

		if (!rigidBody::canCollide(body_a, body_b)) {
			Contact::Constraint *next = constraint->next;
			removeContact(m, constraint);
			constraint = next;
			continue;
		}

		if (!collision::broadPhase::testOverlap(&m->broadphase, A->broadphase_index, B->broadphase_index)) {
			Contact::Constraint *next = constraint->next;
			removeContact(m, constraint);
			constraint = next;
			continue;
		}

		Contact::Manifold *manifold = &constraint->manifold;

		/* Warm start needs only the previous impulses, keyed by feature, and
		   the tangents they were measured on: a copy of the whole manifold
		   would move 600 bytes per pair per step for these 130. */
		struct {
			uint32_t key;
			float normal_impulse;
			float tangent_impulse[2];
		} old_points[8] __attribute__((uninitialized));
		int32_t old_count = manifold->contact_count;
		for (int32_t j = 0; j < old_count; ++j) {
			const Contact::Point *oc = manifold->contacts + j;
			old_points[j].key = oc->fp.key;
			old_points[j].normal_impulse = oc->normal_impulse;
			old_points[j].tangent_impulse[0] = oc->tangent_impulse[0];
			old_points[j].tangent_impulse[1] = oc->tangent_impulse[1];
		}
		Vector3 ot0 = manifold->tangent_vectors[0];
		Vector3 ot1 = manifold->tangent_vectors[1];

		constraint::solveCollision(constraint);

		/* Tangents are read by the solver and by next step's warm start, both
		   gated by contact_count: a pair that does not touch skips the
		   normalization the basis costs. */
		if (!manifold->contact_count) {
			constraint = constraint->next;
			continue;
		}

		vector3::computeBasis(&manifold->normal, &manifold->tangent_vectors[0], &manifold->tangent_vectors[1]);

		for (int32_t i = 0; i < manifold->contact_count; ++i) {
			Contact::Point *c = manifold->contacts + i;
			c->tangent_impulse[0] = 0.0f;
			c->tangent_impulse[1] = 0.0f;
			c->normal_impulse = 0.0f;
			uint8_t old_warm = c->warm_started;
			c->warm_started = 0;

			for (int32_t j = 0; j < old_count; ++j) {
				if (c->fp.key == (int32_t)old_points[j].key) {
					c->normal_impulse = old_points[j].normal_impulse;

					Vector3 t0_imp = vector3::scaled(&ot0, old_points[j].tangent_impulse[0]);
					Vector3 t1_imp = vector3::scaled(&ot1, old_points[j].tangent_impulse[1]);
					Vector3 friction = vector3::sum(&t0_imp, &t1_imp);
					c->tangent_impulse[0] = vector3::dot(&friction, &manifold->tangent_vectors[0]);
					c->tangent_impulse[1] = vector3::dot(&friction, &manifold->tangent_vectors[1]);
					uint8_t next_warm = (uint8_t)(old_warm + 1);
					c->warm_started = (old_warm > next_warm) ? old_warm : next_warm;
					break;
				}
			}
		}

		constraint = constraint->next;
	}
}

}
}
}
