#include <assert.h>

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3danim.h>

#include "engine/e64_common.h"
#include "time/e64_time.h"
#include "sound/e64_sound.h"
#include "camera/e64_camera.h"
#include "menu/e64_settings.h"
#include "viewport/e64_viewport.h"


static Viewport viewport;


Viewport* viewport_get(void) { return &viewport; }

/* The camera stands in the world, so it is in metres like everything else in a
   scene, and it has to hand the view over in the units the models are drawn in.
   That is the whole of the conversion on this side. */
void viewport_setPerspectiveCamera()
{
	const Camera *camera = &viewport.camera;

	t3d_viewport_set_projection(
		&viewport.t3d_viewport,
		T3D_DEG_TO_RAD(camera->field_of_view),
		camera->near_clipping * RENDER_SCALE,
		camera->far_clipping  * RENDER_SCALE
	);

	T3DVec3 eye    = {{camera->position.x * RENDER_SCALE, camera->position.y * RENDER_SCALE, camera->position.z * RENDER_SCALE}};
	T3DVec3 target = {{camera->target.x   * RENDER_SCALE, camera->target.y   * RENDER_SCALE, camera->target.z   * RENDER_SCALE}};
	T3DVec3 up     = {{0, 0, 1}};
	t3d_viewport_look_at(&viewport.t3d_viewport, &eye, &target, &up);
}

void viewport_setIsometricCamera()
{
	t3d_viewport_set_ortho(
		&viewport.t3d_viewport,
		-640, 640,
		-480, 480,
		viewport.camera.near_clipping,
		viewport.camera.far_clipping
	);

	T3DVec3 eye    = {{viewport.camera.position.x, viewport.camera.position.y, viewport.camera.position.z}};
	T3DVec3 target = {{viewport.camera.target.x, viewport.camera.target.y, viewport.camera.target.z}};
	T3DVec3 up     = {{0, 0, 1}};
	t3d_viewport_look_at(&viewport.t3d_viewport, &eye, &target, &up);
}

/* A width of zero is a viewport nobody gave a screen yet. Needed because
   the swap below closes the open screen first, and closing one that was
   never opened is not allowed. */
static bool viewport_hasScreen(void) { return viewport.mode.width > 0; }

/* Opens the screen the game asked for with the player's choices laid over
   it. Both halves are read here and nowhere else, so a change to either one
   takes the same path.

   Closed and opened again rather than switched in place: libdragon's hot
   switch reuses the buffers it already reserved, so it can only shrink the
   picture, never grow it, and it lives behind the preview flag besides. */
static void viewport_applyMode(void)
{
	const ViewportModeDef *mode     = &viewport.mode;
	const Settings        *settings = settings_get();

	/* The picture's shape and the margin for a television's overscan are
	   settings libdragon also keeps behind that flag, so the screen fills a
	   4:3 frame and nothing is trimmed. */
	resolution_t resolution = {
		.width      = mode->width,
		.height     = mode->height,
		.interlaced = mode->interlaced,
	};

	/* Gamma is a level from 0 to 100 and half of it is neutral: only above
	   that is the correction asked for. Below neutral there is nothing to
	   ask the video hardware, which darkens no picture. */
	gamma_t gamma = settings->gamma > 50 ? GAMMA_CORRECT_DITHER : GAMMA_NONE;

	/* The video hardware's own smoothing pass, off. It works on the coverage
	   each draw leaves behind, and 3D edges leave partial coverage where 2D
	   leaves it whole, so turning it on makes the picture change with
	   whatever happens to be on screen. Wiring it to a setting is a separate
	   job from opening the screen. */
	display_init(
		resolution,
		mode->depth   ? mode->depth   : DEPTH_16_BPP,
		mode->buffers ? mode->buffers : FB_COUNT,
		gamma,
		FILTERS_DISABLED);
}

/* Nothing of the screen and nothing of tiny3d happens here: the engine
   picks no resolution of its own, and a game with no 3D scene never pays
   for the 3D pipeline. */
void viewport_init(void)
{
	viewport.mode = (ViewportModeDef){};

	camera_init(&viewport.camera);
	viewport.fb_index = 0;
}

Vector2 viewport_getScale(void)
{
	return (Vector2){
		viewport.mode.scale_x > 0.0f ? viewport.mode.scale_x : 1.0f,
		viewport.mode.scale_y > 0.0f ? viewport.mode.scale_y : 1.0f,
	};
}

/* Whether the 3D pipeline is up. A view with no width was never opened. */
bool viewport_has3D(void) { return viewport.t3d_viewport.size[0] > 0; }

/* Brought up when a 3D scene is loaded and taken down when it is dropped,
   so the 2D game runs without it. The view is sized against the screen in
   force, so this comes after the mode is set. */
void viewport_open3D(void)
{
	if (viewport_has3D()) return;

	t3d_init((T3DInitParams){});
	viewport.t3d_viewport = t3d_viewport_create_buffered(FB_COUNT);
	t3d_viewport_set_area(&viewport.t3d_viewport, 0, 0, display_get_width(), display_get_height());
}

void viewport_close3D(void)
{
	if (!viewport_has3D()) return;

	t3d_destroy();
	viewport.t3d_viewport = (T3DViewport){};
}

void viewport_setMode(const ViewportModeDef *mode)
{
	assert(mode && mode->width > 0 && mode->height > 0);

	if (viewport_hasScreen()
	 && mode->width      == viewport.mode.width
	 && mode->height     == viewport.mode.height
	 && mode->interlaced == viewport.mode.interlaced
	 && mode->depth      == viewport.mode.depth
	 && mode->buffers    == viewport.mode.buffers) return;

	/* The buffers being handed back are the ones still being drawn into:
	   nothing may be in flight when they are freed. */
	if (viewport_hasScreen()) {
		rspq_wait();
		display_close();
	}

	viewport.mode     = *mode;
	viewport.fb_index = 0;
	viewport_applyMode();
}

void viewport_refreshMode(void)
{
	if (!viewport_hasScreen()) return;

	rspq_wait();
	display_close();
	viewport_applyMode();
}

/* The frame's buffers: what is drawn into until it is shown. */
void viewport_attach(void)
{
	rdpq_attach(display_get(), display_get_zbuf());
}

void viewport_clear(color_t color)
{
	rdpq_clear(color);
}

void viewport_detach(void)
{
	rdpq_detach_show();
}

void viewport_updateCamera(Vector3 *center, const struct Scene3D *scene)
{
	camera_update(&viewport.camera, center, scene, time_get()->delta);
}
