/*
	Ported from qu3e q3ContactManager.h — altered source, not the original software.

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

/*
	Owns the list of Contact::Constraint and the BroadPhase.
*/
#ifndef ENGINE64_CONTACT_MANAGER_H
#define ENGINE64_CONTACT_MANAGER_H

#include <stdint.h>

#include "physics3d/collision/e64_contact.h"
#include "physics3d/collision/e64_broad_phase.h"
#include "memory/e64_paged_allocator.h"

namespace e64 {

namespace memory { class Stack; }


class Contact::Manager {
public:

	Constraint *contact_list;
	int32_t contact_count;
	memory::Stack *stack;
	memory::PagedAllocator allocator;
	BroadPhase broadphase;
	void *contact_listener;
};


namespace contact {

namespace manager {

void init (Contact::Manager *m, memory::Stack *stack);
void shutdown(Contact::Manager *m);

void addContact (Contact::Manager *m, physics::Shape *A, physics::Shape *B);
void findNewContacts (Contact::Manager *m);
void removeContact (Contact::Manager *m, Contact::Constraint *contact);
void removeContactsFromBody(Contact::Manager *m, RigidBody *body);
void removeFromBroadphase(Contact::Manager *m, RigidBody *body);
void testCollisions (Contact::Manager *m);

}

}

}

#endif
