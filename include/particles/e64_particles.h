#ifndef ENGINE64_PARTICLES_H
#define ENGINE64_PARTICLES_H

#include <stdbool.h>
#include <stdint.h>
#include <t3d/t3dmath.h>
#include <t3d/tpx.h>

namespace e64 {

class Particle {

public:

	static constexpr uint8_t MAX = 8;

	/* tpx stores particles interleaved in pairs, so a buffer always holds an
	   even count. S8 keeps local coords in one byte (16 bytes per pair) for
	   local effects; S16 covers a larger range (24 bytes per pair) for world
	   placement. */
	enum Type {

		S8,
		S16,

	};

	struct Buffer {

		Type type;
		uint32_t count;

		union {
			TPXParticleS8 *s8;
			TPXParticleS16 *s16;
		};

		T3DMat4FP *matrix; /* one per framebuffer */

	};

	/* Dedicated input function: reads whatever drives the effect and fills
	   visibility and this frame's matrix. Reaches what it reads itself. */
	typedef void (*Update)(Particle *particle, uint8_t fb_index);

	/* rdpq state (combiner, textures) set right before the buffer is drawn. */
	typedef void (*SetRenderState)(void);

	Buffer buffer;
	Update update;
	SetRenderState set_render_state;
	bool textured;
	bool visible;
	const T3DMat4FP *matrix; /* the one written this frame */

};


namespace particles {

namespace buffer {

Particle::Buffer create(Particle::Type type, uint32_t count);
void destroy(Particle::Buffer *buffer);
void setMatrix(Particle::Buffer *buffer, const float scale[3], const float rotation[3], const float position[3], uint8_t fb_index);
void draw(const Particle::Buffer *buffer, const T3DMat4FP *matrix);
void drawTextured(const Particle::Buffer *buffer, const T3DMat4FP *matrix);

}

void init(void);

Particle *add(const Particle *def);
void update(uint8_t fb_index);
void draw(void);

}

}

#endif
