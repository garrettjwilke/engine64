#ifndef ENGINE64_COLOR_H
#define ENGINE64_COLOR_H

#include <libdragon.h>

namespace e64 {

namespace color {

struct HSV {
	float h, s, v, a;
};


color_t lerp(color_t *a, color_t *b, float t);

color_t lerpRGB(color_t *a, color_t *b, float t);

}

}

#endif
