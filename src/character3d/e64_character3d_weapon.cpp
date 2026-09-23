/*
	Weapon slots, equip and bone posing. Weapon meshes live inside the
	character model, skinned to dedicated root-level bones; posing those bones
	parks the weapon on its holster reference bone or on the hand.
*/
#include <assert.h>
#include <t3d/t3dskeleton.h>

#include "math/e64_quaternion.h"

#include "entity/e64_entity3d.h"
#include "character3d/e64_character3d.h"

namespace e64 {

namespace character3d {
namespace weapon {

void equip(Character3D *character, uint8_t slot_id, const WeaponDef *weapon)
{
	assert(slot_id < WEAPON_SLOT_COUNT);

	Weapons *weapons = &character->weapons;
	T3DSkeleton *skeleton = &character->animation.graph.main;

	uint8_t part = mesh::part::find(character->entity->mesh, weapon->mesh);
	assert(part); /* weapon mesh must exist in the character model */

	weapons->slot[slot_id] = (WeaponSlot){
		.weapon = weapon,
		.rounds = weapon->magazine_size,
		.integrity = weapon->max_integrity,
		.part = part,
		.bone = (int16_t)t3d_skeleton_find_bone(skeleton, (char *)weapon->bone),
		.holster_bone = (int16_t)t3d_skeleton_find_bone(skeleton, (char *)weapon->holster_bone),
		.hand_bone = (int16_t)t3d_skeleton_find_bone(skeleton, (char *)weapon->hand_bone),
	};

	mesh::part::setVisible(character->entity->mesh, part, true);
}

void unequip(Character3D *character, uint8_t slot_id)
{
	assert(slot_id < WEAPON_SLOT_COUNT);

	WeaponSlot *slot = &character->weapons.slot[slot_id];
	if (!slot->weapon) return;

	mesh::part::setVisible(character->entity->mesh, slot->part, false);
	if (character->weapons.drawn == slot_id)
		character->weapons.drawn = CHARACTER3D_WEAPON_DRAWN_NONE;

	*slot = (WeaponSlot){};
}

const WeaponDef *drawn(const Character3D *character)
{
	const Weapons *weapons = &character->weapons;
	if (weapons->drawn == CHARACTER3D_WEAPON_DRAWN_NONE) return NULL;
	return weapons->slot[weapons->drawn].weapon;
}

/* The ring is: unarmed, then every occupied slot in order. Empty slots are
   stepped over, so with one weapon carried both directions just toggle it
   in and out of the hand. */
void cycle(Character3D *character, int8_t dir)
{
	Weapons *weapons = &character->weapons;

	int8_t pos = (weapons->drawn == CHARACTER3D_WEAPON_DRAWN_NONE) ? -1 : (int8_t)weapons->drawn;

	for (int i = 0; i < WEAPON_SLOT_COUNT + 1; i++) {
		pos += dir;
		if (pos > WEAPON_SLOT_COUNT - 1) pos = -1;
		if (pos < -1) pos = WEAPON_SLOT_COUNT - 1;
		if (pos == -1) break;
		if (weapons->slot[pos].weapon) break;
	}

	weapons->drawn = (pos < 0) ? CHARACTER3D_WEAPON_DRAWN_NONE : (uint8_t)pos;
}

void setBones(Character3D *character)
{
	Weapons *weapons = &character->weapons;
	T3DSkeleton *skeleton = &character->animation.graph.main;

	for (int s = 0; s < WEAPON_SLOT_COUNT; s++) {
		WeaponSlot *slot = &weapons->slot[s];
		if (!slot->weapon || slot->bone < 0) continue;

		bool drawn = (weapons->drawn == s);
		int16_t reference = drawn ? slot->hand_bone : slot->holster_bone;
		const T3DVec3 *offset_pos = drawn ? &slot->weapon->holding_position : &slot->weapon->holster_position;
		const T3DQuat *offset_rot = drawn ? &slot->weapon->holding_rotation : &slot->weapon->holster_rotation;
		if (reference < 0) continue;

		T3DVec3 ref_pos;
		T3DQuat ref_rot;
		skeleton::getBonePose(skeleton, reference, &ref_pos, &ref_rot);

		/* T3DQuat and T3DVec3 are laid out like the math module's types. */
		Vector3 step = quaternion::rotateVector((const Quaternion *)&ref_rot, (const Vector3 *)offset_pos);

		T3DBone *bone = &skeleton->bones[slot->bone];
		bone->position = (T3DVec3){{
			ref_pos.v[0] + step.x,
			ref_pos.v[1] + step.y,
			ref_pos.v[2] + step.z,
		}};
		t3d_quat_mul(&bone->rotation, &ref_rot, (T3DQuat *)offset_rot);
		bone->hasChanged = 1;
	}
}

}
}

}
