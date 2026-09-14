#ifndef ENGINE64_CHARACTER2D_H
#define ENGINE64_CHARACTER2D_H

#include <stdbool.h>
#include <stdint.h>

#include "entity/e64_entity2d.h"
#include "character2d/e64_character2d_movement.h"
#include "character2d/e64_character2d_animation.h"
#include "character2d/e64_character2d_physics.h"

typedef struct Stage2D Stage2D;


typedef struct Character2DDef {

	const Character2DMovementSettings  *movement_settings;
	const Character2DAnimationDef      *animation_def;
	const Character2DColliderSettings  *collider_settings;

} Character2DDef;

typedef struct Character2D {

	const Character2DDef *def;

	/* The scene entity this character draws through. The character writes
	   its frame, flip and position from create to delete, and puts the
	   entity's own sprite back when it goes. */
	Entity2D        *entity;
	struct sprite_s *entity_sprite;

	/* The stage the body collides with, handed over by the scene once it
	   is loaded. */
	const Stage2D *stage;

	Vector2 position;      /* feet, in pixels; float so movement stays smooth */
	bool    facing_left;

	Character2DMovement  movement;
	Character2DAnimation animation;

} Character2D;


Character2D *character2d_create(const Character2DDef *def, Entity2D *entity);
void character2d_delete(Character2D *character);

/* Advances the animation off the movement and writes the frame to the
   entity, anchored at the feet and snapped to whole pixels. */
void character2d_update(Character2D *character, float dt);

#endif
