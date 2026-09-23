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
#include <stddef.h>

#include "memory/e64_paged_allocator.h"
#include "memory/e64_memory.h"

namespace e64 {

namespace memory {

namespace paged {

typedef PagedAllocator::Block Block;
typedef PagedAllocator::Page Page;


void init(PagedAllocator *a, int32_t element_size, int32_t elements_per_page)
{
	a->block_size = element_size;
	a->blocks_per_page = elements_per_page;
	a->pages = NULL;
	a->page_count = 0;
	a->free_list = NULL;
}


void shutdown(PagedAllocator *a)
{
	clear(a);
}


void *allocate(PagedAllocator *a)
{
	if (a->free_list) {
		Block *data = a->free_list;
		a->free_list = data->next;
		return data;
	}

	/* One page is block_size * blocks_per_page in a single allocation, which
	   for contacts is over 150 KB. On a fragmented heap that malloc fails, and
	   building the free list through the NULL below writes pointers from
	   address zero onwards — the corruption only shows up later, inside the
	   solver, on data that looks nothing like this. */
	Page *page = (Page *)memory::alloc(a->block_size * a->blocks_per_page + (int32_t)sizeof(Page));
	assert(page);
	++a->page_count;

	page->next = a->pages;
	page->data = (Block *)E64_PTR_ADD(page, sizeof(Page));
	a->pages = page;

	int32_t blocks_minus_one = a->blocks_per_page - 1;
	for (int32_t i = 0; i < blocks_minus_one; ++i) {
		Block *node = (Block *)E64_PTR_ADD(page->data, a->block_size * i);
		Block *next = (Block *)E64_PTR_ADD(page->data, a->block_size * (i + 1));
		node->next = next;
	}

	Block *last = (Block *)E64_PTR_ADD(page->data, a->block_size * blocks_minus_one);
	last->next = NULL;

	a->free_list = page->data->next;
	return page->data;
}


void free(PagedAllocator *a, void *data)
{
	((Block *)data)->next = a->free_list;
	a->free_list = (Block *)data;
}


void clear(PagedAllocator *a)
{
	Page *page = a->pages;
	for (int32_t i = 0; i < a->page_count; ++i) {
		Page *next = page->next;
		memory::free(page);
		page = next;
	}
	a->free_list = NULL;
	a->page_count = 0;
	a->pages = NULL;
}

}

}

}
