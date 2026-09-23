#include <libdragon.h>

#include "time/e64_time.h"

namespace e64 {

namespace time {

static Time t;
static float scale = 1.0f;
static uint32_t last_ticks;


Time *get(void) { return &t; }

void setScale(float new_scale) { scale = new_scale; }

void init(void)
{
	t.counter = 1.0f;
	t.delta = 0.0f;
	t.rate = 0.0f;

	last_ticks = TICKS_READ();
}

void reset(void)
{
	last_ticks = TICKS_READ();
}

void update(void)
{
	uint32_t now = TICKS_READ();
	t.delta = (float)TICKS_DISTANCE(last_ticks, now) / TICKS_PER_SECOND * scale;
	last_ticks = now;

	t.counter += t.delta;
	t.rate = display_get_fps();
}

}

}
