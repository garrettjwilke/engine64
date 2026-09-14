#ifndef ENGINE64_ENTITY3D_H
#define ENGINE64_ENTITY3D_H

#include "render/e64_render.h"
#include "physics/body/e64_rigid_body.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/cloth/e64_cloth.h"
#include "shaders/e64_water.h"
#include "graphics/e64_mesh.h"
#include "sound/e64_sound.h"
#include "character3d/e64_character3d.h"

typedef struct Entity3D {

	RenderTransform transform;
	Mesh *mesh;

	/* Set when the entity has a rigid body. If that body is simulated, it is
	   what places the mesh each frame instead of the transform above. */
	RigidBody *body;

	/* Test the model's bounding box against the view frustum before drawing
	   it. Off means the entity is drawn every frame, no questions asked. */
	bool cull;

	/* The prefab's sounds, open: one per entry, in the same order, from
	   create to delete. The looping ones play from the entity for as long
	   as it exists; the rest wait for whoever fires them, the character
	   its own, the game the prop's. */
	Sound   *sound;
	uint8_t  sound_count;

} Entity3D;


/* A collider is one or more primitives, each carrying its own offset in its
   .tx. The entity transform and scale apply to all of them, so the group
   stays consistent at any prop size. */
typedef struct Entity3DColliderDef {

	const PhysicsShapeDef *shape;
	uint8_t                count;
	
} Entity3DColliderDef;


typedef struct Entity3DDef {

	const char *model_path;   /* NULL: nothing to draw, a sound placed alone */

	/* Objects of the model the game shows and hides on its own. Left out, the
	   model is drawn whole. */
	const char *const *part;
	uint8_t            part_count;

	/* Where each of those is drawn, in the entity's own space. */
	const Vector3 *part_position;

	const SoundDef *const *sound;
	uint8_t                sound_count;
	Vector3 position;
	Vector3 rotation;
	Vector3 scale;
	const Character3DDef *character;
	const RigidBodyDef      *body;
	const Entity3DColliderDef *collider;
	const ClothDef          *cloth;
	const WaterDef          *water;
	bool cull;

} Entity3DDef;


struct PhysicsWorld;

void entity3d_init(Entity3D *entity, const Entity3DDef *def);
Entity3D *entity3d_create(const Entity3DDef *def);
void entity3d_delete(Entity3D *entity);
void entity3d_setTransform(Entity3D *entity, const KinematicBody *body);
void entity3d_setMatrix(Entity3D *entity, uint8_t fb_index);
void entity3d_setMatrixFromBody(Entity3D *entity, uint8_t fb_index);

/* Shows or hides one of the objects the prefab declared as a part, by the same
   name. Nothing happens when the entity declared no part by that name. */
void entity3d_setPartVisible(Entity3D *entity, const char *name, bool visible);

/* Draws one of those objects away from where it was modelled, without touching
   the rest of the model. The offset is read in the entity's own space and the
   part keeps following the entity. */
void entity3d_setPartOffset(Entity3D *entity, const char *name, const RenderTransform *offset);

/* Fires a trigger on the entity: one of its sounds declared with that trigger
   plays, picked at random. position NULL plays from where the entity is; a
   given one is in render units. Nothing declared, nothing plays. */
void entity3d_playSound(const Entity3D *entity, uint8_t trigger, const Vector3 *position, float volume_scale);

/* Def → physics wiring. The caller owns the destinations. */
Transform entity3d_colliderTransform(const Entity3DDef *def);

/* Builds the entity's body and hangs its collider off it. Without a .body def
   the body comes out static, which is what scenery wants. */
RigidBody *entity3d_attachPhysics(Entity3D *entity, const Entity3DDef *def, struct PhysicsWorld *world);

#endif
