/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "VisualObject.h"

#include "Graphic/Scene/Hierarchy/Node.h"

/*
	VisualActiveObject
*/
VisualActiveObject::VisualActiveObject(ActorId id, VisualEnvironment* env) 
    : ActiveObject(id), mEnvironment(env)
{
}

VisualActiveObject::~VisualActiveObject()
{
	RemoveFromScene(true);
}

VisualActiveObject* VisualActiveObject::Create(ActiveObjectType type, VisualEnvironment* env)
{
	// Find factory function
	auto n = mTypes.find(type);
	if (n == mTypes.end()) 
    {
		// If factory is not found, just return.
        LogWarning("VisualActiveObject: No factory for type=" + std::to_string((int)type));
		return NULL;
	}

	Factory f = n->second;
	VisualActiveObject *object = (*f)(env);
	return object;
}

void VisualActiveObject::RegisterType(unsigned short type, Factory f)
{
	auto n = mTypes.find(type);
	if (n != mTypes.end())
		return;
	mTypes[type] = f;
}


