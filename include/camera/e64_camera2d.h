/*
	The 2D camera: where the screen looks into the world and how much of it
	fits, on the model of Godot's Camera2D.

	What a prop takes of its movement is that prop's own parallax, so one
	camera carries the ground and the backdrops behind it at once.
*/
#ifndef ENGINE64_CAMERA2D_H
#define ENGINE64_CAMERA2D_H

#include <stdbool.h>

#include "physics/math/e64_vector2.h"

namespace e64 {

#define CAMERA2D_SIDE_COUNT 4

/* The screen comes from the display mode in force, which changes while the
   game runs: display_get_width and display_get_height, asked every time,
   never a size taken once. */

#define CAMERA2D_ZOOM_MIN 0.25f
#define CAMERA2D_ZOOM_MAX 4.0f


namespace camera2d {

typedef enum {

	CAMERA2D_TYPE_NONE,
	CAMERA2D_TYPE_FOLLOW,
	CAMERA2D_TYPE_COUNT,

} Type;


/* Where the camera's position sits on the screen: at its middle, or at the
   top left corner. */
typedef enum {

	CAMERA2D_ANCHOR_CENTER,
	CAMERA2D_ANCHOR_TOP_LEFT,

} Anchor;


typedef enum { CAMERA2D_SIDE_LEFT, CAMERA2D_SIDE_TOP, CAMERA2D_SIDE_RIGHT, CAMERA2D_SIDE_BOTTOM } Side;


typedef struct FollowSettings {

	/* The box the target moves inside without the camera answering, per side,
	   as a fraction of half the screen. Each axis is enabled on its own: with
	   the drag off, the offset below places the centre instead. */
	float drag_margin[CAMERA2D_SIDE_COUNT];
	bool  drag_horizontal;
	bool  drag_vertical;

	/* Where the target sits inside the margins when the drag is off, -1 to 1.
	   Written as the body turns, this is the look ahead. */
	float drag_offset_x;
	float drag_offset_y;

	/* How fast the view reaches where it was asked to be, per second. Zero
	   plants it there on the frame it is asked. */
	float position_smoothing_speed;

	/* Whether the world's edges are met before the smoothing or after it:
	   before, the view eases into the border; after, it stops dead on it. */
	bool limit_smoothing;

	/* How fast the view turns toward the rotation it is given. Zero snaps. */
	float rotation_smoothing_speed;

} FollowSettings;


typedef struct FollowData {

	/* Where the view was asked to be, and where the smoothing has it. The
	   first frame plants both on the target: easing in from wherever the
	   camera was declared would sweep the whole world once. */
	Vector2 target_position;
	bool    settled;

	float rotation;
	float target_rotation;

} FollowData;


typedef struct Def {

	Type type;

	/* World pixels: where it starts looking, and what the anchor above says
	   that point is on the screen. */
	Vector2 position;
	Anchor  anchor;

	/* World pixels to a screen pixel: above 1 the view closes in and shows
	   less. Zero means 1. */
	float zoom;

	/* Screen pixels added after everything else, the shake and the framing
	   nudge: it moves the picture without moving where the camera looks. */
	Vector2 offset;

	/* The world's edges in world pixels, left, top, right, bottom. The view
	   is pushed inside them; a world narrower than the screen centres on
	   that axis. Off leaves the camera free. */
	bool  limit_enabled;
	float limit[CAMERA2D_SIDE_COUNT];

	/* Turning the view costs a rotated blit, so it stays off unless asked. */
	bool rotate;

	FollowSettings follow;

} Def;

}


typedef struct Camera2D {

	camera2d::Type   type;
	camera2d::Anchor anchor;

	/* World pixels, at the anchor. */
	Vector2 position;
	float   zoom;
	Vector2 offset;

	bool  limit_enabled;
	float limit[CAMERA2D_SIDE_COUNT];

	bool  rotate;
	float rotation;

	/* Half the screen in world pixels: what carries the anchor to the corner
	   the blit draws from. Written with the zoom, so what draws never
	   divides. */
	Vector2 extent;

	camera2d::FollowSettings settings;
	camera2d::FollowData     data;

} Camera2D;


namespace camera2d {

void init(Camera2D *camera, const Def *def);
void setZoom(Camera2D *camera, float zoom);

/* Chases the target, which is where the body stands in the world. Facing is
   which way it looks, -1 or +1, and is what the look ahead reads. */
void update(Camera2D *camera, Vector2 target, float facing, float dt);

/* Where a world point lands on the screen for something that takes this much
   of the camera's movement: 1 sits in the world, 0 ignores it. */
Vector2 toScreen(const Camera2D *camera, Vector2 position, float parallax);

/* The other way around, for whoever asks what part of the world a corner of
   the screen reaches: the terrain walks its grid off this. */
Vector2 toWorld(const Camera2D *camera, Vector2 screen);

}

}

#endif
