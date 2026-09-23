/*
	Ported from qu3e q3ContactSolver.h — altered source, not the original software.

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
	Sequential impulse constraint solver. The island's velocity buffer is read
	through the island itself: physics::Island holds ConstraintState by pointer,
	so this header cannot include it back.
*/
#ifndef ENGINE64_CONTACT_SOLVER_H
#define ENGINE64_CONTACT_SOLVER_H

#include <stdint.h>

#include "math/e64_vector3.h"
#include "math/e64_matrix3.h"
#include "physics/collision/e64_contact.h"

namespace e64 {

namespace physics { class Island; }


class Contact::Solver {
public:

	struct State {
		Vector3 ra;
		Vector3 rb;
		float penetration;
		float normal_impulse;
		float tangent_impulse[2];
		float bias;
		float normal_mass;
		float tangent_mass[2];
	};


	struct ConstraintState {
		State contacts[8];
		int32_t contact_count;
		Vector3 tangent_vectors[2];
		Vector3 normal;
		Vector3 center_a;
		Vector3 center_b;
		Matrix3 iA;
		Matrix3 iB;
		float mA;
		float mB;
		float restitution;
		float friction;
		int32_t index_a;
		int32_t index_b;
	};


	physics::Island *island;
	ConstraintState *contacts;
	int32_t contact_count;
	int enable_friction;
};


namespace contact {

namespace solver {

void initialize(Contact::Solver *s, physics::Island *island);
void shutdown (Contact::Solver *s);
void preSolve (Contact::Solver *s, float dt);
void solve (Contact::Solver *s);

}

}

}

#endif
