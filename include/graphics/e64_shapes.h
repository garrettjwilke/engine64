#ifndef ENGINE64_SHAPES_H
#define ENGINE64_SHAPES_H

#include <libdragon.h>
#include "math/e64_vector2.h"

namespace e64 {

class Rectangle {
public:

	enum Fill {
		SOLID,
		GRADIENT,
	};


	Fill fill;
	union {
		color_t color;
		color_t gradient[4];
	};
};


namespace rectangle {

void draw(const Rectangle *rect, Vector2 position, Vector2 scale);

}

}

#endif
