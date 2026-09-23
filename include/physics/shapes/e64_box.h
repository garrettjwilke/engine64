/*
	Ported from qu3e q3Box.h — altered source, not the original software.

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
	OBB geometry (half-extents on each local axis). Admin fields live in
	physics::Shape.
*/
#ifndef ENGINE64_BOX_H
#define ENGINE64_BOX_H

#include "math/e64_vector3.h"
#include "math/e64_transform.h"
#include "physics/geometry/e64_aabb.h"
#include "physics/geometry/e64_raycast.h"

namespace e64 {

namespace physics { namespace shape { struct MassData; } }


class Box {
public:

	struct Def {
		Transform tx;
		Vector3 e;
		float friction;
		float restitution;
		float density;
		int sensor;
	};


	Vector3 e; /* half-extents on each OBB axis */
};


namespace box {

int testPoint(const Box *b, const Transform *world, const Vector3 *p);
int raycast(const Box *b, const Transform *world, RaycastData *raycast);
void computeAABB(const Box *b, const Transform *world, AABB *aabb);
void computeMass(const Box *b, const Transform *local, float density, physics::shape::MassData *md);


namespace def {

void init(Box::Def *d);
void set(Box::Def *d, const Transform *tx, const Vector3 *full_extents);
void setFriction(Box::Def *d, float f);
void setRestitution(Box::Def *d, float r);
void setDensity(Box::Def *d, float rho);
void setSensor(Box::Def *d, int s);

}

}

}

#endif
