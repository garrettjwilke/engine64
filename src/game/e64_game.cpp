#include "viewport/e64_viewport.h"
#include "time/e64_time.h"
#include "scene3d/e64_scene3d.h"
#include "render/e64_render.h"
#include "player/e64_player.h"
#include "control/e64_controller.h"
#include "control/e64_player_control.h"
#include "game/e64_game.h"
#include "game/e64_game_states.h"
#include "sound/e64_sound.h"
#include "menu/e64_settings.h"
#include "menu/e64_menu.h"
#include "particles/e64_particles.h"
#include "debug/e64_profiler.h"

namespace e64 {

static Game game;


Game *game_get(void) { return &game; }

void game_init()
{
	asset_init_compression(2);

	dfs_init(DFS_DEFAULT_LOCATION);

	srand(getentropy32());
	
	rdpq_init();
	
	joypad_init();
	
	controller::start();
	
	time_init();

	viewport_init();

	particles_init();

	player::init();

	settings_init();

	menuStack_init();

	/* Measures from the first frame and prints a block per second of play.
	   Costs nothing built with -DE64_PROFILE=0. */
	profiler_init(60);

	/* The game's own inits and game_start (state table, initial state)
	   run after this, from the game's main. */
}

void game_runStep(void)
{
	/* The stages the profiler measures are these, in this order: the two
	   sound calls land in the same slot because they are one job split in
	   two by where the mixer needs to run. */
	E64_PROFILER_BEGIN(PROFILER_SOUND);
	sound_poll();
	E64_PROFILER_END(PROFILER_SOUND);

	E64_PROFILER_BEGIN(PROFILER_TIME);
	time_update();
	E64_PROFILER_END(PROFILER_TIME);

	E64_PROFILER_BEGIN(PROFILER_INPUT);
	controller::poll();
	E64_PROFILER_END(PROFILER_INPUT);

	E64_PROFILER_BEGIN(PROFILER_UPDATE);
	game_updateState();
	E64_PROFILER_END(PROFILER_UPDATE);

	E64_PROFILER_BEGIN(PROFILER_SOUND);
	sound_update();
	E64_PROFILER_END(PROFILER_SOUND);

	E64_PROFILER_BEGIN(PROFILER_RENDER);
	render();
	E64_PROFILER_END(PROFILER_RENDER);

	profiler_frame();
}

void game_close()
{
	t3d_destroy();
}

}
