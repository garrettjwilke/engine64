#ifndef ENGINE64_SCENE3D_H
#define ENGINE64_SCENE3D_H

#include "prefab/e64_prefab3d.h"
#include "entity/e64_entity3d.h"
#include "scene3d/e64_light.h"
#include "scene3d/e64_fog.h"
#include "camera/e64_camera3d.h"
#include "camera/e64_spring_arm.h"
#include "physics3d/shapes/e64_physics_shape.h"
#include "physics3d/e64_rigid_body.h"
#include "physics3d/collision/e64_mesh_collider.h"
#include "sound/e64_sound.h"

namespace e64 {

namespace controls { struct Def; }
namespace physics { class World; }

namespace scene3d {

constexpr int MAX_CHARACTERS = 16;
constexpr int MAX_ENTITIES = 64;
/* Counts primitives, not entities: one compound collider takes several. */

/* An entity of the scene as declared: which prefab and where. A scene's
   content is an array of these; the load walks it in order and builds the
   live entity of the same index. A zero scale means identity. */
typedef struct Entity {

	const Prefab3D *prefab;
	Vector3 position;
	Vector3 rotation;
	Vector3 scale;

} Entity;

typedef struct Def {

	const e64::light::Def *light;
	const e64::fog::Def *fog;
	const camera3d::Def *camera;
	Vector3 wind;

	const Entity *entity;
	uint8_t entity_count;

} Def;

}


typedef struct Scene3D {

	Entity3D *entity[scene3d::MAX_ENTITIES];
	uint8_t entity_count;

	Character3D *character[scene3d::MAX_CHARACTERS];
	uint8_t character3d_count;

} Scene3D;


namespace scene3d {

Scene3D *get(void);

/* The physics world the scene loaded its bodies and cloths into. */
physics::World *getPhysics(void);

/* The entities come out in placement order: the scene's entity list reads
   by the same index as the def's prefab list.

   The controls are the state's. A character built from a prefab its binding
   names is seated on that binding's player as it is created, and the camera
   takes its binding. NULL drives nothing. */
void load(const Def *def, const controls::Def *controls);
void clear(void);
void unload(void);

/* Collides every character and carries the result to what draws it. Call
   after physics::update, with the frame's buffer index. */
void updateCharacters(uint8_t fb_index);

/* The same for everything else the world holds. Call after physics::update,
   with the frame's buffer index. */
void updateEntities(uint8_t fb_index);

/* Reads the buttons the scene declared for its camera, settles the camera
   around the point it is given, and hands the result to the projection. The
   point is what the game decides; the rest it never touches. */
void updateCamera(const Vector3 *target);

void addEntity(Entity3D *entity);
Character3D *getCharacter3D(uint8_t index);

/* Pushes one Render::Element3D per visible model object into the frame's
   context. The culling runs here, against the frame's frustum, before each
   mesh is read. */
void setRenderContext(const Scene3D *scene, Render::Context *ctx, const Viewport *viewport);

}

}

#endif
