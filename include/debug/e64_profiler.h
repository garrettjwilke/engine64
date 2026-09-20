/*
	Frame profiler: where the frame's time goes, printed as text.

	Two sources, both read by the CPU and neither one modelled by an
	emulator. CPU: the COP0 tick counter around each stage of the frame.
	RDP: the DP_CLOCK, DP_PIPE_BUSY and DP_TMEM_BUSY counters, which are the
	only way to see fill rate and texture upload, and which ares always
	reports as zero because it gives the RDP no cost in time.

	Every window of frames it prints one block through debugf: ISViewer under
	an emulator, USB under a SummerCart64. The same text on both sides, with
	a fixed E64PROF prefix so one parser reads either.

	Compiled out whole with -DE64_PROFILE=0: the macros become nothing and
	the module keeps no state.
*/
#ifndef ENGINE64_PROFILER_H
#define ENGINE64_PROFILER_H

#include <stdint.h>

#ifndef E64_PROFILE
#define E64_PROFILE 1
#endif

namespace e64 {

/* One per stage of game_runStep. Adding one here and a name in the table
   is the whole cost of profiling a new stage. */
enum ProfilerSlot {
	PROFILER_SOUND,
	PROFILER_TIME,
	PROFILER_INPUT,
	PROFILER_UPDATE,
	PROFILER_RENDER,
	PROFILER_SLOT_COUNT,
};

#if E64_PROFILE

/* Starts measuring and prints a block every window frames. Once, after
   game_init. Never called, nothing is measured and nothing is printed. */
void profiler_init(uint32_t window);

void profiler_stageBegin(ProfilerSlot slot);
void profiler_stageEnd(ProfilerSlot slot);

/* Closes the frame: accumulates the RDP counters, clears them, and prints
   the block when the window is full. At the tail of game_runStep. */
void profiler_frame(void);

#define E64_PROFILER_BEGIN(slot) e64::profiler_stageBegin(e64::slot)
#define E64_PROFILER_END(slot)   e64::profiler_stageEnd(e64::slot)

#else

static inline void profiler_init(uint32_t window) { (void)window; }
static inline void profiler_frame(void) {}

#define E64_PROFILER_BEGIN(slot) ((void)0)
#define E64_PROFILER_END(slot)   ((void)0)

#endif

}

#endif
