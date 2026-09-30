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
	Axis-aligned bounding box in the plane, the 2D counterpart of AABB.
*/
#ifndef ENGINE64_AABB2D_H
#define ENGINE64_AABB2D_H

#include "math/e64_vector2.h"

namespace e64 {

typedef struct AABB2D {
	Vector2 min;
	Vector2 max;
} AABB2D;


namespace aabb2d {

int containsAABB(const AABB2D *a, const AABB2D *other);
int containsPoint(const AABB2D *a, const Vector2 *p);
float perimeter(const AABB2D *a); /* what surfaceArea is to the 3D tree */
int overlaps(const AABB2D *a, const AABB2D *b);
AABB2D combine(const AABB2D *a, const AABB2D *b);

Vector2 closestToPoint (const AABB2D *a, const Vector2 *p);
Vector2 closestToSegment(const AABB2D *a, const Vector2 *p, const Vector2 *q);

}

}

#endif
