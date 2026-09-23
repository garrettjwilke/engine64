#include "ui/e64_ui.h"
#include "scene2d/e64_scene2d.h"
#include "time/e64_time.h"

namespace e64 {

namespace ui {

static UIAnimation::Player player;


void play(const UIAnimation *animation, bool reversed)
{
	uiAnimation::player::start(&player, scene2d::get(), animation, UIAnimation::PLAY_ONCE, reversed);
}

bool isTransitioning(void)
{
	return player.is_active;
}

void update(const UIAnimation *idle)
{
	uiAnimation::player::update(&player, scene2d::get(), time::get()->delta);

	if (idle) uiAnimation::apply(scene2d::get(), idle, 0.0f);
}

}

}
