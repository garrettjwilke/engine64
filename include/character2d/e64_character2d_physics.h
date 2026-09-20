/*
	Capsule collision response for the 2D body, against the solid tiles of
	the stage. Port of character3d::physics to the plane: the same
	depenetration and floor detection from Godot, the same contact
	classification by normal, with every cell of the stage that is solid
	standing in for a triangle of the collision mesh. No swept motion: the
	body moves the full step first and is recovered out of penetration
	here.

	The plane is the screen: x right, y down. Up is -y, and the floor's
	normal points that way.
*/
#ifndef ENGINE64_CHARACTER2D_PHYSICS_H
#define ENGINE64_CHARACTER2D_PHYSICS_H

#include <stdbool.h>

namespace e64 {

typedef struct Character2D Character2D;


namespace character2d {

/* A vertical capsule standing on the feet: radius, and the full height
   from the feet to the top of the head, in world pixels. */
typedef struct ColliderSettings {

	float radius;
	float height;

} ColliderSettings;


namespace physics {

/* Depenetrates against the stage's solid cells, every contact classified as
   floor, wall or ceiling, one combined recovery per pass, then snaps to the
   floor. Writes is_grounded, floor_distance and the velocity responses
   into the movement, and sends locomotion into a fall when the floor runs
   out. Runs after the movement has moved the body. */
void collide(Character2D *character);

}
}

}

#endif
