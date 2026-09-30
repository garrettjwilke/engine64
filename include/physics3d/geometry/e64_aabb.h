/*
	Ported from qu3e q3Geometry.h — altered source, not the original software.

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
	Axis-aligned bounding box.
*/
#ifndef ENGINE64_AABB_H
#define ENGINE64_AABB_H

#include "math/e64_vector3.h"

namespace e64 {

typedef struct AABB {
	Vector3 min;
	Vector3 max;
} AABB;


namespace aabb {

int containsAABB(const AABB *a, const AABB *other);
int containsPoint(const AABB *a, const Vector3 *p);
float surfaceArea(const AABB *a);
int overlaps(const AABB *a, const AABB *b);
AABB combine(const AABB *a, const AABB *b);

Vector3 closestToPoint (const AABB *a, const Vector3 *p);
Vector3 closestToSegment(const AABB *a, const Vector3 *p, const Vector3 *q);

}

}

#endif
