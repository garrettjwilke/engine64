#ifndef ENGINE64_SCENE3D_H
#define ENGINE64_SCENE3D_H

#include "prefab/e64_prefab3d.h"
#include "entity/e64_entity3d.h"
#include "scene3d/e64_lighting.h"
#include "scene3d/e64_fog.h"
#include "camera/e64_camera.h"
#include "camera/e64_spring_arm.h"
#include "physics/shapes/e64_physics_shape.h"
#include "physics/body/e64_rigid_body.h"
#include "physics/collision/e64_collision_mesh.h"
#include "sound/e64_sound.h"

namespace e64 {

#define SCENE_MAX_CHARACTERS 6

#define SCENE_MAX_ENTITIES 64
/* Counts primitives, not entities: one compound collider takes several. */


/* One prefab placed in a scene. A scene's content is an array of these; the
   load walks it in order. A zero scale means identity. */
typedef struct Scene3DPrefab {

	const Prefab3D *prefab;
	Vector3 position;
	Vector3 rotation;
	Vector3 scale;

} Scene3DPrefab;

typedef struct Scene3DDef {

	const LightDef *light;
	const FogDef *fog;
	const camera::Def *camera;
	Vector3 wind;

	const Scene3DPrefab *prefab;
	uint8_t prefab_count;

} Scene3DDef;


typedef struct Scene3D {

	Entity3D *entity[SCENE_MAX_ENTITIES];
	uint8_t entity_count;

	Character3D *character[SCENE_MAX_CHARACTERS];
	uint8_t character3d_count;

} Scene3D;

Scene3D *scene3d_get(void);

/* The physics world the scene loaded its bodies and cloths into. */
PhysicsWorld *scene3d_getPhysics(void);

/* The entities come out in placement order: the scene's entity list reads
   by the same index as the def's prefab list. */
void scene3d_load(const Scene3DDef *def);
void scene3d_clear(void);
void scene3d_unload(void);
/* Collides every character and carries the result to what draws it. Call
   after physics_update, with the frame's buffer index. */
void scene3d_updateCharacters(uint8_t fb_index);

/* The same for everything else the world holds. Call after physics_update,
   with the frame's buffer index. */
void scene3d_updateEntities(uint8_t fb_index);

/* Reads the buttons the scene declared for its camera, settles the camera
   around the point it is given, and hands the result to the projection. The
   point is what the game decides; the rest it never touches. */
void scene3d_updateCamera(const Vector3 *target);

void scene3d_addEntity(Entity3D *entity);
Character3D *scene3d_getCharacter3D(uint8_t index);

/* Pushes one RenderPiece per visible model object into the frame's context.
   The culling runs here, against the frame's frustum, before each mesh is
   read. */
void scene3d_setRenderContext(const Scene3D *scene, RenderContext *ctx, const Viewport *viewport);

}

#endif
