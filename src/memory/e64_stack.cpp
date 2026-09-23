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

#include "memory/e64_stack.h"
#include "memory/e64_memory.h"

namespace e64 {

namespace memory {

namespace stack {

typedef Stack::Entry Entry;


void init(Stack *s)
{
	s->memory = NULL;
	s->entries = (Entry *)memory::alloc((int32_t)(sizeof(Entry) * Stack::INITIAL_ENTRIES));
	s->index = 0;
	s->allocation = 0;
	s->entry_count = 0;
	s->entry_capacity = Stack::INITIAL_ENTRIES;
	s->stack_size = 0;
}


void shutdown(Stack *s)
{
	if (s->memory) memory::free(s->memory);
	assert(s->index == 0);
	assert(s->entry_count == 0);
	if (s->entries) memory::free(s->entries);
	s->memory = NULL;
	s->entries = NULL;
}


void reserve(Stack *s, uint32_t size)
{
	assert(!s->index);
	if (size == 0) return;

	if (size >= s->stack_size) {
		if (s->memory) memory::free(s->memory);
		s->memory = (uint8_t *)memory::alloc((int32_t)size);
		s->stack_size = size;
	}
}


void *allocate(Stack *s, int32_t size)
{
	assert(s->index + (uint32_t)size <= s->stack_size);

	if (s->entry_count == s->entry_capacity) {
		Entry *old = s->entries;
		s->entry_capacity *= 2;
		s->entries = (Entry *)memory::alloc((int32_t)(s->entry_capacity * (int32_t)sizeof(Entry)));
		memcpy(s->entries, old, (size_t)s->entry_count * sizeof(Entry));
		memory::free(old);
	}

	Entry *entry = s->entries + s->entry_count;
	entry->size = size;
	entry->data = s->memory + s->index;
	s->index += (uint32_t)size;

	s->allocation += size;
	++s->entry_count;

	return entry->data;
}


void free(Stack *s, void *data)
{
	assert(s->entry_count > 0);
	Entry *entry = s->entries + s->entry_count - 1;
	assert(data == entry->data);
	(void)data;

	s->index -= (uint32_t)entry->size;
	s->allocation -= entry->size;
	--s->entry_count;
}

}

}

}
