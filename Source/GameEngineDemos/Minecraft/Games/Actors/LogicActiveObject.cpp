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

#include "LogicActiveObject.h"

#include "Inventory.h"

#include "Core/Utility/Serialize.h"

LogicActiveObject::LogicActiveObject(LogicEnvironment* env, Vector3<float> pos) :
	ActiveObject(), mEnvironment(env), mBasePosition(pos)
{
}

float LogicActiveObject::GetMinimumSavedMovement()
{
	return 2.0*BS;
}

ItemStack LogicActiveObject::GetWieldedItem(ItemStack* selected, ItemStack* hand) const
{
	*selected = ItemStack();
	if (hand)
		*hand = ItemStack();

	return ItemStack();
}

bool LogicActiveObject::SetWieldedItem(const ItemStack& item)
{
	return false;
}

std::string LogicActiveObject::GenerateUpdateInfantCommand(uint16_t infantId)
{
	std::ostringstream os(std::ios::binary);
	// command
	WriteUInt8(os, AO_CMD_SPAWN_INFANT);
	// parameters
	WriteUInt16(os, infantId);
	WriteUInt8(os, GetSendType());
	return os.str();
}

void LogicActiveObject::DumpAOMessagesToQueue(std::queue<ActiveObjectMessage>& queue)
{
	while (!mMessagesOut.empty()) 
    {
		queue.push(std::move(mMessagesOut.front()));
		mMessagesOut.pop();
	}
}

void LogicActiveObject::MarkForRemoval()
{
	if (!mPendingRemoval) 
    {
		OnMarkedForRemoval();
		mPendingRemoval = true;
	}
}

void LogicActiveObject::MarkForDeactivation()
{
	if (!mPendingDeactivation) 
    {
		OnMarkedForDeactivation();
		mPendingDeactivation = true;
	}
}
