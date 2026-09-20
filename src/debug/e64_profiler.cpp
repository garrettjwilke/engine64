#include <libdragon.h>
#include <rdp.h>

#include "debug/e64_profiler.h"

namespace e64 {

#if E64_PROFILE

static const char *slot_name[PROFILER_SLOT_COUNT] = {
	"sound", "time", "input", "update", "render",
};

/* COP0 ticks, at half the CPU clock. Per stage and for the whole frame. */
static uint32_t slot_ticks[PROFILER_SLOT_COUNT];
static uint32_t slot_open[PROFILER_SLOT_COUNT];
static uint32_t frame_ticks;
static uint32_t frame_open;

/* RDP cycles, at 62.5 MHz, read from the hardware counters. */
static uint32_t dp_clock;
static uint32_t dp_pipe;
static uint32_t dp_tmem;

static uint32_t window_frames;
static uint32_t frames;


static void profiler_reset(void)
{
	for (int i = 0; i < PROFILER_SLOT_COUNT; i++) slot_ticks[i] = 0;

	frame_ticks = 0;
	dp_clock = dp_pipe = dp_tmem = 0;
	frames = 0;
}

void profiler_init(uint32_t window)
{
	window_frames = window ? window : 60;

	profiler_reset();

	/* From here on every read is a delta over one frame: the counters go
	   back to zero at the tail of each one. */
	*DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER
	           | DP_WSTATUS_RESET_TMEM_COUNTER  | DP_WSTATUS_RESET_CMD_COUNTER;

	frame_open = TICKS_READ();
}

void profiler_stageBegin(ProfilerSlot slot)
{
	slot_open[slot] = TICKS_READ();
}

void profiler_stageEnd(ProfilerSlot slot)
{
	slot_ticks[slot] += TICKS_READ() - slot_open[slot];
}

void profiler_frame(void)
{
	uint32_t now = TICKS_READ();
	frame_ticks += now - frame_open;
	frame_open = now;

	/* The RDP runs behind the CPU, so what is read here is not exactly this
	   frame's work: part of it belongs to the previous one. Over a window it
	   evens out, which is why nothing is reported per frame. */
	dp_clock += *DP_CLOCK;
	dp_pipe  += *DP_PIPE_BUSY;
	dp_tmem  += *DP_TMEM_BUSY;

	*DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER
	           | DP_WSTATUS_RESET_TMEM_COUNTER;

	if (++frames < window_frames) return;

	/* Microseconds for the CPU, RDP cycles for the RDP: each one in the
	   unit its own counter speaks. */
	debugf("E64PROF\tframes\t%lu\n", frames);
	debugf("E64PROF\tframe_us\t%lu\n", TIMER_MICROS(frame_ticks) / frames);

	for (int i = 0; i < PROFILER_SLOT_COUNT; i++)
		debugf("E64PROF\tcpu_%s_us\t%lu\n",
		       slot_name[i], TIMER_MICROS(slot_ticks[i]) / frames);

	debugf("E64PROF\tdp_clock\t%lu\n", dp_clock / frames);
	debugf("E64PROF\tdp_pipe_busy\t%lu\n", dp_pipe / frames);
	debugf("E64PROF\tdp_tmem_busy\t%lu\n", dp_tmem / frames);
	debugf("E64PROF\tend\n");

	profiler_reset();
	frame_open = TICKS_READ();
}

#endif

}
