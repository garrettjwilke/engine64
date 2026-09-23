/*
	Low-level malloc/free wrappers + PTR_ADD helper. Shared by Stack / Heap /
	PagedAllocator.
*/
#ifndef ENGINE64_MEMORY_H
#define ENGINE64_MEMORY_H

#include <stdint.h>
#include <stdlib.h>

namespace e64 {

namespace memory {

static inline void *alloc(int32_t bytes) { return malloc((size_t)bytes); }
static inline void free(void *memory) { ::free(memory); }

}

#define E64_PTR_ADD(P, BYTES) ((void*)(((uint8_t*)(P)) + (BYTES)))

}

#endif
