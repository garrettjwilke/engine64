/*
	Ported from qu3e q3Contact.h — altered source, not the original software.

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
	Contact point, manifold, edge, constraint. Pairs work on physics::Shape
	(box / sphere / capsule via tagged union). Manager and Solver are defined
	in their own headers as Contact::Manager and Contact::Solver.
*/
#ifndef ENGINE64_CONTACT_H
#define ENGINE64_CONTACT_H

#include <math.h>
#include <stdint.h>

#include "math/e64_vector3.h"
#include "math/e64_math.h"
#include "physics/shapes/e64_physics_shape.h"

namespace e64 {

class RigidBody;


enum {
	CONSTRAINT_COLLIDING = 0x00000001,
	CONSTRAINT_WAS_COLLIDING = 0x00000002,
	CONSTRAINT_ISLAND = 0x00000004,
};


class Contact {
public:

	/* 32-bit key identifying a contact point across frames. */
	union FeaturePair {
		struct {
			uint8_t in_r;
			uint8_t out_r;
			uint8_t in_i;
			uint8_t out_i;
		};
		int32_t key;
	};


	struct Point {
		Vector3 position;
		float penetration;
		float normal_impulse;
		float tangent_impulse[2];
		float bias;
		float normal_mass;
		float tangent_mass[2];
		FeaturePair fp;
		uint8_t warm_started;
	};


	/* Up to 8 contact points between two shapes. */
	struct Manifold {
		physics::Shape *A;
		physics::Shape *B;

		Vector3 normal; /* from A to B */
		Vector3 tangent_vectors[2];
		Point contacts[8];
		int32_t contact_count;

		Manifold *next;
		Manifold *prev;

		int sensor;
	};


	struct Constraint;

	/* Node in a body's intrusive contact list. */
	struct Edge {
		RigidBody *other;
		Constraint *constraint;
		Edge *next;
		Edge *prev;
	};


	/* Persistent constraint between two bodies. */
	struct Constraint {
		physics::Shape *A;
		physics::Shape *B;
		RigidBody *body_a;
		RigidBody *body_b;

		Edge edge_a;
		Edge edge_b;
		Constraint *next;
		Constraint *prev;

		float friction;
		float restitution;

		Manifold manifold;

		int32_t flags;
	};


	/* Broadphase proxy pair, by tree index. */
	struct Pair {
		int32_t A;
		int32_t B;
	};


	class Manager; /* e64_contact_manager.h */
	class Solver; /* e64_contact_solver.h */
};


namespace contact {

namespace manifold {

void setPair(Contact::Manifold *m, physics::Shape *a, physics::Shape *b);

}


namespace constraint {

void solveCollision(Contact::Constraint *c);

}


/* Restitution keeps the max, so the bounciest side wins; friction takes the
   geometric mean, so the slippery side dominates. */
static inline float mixRestitution(const physics::Shape *A, const physics::Shape *B) {
	return (A->restitution > B->restitution) ? A->restitution : B->restitution;
}

static inline float mixFriction(const physics::Shape *A, const physics::Shape *B) {
	return sqrtf(A->friction * B->friction);
}

}

}

#endif
