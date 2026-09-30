/*
	rectangleToRectangle is ported from Box2D b2_collide_polygon.cpp
	(b2CollidePolygons) — altered source, not the original software.

	MIT License

	Copyright (c) 2019 Erin Catto

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include <math.h>

#include "physics2d/collision/e64_collision2d.h"
#include "physics2d/geometry/e64_aabb2d.h"

namespace e64 {
namespace collision2d {

static const float EPSILON_2D = 1.0e-6f;


/*
	Circle, capsule and segment are one shape to the narrowphase: a core
	segment swept by a radius. The circle's core is a single point and the
	segment's radius is zero.
*/
struct Rounded {
	Vector2 a;
	Vector2 b;
	float radius;
};


static Rounded fromCircle(const Circle2D *c, const Transform2D *world)
{
	return (Rounded){ world->position, world->position, c->radius };
}


static Rounded fromCapsule(const Capsule2D *c, const Transform2D *world)
{
	Rounded r;
	capsule2d::getSegment(c, world, &r.a, &r.b);
	r.radius = c->radius;
	return r;
}


static Rounded fromSegment(const Segment2D *s, const Transform2D *world)
{
	Rounded r;
	segment2d::getPoints(s, world, &r.a, &r.b);
	r.radius = 0.0f;
	return r;
}


/* Unit perpendicular of the core, or zero when the core is a point. */
static Vector2 corePerpendicular(const Rounded *r)
{
	Vector2 d = vector2::difference(&r->b, &r->a);
	float len = vector2::magnitude(&d);
	if (len <= EPSILON_2D) return vector2::zero();
	return (Vector2){ -d.y / len, d.x / len };
}


/* When the cores touch there is no direction between the closest points:
   the normal is the perpendicular of A's core, or of B's when A's is a
   point, turned toward B. Two point cores fall back to +y. */
static Vector2 touchingNormal(const Rounded *a, const Rounded *b)
{
	Vector2 mid_a = { (a->a.x + a->b.x) * 0.5f, (a->a.y + a->b.y) * 0.5f };
	Vector2 mid_b = { (b->a.x + b->b.x) * 0.5f, (b->a.y + b->b.y) * 0.5f };
	Vector2 to_b = vector2::difference(&mid_b, &mid_a);

	Vector2 n = corePerpendicular(a);
	if (vector2::squaredMagnitude(&n) == 0.0f) n = corePerpendicular(b);
	if (vector2::squaredMagnitude(&n) == 0.0f) return (Vector2){ 0.0f, 1.0f };

	if (vector2::dot(&n, &to_b) < 0.0f) vector2::invert(&n);
	return n;
}


static void roundedToRounded(Contact2D::Manifold *m, const Rounded *a, const Rounded *b)
{
	m->contact_count = 0;

	Vector2 ca, cb;
	segment2d::closestToSegment(&a->a, &a->b, &b->a, &b->b, &ca, &cb);

	Vector2 d = vector2::difference(&cb, &ca);
	float rsum = a->radius + b->radius;
	float d2 = vector2::squaredMagnitude(&d);
	if (d2 > rsum * rsum) return;

	float dist = sqrtf(d2);
	Vector2 n = (dist > EPSILON_2D)
		? vector2::scaled(&d, 1.0f / dist)
		: touchingNormal(a, b);

	m->normal = n;
	m->contact_count = 1;
	Contact2D::Point *c = m->contacts;
	Vector2 off = vector2::scaled(&n, -b->radius);
	c->position = vector2::sum(&cb, &off);
	c->penetration = dist - rsum;
}


/*
	Rounded shape is A, rectangle is B, solved in the rectangle's space. While
	the core stays outside the box, the closest points give the normal, as in
	capsuleToStaticBox. Once the core reaches inside there are no closest
	points left, and the normal is the axis of least penetration among the box
	axes and the core's perpendicular.
*/
static void roundedToRectangle(Contact2D::Manifold *m, const Rounded *r,
                               const Rectangle2D *rect, const Transform2D *rect_world)
{
	m->contact_count = 0;

	Vector2 la = transform2d::mulVectorTransposed(rect_world, &r->a);
	Vector2 lb = transform2d::mulVectorTransposed(rect_world, &r->b);

	Vector2 e = rect->e;
	AABB2D box_local = { { -e.x, -e.y }, { e.x, e.y } };

	Vector2 c_on_box = aabb2d::closestToSegment(&box_local, &la, &lb);
	Vector2 c_on_seg = segment2d::closestToPoint(&la, &lb, &c_on_box);
	Vector2 d = vector2::difference(&c_on_box, &c_on_seg);
	float dist2 = vector2::squaredMagnitude(&d);
	if (dist2 > r->radius * r->radius) return;

	Vector2 normal;
	Vector2 point;
	float penetration;

	if (dist2 > EPSILON_2D * EPSILON_2D) {
		float dist = sqrtf(dist2);
		normal = vector2::scaled(&d, 1.0f / dist);
		point = c_on_box;
		penetration = dist - r->radius;
	}
	else {
		Vector2 axes[3] = { { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } };
		int axis_count = 2;

		Vector2 core = vector2::difference(&lb, &la);
		float core_len = vector2::magnitude(&core);
		if (core_len > EPSILON_2D) {
			axes[2] = (Vector2){ -core.y / core_len, core.x / core_len };
			axis_count = 3;
		}

		float best = INFINITY;
		Vector2 best_n = axes[0];
		Vector2 best_s = la;

		for (int i = 0; i < axis_count; ++i) {
			Vector2 n = axes[i];
			float h = e.x * fabsf(n.x) + e.y * fabsf(n.y);
			float sa = vector2::dot(&la, &n);
			float sb = vector2::dot(&lb, &n);
			float smax = fmaxf(sa, sb);
			float smin = fminf(sa, sb);

			/* Normal +n: A's furthest reach along n against B's near side. */
			float depth = smax + r->radius + h;
			if (depth < best) {
				best = depth;
				best_n = n;
				best_s = (sa >= sb) ? la : lb;
			}

			/* Normal -n. */
			depth = h - smin + r->radius;
			if (depth < best) {
				best = depth;
				best_n = vector2::inverted(&n);
				best_s = (sa <= sb) ? la : lb;
			}
		}

		/* A's deepest point along the normal, pulled back by the depth,
		   lands on B's surface. */
		normal = best_n;
		float back = r->radius - best;
		Vector2 off = vector2::scaled(&normal, back);
		point = vector2::sum(&best_s, &off);
		penetration = -best;
	}

	m->normal = transform2d::rotateVector(rect_world, &normal);
	m->contact_count = 1;
	Contact2D::Point *c = m->contacts;
	c->position = transform2d::mulVector(rect_world, &point);
	c->penetration = penetration;
}


/* Rectangle as a counterclockwise polygon in world space: vertex i starts
   edge i, whose outward normal is normals[i]. */
struct Polygon {
	Vector2 vertices[4];
	Vector2 normals[4];
};


static void toPolygon(Polygon *p, const Rectangle2D *rect, const Transform2D *world)
{
	Vector2 e = rect->e;
	Vector2 v[4] = { { -e.x, -e.y }, { e.x, -e.y }, { e.x, e.y }, { -e.x, e.y } };
	Vector2 n[4] = { { 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f } };

	for (int i = 0; i < 4; ++i) {
		p->vertices[i] = transform2d::mulVector(world, &v[i]);
		p->normals[i] = transform2d::rotateVector(world, &n[i]);
	}
}


/* Largest separation of p2 from any edge of p1, and that edge. */
static float findMaxSeparation(int *edge, const Polygon *p1, const Polygon *p2)
{
	int best_index = 0;
	float max_separation = -INFINITY;

	for (int i = 0; i < 4; ++i) {
		const Vector2 *n = &p1->normals[i];
		const Vector2 *v1 = &p1->vertices[i];

		float si = INFINITY;
		for (int j = 0; j < 4; ++j) {
			Vector2 d = vector2::difference(&p2->vertices[j], v1);
			float sij = vector2::dot(n, &d);
			if (sij < si) si = sij;
		}

		if (si > max_separation) {
			max_separation = si;
			best_index = i;
		}
	}

	*edge = best_index;
	return max_separation;
}


/* The edge of p2 whose normal is most anti-parallel to p1's reference edge. */
static void findIncidentEdge(Vector2 out[2], const Polygon *p1, int edge1, const Polygon *p2)
{
	const Vector2 *normal1 = &p1->normals[edge1];

	int index = 0;
	float min_dot = INFINITY;
	for (int i = 0; i < 4; ++i) {
		float dot = vector2::dot(normal1, &p2->normals[i]);
		if (dot < min_dot) {
			min_dot = dot;
			index = i;
		}
	}

	out[0] = p2->vertices[index];
	out[1] = p2->vertices[(index + 1) & 3];
}


/* Sutherland-Hodgman clipping of a segment against the half plane
   dot(normal, x) <= offset. */
static int clipSegmentToLine(Vector2 out[2], const Vector2 in[2], const Vector2 *normal, float offset)
{
	int count = 0;

	float distance0 = vector2::dot(normal, &in[0]) - offset;
	float distance1 = vector2::dot(normal, &in[1]) - offset;

	if (distance0 <= 0.0f) out[count++] = in[0];
	if (distance1 <= 0.0f) out[count++] = in[1];

	if (distance0 * distance1 < 0.0f) {
		float interp = distance0 / (distance0 - distance1);
		out[count].x = in[0].x + interp * (in[1].x - in[0].x);
		out[count].y = in[0].y + interp * (in[1].y - in[0].y);
		++count;
	}

	return count;
}


/* After a swapped call the points lie on the original A and the normal runs
   B to A: every point crosses the gap to B's surface, then the normal turns
   around. */
static void flip(Contact2D::Manifold *m)
{
	Vector2 n = m->normal;
	for (int i = 0; i < m->contact_count; ++i) {
		Contact2D::Point *c = &m->contacts[i];
		Vector2 off = vector2::scaled(&n, -c->penetration);
		c->position = vector2::sum(&c->position, &off);
	}
	m->normal = vector2::inverted(&n);
}


void circleToCircle(Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                    const Circle2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCircle(a, a_world);
	Rounded rb = fromCircle(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void circleToCapsule(Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                     const Capsule2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCircle(a, a_world);
	Rounded rb = fromCapsule(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void circleToSegment(Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                     const Segment2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCircle(a, a_world);
	Rounded rb = fromSegment(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void circleToRectangle(Contact2D::Manifold *m, const Circle2D *a, const Transform2D *a_world,
                       const Rectangle2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCircle(a, a_world);
	roundedToRectangle(m, &ra, b, b_world);
}


void capsuleToCapsule(Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                      const Capsule2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCapsule(a, a_world);
	Rounded rb = fromCapsule(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void capsuleToSegment(Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                      const Segment2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCapsule(a, a_world);
	Rounded rb = fromSegment(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void capsuleToRectangle(Contact2D::Manifold *m, const Capsule2D *a, const Transform2D *a_world,
                        const Rectangle2D *b, const Transform2D *b_world)
{
	Rounded ra = fromCapsule(a, a_world);
	roundedToRectangle(m, &ra, b, b_world);
}


void segmentToSegment(Contact2D::Manifold *m, const Segment2D *a, const Transform2D *a_world,
                      const Segment2D *b, const Transform2D *b_world)
{
	Rounded ra = fromSegment(a, a_world);
	Rounded rb = fromSegment(b, b_world);
	roundedToRounded(m, &ra, &rb);
}


void segmentToRectangle(Contact2D::Manifold *m, const Segment2D *a, const Transform2D *a_world,
                        const Rectangle2D *b, const Transform2D *b_world)
{
	Rounded ra = fromSegment(a, a_world);
	roundedToRectangle(m, &ra, b, b_world);
}


/* Separating axis over the four edge normals of each rectangle, then the
   incident edge clipped against the reference face. */
void rectangleToRectangle(Contact2D::Manifold *m, const Rectangle2D *a, const Transform2D *a_world,
                          const Rectangle2D *b, const Transform2D *b_world)
{
	m->contact_count = 0;

	Polygon poly_a, poly_b;
	toPolygon(&poly_a, a, a_world);
	toPolygon(&poly_b, b, b_world);

	int edge_a = 0;
	float separation_a = findMaxSeparation(&edge_a, &poly_a, &poly_b);
	if (separation_a > 0.0f) return;

	int edge_b = 0;
	float separation_b = findMaxSeparation(&edge_b, &poly_b, &poly_a);
	if (separation_b > 0.0f) return;

	/* Prefer A's face unless B's is clearly better, so the reference face
	   does not flicker between two nearly equal ones. */
	const float k_tol = 1.0e-3f;

	const Polygon *poly1;
	const Polygon *poly2;
	int edge1;
	int flipped;

	if (separation_b > separation_a + k_tol) {
		poly1 = &poly_b;
		poly2 = &poly_a;
		edge1 = edge_b;
		flipped = 1;
	}
	else {
		poly1 = &poly_a;
		poly2 = &poly_b;
		edge1 = edge_a;
		flipped = 0;
	}

	Vector2 incident_edge[2];
	findIncidentEdge(incident_edge, poly1, edge1, poly2);

	Vector2 v11 = poly1->vertices[edge1];
	Vector2 v12 = poly1->vertices[(edge1 + 1) & 3];

	Vector2 tangent = vector2::difference(&v12, &v11);
	tangent = vector2::normalized(&tangent);
	Vector2 normal = poly1->normals[edge1];

	float front_offset = vector2::dot(&normal, &v11);
	float side_offset1 = -vector2::dot(&tangent, &v11);
	float side_offset2 = vector2::dot(&tangent, &v12);

	Vector2 clip1[2];
	Vector2 clip2[2];
	Vector2 neg_tangent = vector2::inverted(&tangent);

	if (clipSegmentToLine(clip1, incident_edge, &neg_tangent, side_offset1) < 2) return;
	if (clipSegmentToLine(clip2, clip1, &tangent, side_offset2) < 2) return;

	/* Reference on A: the clipped points sit on B, the normal is the face's.
	   Reference on B: they sit on A, so each crosses the overlap to B's
	   surface and the normal turns around to run A to B. */
	Vector2 n = flipped ? vector2::inverted(&normal) : normal;

	int count = 0;
	for (int i = 0; i < 2; ++i) {
		float separation = vector2::dot(&normal, &clip2[i]) - front_offset;
		if (separation > 0.0f) continue;

		Contact2D::Point *c = &m->contacts[count++];
		c->position = clip2[i];
		if (flipped) {
			Vector2 off = vector2::scaled(&n, separation);
			c->position = vector2::sum(&c->position, &off);
		}
		c->penetration = separation;
	}

	m->normal = n;
	m->contact_count = count;
}


/* Order of the pair table: a pair is written with the lower rank first. */
static int rank(physics2d::Shape2D::Type type)
{
	switch (type) {
	case physics2d::Shape2D::SHAPE_CIRCLE: return 0;
	case physics2d::Shape2D::SHAPE_CAPSULE: return 1;
	case physics2d::Shape2D::SHAPE_SEGMENT: return 2;
	case physics2d::Shape2D::SHAPE_RECTANGLE: return 3;
	}
	return 0;
}


static void collideOrdered(Contact2D::Manifold *m, const physics2d::Shape2D *a, const physics2d::Shape2D *b)
{
	const Transform2D *aw = &a->world;
	const Transform2D *bw = &b->world;

	switch (a->type) {
	case physics2d::Shape2D::SHAPE_CIRCLE:
		switch (b->type) {
		case physics2d::Shape2D::SHAPE_CIRCLE: circleToCircle(m, &a->circle, aw, &b->circle, bw); break;
		case physics2d::Shape2D::SHAPE_CAPSULE: circleToCapsule(m, &a->circle, aw, &b->capsule, bw); break;
		case physics2d::Shape2D::SHAPE_SEGMENT: circleToSegment(m, &a->circle, aw, &b->segment, bw); break;
		case physics2d::Shape2D::SHAPE_RECTANGLE: circleToRectangle(m, &a->circle, aw, &b->rectangle, bw); break;
		}
		break;

	case physics2d::Shape2D::SHAPE_CAPSULE:
		switch (b->type) {
		case physics2d::Shape2D::SHAPE_CAPSULE: capsuleToCapsule(m, &a->capsule, aw, &b->capsule, bw); break;
		case physics2d::Shape2D::SHAPE_SEGMENT: capsuleToSegment(m, &a->capsule, aw, &b->segment, bw); break;
		case physics2d::Shape2D::SHAPE_RECTANGLE: capsuleToRectangle(m, &a->capsule, aw, &b->rectangle, bw); break;
		default: break;
		}
		break;

	case physics2d::Shape2D::SHAPE_SEGMENT:
		switch (b->type) {
		case physics2d::Shape2D::SHAPE_SEGMENT: segmentToSegment(m, &a->segment, aw, &b->segment, bw); break;
		case physics2d::Shape2D::SHAPE_RECTANGLE: segmentToRectangle(m, &a->segment, aw, &b->rectangle, bw); break;
		default: break;
		}
		break;

	case physics2d::Shape2D::SHAPE_RECTANGLE:
		if (b->type == physics2d::Shape2D::SHAPE_RECTANGLE)
			rectangleToRectangle(m, &a->rectangle, aw, &b->rectangle, bw);
		break;
	}
}


void collide(Contact2D::Manifold *m, const physics2d::Shape2D *a, const physics2d::Shape2D *b)
{
	m->contact_count = 0;

	if (rank(a->type) <= rank(b->type)) {
		collideOrdered(m, a, b);
		return;
	}

	collideOrdered(m, b, a);
	if (m->contact_count > 0) flip(m);
}

}
}
