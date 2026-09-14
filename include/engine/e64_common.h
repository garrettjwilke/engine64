#ifndef ENGINE64_COMMON_H
#define ENGINE64_COMMON_H

/* How many entries a declared array holds. Only works on the array itself, not
   on a pointer to it: a table and the count handed over with it are written
   together, and this is what keeps the two from drifting apart. */
#define E64_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

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

#endif
