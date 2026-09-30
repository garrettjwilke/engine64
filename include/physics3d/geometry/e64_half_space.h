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
	Plane as (normal, distance from origin).
*/
#ifndef ENGINE64_HALF_SPACE_H
#define ENGINE64_HALF_SPACE_H

#include "math/e64_vector3.h"

namespace e64 {

typedef struct HalfSpace {
	Vector3 normal;
	float distance;
} HalfSpace;


namespace halfSpace {

HalfSpace create(const Vector3 *normal, float distance);
void setFromTriangle(HalfSpace *h, const Vector3 *a, const Vector3 *b, const Vector3 *c);
void setFromNormalPoint(HalfSpace *h, const Vector3 *n, const Vector3 *p);
float distance(const HalfSpace *h, const Vector3 *p);
Vector3 projected(const HalfSpace *h, const Vector3 *p);

/* A single scale, inline. */
static inline Vector3 origin(const HalfSpace *h)
{
	return vector3::scaled(&h->normal, h->distance);
}

}

}

#endif
