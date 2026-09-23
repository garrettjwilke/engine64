#include <libdragon.h>
#include <rdp.h>
#include <rsp.h>
#include <t3d/t3dmodel.h>

#include "debug/e64_profiler.h"

namespace e64 {

#if E64_PROFILE

namespace profiler {

static Profiler p;

/* Only the E64PROF block prints these: a pin carries its own name. */
#if E64_PROFILE_BLOCK
static const char *slot_name[Profiler::SLOT_COUNT] = {
	"sound", "time", "input", "update", "render",
	"anim_params", "anim_graph", "anim_blend", "anim_modifiers", "anim_skeleton",
	"wait_fb",
	"physics", "characters", "particles", "camera",
};

static const char *counter_name[Profiler::COUNTER_COUNT] = {
	"pieces", "parts", "verts", "tris", "bone_parts",
	"pieces_skinned", "parts_skinned", "verts_skinned", "tris_skinned",
	"material_switch", "matrix_push", "skeleton_bind",
};
#endif

/* The fourth RDP counter, which libdragon does not declare. It counts the
   cycles the command buffer was busy, and it is the one that tells apart a
   RDP working from a RDP with a full pipe waiting for something to do. */
#define DP_BUF_BUSY ((volatile uint32_t*)0xA4100014)


void count(Profiler::Counter c, uint32_t n)
{
	p.counter[c] += n;
}

static void printMaterials(void)
{
	for (int i = 0; i < Profiler::MAX_MATERIALS; i++) {
		const Profiler::MaterialLine *l = &p.material_line[i];
		if (!l->used) continue;

		/* Two cycle mode runs the pixel through the pipeline twice, so
		   everything else doubles with it. Blending reads the framebuffer to
		   mix into it, the depth compare reads the z buffer and the write
		   puts it back: each one is another memory access per pixel. */
		int cycles = ((l->som & SOM_CYCLE_MASK) == SOM_CYCLE_2) ? 2 : 1;
		int blend = (l->som & SOM_BLENDING) != 0;
		int zread = (l->som & SOM_Z_COMPARE) != 0;
		int zwrite = (l->som & SOM_Z_WRITE) != 0;
		int aa = (l->som & SOM_AA_ENABLE) != 0;

		/* Face culling: a back face dropped in the RSP never reaches the
		   RDP, so with it off every triangle is paid for twice over. */
		int cull_back = (l->flags & T3D_FLAG_CULL_BACK) != 0;
		int cull_front = (l->flags & T3D_FLAG_CULL_FRONT) != 0;

		debugf("E64MAT\t%d\t%s\tcycles=%d\tblend=%d\tzread=%d\tzwrite=%d\taa=%d"
		       "\tcull_back=%d\tcull_front=%d"
		       "\ttex=%dx%d\tflags=%08lx\tsom=%08lx%08lx\tcc=%08lx%08lx\n",
		       l->id, l->name ? l->name : "?",
		       cycles, blend, zread, zwrite, aa,
		       cull_back, cull_front,
		       l->tex_w, l->tex_h, (unsigned long)l->flags,
		       (unsigned long)(l->som >> 32), (unsigned long)l->som,
		       (unsigned long)(l->cc >> 32), (unsigned long)l->cc);
	}
}

/* Per pixel the RDP covers, what this material makes it do. The RDP charges
   by cycle and not by triangle, so this is what decides whether a small
   object on screen is cheap or is the one eating the frame. */
void material(int id, const void *t3d_material)
{
	const T3DMaterial *m = (const T3DMaterial *)t3d_material;
	if (!m || id < 0 || id >= Profiler::MAX_MATERIALS) return;

	Profiler::MaterialLine *l = &p.material_line[id];

	l->id = (uint8_t)id;
	l->name = m->name;
	l->som = m->otherModeValue;
	l->cc = m->colorCombiner;
	l->flags = m->renderFlags;
	l->tex_w = m->textureA.texWidth;
	l->tex_h = m->textureA.texHeight;
	l->used = true;
}

void pin(const char *name)
{
	if (!E64_PROFILE_PINS || !p.burst_open || p.pin_count >= Profiler::MAX_PINS) return;

	/* Drains the RDP so the counters below hold this stretch's work and not
	   what was still in flight from an earlier one. The wait is inside the
	   measured time on purpose: both sides of a stretch pay it, so the
	   difference between two pins stays clean. */
#if E64_PROFILE_PINS_SYNC
	rspq_wait();
#endif

	Profiler::Pin *pin = &p.pin[p.pin_count++];

	pin->name = name;
	pin->ticks = TICKS_READ() - p.frame_open;
	pin->dp_clock = *DP_CLOCK;
	pin->dp_buf = *DP_BUF_BUSY;
	pin->dp_pipe = *DP_PIPE_BUSY;
	pin->dp_tmem = *DP_TMEM_BUSY;
}


static void sample(int ovfl)
{
	(void)ovfl;

	p.samples++;
	if (!(*SP_STATUS & SP_STATUS_HALTED)) p.rsp_busy++;
	if ( *DP_STATUS & DP_STATUS_PIPE_BUSY) p.rdp_busy++;
}


static void reset(void)
{
	for (int i = 0; i < Profiler::SLOT_COUNT; i++) p.slot_ticks[i] = 0;
	for (int i = 0; i < Profiler::COUNTER_COUNT; i++) p.counter[i] = 0;

	p.frame_ticks = 0;
	p.dp_clock = p.dp_pipe = p.dp_tmem = 0;
	p.samples = p.rsp_busy = p.rdp_busy = 0;
	p.frames = 0;
}

void init(uint32_t window)
{
	p.window_frames = window ? window : 60;

	reset();

	/* From here on every read is a delta over one frame: the counters go
	   back to zero at the tail of each one. */
	*DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER
	           | DP_WSTATUS_RESET_TMEM_COUNTER | DP_WSTATUS_RESET_CMD_COUNTER;

	/* One sampler for the whole run: it keeps ticking across windows and
	   only its counters reset. */
	if (!p.sampler)
		p.sampler = new_timer(TIMER_TICKS(Profiler::SAMPLE_US), TF_CONTINUOUS,
		                      sample);

	/* The first burst counts from here, not from whatever the loader left in
	   the variable. */
	p.burst_wait = Profiler::BURST_PERIOD;
	p.burst_open = false;
	p.burst_frame = 0;
	p.pin_count = 0;

	p.frame_open = TICKS_READ();
	p.frame_open_prev = p.frame_open;
}

void stageBegin(Profiler::Slot slot)
{
	p.slot_open[slot] = TICKS_READ();
}

void stageEnd(Profiler::Slot slot)
{
	p.slot_ticks[slot] += TICKS_READ() - p.slot_open[slot];
}

void frame(void)
{
	uint32_t now = TICKS_READ();
	p.frame_ticks += now - p.frame_open;
	p.frame_open = now;

	/* The RDP runs behind the CPU, so what is read here is not exactly this
	   frame's work: part of it belongs to the previous one. Over a window it
	   evens out, which is why nothing is reported per frame. */
	p.dp_clock += *DP_CLOCK;
	p.dp_pipe += *DP_PIPE_BUSY;
	p.dp_tmem += *DP_TMEM_BUSY;

#if E64_PROFILE_PINS
	/* The frame's pins, printed here and not where each one was dropped: a
	   debugf in the middle of a stretch would be measuring the print. The
	   counters go out raw, in RDP cycles, so the difference between two pins
	   is the stretch between them. */
	if (p.burst_open) {
		debugf("E64PIN\tframe\t%lu\n", (unsigned long)p.burst_frame);
		for (uint32_t i = 0; i < p.pin_count; i++) {
			const Profiler::Pin *pin = &p.pin[i];
			debugf("E64PIN\tpin\t%s\t%lu\t%lu\t%lu\t%lu\t%lu\n",
			       pin->name, (unsigned long)TIMER_MICROS(pin->ticks),
			       (unsigned long)pin->dp_clock, (unsigned long)pin->dp_buf,
			       (unsigned long)pin->dp_pipe, (unsigned long)pin->dp_tmem);
		}
		debugf("E64PIN\tframe_us\t%lu\n",
		       (unsigned long)TIMER_MICROS(now - p.frame_open_prev));

		if (++p.burst_frame >= Profiler::BURST_FRAMES) {
			debugf("E64PIN\tburst\tend\n");
			p.burst_open = false;
			p.burst_wait = Profiler::BURST_PERIOD;
		}
	} else if (p.burst_wait > 0 && --p.burst_wait == 0) {
		/* Opened at the tail of a frame, so the first frame with pins is the
		   next one and no burst ever starts half way through a frame.

		   Counting down with a guard and not with a bare decrement: a counter
		   that starts at zero would wrap to four billion on the first frame
		   and the burst would never open. */
		debugf("E64PIN\tburst\tbegin\n");
		printMaterials();
		p.burst_open = true;
		p.burst_frame = 0;
	}

	p.pin_count = 0;
	p.frame_open_prev = now;
#endif

	/* The command counter goes back to zero with the rest, so a pin's
	   DPC_BUSY is also measured from the start of the frame. */
	*DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER
	           | DP_WSTATUS_RESET_TMEM_COUNTER | DP_WSTATUS_RESET_CMD_COUNTER;

	if (++p.frames < p.window_frames) return;

#if E64_PROFILE_BLOCK
	/* Microseconds for the CPU, RDP cycles for the RDP: each one in the
	   unit its own counter speaks. */
	debugf("E64PROF\tframes\t%lu\n", p.frames);
	debugf("E64PROF\tframe_us\t%lu\n", TIMER_MICROS(p.frame_ticks) / p.frames);

	for (int i = 0; i < Profiler::SLOT_COUNT; i++)
		debugf("E64PROF\tcpu_%s_us\t%lu\n",
		       slot_name[i], TIMER_MICROS(p.slot_ticks[i]) / p.frames);

	debugf("E64PROF\tdp_clock\t%lu\n", p.dp_clock / p.frames);
	debugf("E64PROF\tdp_pipe_busy\t%lu\n", p.dp_pipe / p.frames);
	debugf("E64PROF\tdp_tmem_busy\t%lu\n", p.dp_tmem / p.frames);

	/* Raw counts: the share of the frame each one was busy is its count over
	   the samples taken. */
	/* What the frame drew, per frame. */
	for (int i = 0; i < Profiler::COUNTER_COUNT; i++)
		debugf("E64PROF\tn_%s\t%lu\n", counter_name[i], p.counter[i] / p.frames);

	debugf("E64PROF\tsamples\t%lu\n", p.samples);
	debugf("E64PROF\trsp_busy\t%lu\n", p.rsp_busy);
	debugf("E64PROF\trdp_busy\t%lu\n", p.rdp_busy);
	debugf("E64PROF\tend\n");
#endif

	reset();
	p.frame_open = TICKS_READ();
}

}

#endif

}
