/*
	Everything the engine loads from the ROM goes through here: models,
	sprites, fonts and waves. One table of what is open right now, found by
	path. Two owners asking for the same file get the same pointer, and the
	file closes when the last of them lets go.

	The owner keeps the pointer it was handed, the way an entity keeps its
	mesh, and gives it back when it dies. Nothing else needs to know the
	table exists.
*/
#ifndef ENGINE64_RESOURCE_H
#define ENGINE64_RESOURCE_H

#include <stdint.h>

namespace e64 {

class Resource {

public:

	/* Files open at once, across every scene up at the same time. A 2D
	   character opens one file per frame and a stage one per tile it uses,
	   so a 2D scene alone runs into the hundreds. */
	static constexpr uint16_t MAX = 512;

	/* What kind of file a path is, which picks how it opens and closes. */
	enum Type {

		MODEL, /* T3DModel */
		SPRITE, /* sprite_t */
		FONT, /* rdpq_font_t, still to be registered by whoever asked */
		WAVE, /* wav64_t */

	};

	const char *path; /* NULL = free slot */
	void *data;
	Type type;
	uint8_t users;

};


namespace resource {

/* Hands back the loaded data for the path, opening it on the first ask and
   sharing it after that. The pointer is the type's own: cast at the caller.

   parms is what the type's loader takes besides the path, or NULL when it
   takes nothing: a wave passes its wav64_loadparms_t here, since libdragon
   fixes at load whether the sample is decoded into RAM or streamed from ROM.
   A file already open is handed back as it is, whatever parms say. */
void *load(const char *path, Resource::Type type, const void *parms);

/* Lets go of a pointer load handed out. Closes the file when nobody else
   holds it. */
void unload(void *data);

}

}

#endif
