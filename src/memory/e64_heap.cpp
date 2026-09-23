/*
	Ported from qu3e q3Memory.cpp — altered source, not the original software.

	Copyright (c) 2014 Randy Gaul http://www.randygaul.net

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
	     claim that you wrote the original software. If you use this software
	     in a product, an acknowledgment in the product documentation would be
	     appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
	     be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
*/

#include <assert.h>
#include <string.h>

#include "memory/e64_heap.h"
#include "memory/e64_memory.h"

namespace e64 {

namespace memory {

namespace heap {

typedef Heap::Header Header;
typedef Heap::FreeBlock FreeBlock;


void init(Heap *h)
{
	h->memory = (Header *)memory::alloc(Heap::SIZE);
	h->memory->next = NULL;
	h->memory->prev = NULL;
	h->memory->size = Heap::SIZE;

	h->free_blocks = (FreeBlock *)memory::alloc((int32_t)(sizeof(FreeBlock) * Heap::INITIAL_CAPACITY));
	h->free_block_count = 1;
	h->free_block_capacity = Heap::INITIAL_CAPACITY;

	h->free_blocks->header = h->memory;
	h->free_blocks->size = Heap::SIZE;
}


void shutdown(Heap *h)
{
	memory::free(h->memory);
	memory::free(h->free_blocks);
	h->memory = NULL;
	h->free_blocks = NULL;
}


void *allocate(Heap *h, int32_t size)
{
	int32_t size_needed = size + (int32_t)sizeof(Header);
	FreeBlock *first_fit = NULL;

	for (int32_t i = 0; i < h->free_block_count; ++i) {
		FreeBlock *block = h->free_blocks + i;
		if (block->size >= size_needed) { first_fit = block; break; }
	}
	if (!first_fit) return NULL;

	Header *node = first_fit->header;
	Header *new_node = (Header *)E64_PTR_ADD(node, size_needed);
	node->size = size_needed;

	first_fit->size -= size_needed;
	first_fit->header = new_node;

	new_node->next = node->next;
	if (node->next) node->next->prev = new_node;
	node->next = new_node;
	new_node->prev = node;

	return E64_PTR_ADD(node, sizeof(Header));
}


void free(Heap *h, void *memory)
{
	assert(memory);
	Header *node = (Header *)E64_PTR_ADD(memory, -(int32_t)sizeof(Header));

	Header *next = node->next;
	Header *prev = node->prev;
	FreeBlock *next_block = NULL;
	int32_t prev_block_idx = ~0;
	FreeBlock *prev_block = NULL;
	int32_t free_block_count = h->free_block_count;

	for (int32_t i = 0; i < free_block_count; ++i) {
		FreeBlock *block = h->free_blocks + i;
		Header *header = block->header;
		if (header == next) next_block = block;
		else if (header == prev) { prev_block = block; prev_block_idx = i; }
	}

	int merged = 0;

	if (prev_block) {
		merged = 1;
		prev->next = next;
		if (next) next->prev = prev;

		prev_block->size += node->size;
		prev->size = prev_block->size;

		if (next_block) {
			next_block->header = prev;
			next_block->size += prev->size;
			prev->size = next_block->size;

			Header *nextnext = next->next;
			prev->next = nextnext;
			if (nextnext) nextnext->prev = prev;

			assert(h->free_block_count);
			assert(prev_block_idx != ~0);
			--h->free_block_count;
			h->free_blocks[prev_block_idx] = h->free_blocks[h->free_block_count];
		}
	}
	else if (next_block) {
		merged = 1;
		next_block->header = node;
		next_block->size += node->size;
		node->size = next_block->size;

		Header *nextnext = next->next;
		if (nextnext) nextnext->prev = node;
		node->next = nextnext;
	}

	if (!merged) {
		FreeBlock block;
		block.header = node;
		block.size = node->size;

		if (h->free_block_count == h->free_block_capacity) {
			FreeBlock *old_blocks = h->free_blocks;
			int32_t old_cap = h->free_block_capacity;

			h->free_block_capacity *= 2;
			h->free_blocks = (FreeBlock *)memory::alloc((int32_t)(sizeof(FreeBlock) * h->free_block_capacity));
			memcpy(h->free_blocks, old_blocks, sizeof(FreeBlock) * (size_t)old_cap);
			memory::free(old_blocks);
		}

		h->free_blocks[h->free_block_count++] = block;
	}
}

}

}

}
