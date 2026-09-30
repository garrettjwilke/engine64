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
	Ray in the plane with impact info, the 2D counterpart of RaycastData. dir
	is unit length, so t and toi are distances.
*/
#ifndef ENGINE64_RAYCAST2D_H
#define ENGINE64_RAYCAST2D_H

#include "math/e64_vector2.h"

namespace e64 {

typedef struct RaycastData2D {
	Vector2 start;
	Vector2 dir;
	float t;
	float toi;
	Vector2 normal;
} RaycastData2D;


namespace raycast2d {

void set(RaycastData2D *r, const Vector2 *start, const Vector2 *dir, float endTime);
Vector2 getImpactPoint(const RaycastData2D *r);

}

}

#endif
