#ifndef ENGINE64_CHARACTER2D_H
#define ENGINE64_CHARACTER2D_H

#include <stdbool.h>
#include <stdint.h>

#include "entity/e64_entity2d.h"
#include "character2d/e64_character2d_movement.h"
#include "character2d/e64_character2d_animation.h"
#include "character2d/e64_character2d_physics.h"

namespace e64 {

typedef struct Stage2D Stage2D;
namespace character2d { struct Def; }


class Character2D {
public:

	const character2d::Def *def;

	/* The scene entity this character draws through. The character writes
	   its frame, flip and position from create to destroy, and puts the
	   entity's own sprite back when it goes. */
	Entity2D        *entity;
	struct sprite_s *entity_sprite;

	/* The stage the body collides with, handed over by the scene once it
	   is loaded. */
	const Stage2D *stage;

	Vector2 position;      /* feet, in pixels; float so movement stays smooth */
	bool    facing_left;

	character2d::Movement  movement;
	character2d::Animation animation;

	void updateMovement(character2d::MovementCommand *cmd, float dt);
};


namespace character2d {

typedef struct Def {

	const MovementSettings *movement_settings;
	const AnimationDef     *animation_def;
	const ColliderSettings *collider_settings;

} Def;


Character2D *create(const Def *def, Entity2D *entity);
void destroy(Character2D *character);

/* Advances the animation off the movement and writes the frame to the
   entity, anchored at the feet and snapped to whole pixels. */
void update(Character2D *character, float dt);

}

}

#endif
