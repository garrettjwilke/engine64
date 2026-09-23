/*
	Ported from qu3e q3Collide.h — altered source, not the original software.

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
	Narrowphase dispatcher.

	collide() takes a manifold and two shapes and dispatches on
	(A->type, B->type) to the right pairwise function. The manifold normal
	always points from A to B.
*/
#ifndef ENGINE64_COLLISION_H
#define ENGINE64_COLLISION_H

#include "physics/shapes/e64_physics_shape.h"
#include "physics/collision/e64_contact.h"
#include "physics/geometry/e64_triangle.h"

namespace e64 {

namespace collision {

void collide(Contact::Manifold *m, physics::Shape *a, physics::Shape *b);

void boxToBox (Contact::Manifold *m, physics::Shape *a, physics::Shape *b);
void sphereToSphere (Contact::Manifold *m, physics::Shape *a, physics::Shape *b);
void sphereToBox (Contact::Manifold *m, physics::Shape *sphere, physics::Shape *box);
void sphereToCapsule (Contact::Manifold *m, physics::Shape *sphere, physics::Shape *capsule);
void capsuleToCapsule(Contact::Manifold *m, physics::Shape *a, physics::Shape *b);
void capsuleToBox (Contact::Manifold *m, physics::Shape *capsule, physics::Shape *box);

/* Contact normal correction on inactive triangle edges (port of Jolt's
   ActiveEdges::FixNormal). normal and the result follow the manifold
   convention: unit, from the capsule toward the surface. point is the
   contact point on the triangle, in the triangle's space.
   movement_direction may be zero. */
Vector3 fixTriangleNormal(const Triangle *triangle, const Vector3 *point,
                          const Vector3 *normal, const Vector3 *movement_direction);

/* Triangles come raw from a mesh collider, without a RigidBody, so this
   pair takes the capsule shape and its world transform directly. */
void capsuleToTriangle(Contact::Manifold *m, const Capsule *capsule, const Transform *world,
                       const Triangle *triangle);

/* Static geometry placed by a world transform, without a RigidBody. */
void capsuleToStaticBox(Contact::Manifold *m, const Capsule *capsule, const Transform *capsule_world,
                        const Box *box, const Transform *box_world);
void capsuleToStaticSphere(Contact::Manifold *m, const Capsule *capsule, const Transform *capsule_world,
                           const Sphere *sphere, const Transform *sphere_world);
void capsuleToStaticCapsule(Contact::Manifold *m, const Capsule *capsule, const Transform *capsule_world,
                            const Capsule *other, const Transform *other_world);

}

}

#endif
