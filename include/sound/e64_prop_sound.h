#ifndef ENGINE64_PROP_SOUND_H
#define ENGINE64_PROP_SOUND_H

#include <stdint.h>

namespace e64 {

namespace physics { class World; }

namespace propSound {

/* What wakes a sound of a prop. A sound is declared with one of these in its
   def; the update below fires the matching ones. A new event is a new value
   here. */
enum Trigger {

	TRIGGER_COLLISION, /* the body's first frame against anything solid */
	TRIGGER_WATER_ENTRY, /* something fell into this water surface */

};


/* Walks the frame's new contacts: a dynamic body that just met something
   solid fires TRIGGER_COLLISION on its entity, scaled by the speed the
   solver killed; a water surface something just fell into fires
   TRIGGER_WATER_ENTRY on its own entity, from where the body entered,
   scaled by the plunge speed. Call after physics::update. */
void update(physics::World *world);

}

}

#endif
