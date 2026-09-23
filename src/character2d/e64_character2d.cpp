/*
	The 2D character: the movement moves it, the animation picks its frame,
	and this hands the frame to the scene entity. The position moves at the
	game's rate and is snapped to whole pixels only when set on the entity,
	so the sprite never lands on a half pixel.
*/
#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <libdragon.h>

#include "character2d/e64_character2d.h"

namespace e64 {

namespace character2d {

/* Hands the scene entity the current frame: its sprite, the flip, and the
   feet position carried to the top-left corner the blit draws from. */
static void setEntity(Character2D *character)
{
	Entity2D *entity = character->entity;
	Sprite *sprite = &entity->graphic->sprite;

	sprite->asset = animation::getSprite(character);
	sprite->cols = 0;
	sprite->rows = 0;
	sprite->frame = 0;
	/* The frames are drawn facing left, so the mirror is the right side. */
	sprite->flip_x = !character->facing_left;

	/* Feet at the bottom center of the frame as drawn: the placement's
	   scale is applied at the blit, so it counts here too.

	   Left in world pixels, fractions and all: the scene rounds once, after
	   the camera, the same as it does with a tile. Rounding here as well
	   would land the body on whole world pixels and then on whole screen
	   ones, and the two together hold it still for a frame and move it two
	   the next. */
	float w = sprite->asset->width * entity->scale.x;
	float h = sprite->asset->height * entity->scale.y;

	entity->position.x = character->position.x - w * 0.5f;
	entity->position.y = character->position.y - h;
}


Character2D *create(const Def *def, Entity2D *entity)
{
	assert(def && entity && entity->graphic->type == Graphic::SPRITE);

	Character2D *character = (Character2D *)malloc(sizeof(Character2D));
	assert(character);

	/* The placement put the feet where the entity stands. */
	*character = (Character2D){
		.def = def,
		.entity = entity,
		.entity_sprite = entity->graphic->sprite.asset,
		.position = entity->position,
		.movement = (Movement){ .settings = def->movement_settings, .data = { .is_grounded = true }, .current = MOVEMENT2D_STATE_IDLE },
	};

	animation::init(character, def->animation_def);
	setEntity(character);
	return character;
}

void destroy(Character2D *character)
{
	if (!character) return;

	/* The entity closes its own sprite when it goes; it gets it back. */
	character->entity->graphic->sprite.asset = character->entity_sprite;

	animation::free(character);
	::free(character);
}

void update(Character2D *character, float dt)
{
	animation::update(character, dt);
	setEntity(character);
}

}

}
