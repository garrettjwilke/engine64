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
#include <libdragon.h>

#ifndef E64_PROFILE
#define E64_PROFILE 1
#endif

/* The two printers, each one on its own. They are not meant to run together:
   printing costs CPU inside the frame, so with both on each one is measuring
   the other.

   E64_PROFILE_BLOCK is the E64PROF block, the averages over a window.
   E64_PROFILE_PINS is the E64PIN burst, the frame cut into stretches. */
#ifndef E64_PROFILE_BLOCK
#define E64_PROFILE_BLOCK 0
#endif

#ifndef E64_PROFILE_PINS
#define E64_PROFILE_PINS 1
#endif

/* The RDP runs behind the CPU, so the cycles a pin sees are work handed over
   earlier, not the work of the stretch it closes. With this on, a pin waits
   for the RDP to drain before reading the counters, and then every stretch
   carries its own cycles: what the 3D scene costs the RDP, what the particles
   cost, what the 2D costs.

   It serializes the CPU and the RDP, so the frame gets slower while it is on.
   It is a diagnosis run, not a mode to play in. */
#ifndef E64_PROFILE_PINS_SYNC
#define E64_PROFILE_PINS_SYNC 1
#endif

namespace e64 {

class Profiler {
public:

	/* One per stage of game::runStep. Adding one here and a name in the
	   table is the whole cost of profiling a new stage. */
	enum Slot {
		SOUND,
		TIME,
		INPUT,
		UPDATE,
		RENDER,

		/* Inside the update, one per step of Character3D::setAnimation,
		   summed over every character the frame animates. They are a part of
		   UPDATE, not extra time; and the blend is a part of the graph in the
		   same way. */
		ANIM_PARAMS,
		ANIM_GRAPH,
		ANIM_BLEND,
		ANIM_MODIFIERS,
		ANIM_SKELETON,

		/* Inside the render: the wait for a free framebuffer, which is the
		   whole of the frame's idle time. Everything the RCP owes shows up
		   here, so the render's own work is what is left after taking it
		   out. */
		WAIT_FB,

		/* Inside the update, the engine's own jobs. The game's update calls
		   them in its own order, so these do not have to add up to the
		   update. */
		PHYSICS,
		CHARACTERS,
		PARTICLES,
		CAMERA,

		SLOT_COUNT,
	};

	/* What the frame hands the RSP, counted as the render emits it. Time says
	   the RSP is full; these say with what. */
	enum Counter {
		PIECES, /* geometry blocks run */
		PARTS, /* vertex loads: one per part of every piece */
		VERTS, /* vertices loaded, over every piece */
		TRIS,
		BONE_PARTS, /* parts carrying a bone matrix of their own */
		PIECES_SKINNED, /* the same, only for pieces bound to a skeleton */
		PARTS_SKINNED,
		VERTS_SKINNED,
		TRIS_SKINNED,
		MATERIAL_SWITCH, /* material blocks run */
		MATRIX_PUSH,
		SKELETON_BIND,

		COUNTER_COUNT,
	};

	/* Occupancy by sampling: neither the RSP nor the RDP keeps a busy-time
	   counter the CPU can read, so a timer looks at both often enough that
	   the proportion of samples that found them working is their share of
	   the frame. It is what tells apart who the frame is waiting for. */
	static constexpr uint32_t SAMPLE_US = 250;

	/* Pins: every BURST_PERIOD frames, BURST_FRAMES frames of them go out and
	   then nothing, so the frames in between pay no printing and the engine
	   settles before the next reading. */
	static constexpr uint32_t BURST_PERIOD = 240; /* four seconds at 60 Hz */
	static constexpr uint32_t BURST_FRAMES = 3;
	static constexpr uint32_t MAX_PINS = 64;

	/* Materials register while the scene loads, long before anyone is
	   listening on the USB, so what they say is held here and printed with
	   every burst. */
	static constexpr int MAX_MATERIALS = 16;

	/* The frame cut into stretches, in the order they run. A pin is dropped
	   at each boundary and what lies between two pins is one stretch: the
	   CPU time it took, and the RDP cycles the counters advanced while it
	   ran.

	   The RDP runs behind, so a stretch's cycles are not the cost of what
	   that stretch drew: they are what the RDP got through during it, which
	   is mostly the work handed over earlier. Over a frame it adds up to the
	   whole, and that is what these are for: where the RDP's time goes, not
	   what each draw costs.

	   A pin is named where it is dropped, with a string literal, so adding
	   one costs a line there and nothing here. See profiler::pin below. */
	struct Pin {
		const char *name;
		uint32_t ticks; /* COP0 ticks since the frame opened */
		uint32_t dp_clock;
		uint32_t dp_buf;
		uint32_t dp_pipe;
		uint32_t dp_tmem;
	};

	struct MaterialLine {
		const char *name;
		uint64_t som;
		uint64_t cc;
		uint32_t flags; /* T3D draw flags: this is where face culling lives */
		uint16_t tex_w, tex_h;
		uint8_t id;
		bool used;
	};

	uint32_t counter[COUNTER_COUNT];

	/* COP0 ticks, at half the CPU clock. Per stage and for the whole frame. */
	uint32_t slot_ticks[SLOT_COUNT];
	uint32_t slot_open[SLOT_COUNT];
	uint32_t frame_ticks;
	uint32_t frame_open;
	uint32_t frame_open_prev; /* tick the frame being closed opened at */

	/* RDP cycles, at 62.5 MHz, read from the hardware counters. */
	uint32_t dp_clock;
	uint32_t dp_pipe;
	uint32_t dp_tmem;

	timer_link_t *sampler;
	uint32_t samples;
	uint32_t rsp_busy;
	uint32_t rdp_busy;

	uint32_t window_frames;
	uint32_t frames;

	/* A frame's pins are held here and printed at its tail, all in one go:
	   doing it as they are dropped would put a debugf in the middle of the
	   stretch being measured. */
	Pin pin[MAX_PINS];
	uint32_t pin_count;
	uint32_t burst_frame; /* frames printed of the burst in progress */
	uint32_t burst_wait; /* frames to the next burst */
	bool burst_open;

	MaterialLine material_line[MAX_MATERIALS];

};


#if E64_PROFILE

namespace profiler {

/* Starts measuring. Once, after game::init. Never called, nothing is
   measured and nothing is printed.

   The window is what the E64PROF block used to average over. That block is
   off for now: what goes out are the pins. */
void init(uint32_t window);

/* A pin marks a point of the frame. Between two pins lies one stretch, and
   what the pin records is where that boundary fell: microseconds from the
   start of the frame, and the four RDP counters as they stood.

   Subtracting one pin's counters from the next gives the RDP cycles of the
   stretch between them. The RDP runs behind the CPU, so those cycles are not
   the cost of what that stretch drew: they are what the RDP got through while
   it ran, which is mostly work handed over earlier. Over a frame they add up
   to the whole, and that is what a pin is for.

   The name is a string literal and not an id, so a pin costs one line where
   it is dropped and nothing here. There is no reason to be sparing.

   Pins print in bursts: every Profiler::BURST_PERIOD frames the module opens
   a burst, prints Profiler::BURST_FRAMES frames of pins and closes it.
   Nothing goes out in between, so the frames between bursts pay no printing
   at all and the engine settles before the next reading. */
void pin(const char *name);

/* What one material asks of the RDP, printed once, when it is registered.
   Time says the RDP is full; this says with what it was programmed, which is
   what decides how many cycles a pixel of that material costs. */
void material(int id, const void *t3d_material);

void stageBegin(Profiler::Slot slot);
void stageEnd(Profiler::Slot slot);

void count(Profiler::Counter counter, uint32_t n);

/* Closes the frame: accumulates the RDP counters, clears them, and prints
   the block when the window is full. At the tail of game::runStep. */
void frame(void);

}

#define E64_PROFILER_BEGIN(slot) e64::profiler::stageBegin(e64::Profiler::slot)
#define E64_PROFILER_END(slot) e64::profiler::stageEnd(e64::Profiler::slot)
#define E64_PROFILER_COUNT(c, n) e64::profiler::count(e64::Profiler::c, (uint32_t)(n))
#define E64_PROFILER_PIN(name) e64::profiler::pin(name)
#define E64_PROFILER_MATERIAL(id, m) e64::profiler::material((int)(id), (m))

#else

namespace profiler {

static inline void init(uint32_t window) { (void)window; }
static inline void frame(void) {}

}

#define E64_PROFILER_BEGIN(slot) ((void)0)
#define E64_PROFILER_END(slot) ((void)0)
#define E64_PROFILER_COUNT(c, n) ((void)0)
#define E64_PROFILER_PIN(name) ((void)0)
#define E64_PROFILER_MATERIAL(id, m) ((void)0)

#endif

}

#endif
