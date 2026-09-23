#ifndef ENGINE64_LIGHT_H
#define ENGINE64_LIGHT_H

#include <libdragon.h>
#include <t3d/t3dmath.h>

namespace e64 {

namespace light {

/* t3d hands out seven slots and a light takes one whatever its kind, so the
   split between directional and point is the scene's to make. */
constexpr int COUNT = 7;


typedef enum {

	/* An empty slot. Zero on purpose: the set walks the table in order and
	   stops at the first one, so a scene pays only for what it declared. */
	LIGHT_NONE,

	LIGHT_DIRECTIONAL,
	LIGHT_POINT,

} Type;


typedef struct {

	Type type;
	color_t color;

	union {
		/* Where the light comes from; normalised by the init. */
		struct { T3DVec3 direction; } directional;

		/* Where it stands and how far it carries. */
		struct { T3DVec3 position; float size; } point;
	};

} Source;


typedef struct {

	color_t ambient_color;
	Source source[COUNT];

} Def;

}

typedef light::Def Light;


namespace light {

Light *get(void);

/* Copies the scene's declaration into the live lights. */
void init(const Def *def);

/* Hands the lights to t3d, stopping at the first empty slot. */
void set(const Light *source);

}

}

#endif
