/*
	Ported from qu3e q3Geometry.cpp — altered source, not the original software.

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

#include <float.h>

#include "physics2d/geometry/e64_aabb2d.h"

namespace e64 {
namespace aabb2d {

int containsAABB(const AABB2D *a, const AABB2D *other)
{
	return
		a->min.x <= other->min.x &&
		a->min.y <= other->min.y &&
		a->max.x >= other->max.x &&
		a->max.y >= other->max.y;
}

int containsPoint(const AABB2D *a, const Vector2 *p)
{
	return
		a->min.x <= p->x && a->min.y <= p->y &&
		a->max.x >= p->x && a->max.y >= p->y;
}

float perimeter(const AABB2D *a)
{
	float x = a->max.x - a->min.x;
	float y = a->max.y - a->min.y;
	return 2.0f * (x + y);
}

int overlaps(const AABB2D *a, const AABB2D *b)
{
	if (a->max.x < b->min.x || a->min.x > b->max.x) return 0;
	if (a->max.y < b->min.y || a->min.y > b->max.y) return 0;
	return 1;
}

AABB2D combine(const AABB2D *a, const AABB2D *b)
{
	AABB2D c;
	c.min = vector2::min(&a->min, &b->min);
	c.max = vector2::max(&a->max, &b->max);
	return c;
}


Vector2 closestToPoint(const AABB2D *a, const Vector2 *p)
{
	Vector2 c;
	c.x = p->x < a->min.x ? a->min.x : (p->x > a->max.x ? a->max.x : p->x);
	c.y = p->y < a->min.y ? a->min.y : (p->y > a->max.y ? a->max.y : p->y);
	return c;
}


/* Closest point on the box to the segment [a, b]: aabb::closestToSegment
   with the z slab taken out. */
Vector2 closestToSegment(const AABB2D *bounds, const Vector2 *a, const Vector2 *b)
{
	Vector2 center = { 0.5f * (bounds->min.x + bounds->max.x),
	                   0.5f * (bounds->min.y + bounds->max.y) };
	Vector2 half_size = { 0.5f * (bounds->max.x - bounds->min.x),
	                      0.5f * (bounds->max.y - bounds->min.y) };

	Vector2 s = vector2::difference(a, &center);
	Vector2 v = vector2::difference(b, a);
	Vector2 sign = { 1.0f, 1.0f };

	if (v.x < 0.0f) { s.x = -s.x; v.x = -v.x; sign.x = -1.0f; }
	if (v.y < 0.0f) { s.y = -s.y; v.y = -v.y; sign.y = -1.0f; }

	Vector2 v2 = { v.x * v.x, v.y * v.y };
	Vector2 tanchor = { 2.0f, 2.0f };
	Vector2 region = { 0.0f, 0.0f };

	if (v.x > FLT_MIN) {
		if (s.x < -half_size.x) { region.x = -1.0f; tanchor.x = (-half_size.x - s.x) / v.x; }
		else if (s.x > half_size.x) { region.x = 1.0f; tanchor.x = ( half_size.x - s.x) / v.x; }
	}
	if (v.y > FLT_MIN) {
		if (s.y < -half_size.y) { region.y = -1.0f; tanchor.y = (-half_size.y - s.y) / v.y; }
		else if (s.y > half_size.y) { region.y = 1.0f; tanchor.y = ( half_size.y - s.y) / v.y; }
	}

	float t = 0.0f;
	float dd2dt = 0.0f;
	if (region.x != 0.0f) dd2dt -= v2.x * tanchor.x;
	if (region.y != 0.0f) dd2dt -= v2.y * tanchor.y;

	if (dd2dt < 0.0f) {
		for (;;) {
			float next_t = 1.0f;
			if (tanchor.x > t && tanchor.x < 1.0f && tanchor.x < next_t) next_t = tanchor.x;
			if (tanchor.y > t && tanchor.y < 1.0f && tanchor.y < next_t) next_t = tanchor.y;

			float next_dd2dt = 0.0f;
			next_dd2dt += (region.x != 0.0f ? v2.x : 0.0f) * (next_t - tanchor.x);
			next_dd2dt += (region.y != 0.0f ? v2.y : 0.0f) * (next_t - tanchor.y);

			if (next_dd2dt >= 0.0f) {
				float m = (next_dd2dt - dd2dt) / (next_t - t);
				t -= dd2dt / m;
				break;
			}

			if (tanchor.x == next_t) { tanchor.x = ( half_size.x - s.x) / v.x; region.x += 1.0f; }
			if (tanchor.y == next_t) { tanchor.y = ( half_size.y - s.y) / v.y; region.y += 1.0f; }

			t = next_t;
			dd2dt = next_dd2dt;
			if (t >= 1.0f) { t = 1.0f; break; }
		}
	}

	Vector2 tmp = {
		sign.x * (s.x + t * v.x),
		sign.y * (s.y + t * v.y),
	};

	if (tmp.x < -half_size.x) tmp.x = -half_size.x;
	else if (tmp.x > half_size.x) tmp.x = half_size.x;
	if (tmp.y < -half_size.y) tmp.y = -half_size.y;
	else if (tmp.y > half_size.y) tmp.y = half_size.y;

	return (Vector2){ tmp.x + center.x, tmp.y + center.y };
}

}
}
