#include "menu/e64_settings.h"

namespace e64 {

namespace settings {

static Settings s;


Settings *get(void) { return &s; }

void init(void)
{
	s = (Settings){
		.master_volume = 80,
		.music_volume = 70,
		.sfx_volume = 80,
		.voice_volume = 80,
		.mute = false,

		.invert_camera_y = false,
		.invert_camera_x = false,
		.camera_max_speed = 50,
		.camera_response = 50,
		.vibration = true,

		.difficulty = Settings::DIFFICULTY_NORMAL,
		.language = Settings::LANGUAGE_EN,
		.subtitles = true,
		.auto_save = true,
		.hud_visible = true,
		.tutorial_hints = true,

		.brightness = 50,
		.contrast = 50,
		.gamma = 50,
		.aspect_ratio = Settings::ASPECT_4_3,
		.anti_aliasing = true,
		.vsync = true,
		.screen_shake = true,
	};
}

void reset(void) { init(); }

}

}
