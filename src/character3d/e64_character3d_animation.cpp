#include <assert.h>
#include <math.h>
#include <fmath.h>
#include <libdragon.h>
#include "time/e64_time.h"
#include "entity/e64_entity3d.h"
#include "viewport/e64_viewport.h"
#include "math/e64_math.h"

namespace e64 {
namespace character3d {

/* --- graph shortcuts -------------------------------------------------------- */

T3DAnim *Animation::getClip(uint8_t index)
{
	return animation::getClip(graph,index);
}

void Animation::syncGridClips(const e64::Animation::Node *node, float cols_value, float rows_value)
{
	animation::syncGridClips(graph,node, cols_value, rows_value);
}

void Animation::snapGridFromClip(uint8_t src_clip, uint8_t dst_node_idx)
{
	animation::snapGridFromClip(graph,src_clip, dst_node_idx);
}

void Animation::snapGridFromGrid(uint8_t src_node_idx, float src_dir, uint8_t dst_node_idx)
{
	animation::snapGridFromGrid(graph,src_node_idx, src_dir, dst_node_idx);
}


/* --- shared helpers --------------------------------------------------------- */

/* gait axis: gait i sits at i / (count - 1) */
float Animation::getGaitParam(float speed, const MovementSettings *movement)
{
	uint8_t last = movement->gait_count - 1;
	if (last == 0 || speed <= movement->gait[0].target_speed) return 0.0f;

	for (uint8_t i = 0; i < last; i++) {
		float lo = movement->gait[i].target_speed;
		float hi = movement->gait[i + 1].target_speed;
		if (speed > hi) continue;
		return (i + (speed - lo) / (hi - lo)) / last;
	}
	return 1.0f;
}

/* grid weight over the idle: covers 0 to the first gait */
float Animation::getWalkWeight(float speed, const MovementSettings *movement)
{
	float first = movement->gait[0].target_speed;
	if (first <= 0.0f) return (speed > 0.0f) ? 1.0f : 0.0f;
	if (speed >= first) return 1.0f;
	return speed / first;
}

/* In the air, or about to be: the crouch that starts a jump runs on the ground
   but already belongs to the air layer. */
bool Animation::isAerial(const Character3D &character)
{
	return character.movement.current == MOVEMENT_STATE_FALLING
	    || character.movement.data.jump_timer > 0.0f;
}

/* axis: back 0 | left 1/4 | fwd 2/4 | right 3/4 | back 1
   Shared by every camera-locked grid; at a standstill the direction holds
   whatever its param last carried. */
float Animation::getLockedDirectionWeight(const Character3D &character, uint8_t dir_param)
{
	const KinematicBody *body = &character.body;

	if (body->velocity.x == 0.0f && body->velocity.y == 0.0f)
		return graph->param[dir_param];

	float velocity_yaw = rad_to_deg(fm_atan2f(-body->velocity.x, -body->velocity.y));
	float rel = angle_wrap_relative(velocity_yaw, body->rotation.z) - body->rotation.z;

	return (rel + 180.0f) / 360.0f;
}

/* hands a grid's phase back to the locomotion clips on the way out */
void Animation::snapLocomotionFromGrid(uint8_t src_node_idx, float src_dir)
{
	const e64::Animation::Node *node = &graph->def->node[src_node_idx];

	/* source: the walk-row clip closest to the current weight */
	uint8_t src_col = (uint8_t)(src_dir * (node->cols - 1) + 0.5f);
	if (src_col > node->cols - 1) src_col = node->cols - 1;

	T3DAnim *src = getClip(node->animation[src_col]);
	float src_length = t3d_anim_get_length(src);
	if (src_length <= 0.0f) return;
	float phase = src->time / src_length;

	const uint8_t target[] = {
		def->walk_animation, def->run_animation, def->sprint_animation,
		def->turn_walk_animation, (uint8_t)(def->turn_walk_animation + 1),
		def->turn_run_animation, (uint8_t)(def->turn_run_animation + 1),
	};

	for (unsigned t = 0; t < sizeof(target); t++) {
		T3DAnim *dst = getClip(target[t]);
		t3d_anim_set_time(dst, phase * t3d_anim_get_length(dst));
	}
}

/* The walk row keeps step with the walk clip, the run row with the run clip:
   every clip of a row gets the speed that makes its cycle last as long as
   that clip's, its own length times the clip's cycle rate. Copying the
   clip's speed as it was would only match cycles between clips of equal
   length; a side clip of another length would drift away from the centre
   column while it played, and any phase carried between the two would land
   out of step. */
void Animation::setGridRowSpeeds(const e64::Animation::Node *node)
{
	const T3DAnim *walk = getClip(def->walk_animation);
	const T3DAnim *run = getClip(def->run_animation);

	float walk_length = t3d_anim_get_length(walk);
	float run_length = t3d_anim_get_length(run);
	if (walk_length <= 0.0f || run_length <= 0.0f) return;

	float walk_rate = walk->speed / walk_length;
	float run_rate = run->speed / run_length;

	for (uint8_t r = 0; r < node->rows; r++) {
		float rate = (r == 0) ? walk_rate : run_rate;
		for (uint8_t c = 0; c < node->cols; c++) {
			T3DAnim *clip = getClip(node->animation[r * node->cols + c]);
			t3d_anim_set_speed(clip, t3d_anim_get_length(clip) * rate);
		}
	}
}


/* --- idle ------------------------------------------------------------------- */

/* the idle profile is the footing itself: it holds because the footing does */
void Animation::setIdleRightParam()
{
	graph->param[ANIMATION_PARAM_IDLE_RIGHT] =
		def->settings->action_idle_max_blending_ratio * footing;
}


/* --- walk ------------------------------------------------------------------- */

/* Footing wave: 0 on the left plant, 1 on the right one, eased through the
   cycle. The plant phases come from the asset's settings. */
float Animation::getLocomotionPhase(const AnimationSettings *settings, float clip_time, float clip_length)
{
	float left = settings->footing_left;
	float right = settings->footing_right;

	/* rises left plant -> right plant, and falls at ONE rate across the wrap
	   back to the left plant: no anchor at the cycle seam, so asymmetric
	   plants keep the wave speed continuous */
	float phase = clip_time / clip_length;
	float f;
	if (phase <= left) f = (left - phase) / (1.0f - right + left);
	else if (phase <= right) f = (phase - left) / (right - left);
	else f = 1.0f - (phase - right) / (1.0f - right + left);
	if (f > 0.9999999f) f = 0.9999999f;
	if (f < 0.0000001f) f = 0.0000001f;
	return f;
}

float Animation::getTurningAvg(const AnimationSettings *settings, float current_yaw, float previous_yaw)
{
	float delta_yaw = current_yaw - previous_yaw;
	if (delta_yaw > 180.0f) delta_yaw -= 360.0f;
	if (delta_yaw <= -180.0f) delta_yaw += 360.0f;

	/* Left as written on purpose: libdragon builds with -ffast-math, so the
	   divide by the constant count is already a multiply, and the modulo by a
	   constant compiles to shifts and adds, not a DIV (checked in the
	   assembly). Nothing to gain by hand. */
	turn_avg[turn_avg_idx] = delta_yaw;
	turn_avg_idx = (turn_avg_idx + 1) % ANIMATION_TURN_AVG_COUNT;

	float sum = 0.0f;
	for (int i = 0; i < ANIMATION_TURN_AVG_COUNT; i++) sum += turn_avg[i];
	float avg_delta_yaw = sum / ANIMATION_TURN_AVG_COUNT;

	float r = avg_delta_yaw / settings->turn_max_angle;

	if (r > 1.0f) r = 1.0f;
	if (r < -1.0f) r = -1.0f;
	if (fabsf(r) < 0.001f) r = 0.0f;
	return r * settings->turn_max_weight;
}

/* The gait axis freezes while the idle takes over: braking would sweep the
   raw value through every gait with the grid still visible. On resuming the
   walk it lerps back to the live value at the gait's own response rate.

   Moving, the axis heads for the higher of the gait the speed is at and the
   gait the stick asks for. Setting off, the stick wins and a run does not
   pass through the walk while the body picks up; braking, the speed wins
   and the axis comes down with the body, never ahead of it. The clips are
   paced on the real speed either way, so the feet hold.

   Returned, not stored: ANIMATION_PARAM_WALK_GAIT has to keep last frame's
   value until the locomotion phase has re-phased its grid on it. */
float Animation::getGaitAxis(const Character3D &character, float delta)
{
	const MovementSettings *movement = character.movement.settings;
	float speed = character.movement.data.horizontal_speed;

	float raw_gait = getGaitParam(speed, movement);
	float prev_gait = graph->param[ANIMATION_PARAM_WALK_GAIT];

	uint8_t state = character.movement.current;
	if (!character3d::movement::isLocomotion(state)) state = character.movement.locomotion;

	if (getWalkWeight(speed, movement) == 0.0f)
		return raw_gait;

	if (state == MOVEMENT_STATE_IDLE)
		return prev_gait;

	uint8_t last = movement->gait_count - 1;
	float gait = character.movement.data.gait;
	if (gait > (float)last) gait = (float)last;

	float asked_gait = (last > 0) ? gait / (float)last : 0.0f;
	float target_gait = (asked_gait > raw_gait) ? asked_gait : raw_gait;

	float factor = fm_expf(-movement->gait[(uint8_t)gait].response_rate * delta);
	return prev_gait * factor + target_gait * (1.0f - factor);
}

/* the footing is read from the clip that is actually running: the center
   column of the row the gait sits on. The row comes from the previous
   frame's value, because the clips of a row the axis just reached are only
   brought into phase further down — read now they still hold the time they
   were left at, and the footing jumps for one frame. */
void Animation::setFooting(Character3D &character)
{
	float prev_gait = graph->param[ANIMATION_PARAM_WALK_GAIT];

	const e64::Animation::Node *locomotion = &graph->def->node[def->locomotion_node];
	float row_t;
	uint8_t row = animation::blendSegment(prev_gait, locomotion->rows, &row_t);
	if (row_t > 0.5f) row++;

	T3DAnim *base = getClip(locomotion->animation[row * locomotion->cols + locomotion->cols / 2]);

	/* The footing follows the clip only while the body moves: stopped, the
	   clip's phase is whatever it froze at, and the idle already settled on
	   the foot it had. Everyone downstream reads the held value. */
	if (character.movement.data.horizontal_speed > 0.0f)
		footing = getLocomotionPhase(def->settings, base->time, t3d_anim_get_length(base));
}

void Animation::setJumpFootingSpeed(Character3D &character)
{
	if (!isAerial(character)) return;

	float jump = graph->param[ANIMATION_PARAM_JUMP_L] + graph->param[ANIMATION_PARAM_JUMP_R];
	float factor = def->settings->jump_footing_speed * (1.0f - jump);
	if (factor < 0.0f) factor = 0.0f;

	getClip(def->walk_animation)->speed *= factor;
	getClip(def->run_animation)->speed *= factor;
	getClip(def->sprint_animation)->speed *= factor;
	getClip(def->turn_walk_animation)->speed *= factor;
	getClip(def->turn_walk_animation + 1)->speed *= factor;
	getClip(def->turn_run_animation)->speed *= factor;
	getClip(def->turn_run_animation + 1)->speed *= factor;
}

/* the grid runs at the cycle rate the current gait asks for: real speed over
   target speed, and every clip gets the speed that makes its cycle last that long */
void Animation::setLocomotionSpeed(Character3D &character, float gait)
{
	const e64::Animation::Node *node = &graph->def->node[def->locomotion_node];
	const MovementSettings *movement = character.movement.settings;
	uint8_t center = node->cols / 2;

	float t;
	uint8_t row = animation::blendSegment(gait, node->rows, &t);

	float low = t3d_anim_get_length(getClip(node->animation[row * node->cols + center]));
	float high = t3d_anim_get_length(getClip(node->animation[(row + 1) * node->cols + center]));
	float length = low + t * (high - low);

	float target = movement->gait[row].target_speed
	             + t * (movement->gait[row + 1].target_speed - movement->gait[row].target_speed);

	if (length <= 0.0f || target <= 0.0f) return;

	float scale = (character.movement.data.horizontal_speed / target) / length;

	for (int i = 0; i < node->cols * node->rows; i++) {
		T3DAnim *clip = getClip(node->animation[i]);
		t3d_anim_set_speed(clip, t3d_anim_get_length(clip) * scale);
	}

	setJumpFootingSpeed(character);
}

void Animation::setLocomotionParam(Character3D &character, float gait)
{
	const e64::Animation::Node *node = &graph->def->node[def->locomotion_node];
	const MovementSettings *movement = character.movement.settings;
	float speed = character.movement.data.horizontal_speed;

	float turning = getTurningAvg(def->settings, character.body.rotation.z, character.movement.data.previous_yaw);

	/* turn axis: 0 left, 0.5 straight, 1 right */
	float turn = (turning + 1.0f) * 0.5f;

	syncGridClips(node, turn, gait);

	graph->param[ANIMATION_PARAM_WALK] = getWalkWeight(speed, movement);
	graph->param[ANIMATION_PARAM_WALK_GAIT] = gait;
	graph->param[ANIMATION_PARAM_WALK_TURN] = turn;
}


/* --- strafe ----------------------------------------------------------------- */

float Animation::getStrafeDirectionWeight(const Character3D &character, float delta)
{
	const AnimationSettings *s = def->settings;
	const KinematicBody *body = &character.body;
	float *param = graph->param;

	if (body->velocity.x == 0.0f && body->velocity.y == 0.0f)
		return param[ANIMATION_PARAM_STRAFE_DIR];

	float velocity_yaw = rad_to_deg(fm_atan2f(-body->velocity.x, -body->velocity.y));
	float rel = angle_wrap_relative(velocity_yaw, body->rotation.z) - body->rotation.z;

	/* axis:    back 0 | back_l 1/6 | strafe_l 2/6 | fwd 3/6 | strafe_r 4/6 | back_r 5/6 | back 1
	   anchors: fwd 0º, strafe ±90º, back_l/back_r ±90º on the back side, back ±180º
	   the 1/6-2/6 and 4/6-5/6 stretches are never reached by direction: they are the hip turn */
	float raw;
	if (rel < -90.0f) raw = (rel + 180.0f) / 90.0f * (1.0f / 6.0f);
	else if (rel < 0.0f) raw = (2.0f + (rel + 90.0f) / 90.0f) * (1.0f / 6.0f);
	else if (rel < 90.0f) raw = (3.0f + rel / 90.0f) * (1.0f / 6.0f);
	else raw = (5.0f + (rel - 90.0f) / 90.0f) * (1.0f / 6.0f);

	if (param[ANIMATION_PARAM_STRAFE] == 0.0f) return raw;

	float out = param[ANIMATION_PARAM_STRAFE_DIR];

	bool front_raw = (raw >= 2.0f / 6.0f && raw <= 4.0f / 6.0f);
	bool front_out = (out >= 2.0f / 6.0f && out <= 4.0f / 6.0f);

	if (!strafe_turning) {
		if (front_raw == front_out) return raw;
		strafe_turning = true;
	}

	/* ends 0 and 1 of the axis are the same clip: if the target is more than
	   half the axis away, the shortest path crosses the seam */
	if (raw - out > 0.5f) raw -= 1.0f;
	if (out - raw > 0.5f) raw += 1.0f;

	/* exponential lerp toward the live weight, released once it lands */
	float factor = fm_expf(-s->strafe_turn_rate * delta);
	out = out * factor + raw * (1.0f - factor);

	if (fabsf(out - raw) < 0.001f) {
		out = raw;
		strafe_turning = false;
	}

	if (out < 0.0f) out += 1.0f;
	if (out > 1.0f) out -= 1.0f;

	return out;
}

void Animation::snapStrafeEntry()
{
	const e64::Animation::Node *node = &graph->def->node[def->strafe_node];

	T3DAnim *src = getClip(def->walk_animation);
	float src_length = t3d_anim_get_length(src);
	if (src_length <= 0.0f) return;
	float phase = src->time / src_length;

	for (int c = 0; c < node->cols * node->rows; c++) {
		T3DAnim *dst = getClip(node->animation[c]);
		if (dst == src) continue;
		t3d_anim_set_time(dst, phase * t3d_anim_get_length(dst));
	}
}

void Animation::snapStrafeExit()
{
	const e64::Animation::Node *node = &graph->def->node[def->strafe_node];

	/* source: the walk-row clip closest to the current weight, already active */
	float out = graph->param[ANIMATION_PARAM_STRAFE_DIR];
	uint8_t src_col = (uint8_t)(out * (node->cols - 1) + 0.5f);
	if (src_col > node->cols - 1) src_col = node->cols - 1;

	T3DAnim *src = getClip(node->animation[src_col]);
	float src_length = t3d_anim_get_length(src);
	if (src_length <= 0.0f) return;
	float phase = src->time / src_length;

	const uint8_t target[] = {
		def->walk_animation, def->run_animation, def->sprint_animation,
		def->turn_walk_animation, (uint8_t)(def->turn_walk_animation + 1),
		def->turn_run_animation, (uint8_t)(def->turn_run_animation + 1),
	};

	for (unsigned t = 0; t < sizeof(target); t++) {
		T3DAnim *dst = getClip(target[t]);
		t3d_anim_set_time(dst, phase * t3d_anim_get_length(dst));
	}
}

void Animation::setStrafeParams(Character3D &character, float delta)
{
	const MovementData *data = &character.movement.data;
	const MovementSettings *movement = character.movement.settings;
	float *param = graph->param;

	/* Aiming owns the pose while either of its flags is up: the free strafe
	   bows out entirely, so its exit can never hand the locomotion a phase
	   gone stale while its grid sat frozen underneath. */
	/* The air keeps the grid up: the jump and landing layers sit on top of
	   it, and the landing comes back down onto the same strafe it left. */
	uint8_t current = character.movement.current;
	bool strafing = data->strafe
		&& !data->aiming && !data->charging_shoot
		&& (character3d::movement::isLocomotion(current) || current == MOVEMENT_STATE_FALLING)
		&& data->horizontal_speed > 0.0f;

	/* the strafe grid takes over the locomotion one gradually; the ramp is
	   only worth computing while it is up or on its way */
	float prev_blend = strafe_blend;
	float blend = 0.0f;
	if (strafing || prev_blend > 0.0f) {
		float factor = fm_expf(-def->settings->strafe_blend_rate * delta);
		blend = strafing ? 1.0f - (1.0f - prev_blend) * factor : prev_blend * factor;
		if (blend > 0.999f) blend = 1.0f;
		if (blend < 0.001f) blend = 0.0f;
	}

	if (blend == 0.0f) {
		if (prev_blend > 0.0f) snapStrafeExit();
		strafe_blend = 0.0f;
		param[ANIMATION_PARAM_STRAFE] = 0.0f;
		strafe_turning = false;
		return;
	}

	if (prev_blend == 0.0f)
		snapStrafeEntry();

	const e64::Animation::Node *node = &graph->def->node[def->strafe_node];
	setGridRowSpeeds(node);

	float dir = getStrafeDirectionWeight(character, delta);

	float gait = (data->horizontal_speed - movement->gait[0].target_speed)
	           / (movement->gait[1].target_speed - movement->gait[0].target_speed);
	if (gait < 0.0f) gait = 0.0f;
	if (gait > 1.0f) gait = 1.0f;

	syncGridClips(node, dir, gait);

	float weight = getWalkWeight(data->horizontal_speed, movement);

	strafe_blend = blend;
	param[ANIMATION_PARAM_STRAFE] = weight * blend;
	param[ANIMATION_PARAM_STRAFE_DIR] = dir;
	param[ANIMATION_PARAM_STRAFE_GAIT] = gait;

	param[ANIMATION_PARAM_WALK] = weight * (1.0f - blend);
}

void Animation::setStrafeLockedParams(Character3D &character, float delta)
{
	const MovementData *data = &character.movement.data;
	const MovementSettings *movement = character.movement.settings;
	float *param = graph->param;

	bool locked = data->strafe_locked
		&& character3d::movement::isLocomotion(character.movement.current)
		&& data->horizontal_speed > 0.0f;

	float prev_blend = strafe_locked_blend;
	float blend = 0.0f;
	if (locked || prev_blend > 0.0f) {
		float factor = fm_expf(-def->settings->strafe_locked_blend_rate * delta);
		blend = locked ? 1.0f - (1.0f - prev_blend) * factor : prev_blend * factor;
		if (blend > 0.999f) blend = 1.0f;
		if (blend < 0.001f) blend = 0.0f;
	}

	if (blend == 0.0f) {
		if (prev_blend > 0.0f)
			snapLocomotionFromGrid(def->strafe_locked_node,
			                       param[ANIMATION_PARAM_STRAFE_LOCKED_DIR]);
		strafe_locked_blend = 0.0f;
		param[ANIMATION_PARAM_STRAFE_LOCKED] = 0.0f;
		return;
	}

	if (prev_blend == 0.0f)
		snapGridFromClip(def->walk_animation, def->strafe_locked_node);

	const e64::Animation::Node *node = &graph->def->node[def->strafe_locked_node];
	setGridRowSpeeds(node);

	float dir = getLockedDirectionWeight(character, ANIMATION_PARAM_STRAFE_LOCKED_DIR);

	float gait = (data->horizontal_speed - movement->gait[0].target_speed)
	           / (movement->gait[1].target_speed - movement->gait[0].target_speed);
	if (gait < 0.0f) gait = 0.0f;
	if (gait > 1.0f) gait = 1.0f;

	syncGridClips(node, dir, gait);

	float weight = getWalkWeight(data->horizontal_speed, movement);

	strafe_locked_blend = blend;
	param[ANIMATION_PARAM_STRAFE_LOCKED] = weight * blend;
	param[ANIMATION_PARAM_STRAFE_LOCKED_DIR] = dir;
	param[ANIMATION_PARAM_STRAFE_LOCKED_GAIT] = gait;

	param[ANIMATION_PARAM_WALK] *= (1.0f - blend);
	param[ANIMATION_PARAM_STRAFE] *= (1.0f - blend);
}


/* --- jump ------------------------------------------------------------------- */

void Animation::syncLandToJump()
{
	const AnimationSettings *j = def->settings;
	T3DAnim *jump_l = getClip(def->jump_animation);
	T3DAnim *jump_r = getClip(def->jump_animation + 1);
	float land_t = getClip(def->land_animation)->time;
	float jump_t;

	if (land_t < j->land_anim_crouch)
		jump_t = (land_t / j->land_anim_crouch) * j->jump_anim_crouch;
	else
		jump_t = (1.0f - (land_t - j->land_anim_crouch) / (j->land_anim_stand - j->land_anim_crouch)) * j->jump_anim_crouch;

	if (jump_t < 0.0f) jump_t = 0.0f;
	if (jump_t > j->jump_anim_crouch) jump_t = j->jump_anim_crouch;

	t3d_anim_set_time(jump_l, jump_t);
	t3d_anim_set_time(jump_r, jump_t);
}

void Animation::snapToJump()
{
	T3DAnim *jump_l = getClip(def->jump_animation);
	T3DAnim *jump_r = getClip(def->jump_animation + 1);
	T3DAnim *land_animation = getClip(def->land_animation);

	T3DAnim *fall_l = getClip(def->fall_animation);
	T3DAnim *fall_r = getClip(def->fall_animation + 1);

	t3d_anim_set_playing(jump_l, true);
	t3d_anim_set_playing(jump_r, true);

	t3d_anim_set_time(fall_l, 0.0f);
	t3d_anim_set_time(fall_r, 0.0f);

	/* Jumping straight out of a landing: the take-off starts at the crouch
	   depth the landing is already holding, so the pose does not jump. With no
	   landing running there is nothing to match and it starts from the top. */
	if (land_animation->isPlaying) {
		syncLandToJump();
	} else {
		t3d_anim_set_time(jump_l, 0.0f);
		t3d_anim_set_time(jump_r, 0.0f);
	}

	graph->param[ANIMATION_PARAM_JUMP_L] = 0.0f;
	graph->param[ANIMATION_PARAM_JUMP_R] = 0.0f;
}

void Animation::snapToLand()
{
	T3DAnim *land_l = getClip(def->land_animation);
	T3DAnim *land_r = getClip(def->land_animation + 1);
	t3d_anim_set_time(land_l, 0.0f);
	t3d_anim_set_time(land_r, 0.0f);
	t3d_anim_set_playing(land_l, true);
	t3d_anim_set_playing(land_r, true);
	graph->param[ANIMATION_PARAM_LAND_L] = 0.0f;
	graph->param[ANIMATION_PARAM_LAND_R] = 0.0f;
}

/* Falling with no crouch behind it — off a ledge, or a roll that ran out of
   ground. The take-off clip never played, so the sequence is sent straight to
   the falling one by marking it done. */
void Animation::snapToFall()
{
	T3DAnim *jump_l = getClip(def->jump_animation);
	T3DAnim *jump_r = getClip(def->jump_animation + 1);
	T3DAnim *fall_l = getClip(def->fall_animation);
	T3DAnim *fall_r = getClip(def->fall_animation + 1);

	t3d_anim_set_playing(jump_l, false);
	t3d_anim_set_playing(jump_r, false);
	t3d_anim_set_time(fall_l, 0.0f);
	t3d_anim_set_time(fall_r, 0.0f);
}

void Animation::setJumpParams(Character3D &character, float delta)
{
	const AnimationSettings *j = def->settings;
	T3DAnim *land_animation = getClip(def->land_animation);
	uint8_t cur = character.movement.current;
	uint8_t *as = &action_state;
	float *param = graph->param;

	float jump = param[ANIMATION_PARAM_JUMP_L] + param[ANIMATION_PARAM_JUMP_R];
	float land = param[ANIMATION_PARAM_LAND_L] + param[ANIMATION_PARAM_LAND_R];

	/* One owner for the air layer: the crouch on the ground opens it and the
	   fall keeps it. Entering with a crouch plays the take-off clip; entering
	   without one starts on the falling clip. */
	bool aerial = isAerial(character);

	if (aerial && *as != MOVEMENT_STATE_FALLING) {
		if (character.movement.data.jump_timer > 0.0f)
			snapToJump();
		else
			snapToFall();

		jump = 0.0f;
		*as  = MOVEMENT_STATE_FALLING;
	}

	if (*as == MOVEMENT_STATE_FALLING && !aerial)
		*as = cur;

	/* The landing starts one clip-to-contact away from the floor, measured by
	   the fall probe: the foot meets the ground on the frame the clip has it
	   touching, whatever the drop was. */
	float floor_distance = character.movement.data.floor_distance;
	float fall_speed = -character.body.velocity.z;

	if (aerial && !land_animation->isPlaying && floor_distance >= 0.0f && fall_speed > 0.0f
	    && floor_distance <= fall_speed * j->land_anim_ground) {
		snapToLand();
		land = 0.0f;
	}

	/* The landing weight is full on the contact frame, so the pose at touchdown
	   is the landing clip alone. Until then the falling layer drains at the same
	   rate, otherwise it keeps its full weight against the landing all the way
	   down. */
	if (land_animation->isPlaying) {
		if (land_animation->time < j->land_anim_ground) {
			float ground_rate = j->jump_max_blending_ratio * delta / j->land_anim_ground;
			land += ground_rate;
			if (land > j->jump_max_blending_ratio) land = j->jump_max_blending_ratio;
			jump -= ground_rate;
			if (jump < 0.0f) jump = 0.0f;
		} else if (land_animation->time < j->land_anim_crouch) {
			land = j->jump_max_blending_ratio;
		} else {
			float stand_rate = j->jump_max_blending_ratio * delta / (j->land_anim_length - j->land_anim_crouch);
			land -= stand_rate;
			if (land < 0.0f) {
				land = 0.0f;
				t3d_anim_set_playing(land_animation, false);
				t3d_anim_set_playing(getClip(def->land_animation + 1), false);
			}
		}

	}

	/* The air layer only grows while no landing is running: once the landing
	   has started it owns the drain above. */
	if (aerial && !land_animation->isPlaying) {
		jump += j->jump_max_blending_ratio * delta / j->jump_anim_crouch;
		if (jump > j->jump_max_blending_ratio) jump = j->jump_max_blending_ratio;
	}
	/* Back on the ground the air layer drains on its own. Tied to the landing
	   clip it left a remnant, because that clip had already run most of its
	   length during the drop and ended before the weight was gone. */
	else if (!aerial && jump > 0.0f) {
		jump -= j->jump_max_blending_ratio * delta / j->land_anim_crouch;
		if (jump < 0.0f) jump = 0.0f;
	}

	param[ANIMATION_PARAM_JUMP_L] = jump * (1.0f - footing);
	param[ANIMATION_PARAM_JUMP_R] = jump * footing;
	param[ANIMATION_PARAM_LAND_L] = land * (1.0f - footing);
	param[ANIMATION_PARAM_LAND_R] = land * footing;
}


/* --- roll ------------------------------------------------------------------- */

/* Weight the roll sheds per second on its way out: the exit ramp runs from the
   stand pose to the end of the clip. */
float Animation::rollExitRate(const AnimationSettings *r)
{
	return 1.0f / (r->run_to_rolling_anim_length - r->run_to_rolling_anim_stand);
}

void Animation::snapRollToLocomotion(bool left)
{
	const AnimationSettings *settings = def->settings;
	const e64::Animation::Node *node = &graph->def->node[def->locomotion_node];

	/* the plant phases are where the footing wave peaks: footing_left is
	   footing 0, footing_right is footing 1. The exit lands short of the
	   plant so the leg is still reaching for it. */
	float phase = left ? settings->footing_right : settings->footing_left;
	phase -= settings->run_to_rolling_anim_lead;
	if (phase < 0.0f) phase += 1.0f;

	for (int i = 0; i < node->cols * node->rows; i++) {
		T3DAnim *clip = getClip(node->animation[i]);
		t3d_anim_set_time(clip, phase * t3d_anim_get_length(clip));
	}
}

void Animation::setRollParam(Character3D &character, float delta)
{
	const AnimationSettings *r = def->settings;
	uint8_t cur = character.movement.current;
	uint8_t *as = &action_state;
	float *param = graph->param;

	if (cur != MOVEMENT_STATE_ROLLING) {
		if (*as == MOVEMENT_STATE_ROLLING) *as = cur;

		/* Cut short by a ledge: the clip never reached its own exit ramp, so
		   the weight is drained here at that same rate instead of dropping
		   the pose in one frame. */
		float ratio = param[ANIMATION_PARAM_ROLL_RUN];
		if (ratio > 0.0f) {
			ratio -= delta * rollExitRate(r);
			if (ratio < 0.0f) ratio = 0.0f;
		}

		param[ANIMATION_PARAM_ROLL_RUN] = ratio;
		if (ratio == 0.0f) param[ANIMATION_PARAM_ROLL_DIR] = 0.0f;
		return;
	}

	if (*as != MOVEMENT_STATE_ROLLING) {
		uint8_t base = def->roll_animation;
		T3DAnim *roll_l = getClip(base);
		T3DAnim *roll_r = getClip(base + 1);

		t3d_anim_set_playing(roll_l, true);
		t3d_anim_set_time (roll_l, 0.0f);
		t3d_anim_set_playing(roll_r, true);
		t3d_anim_set_time (roll_r, 0.0f);

		float dir = (footing < 0.5f) ? -1.0f : 1.0f;
		param[ANIMATION_PARAM_ROLL_RUN] = 0.0f;
		param[ANIMATION_PARAM_ROLL_DIR] = dir;
		*as = MOVEMENT_STATE_ROLLING;
	}

	float dir = param[ANIMATION_PARAM_ROLL_DIR];
	bool left = dir < 0.0f;

	uint8_t base = def->roll_animation;
	uint8_t roll_idx = left ? base : base + 1;
	float roll_time = getClip(roll_idx)->time;
	float ratio = param[ANIMATION_PARAM_ROLL_RUN];

	if (roll_time < r->run_to_rolling_anim_ground && ratio <= 1.0f)
		ratio += delta / r->run_to_rolling_anim_ground;

	if (roll_time > r->run_to_rolling_anim_stand && ratio > 0.0f)
		ratio -= delta * rollExitRate(r);

	if (ratio > 1.0f) {
		ratio = 1.0f;
		snapRollToLocomotion(left);
	}

	if (ratio < 0.0f) ratio = 0.0f;

	param[ANIMATION_PARAM_ROLL_RUN] = ratio;
}


/* --- swim ------------------------------------------------------------------- */

/* The swim clips run at their own native lengths; blending two strokes of
   different period desyncs the arms mid-blend. Same cure as the locomotion
   grid: the cycle length is interpolated at the blend point and every clip
   gets the speed that makes its cycle last exactly that long. */
void Animation::setSwimSpeed(const e64::Animation::Node *node, float gait)
{
	float t;
	uint8_t col = animation::blendSegment(gait, node->cols, &t);

	float low = t3d_anim_get_length(getClip(node->animation[col]));
	float high = t3d_anim_get_length(getClip(node->animation[col + 1]));
	float length = low + t * (high - low);
	if (length <= 0.0f) return;

	/* one divide for the grid: gcc leaves a div.s per iteration otherwise */
	float rate = 1.0f / length;

	for (int i = 0; i < node->cols * node->rows; i++) {
		T3DAnim *clip = getClip(node->animation[i]);
		t3d_anim_set_speed(clip, t3d_anim_get_length(clip) * rate);
	}
}

/* Swim grid: weight is a timed ramp gated by the SWIMMING state, never the
   raw submersion — the waves oscillate the submerged fraction and would
   jitter the blend. The gait axis crosses idle -> slow -> fast strokes by
   the horizontal speed. While the ramp is up, every land-borne param fades
   with it: the water owns the pose. */
void Animation::setSwimParams(Character3D &character, float delta)
{
	const MovementData *data = &character.movement.data;
	const MovementSettings *movement = character.movement.settings;
	float *param = graph->param;

	bool swimming = character.movement.current == MOVEMENT_STATE_SWIMMING;

	float prev_blend = swim_blend;
	float blend = 0.0f;
	if (swimming || prev_blend > 0.0f) {
		float factor = fm_expf(-def->settings->swim_blend_rate * delta);
		blend = swimming ? 1.0f - (1.0f - prev_blend) * factor : prev_blend * factor;
		if (blend > 0.999f) blend = 1.0f;
		if (blend < 0.001f) blend = 0.0f;
	}

	swim_blend = blend;

	if (blend == 0.0f) {
		param[ANIMATION_PARAM_SWIM] = 0.0f;
		return;
	}

	const e64::Animation::Node *node = &graph->def->node[def->swim_node];

	/* Fading in from nothing: restart the strokes so they enter in phase. */
	if (prev_blend == 0.0f)
		for (int c = 0; c < node->cols * node->rows; c++)
			t3d_anim_set_time(getClip(node->animation[c]), 0.0f);

	float speed = data->horizontal_speed;
	float gait;
	if (speed <= movement->swim_slow_speed)
		gait = 0.5f * speed / movement->swim_slow_speed;
	else
		gait = 0.5f + 0.5f * (speed - movement->swim_slow_speed)
		            / (movement->swim_fast_speed - movement->swim_slow_speed);
	if (gait < 0.0f) gait = 0.0f;
	if (gait > 1.0f) gait = 1.0f;

	setSwimSpeed(node, gait);
	syncGridClips(node, gait, 0.0f);

	param[ANIMATION_PARAM_SWIM] = blend;
	param[ANIMATION_PARAM_SWIM_GAIT] = gait;

	param[ANIMATION_PARAM_WALK] *= (1.0f - blend);
	param[ANIMATION_PARAM_STRAFE] *= (1.0f - blend);
	param[ANIMATION_PARAM_STRAFE_LOCKED] *= (1.0f - blend);
	param[ANIMATION_PARAM_AIMING] *= (1.0f - blend);
	param[ANIMATION_PARAM_AIMING_IDLE] *= (1.0f - blend);
	param[ANIMATION_PARAM_CHARGING_SHOOT] *= (1.0f - blend);
	param[ANIMATION_PARAM_CHARGING_SHOOT_IDLE] *= (1.0f - blend);
	param[ANIMATION_PARAM_JUMP_L] *= (1.0f - blend);
	param[ANIMATION_PARAM_JUMP_R] *= (1.0f - blend);
	param[ANIMATION_PARAM_LAND_L] *= (1.0f - blend);
	param[ANIMATION_PARAM_LAND_R] *= (1.0f - blend);
}


/* --- climb ------------------------------------------------------------------ */

/* Climb layer: same timed ramp as the swim, gated by the CLIMBING state.

   The cycle is timed against distance, not the clock — the hands only land
   on the rungs if one cycle of the clip lasts exactly one rung spacing, so
   the clip speed is the climb speed measured in cycles-worth-of-height per
   second. Stopped on the ladder that speed is zero and the pose freezes
   mid-grip, which is what hanging there looks like.

   The direction only picks which clip the select plays and holds its last
   non-zero value: at a standstill the arms must stay where the last move
   left them rather than snap to a default. */
void Animation::setClimbParams(Character3D &character, float delta)
{
	const MovementSettings *movement = character.movement.settings;
	float *param = graph->param;

	bool climbing = character.movement.current == MOVEMENT_STATE_CLIMBING;

	float prev_blend = climb_blend;
	float blend = 0.0f;
	if (climbing || prev_blend > 0.0f) {
		float factor = fm_expf(-def->settings->climb_blend_rate * delta);
		blend = climbing ? 1.0f - (1.0f - prev_blend) * factor : prev_blend * factor;
		if (blend > 0.999f) blend = 1.0f;
		if (blend < 0.001f) blend = 0.0f;
	}

	climb_blend = blend;

	/* The direction gates the select as well as picking its clip: zeroed off
	   the ladder, the pair stops being stepped every frame for a layer that
	   is contributing nothing. The held direction lives outside the param. */
	if (blend == 0.0f) {
		param[ANIMATION_PARAM_CLIMB] = 0.0f;
		param[ANIMATION_PARAM_CLIMB_DIR] = 0.0f;
		return;
	}

	const e64::Animation::Node *node = &graph->def->node[def->climb_node];

	float velocity = character.body.velocity.z;

	if (velocity > LOCOMOTION_MIN_SPEED) climb_dir = 1.0f;
	if (velocity < -LOCOMOTION_MIN_SPEED) climb_dir = -1.0f;
	if (climb_dir == 0.0f) climb_dir = 1.0f;

	/* The cycle runs on how fast the body is actually moving as a fraction
	   of the speed the climb tops out at: full tilt lands on the clip's own
	   pace and nothing plays it faster, while accelerating into the climb
	   and easing out of it slow the cycle to match. Stopped on the ladder
	   it is zero and the pose holds mid-grip, which is what hanging there
	   looks like.

	   Both clips share the slot, so the one that is not playing has to be
	   kept fed with the same speed: the select hands the time over on a
	   direction change and a stale speed would jump the cycle. */
	float speed = (movement->climb_speed > 0.0f)
		? fabsf(velocity) / movement->climb_speed : 0.0f;

	for (uint8_t i = 0; i < node->cols * node->rows; i++)
		t3d_anim_set_speed(getClip(node->animation[i]), speed);

	param[ANIMATION_PARAM_CLIMB] = blend;
	param[ANIMATION_PARAM_CLIMB_DIR] = climb_dir;

	param[ANIMATION_PARAM_WALK] *= (1.0f - blend);
	param[ANIMATION_PARAM_STRAFE] *= (1.0f - blend);
	param[ANIMATION_PARAM_STRAFE_LOCKED] *= (1.0f - blend);
	param[ANIMATION_PARAM_AIMING] *= (1.0f - blend);
	param[ANIMATION_PARAM_AIMING_IDLE] *= (1.0f - blend);
	param[ANIMATION_PARAM_CHARGING_SHOOT] *= (1.0f - blend);
	param[ANIMATION_PARAM_CHARGING_SHOOT_IDLE] *= (1.0f - blend);
	param[ANIMATION_PARAM_SWIM] *= (1.0f - blend);
	param[ANIMATION_PARAM_JUMP_L] *= (1.0f - blend);
	param[ANIMATION_PARAM_JUMP_R] *= (1.0f - blend);
	param[ANIMATION_PARAM_LAND_L] *= (1.0f - blend);
	param[ANIMATION_PARAM_LAND_R] *= (1.0f - blend);
}


/* --- aim -------------------------------------------------------------------- */

/* Aiming modes: the locked grid twice over, plus an idle of their own per
   mode, so a standstill keeps the weapon up instead of dropping to the bare
   idle. The charge owns the pose while both flags are on: the ready pose
   fades under it and comes back when the shot is let go.

   Entering from plain locomotion the grids inherit the walk cycle's phase;
   hopping between the modes they hand it to each other, and the way out
   returns it, so the feet never skip. */
void Animation::setAimingParams(Character3D &character, float delta)
{
	const MovementData *data = &character.movement.data;
	const MovementSettings *movement = character.movement.settings;
	float *param = graph->param;

	/* Node 0 is the base idle clip: a def without the module leaves these
	   fields zeroed and the whole thing stays out of the graph. */
	if (def->aiming_node == 0) return;

	bool locomotion = character3d::movement::isLocomotion(character.movement.current);
	bool charging = data->charging_shoot && locomotion;
	bool ready = data->aiming && locomotion && !charging;

	float prev_ready = aiming_blend;
	float prev_charging = charging_shoot_blend;

	float ready_blend = 0.0f;
	float charging_blend = 0.0f;

	if (ready || prev_ready > 0.0f) {
		float factor = fm_expf(-def->settings->aiming_blend_rate * delta);
		ready_blend = ready ? 1.0f - (1.0f - prev_ready) * factor : prev_ready * factor;
		if (ready_blend > 0.999f) ready_blend = 1.0f;
		if (ready_blend < 0.001f) ready_blend = 0.0f;
	}

	if (charging || prev_charging > 0.0f) {
		float factor = fm_expf(-def->settings->charging_shoot_blend_rate * delta);
		charging_blend = charging ? 1.0f - (1.0f - prev_charging) * factor : prev_charging * factor;
		if (charging_blend > 0.999f) charging_blend = 1.0f;
		if (charging_blend < 0.001f) charging_blend = 0.0f;
	}

	aiming_blend = ready_blend;
	charging_shoot_blend = charging_blend;

	if (ready_blend == 0.0f && charging_blend == 0.0f) {
		param[ANIMATION_PARAM_AIMING] = 0.0f;
		param[ANIMATION_PARAM_AIMING_IDLE] = 0.0f;
		param[ANIMATION_PARAM_CHARGING_SHOOT] = 0.0f;
		param[ANIMATION_PARAM_CHARGING_SHOOT_IDLE] = 0.0f;
		return;
	}

	/* Entries: the cycle comes from whoever carried it last, and the idle
	   restarts so it never wakes mid-breath. */
	if (prev_ready == 0.0f && ready_blend > 0.0f) {
		if (prev_charging > 0.0f)
			snapGridFromGrid(def->charging_shoot_node,
				param[ANIMATION_PARAM_CHARGING_SHOOT_DIR], def->aiming_node);
		else
			snapGridFromClip(def->walk_animation, def->aiming_node);
		t3d_anim_set_time(getClip(graph->def->node[def->aiming_idle_node].animation[0]), 0.0f);
	}
	if (prev_charging == 0.0f && charging_blend > 0.0f) {
		if (prev_ready > 0.0f)
			snapGridFromGrid(def->aiming_node,
				param[ANIMATION_PARAM_AIMING_DIR], def->charging_shoot_node);
		else
			snapGridFromClip(def->walk_animation, def->charging_shoot_node);
		t3d_anim_set_time(getClip(graph->def->node[def->charging_shoot_idle_node].animation[0]), 0.0f);
	}

	float gait = (data->horizontal_speed - movement->gait[0].target_speed)
	           / (movement->gait[1].target_speed - movement->gait[0].target_speed);
	if (gait < 0.0f) gait = 0.0f;
	if (gait > 1.0f) gait = 1.0f;

	/* Splits each mode between its grid and its idle; only the grids of a
	   mode that weighs something get touched, so the other one's clips can
	   close behind it. */
	float weight = getWalkWeight(data->horizontal_speed, movement);

	if (ready_blend > 0.0f) {
		const e64::Animation::Node *node = &graph->def->node[def->aiming_node];
		float dir = getLockedDirectionWeight(character, ANIMATION_PARAM_AIMING_DIR);
		setGridRowSpeeds(node);
		syncGridClips(node, dir, gait);
		param[ANIMATION_PARAM_AIMING] = weight * ready_blend;
		param[ANIMATION_PARAM_AIMING_IDLE] = (1.0f - weight) * ready_blend;
		param[ANIMATION_PARAM_AIMING_DIR] = dir;
		param[ANIMATION_PARAM_AIMING_GAIT] = gait;
	} else {
		param[ANIMATION_PARAM_AIMING] = 0.0f;
		param[ANIMATION_PARAM_AIMING_IDLE] = 0.0f;
	}

	if (charging_blend > 0.0f) {
		const e64::Animation::Node *node = &graph->def->node[def->charging_shoot_node];
		float dir = getLockedDirectionWeight(character, ANIMATION_PARAM_CHARGING_SHOOT_DIR);
		setGridRowSpeeds(node);
		syncGridClips(node, dir, gait);
		param[ANIMATION_PARAM_CHARGING_SHOOT] = weight * charging_blend;
		param[ANIMATION_PARAM_CHARGING_SHOOT_IDLE] = (1.0f - weight) * charging_blend;
		param[ANIMATION_PARAM_CHARGING_SHOOT_DIR] = dir;
		param[ANIMATION_PARAM_CHARGING_SHOOT_GAIT] = gait;
	} else {
		param[ANIMATION_PARAM_CHARGING_SHOOT] = 0.0f;
		param[ANIMATION_PARAM_CHARGING_SHOOT_IDLE] = 0.0f;
	}

	/* No hand-fading the layers underneath: the stack already dilutes them,
	   and fading twice opens a hole the base idle bleeds through. */

	/* The locomotion underneath stays chained to the aiming cycle the whole
	   time, not just on the way out: it revives mid-fade when the mode
	   drops, and a phase matched every frame gives the crossfade two
	   identical cycles and the exit nothing to correct. */
	bool from_charging = charging_blend >= ready_blend;
	snapLocomotionFromGrid(
		from_charging ? def->charging_shoot_node : def->aiming_node,
		param[from_charging ? ANIMATION_PARAM_CHARGING_SHOOT_DIR : ANIMATION_PARAM_AIMING_DIR]);
}


/* --- per-frame driver ------------------------------------------------------- */

void Animation::setParams(Character3D &character, float delta)
{
	float gait = getGaitAxis(character, delta);

	setFooting (character);
	setLocomotionSpeed (character, gait);

	setLocomotionParam (character, gait);
	setIdleRightParam ();
	setJumpParams (character, delta);
	setRollParam (character, delta);
	setStrafeParams (character, delta);
	setStrafeLockedParams (character, delta);
	setAimingParams (character, delta);
	setSwimParams (character, delta);
	setClimbParams (character, delta);
	animation::setActiveNodes (graph);
}


/* --- lifecycle -------------------------------------------------------------- */

/* The state the designated initializer in character3d::create left at zero
   is right as it is: footing, blends, turn average, action state. The graph
   opens and closes with the mesh. */
void Animation::init(e64::Animation *graph)
{
	this->graph = graph;
}

}


void Character3D::setAnimation()
{
	if (!animation.def) return;

	float delta = time::get()->delta;

	animation.setParams(*this, delta);
	animation::evaluate(animation.graph, delta);
	animation::closeIdleClips(animation.graph);
	skeleton::modifiers::apply(&skeleton_modifiers, &animation.graph->main);
	t3d_skeleton_update(&animation.graph->main);
}

}
