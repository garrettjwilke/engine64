/*
	Track animation over the widgets of the interface. A track names the
	widget it writes and which of its fields, so the animation is data: it
	holds no addresses and survives the interface being loaded again.
*/
#include <stddef.h>

#include "ui/e64_ui_animation.h"
#include "ui/e64_ui.h"
#include "math/e64_math.h"
#include "menu/e64_menu.h"

namespace e64 {

namespace uiAnimation {

/* The three kinds a field can be written as; one of them is set. */
typedef struct {

	float *as_float;
	uint8_t *as_u8;
	bool *as_bool;

} FieldRef;

static FieldRef field(UI *ui, const UIAnimation::Track *track)
{
	Widget *w = ui::getWidget(ui, track->layer, track->entity);
	Graphic *g = &w->graphic;

	switch (track->field) {

		case UIAnimation::FIELD_POSITION_X: return (FieldRef){ .as_float = &w->position.x };
		case UIAnimation::FIELD_POSITION_Y: return (FieldRef){ .as_float = &w->position.y };
		case UIAnimation::FIELD_SCALE_X: return (FieldRef){ .as_float = &w->scale.x };
		case UIAnimation::FIELD_SCALE_Y: return (FieldRef){ .as_float = &w->scale.y };
		case UIAnimation::FIELD_ROTATION: return (FieldRef){ .as_float = &w->rotation };

		case UIAnimation::FIELD_TRANSPARENCY: return (FieldRef){ .as_u8 = &g->transparency };
		case UIAnimation::FIELD_TEXT_STYLE: return (FieldRef){ .as_u8 = &g->text.style };
		case UIAnimation::FIELD_SPRITE_FRAME: return (FieldRef){ .as_u8 = &g->sprite.frame };

		case UIAnimation::FIELD_COLOR_R: return (FieldRef){ .as_u8 = &g->rectangle.color.r };
		case UIAnimation::FIELD_COLOR_G: return (FieldRef){ .as_u8 = &g->rectangle.color.g };
		case UIAnimation::FIELD_COLOR_B: return (FieldRef){ .as_u8 = &g->rectangle.color.b };
		case UIAnimation::FIELD_COLOR_A: return (FieldRef){ .as_u8 = &g->rectangle.color.a };

		case UIAnimation::FIELD_GRADIENT_R: return (FieldRef){ .as_u8 = &g->rectangle.gradient[track->corner].r };
		case UIAnimation::FIELD_GRADIENT_G: return (FieldRef){ .as_u8 = &g->rectangle.gradient[track->corner].g };
		case UIAnimation::FIELD_GRADIENT_B: return (FieldRef){ .as_u8 = &g->rectangle.gradient[track->corner].b };
		case UIAnimation::FIELD_GRADIENT_A: return (FieldRef){ .as_u8 = &g->rectangle.gradient[track->corner].a };

		case UIAnimation::FIELD_HIDDEN: return (FieldRef){ .as_bool = &g->is_hidden };
	}

	return (FieldRef){};
}

static void write(const FieldRef *ref, float value)
{
	if (ref->as_float) *ref->as_float = value;
	if (ref->as_u8) *ref->as_u8 = (uint8_t)value;
}

static bool sourceIndex(const UIAnimation::Track *track, int8_t *index)
{
	switch (track->source) {
		case UIAnimation::SOURCE_MENU_INDEX: *index = menu::getIndex(); return true;
	}
	return false;
}


typedef float (*EaseFunction)(float t);

static const EaseFunction ease_function[UIAnimation::EASING_COUNT] = {
	[UIAnimation::EASING_LINEAR] = ease_linear,
	[UIAnimation::EASING_QUAD_IN] = ease_quad_in,
	[UIAnimation::EASING_QUAD_OUT] = ease_quad_out,
	[UIAnimation::EASING_QUAD_IN_OUT] = ease_quad_in_out,
	[UIAnimation::EASING_CUBIC_IN] = ease_cubic_in,
	[UIAnimation::EASING_CUBIC_OUT] = ease_cubic_out,
	[UIAnimation::EASING_CUBIC_IN_OUT] = ease_cubic_in_out,
	[UIAnimation::EASING_EXPO_IN] = ease_expo_in,
	[UIAnimation::EASING_EXPO_OUT] = ease_expo_out,
	[UIAnimation::EASING_EXPO_IN_OUT] = ease_expo_in_out,
};


static float duration(const UIAnimation *animation)
{
	float max = 0.0f;
	for (int i = 0; i < animation->track_count; i++) {
		float end = animation->track[i].delay + animation->track[i].duration;
		if (end > max) max = end;
	}
	return max;
}

/* Backwards there is no stagger: every track leaves at once, from frame 0,
   so the way out is over as soon as the slowest one is. Waiting out the
   delays again would leave the screen half gone for twice as long, which is
   what a state switch hides behind. */
static float reverseDuration(const UIAnimation *animation)
{
	float max = 0.0f;
	for (int i = 0; i < animation->track_count; i++) {
		float d = animation->track[i].duration;
		if (d > max) max = d;
	}
	return max;
}

/* How far along a track that already started is, 1 for a step. */
static float trackTime(const UIAnimation::Track *track, float local)
{
	if (track->duration <= 0.0f) return 1.0f;

	float t = local / track->duration;
	return (t > 1.0f) ? 1.0f : t;
}

static float progress(const UIAnimation::Track *track, float local)
{
	return ease_function[track->easing](trackTime(track, local));
}

/* Backwards is the same motion seen in reverse, so the curve mirrors too:
   what eases in on the way in eases out on the way out. Re-easing forward
   instead would hold the widget still and then snap it. */
static float progressReversed(const UIAnimation::Track *track, float local)
{
	float t = trackTime(track, local);
	return 1.0f - ease_function[track->easing](1.0f - t);
}

static void applyTrack(UI *ui, const UIAnimation::Track *track, float time)
{
	FieldRef ref = field(ui, track);

	int8_t source_index;
	if (track->values_by_index && sourceIndex(track, &source_index)) {
		write(&ref, track->values_by_index[source_index]);
		return;
	}

	if (ref.as_bool) {
		float end = track->delay + track->duration;
		bool in_window = (time >= track->delay) && (track->duration <= 0.0f || time < end);
		*ref.as_bool = in_window ? track->to_bool : track->from_bool;
		return;
	}

	/* A pending track writes nothing: the prime left every target on its
	   start value, so chained fades over one target hold what the last
	   expired track left. */
	float local = time - track->delay;
	if (local < 0.0f) return;

	write(&ref, lerpf(track->from, track->to, progress(track, local)));
}

static void applyTrackReversed(UI *ui, const UIAnimation::Track *track, float time)
{
	FieldRef ref = field(ui, track);

	int8_t source_index;
	if (track->values_by_index && sourceIndex(track, &source_index)) {
		write(&ref, track->values_by_index[source_index]);
		return;
	}

	if (ref.as_bool) {
		/* The widget stays the way the animation left it for as long as it
		   takes to leave; a step holds it to the end. */
		bool in_window = (track->duration <= 0.0f) || (time < track->duration);
		*ref.as_bool = in_window ? track->to_bool : track->from_bool;
		return;
	}

	write(&ref, lerpf(track->to, track->from, progressReversed(track, time)));
}


void apply(UI *ui, const UIAnimation *animation, float time)
{
	for (int i = 0; i < animation->track_count; i++)
		applyTrack(ui, &animation->track[i], time);
}


namespace player {

static void applyFrame(UIAnimation::Player *player, UI *ui, float time)
{
	const UIAnimation *animation = player->animation;

	if (player->is_reversed) {
		for (int i = 0; i < animation->track_count; i++)
			applyTrackReversed(ui, &animation->track[i], time);
		return;
	}

	for (int i = 0; i < animation->track_count; i++)
		applyTrack(ui, &animation->track[i], time);
}

/* Leaves every lerp target on its start value so pending tracks can stay
   silent. Forward it walks the array backwards, so the chronologically first
   track over a target wins; reversed it walks forwards, since the reverse
   starts from the end state. Live lookups and flags write every frame and
   need no priming. */
static void prime(UIAnimation::Player *player, UI *ui)
{
	const UIAnimation *animation = player->animation;

	for (int i = 0; i < animation->track_count; i++) {
		const UIAnimation::Track *track = player->is_reversed
			? &animation->track[i]
			: &animation->track[animation->track_count - 1 - i];

		if (track->values_by_index) continue;

		FieldRef ref = field(ui, track);
		if (ref.as_bool) continue;

		write(&ref, player->is_reversed ? track->to : track->from);
	}
}


void start(UIAnimation::Player *player, UI *ui, const UIAnimation *animation, UIAnimation::PlayMode mode, bool is_reversed)
{
	player->animation = animation;
	player->mode = mode;
	player->time = 0.0f;
	player->is_active = true;
	player->is_reversed = is_reversed;

	prime(player, ui);
	applyFrame(player, ui, 0.0f);
}

void stop(UIAnimation::Player *player)
{
	player->is_active = false;
}

void update(UIAnimation::Player *player, UI *ui, float dt)
{
	if (!player->is_active || !player->animation) return;

	player->time += dt;
	float total = player->is_reversed
		? reverseDuration(player->animation)
		: duration(player->animation);

	if (player->time < total) {
		applyFrame(player, ui, player->time);
		return;
	}

	switch (player->mode) {

		case UIAnimation::PLAY_ONCE:
			applyFrame(player, ui, total);
			player->is_active = false;
			break;

		case UIAnimation::PLAY_LOOP:
			while (player->time >= total) player->time -= total;
			applyFrame(player, ui, player->time);
			break;

		case UIAnimation::PLAY_PING_PONG:
			while (player->time >= total) player->time -= total;
			player->is_reversed = !player->is_reversed;
			applyFrame(player, ui, player->time);
			break;
	}
}

}

}

}
