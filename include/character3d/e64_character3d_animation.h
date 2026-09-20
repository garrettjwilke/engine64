#ifndef ENGINE64_CHARACTER3D_ANIMATION_H
#define ENGINE64_CHARACTER3D_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>
#include <t3d/t3dskeleton.h>
#include <t3d/t3danim.h>

namespace e64 {

#define ANIMATION_MAX_LAYERS 24

#define ANIMATION_SLOT_MAIN          0xFF
#define ANIMATION_TURN_AVG_COUNT  5

#define ANIMATION_CLIPS(...) ((const uint8_t[]){ __VA_ARGS__ })


namespace character3d {

typedef enum {

	ANIMATION_PARAM_IDLE_RIGHT,
	ANIMATION_PARAM_WALK,   /* weight of the locomotion grid over the idle */
	ANIMATION_PARAM_WALK_GAIT,   /* position on the gait axis, [0,1] over the movement table */
	ANIMATION_PARAM_WALK_TURN,   /* turn axis: 0 left, 0.5 straight, 1 right */
	ANIMATION_PARAM_STRAFE,
	ANIMATION_PARAM_STRAFE_GAIT,
	ANIMATION_PARAM_STRAFE_DIR,
	ANIMATION_PARAM_STRAFE_LOCKED,
	ANIMATION_PARAM_STRAFE_LOCKED_GAIT,
	ANIMATION_PARAM_STRAFE_LOCKED_DIR,
	ANIMATION_PARAM_JUMP_L,
	ANIMATION_PARAM_JUMP_R,
	ANIMATION_PARAM_LAND_L,
	ANIMATION_PARAM_LAND_R,
	ANIMATION_PARAM_ROLL_RUN,
	ANIMATION_PARAM_ROLL_DIR,
	ANIMATION_PARAM_SWIM,        /* weight of the swim grid; a timed ramp, not the submersion */
	ANIMATION_PARAM_SWIM_GAIT,   /* idle -> slow -> fast axis, from the horizontal speed */
	ANIMATION_PARAM_CLIMB,       /* weight of the climb layer; a timed ramp */
	ANIMATION_PARAM_CLIMB_DIR,   /* negative descends, positive climbs */
	ANIMATION_PARAM_AIMING_IDLE,   /* weight of the weapon-up idle over the base idle */
	ANIMATION_PARAM_AIMING,        /* weight of the weapon-up grid */
	ANIMATION_PARAM_AIMING_GAIT,
	ANIMATION_PARAM_AIMING_DIR,
	ANIMATION_PARAM_CHARGING_SHOOT_IDLE,
	ANIMATION_PARAM_CHARGING_SHOOT,
	ANIMATION_PARAM_CHARGING_SHOOT_GAIT,
	ANIMATION_PARAM_CHARGING_SHOOT_DIR,
	ANIMATION_PARAM_COUNT

} AnimationParam;

typedef enum {

	ANIMATION_NODE_CLIP,
	ANIMATION_NODE_SELECT,
	ANIMATION_NODE_SEQUENCE,
	ANIMATION_NODE_BLEND,
	ANIMATION_NODE_BLEND_2D,
	ANIMATION_NODE_LAYER,

} AnimationNodeType;


typedef struct {

	const char *name;
	uint8_t buffer;
	bool is_looping;

} AnimationClipDef;

/* One node of the graph. The def's node table is walked in index order every
   frame, and that order IS the blend order: each active node pushes its
   layer as it is met, and the layers are then applied first to last, every
   one blended over the result of the ones before it with its own weight.

   Two consequences the table has to be built around:

   1. A layer at full weight replaces the whole pose. Everything blended
      before it is discarded. The full-body grids (locomotion, strafe,
      locked, aiming, swim) reach full weight while they drive the body,
      so they go FIRST; the partial and action layers (jump, land, roll,
      climb) go AFTER every grid, or the grid that happens to be at full
      weight erases them. A jump crouch under a strafe at weight 1 is
      invisible if the strafe node comes later in the table.

   2. A SEQUENCE or SELECT node that only advances a clip into a buffer
      can sit anywhere before the LAYER node that reads that buffer; only
      the LAYER's position decides where the pose lands in the stack.

   The engine does not sort this and cannot: which grid weighs 1 changes
   with the state, and the asset is the one that knows what its layers
   mean. */
typedef struct {

	AnimationNodeType type;
	const uint8_t *animation;
	uint8_t cols;
	uint8_t rows;
	uint8_t buffer;
	uint8_t param_cols;
	uint8_t param_rows;
	uint8_t param_weight;   /* BLEND_2D: weight the composed grid enters the main with */

} AnimationNode;

typedef struct {

	float action_idle_max_blending_ratio;

	/* Phase of the locomotion cycle where each foot plants. They shape the
	   footing wave, the roll exit re-phases the cycle on them, and the
	   asset's footstep sounds fire on the same values. */
	float footing_left;
	float footing_right;

	float turn_max_angle;
	float turn_max_weight;

	float jump_max_blending_ratio;
	float jump_anim_length;
	float jump_anim_crouch;
	float jump_anim_air;
	float jump_footing_speed;
	float land_anim_length;
	float land_anim_ground;
	float land_anim_crouch;
	float land_anim_stand;

	/* Phase the roll exit lands ahead of the plant it re-phases the cycle on:
	   the leg is caught reaching for the ground, not already standing on it. */
	float run_to_rolling_anim_lead;

	float run_to_rolling_anim_ground;
	float run_to_rolling_anim_grip;
	float run_to_rolling_anim_stand;
	float run_to_rolling_anim_length;

	float strafe_turn_rate;
	float strafe_blend_rate;

	float strafe_locked_blend_rate;
	float aiming_blend_rate;
	float charging_shoot_blend_rate;
	float swim_blend_rate;
	float climb_blend_rate;

} AnimationSettings;


typedef struct {

	const AnimationClipDef *clip;
	const AnimationNode *node;
	const AnimationSettings *settings;

	uint8_t clip_count;
	uint8_t node_count;
	uint8_t buffer_count;
	uint8_t walk_animation;
	uint8_t run_animation;
	uint8_t sprint_animation;
	uint8_t turn_walk_animation;
	uint8_t turn_run_animation;
	uint8_t jump_animation;
	uint8_t fall_animation;
	uint8_t land_animation;
	uint8_t roll_animation;
	uint8_t locomotion_node;
	uint8_t strafe_node;
	uint8_t strafe_locked_node;

	/* Aiming modes: an idle node and a 5x2 grid node each. Node 0 is the base
	   idle clip, so 0 here means the character does not aim. Whatever weapon
	   is drawn plays through these same nodes: only one is ever in hand. */
	uint8_t aiming_idle_node;
	uint8_t aiming_node;
	uint8_t charging_shoot_idle_node;
	uint8_t charging_shoot_node;
	uint8_t swim_node;
	uint8_t climb_node;

} AnimationDef;


/* The layer stack one frame of the graph builds, applied onto the main in
   push order. */
struct AnimationBuffer {

	const T3DSkeleton *layer[ANIMATION_MAX_LAYERS];
	float weight[ANIMATION_MAX_LAYERS];
	uint8_t count;

	void addLayer(const T3DSkeleton *skel, float layer_weight);
	void blendLayers(const T3DSkeleton *main) const;
};

/* An aggregate on purpose: no constructor, public data. Character3D builds it
   with a designated initializer, and sound, weapon and aim read its fields
   as they are. The helpers are private, which keeps it an aggregate. */
class Animation {
public:

	const AnimationDef *def;
	const T3DModel *model;
	T3DSkeleton  main;
	T3DSkeleton *buffer;
	T3DAnim     *clip;
	void       **clip_data;       /* RAM copy of each open clip's .sdata, NULL when closed */
	uint8_t     *clip_cooldown;   /* frames since the graph last touched each clip */
	uint8_t     *node_state;
	bool        *node_active;
	float        param[ANIMATION_PARAM_COUNT];

	float        locomotion_cycle;
	float        footing;

	uint8_t      action_state;
	float        turn_avg[ANIMATION_TURN_AVG_COUNT];
	uint8_t      turn_avg_idx;
	bool         strafe_turning;
	float        strafe_blend;
	float        strafe_locked_blend;
	float        aiming_blend;
	float        charging_shoot_blend;
	float        swim_blend;
	float        climb_blend;
	float        climb_dir;   /* last non-zero direction, held while stopped */

	void initGraph(Character3D &character);
	void setParams(Character3D &character, float delta);
	void evaluateGraph(float delta);
	void closeIdleClips();

private:

	/* pure helpers */
	static uint8_t blendSegment(float weight, uint8_t count, float *t);
	static float   getGaitParam(float speed, const MovementSettings *movement);
	static float   getWalkWeight(float speed, const MovementSettings *movement);
	static bool    isAerial(const Character3D &character);
	static uint8_t getGridClips(const AnimationNode *node, float cols_value, float rows_value, uint8_t clip[4]);
	static float   getLocomotionPhase(const AnimationSettings *settings, float clip_time, float clip_length);
	static float   rollExitRate(const AnimationSettings *settings);

	/* clips */
	T3DSkeleton *clipBuffer(uint8_t clip);
	void         closeClip(uint8_t index);
	void         openClip(uint8_t index);
	T3DAnim     *getClip(uint8_t index);

	/* grids */
	void  syncGridClips(const AnimationNode *node, float cols_value, float rows_value);
	void  snapGridFromClip(uint8_t src_clip, uint8_t dst_node_idx);
	void  snapLocomotionFromGrid(uint8_t src_node_idx, float src_dir);
	void  setGridRowSpeeds(const AnimationNode *node);
	void  snapGridFromGrid(uint8_t src_node_idx, float src_dir, uint8_t dst_node_idx);
	float getLockedDirectionWeight(const Character3D &character, uint8_t dir_param);

	/* locomotion */
	void  setIdleRightParam();
	float getTurningAvg(const AnimationSettings *settings, float current_yaw, float previous_yaw);
	float getGaitAxis(const Character3D &character, float delta);
	void  setFooting(Character3D &character);
	void  setJumpFootingSpeed(Character3D &character);
	void  setLocomotionSpeed(Character3D &character, float gait);
	void  setLocomotionParam(Character3D &character, float gait);

	/* strafe */
	float getStrafeDirectionWeight(const Character3D &character, float delta);
	void  snapStrafeEntry();
	void  snapStrafeExit();
	void  setStrafeParams(Character3D &character, float delta);
	void  setStrafeLockedParams(Character3D &character, float delta);

	/* jump */
	void  syncLandToJump();
	void  snapToJump();
	void  snapToLand();
	void  snapToFall();
	void  setJumpParams(Character3D &character, float delta);

	/* roll */
	void  snapRollToLocomotion(bool left);
	void  setRollParam(Character3D &character, float delta);

	/* swim, climb, aiming */
	void  setSwimSpeed(const AnimationNode *node, float gait);
	void  setSwimParams(Character3D &character, float delta);
	void  setClimbParams(Character3D &character, float delta);
	void  setAimingParams(Character3D &character, float delta);

	/* graph */
	void  setActiveNodes();
	void  updateClip(bool *updated, uint8_t clip, float delta);
};

}


}

#endif
