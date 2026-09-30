/*
	Ported from qu3e q3Box.cpp — altered source, not the original software.

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

#include <math.h>

#include "physics2d/shapes/e64_rectangle2d.h"

namespace e64 {
namespace rectangle2d {

int testPoint(const Rectangle2D *r, const Transform2D *world, const Vector2 *p)
{
	Vector2 p0 = transform2d::mulVectorTransposed(world, p);

	if (p0.x > r->e.x || p0.x < -r->e.x) return 0;
	if (p0.y > r->e.y || p0.y < -r->e.y) return 0;
	return 1;
}


int raycast(const Rectangle2D *r, const Transform2D *world, RaycastData2D *raycast)
{
	Vector2 d = transform2d::rotateVectorTransposed(world, &raycast->dir);
	Vector2 p = transform2d::mulVectorTransposed(world, &raycast->start);

	const float epsilon = 1.0e-8f;
	float tmin = 0.0f;
	float tmax = raycast->t;

	Vector2 n0 = vector2::zero();

	float dv[2] = { d.x, d.y };
	float pv[2] = { p.x, p.y };
	float ev[2] = { r->e.x, r->e.y };

	for (int i = 0; i < 2; ++i) {
		if (fabsf(dv[i]) < epsilon) {
			if (pv[i] < -ev[i] || pv[i] > ev[i]) return 0;
		}
		else {
			float d0 = 1.0f / dv[i];
			float s = (dv[i] >= 0.0f) ? 1.0f : -1.0f;
			float ei = ev[i] * s;

			float nv[2] = { 0.0f, 0.0f };
			nv[i] = -s;
			Vector2 n = { nv[0], nv[1] };

			float t0 = -(ei + pv[i]) * d0;
			float t1 = (ei - pv[i]) * d0;

			if (t0 > tmin) { n0 = n; tmin = t0; }
			if (t1 < tmax) { tmax = t1; }

			if (tmin > tmax) return 0;
		}
	}

	raycast->normal = transform2d::rotateVector(world, &n0);
	raycast->toi = tmin;
	return 1;
}


/* The two half-extent axes, rotated, bound the rectangle on each world axis
   by the sum of their projections. */
void computeAABB(const Rectangle2D *r, const Transform2D *world, AABB2D *aabb)
{
	Vector2 ex = { r->e.x, 0.0f };
	Vector2 ey = { 0.0f, r->e.y };
	ex = transform2d::rotateVector(world, &ex);
	ey = transform2d::rotateVector(world, &ey);

	Vector2 extent = { fabsf(ex.x) + fabsf(ey.x), fabsf(ex.y) + fabsf(ey.y) };
	aabb->min = vector2::difference(&world->position, &extent);
	aabb->max = vector2::sum(&world->position, &extent);
}

}
}
