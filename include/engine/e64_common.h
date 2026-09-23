#ifndef ENGINE64_COMMON_H
#define ENGINE64_COMMON_H

#include <stddef.h>
#include <stdint.h>

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

namespace e64 {

/* The engine's own file formats are written big-endian, the way the machine
   reads: whoever parses one reads its fields through these. */

inline uint16_t readU16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

inline uint32_t readU32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

inline float readF32(const uint8_t *p)
{
	union { uint32_t bits; float value; } u = { readU32(p) };
	return u.value;
}

/* The next 8 byte boundary: every block of those formats is padded to it. */
inline size_t padded8(size_t n) { return (n + 7) & ~(size_t)7; }

}

#endif
