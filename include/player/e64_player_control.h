#ifndef ENGINE64_PLAYER_CONTROL_H
#define ENGINE64_PLAYER_CONTROL_H

#include "controller/e64_controller.h"

namespace e64 {

typedef struct Viewport Viewport;
typedef struct Player Player;


namespace player {

/* Reads the buttons this player was seated with and turns them into its
   command for this frame, aimed by the camera. */
void setCharacter3DControl(PlayerID id, Viewport *viewport);

/* The same, for a seat driving a 2D body: the screen's own axis needs no
   camera to aim by. */
void setCharacter2DControl(PlayerID id);

}

}

#endif
