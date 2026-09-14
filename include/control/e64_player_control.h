#ifndef ENGINE64_PLAYER_CONTROL_H
#define ENGINE64_PLAYER_CONTROL_H

#include "e64_character3d_control.h"
#include "e64_character2d_control.h"

typedef struct Viewport Viewport;
typedef struct Player   Player;

/* Reads the buttons this player was seated with and turns them into its
   command for this frame, aimed by the camera. */
void player_setCharacter3DControl(PlayerID id, Viewport *viewport);

/* The same, for a seat driving a 2D body: the screen's own axis needs no
   camera to aim by. */
void player_setCharacter2DControl(PlayerID id);

#endif
