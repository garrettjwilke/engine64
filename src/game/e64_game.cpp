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

	/* The game's own inits and game_start (state table, initial state)
	   run after this, from the game's main. */
}

void game_runStep(void)
{
	sound_poll();
	
	time_update();

	controller::poll();

	game_updateState();

	sound_update();

	render();
}

void game_close()
{
	t3d_destroy();
}

}
