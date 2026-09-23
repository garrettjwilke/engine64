/*
	State machinery only: the table itself is the game's, handed over at
	game::state::start. The engine loads, unloads and switches whatever it is given.
*/
#include <assert.h>
#include <libdragon.h>

#include "time/e64_time.h"
#include "scene3d/e64_scene3d.h"
#include "scene2d/e64_scene2d.h"
#include "player/e64_player.h"
#include "controller/e64_controls.h"
#include "viewport/e64_viewport.h"
#include "game/e64_game.h"
#include "game/e64_game_states.h"

namespace e64 {

namespace game {

namespace state {

const Game::State::Def *get(Game::State::ID id)
{
	Game::State *s = &game::get()->state;

	assert(id < s->count);
	return &s->table[id];
}

static void load(Game::State::ID id)
{
	const Game::State::Def *def = get(id);

	/* The screen first: the scenes below place cameras against it. */
	if (def->viewport) viewport::setMode(def->viewport);

	/* The scenes take the state's controls: each seats the players and
	   points the camera as it builds what the bindings name. */
	if (def->scene3d) scene3d::load(def->scene3d, def->controls);
	if (def->scene2d) scene2d::load(def->scene2d, def->controls);

	if (def->onEnter) def->onEnter();
}

static void unload(Game::State::ID id)
{
	const Game::State::Def *def = get(id);

	if (def->onExit) def->onExit();
	if (def->scene2d) scene2d::unload();
	if (def->scene3d) {
		player::init();
		scene3d::unload();
	}
}

static bool isOverlayPair(Game::State::ID prev, Game::State::ID next)
{
	return get(next)->overlay_of == get(prev)
	    || get(prev)->overlay_of == get(next);
}

static void settle(Game::State *s)
{
	if (s->next == s->current) return;

	const Game::State::Def *leaving = get(s->current);
	if (leaving->canLeave && !leaving->canLeave()) return;

	Game::State::ID prev = s->current;
	Game::State::ID new_state = s->next;

	/* An overlay rides its base: the 3D world stays untouched and dropping
	   back does not re-enter the base. Only the 2D scene changes hands, and
	   it comes back the way its definition declares it. */
	if (isOverlayPair(prev, new_state)) {
		const Game::State::Def *def = get(new_state);
		s->current = new_state;
		if (def->scene2d) scene2d::load(def->scene2d, def->controls);
		if (def->overlay_of == get(prev) && def->onEnter) def->onEnter();
		return;
	}

	rspq_wait();
	unload(prev);
	/* An abandoned overlay takes its base state down with it. */
	if (get(prev)->overlay_of)
		unload(get(prev)->overlay_of - s->table);
	s->current = new_state;
	load(new_state);

	/* The load blocked for as long as it took: none of it counts as a
	   played frame, or the enter animations would swallow it as one. */
	time::reset();
}


/* Asking to leave is not leaving: the state names where it goes, and the
   switch happens as soon as it lets go. */
void set(Game::State::ID new_state)
{
	Game::State *s = &game::get()->state;

	assert(new_state < s->count);
	s->next = new_state;
}

void start(const Game::State::Def *table, uint8_t count, Game::State::ID initial)
{
	assert(table && initial < count);

	Game::State *s = &game::get()->state;
	s->table = table;
	s->count = count;
	s->current = initial;
	s->next = initial;

	load(initial);
	time::reset();
}

void update(void)
{
	Game::State *s = &game::get()->state;

	get(s->current)->update();
	settle(s);
}

}

}

}
