/*
	The tuning constants below are ported from qu3e q3Settings.h — altered
	source, not the original software.

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

#ifndef ENGINE64_PHYSICS_H
#define ENGINE64_PHYSICS_H

#include <stdint.h>

#include "math/e64_math.h"

namespace e64 {
namespace physics {

class World;


constexpr float SLEEP_LINEAR = 0.01f;
constexpr float SLEEP_ANGULAR = (3.0f / 180.0f) * PI;
constexpr float SLEEP_TIME = 0.4f;
constexpr float BAUMGARTE = 0.2f;
constexpr float PENETRATION_SLOP = 0.01f;

constexpr int32_t SOLVER_ITERATIONS = 8;
constexpr float TIMESTEP = 1.0f / 60.0f;

/* Margin added to every AABB in the broadphase tree, metres per side. Sets
   both the pair/wake distance (one margin per body, so twice this between
   surfaces) and how far a body may drift before its leaf is reinserted. */
constexpr float AABB_FATTENER = 0.1f;

/* Longest step a frame may take. Past this the simulation slows down instead
   of integrating a huge dt, which is what keeps a hiccup from exploding it. */
constexpr float MAX_TIMESTEP = 1.0f / 20.0f;

/* Cloths step on their own fixed clock of TIMESTEP: a Verlet cloth's look is
   tuned to its step size. Cap on cloth steps one frame may run; below 60/cap
   FPS the cloth slows down instead of piling up debt. */
constexpr int32_t CLOTH_MAX_SUBSTEPS = 3;


/* Advances the world by the frame's elapsed time, running as many fixed steps
   of dt as fit into it. Call this one, not step, from the game loop: stepping
   once per frame ties the simulation's speed to the framerate. */
void update(World *s, float delta);

void step(World *s);

}
}

#endif
