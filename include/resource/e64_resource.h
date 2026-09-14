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


/* What kind of file a path is, which picks how it opens and closes. */
typedef enum {

	RESOURCE_MODEL,    /* T3DModel */
	RESOURCE_SPRITE,   /* sprite_t */
	RESOURCE_FONT,     /* rdpq_font_t, still to be registered by whoever asked */
	RESOURCE_WAVE,     /* wav64_t */

} ResourceType;


/* Files open at once, across every scene up at the same time. A 2D
   character opens one file per frame and a stage one per tile it uses, so
   a 2D scene alone runs into the hundreds. */
#define RESOURCE_MAX 512


/* Hands back the loaded data for the path, opening it on the first ask and
   sharing it after that. The pointer is the type's own: cast at the caller.

   parms is what the type's loader takes besides the path, or NULL when it
   takes nothing: a wave passes its wav64_loadparms_t here, since libdragon
   fixes at load whether the sample is decoded into RAM or streamed from ROM.
   A file already open is handed back as it is, whatever parms say. */
void *resource_load(const char *path, ResourceType type, const void *parms);

/* Lets go of a pointer resource_load handed out. Closes the file when nobody
   else holds it. */
void resource_unload(void *data);

#endif
