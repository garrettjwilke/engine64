/*
	Contact data in the plane, the 2D counterpart of Contact. Detection only:
	no body, no friction, no accumulated impulses.

	The normal points from A to B. Each point lies on the surface of B, and
	its penetration is signed like the 3D static pairs: distance minus the
	radii, negative while the shapes overlap.
*/
#ifndef ENGINE64_CONTACT2D_H
#define ENGINE64_CONTACT2D_H

#include <stdint.h>

#include "math/e64_vector2.h"

namespace e64 {

class Contact2D {
public:

	struct Point {
		Vector2 position;
		float penetration;
	};


	struct Manifold {
		Vector2 normal;
		Point contacts[2];
		int32_t contact_count;
	};
};

}

#endif
