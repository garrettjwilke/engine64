#ifndef ENGINE64_MATH_COMMON_H
#define ENGINE64_MATH_COMMON_H

#define PI 3.141592f
#define PI_TIMES_2 6.283185f

#define TOLERANCE 0.000001f

/* Render units per metre, defined by the build so that the engine, the model
   importer and the collision importer all work off the same number. A power of
   two: scaling by it only moves a float's exponent, so metres and render units
   convert back and forth with nothing lost. Vertices are 16 bit integers,
   which makes the unit the smallest step a vertex can take: 1.56 cm at 64, and
   512 m the most that fits around the origin. */
#ifndef RENDER_SCALE
#define RENDER_SCALE 64.0f
#endif

#define RENDER_SCALE_INV (1.0f / RENDER_SCALE)


/* One-liners are inline: the body is a single float op, a call is not. */

static inline float deg_to_rad(float angle)
{
	return PI / 180 * angle;
}

static inline float rad_to_deg(float rad)
{
	return 180 / PI * rad;
}

static inline float lerpf(float a, float b, float t)
{
	return a + t * (b - a);
}

float angle_wrap(float angle);
float angle_wrap_relative(float angle, float reference);

/*
	Fast inverse square root, Kaze's variant of the Quake III Q_rsqrt.
	Approximates 1/sqrt(x) in ~6 cycles instead of the ~58 that 1.0f / sqrtf(x)
	costs on N64 (29 for the sqrt, 29 for the divide).

	Only worth it when all three hold:
	  1. You need 1/sqrt(x), not sqrt(x). For sqrt use the hardware sqrtf.
	  2. The caller is already in icache. A miss loading the 8 extra
	     instructions kills the gain.
	  3. ~3% error is acceptable. Never in physics, contact normals, raycasts
	     or anything that accumulates.
*/
float qi_sqrt(float x);

float ease_linear(float t);

float ease_quad_in(float t);
float ease_quad_out(float t);
float ease_quad_in_out(float t);

float ease_cubic_in(float t);
float ease_cubic_out(float t);
float ease_cubic_in_out(float t);

float ease_expo_in(float t);
float ease_expo_out(float t);
float ease_expo_in_out(float t);


#endif
