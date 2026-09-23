#ifndef ENGINE64_PLAYER_H
#define ENGINE64_PLAYER_H

#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d_movement.h"
#include "character3d/e64_character3d_control.h"
#include "character2d/e64_character2d_control.h"

namespace e64 {

namespace player {

/* Which kind of body the seat drives. An empty seat is zero, so a player
   nobody took has no body rather than a 3D one that is missing. */
typedef enum {

	CHARACTER_NONE,
	CHARACTER_3D,
	CHARACTER_2D,

} CharacterType;

}


typedef struct Player {

	player::CharacterType type;

	/* The body and the command that drives it, together: they are the same
	   kind or the seat makes no sense. Which one is live is the type's to
	   say. */
	union {
		struct {
			Character3D *character;
			character3d::MovementCommand cmd;
			const character3d::ControlBinding *control;
		} character3d;

		struct {
			Character2D *character;
			character2d::MovementCommand cmd;
			const character2d::ControlBinding *control;
		} character2d;
	};

	Entity3D *entity;

} Player;


namespace player {

Player *get(void);
void init(void);
/* Seats a player: the body it drives and the buttons that drive it, together.
   Seating one kind of body leaves the seat driving that kind and no other. */
void setCharacter3D(Character3D *character, const character3d::ControlBinding *control);
void setCharacter2D(Character2D *character, const character2d::ControlBinding *control);
void switchCharacter3D(PlayerID id, int8_t direction);
void update(void);
void setMatrix(uint8_t fb_index);

}

}

#endif
