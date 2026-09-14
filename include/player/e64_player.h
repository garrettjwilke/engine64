#ifndef ENGINE64_PLAYER_H
#define ENGINE64_PLAYER_H

#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d_movement.h"
#include "control/e64_character3d_control.h"
#include "control/e64_character2d_control.h"


/* Which kind of body the seat drives. An empty seat is zero, so a player
   nobody took has no body rather than a 3D one that is missing. */
typedef enum {

	PLAYER_CHARACTER_NONE,
	PLAYER_CHARACTER_3D,
	PLAYER_CHARACTER_2D,

} PlayerCharacterType;


typedef struct Player {

	PlayerCharacterType type;

	/* The body and the command that drives it, together: they are the same
	   kind or the seat makes no sense. Which one is live is the type's to
	   say. */
	union {
		struct {
			Character3D    *character;
			MovementCommand cmd;
			const Character3DControlBinding *control;
		} character3d;

		struct {
			Character2D      *character;
			Movement2DCommand cmd;
			const Character2DControlBinding *control;
		} character2d;
	};

	Entity3D *entity;

} Player;


Player *player_get(void);
void player_init(void);
/* Seats a player: the body it drives and the buttons that drive it, together.
   Seating one kind of body leaves the seat driving that kind and no other. */
void player_setCharacter3D(Character3D *character, const Character3DControlBinding *control);
void player_setCharacter2D(Character2D *character, const Character2DControlBinding *control);
void player_switchCharacter3D(PlayerID id, int8_t direction);
void player_update(void);
void player_setMatrix(uint8_t fb_index);

#endif
