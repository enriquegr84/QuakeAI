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

#include "Craft.h"
#include "PlayerLAO.h"
#include "LogicPlayer.h"
#include "InventoryManager.h"
#include "LogicActiveObject.h"

#include "../Environment/LogicEnvironment.h"

#include "../Map/Map.h"
#include "../Map/MapNodeMetadata.h"

#include "../Games.h"

#include "Core/Utility/StringUtil.h"

/*
	InventoryLocation
*/
std::string InventoryLocation::Dump() const
{
	std::ostringstream os(std::ios::binary);
	Serialize(os);
	return os.str();
}

void InventoryLocation::Serialize(std::ostream& os) const
{
	switch (type) 
    {
	    case InventoryLocation::UNDEFINED:
		    os<<"undefined";
		    break;
	    case InventoryLocation::CURRENT_PLAYER:
		    os<<"current_player";
		    break;
	    case InventoryLocation::PLAYER:
		    os<<"player:"<<name;
		    break;
	    case InventoryLocation::NODEMETA:
		    os<<"nodemeta:"<< nodePosition[0]<<","<< nodePosition[1]<<","<< nodePosition[2];
		    break;
	    case InventoryLocation::DETACHED:
		    os<<"detached:"<<name;
		    break;
	    default:
		    LogError("Unhandled inventory location type");
	}
}

void InventoryLocation::Deserialize(std::istream& is)
{
	std::string tname;
	std::getline(is, tname, ':');
	if (tname == "undefined") 
    {
		type = InventoryLocation::UNDEFINED;
	} 
    else if (tname == "current_player") 
    {
		type = InventoryLocation::CURRENT_PLAYER;
	} 
    else if (tname == "player") 
    {
		type = InventoryLocation::PLAYER;
		std::getline(is, tname, '\n');
        name = tname;
	} 
    else if (tname == "nodemeta") 
    {
		type = InventoryLocation::NODEMETA;
		std::string pos;
		std::getline(is, pos, '\n');
		Strfnd fn(pos);
        nodePosition[0] = atoi(fn.Next(",").c_str());
        nodePosition[1] = atoi(fn.Next(",").c_str());
        nodePosition[2] = atoi(fn.Next(",").c_str());
	} 
    else if (tname == "detached") 
    {
		type = InventoryLocation::DETACHED;
		std::getline(is, tname, '\n');
        name = tname;
	} 
    else 
    {
		LogInformation("Unknown InventoryLocation type=" + tname);
		throw SerializationError("Unknown InventoryLocation type");
	}
}

void InventoryLocation::Deserialize(const std::string& str)
{
	std::istringstream is(str, std::ios::binary);
	Deserialize(is);
}

/*
	InventoryAction
*/

InventoryAction* InventoryAction::Deserialize(std::istream& is)
{
	std::string type;
	std::getline(is, type, ' ');

	InventoryAction* a = nullptr;

	if (type == "Move")
		a = new BaseMoveAction(is, false);
    else if (type == "MoveSomewhere")
		a = new BaseMoveAction(is, true);
    else if (type == "Drop")
		a = new BaseDropAction(is);
    else if (type == "Craft")
		a = new BaseCraftAction(is);

	return a;
}

/*
	BaseMoveAction
*/

BaseMoveAction::BaseMoveAction(std::istream &is, bool somewhere) : moveSomewhere(somewhere)
{
	std::string ts, fl, tl;

	std::getline(is, ts, ' ');
	count = atoi(ts.c_str());

	std::getline(is, ts, ' ');
	fromInventory.Deserialize(ts);

	std::getline(is, fl, ' ');
    fromList = fl;

	std::getline(is, ts, ' ');
	fromItem = atoi(ts.c_str());

	std::getline(is, ts, ' ');
	toInventory.Deserialize(ts);

	std::getline(is, tl, ' ');
    toList = tl;

	if (!somewhere) 
    {
		std::getline(is, ts, ' ');
		toItem = stoi(ts);
	}
}

void BaseMoveAction::SwapDirections()
{
	std::swap(fromInventory, toInventory);
    std::swap(fromList, toList);
    std::swap(fromItem, toItem);
}

void BaseMoveAction::OnPutAndOnTake(const ItemStack& srcItem, LogicActiveObject* player) const
{
	if (toInventory.type == InventoryLocation::DETACHED)
		BaseGame::Get()->OnPutDetachedInventory(*this, srcItem, player);
	else if (toInventory.type == InventoryLocation::NODEMETA)
		BaseGame::Get()->OnPutMetadataInventory(*this, srcItem, player);
	else if (toInventory.type == InventoryLocation::PLAYER)
		BaseGame::Get()->OnPutPlayerInventory(*this, srcItem, player);
	else
		LogAssert(false, "invalid put inventory location");
	
	if (fromInventory.type == InventoryLocation::DETACHED)
		BaseGame::Get()->OnTakeDetachedInventory(*this, srcItem, player);
	else if (fromInventory.type == InventoryLocation::NODEMETA)
		BaseGame::Get()->OnTakeMetadataInventory(*this, srcItem, player);
	else if (fromInventory.type == InventoryLocation::PLAYER)
		BaseGame::Get()->OnTakePlayerInventory(*this, srcItem, player);
	else
        LogAssert(false, "invalid take inventory location");
}

void BaseMoveAction::OnMove(int count, LogicActiveObject* player) const
{
	if (fromInventory.type == InventoryLocation::DETACHED)
		BaseGame::Get()->OnMoveDetachedInventory(*this, count, player);
	else if (fromInventory.type == InventoryLocation::NODEMETA)
		BaseGame::Get()->OnMoveMetadataInventory(*this, count, player);
	else if (fromInventory.type == InventoryLocation::PLAYER)
		BaseGame::Get()->OnMovePlayerInventory(*this, count, player);
	else
        LogAssert(false, "invalid move inventory location");
}

int BaseMoveAction::AllowPut(const ItemStack& dstItem, LogicActiveObject* player) const
{
	int dstCanPutCount = 0xffff;
	if (toInventory.type == InventoryLocation::DETACHED)
		dstCanPutCount = BaseGame::Get()->AllowPutDetachedInventory(*this, dstItem, player);
	else if (toInventory.type == InventoryLocation::NODEMETA)
		dstCanPutCount = BaseGame::Get()->AllowPutMetadataInventory(*this, dstItem, player);
	else if (toInventory.type == InventoryLocation::PLAYER)
		dstCanPutCount = BaseGame::Get()->AllowPutPlayerInventory(*this, dstItem, player);
	else
        LogAssert(false, "invalid put inventory location");
	return dstCanPutCount;
}

int BaseMoveAction::AllowTake(const ItemStack& srcItem, LogicActiveObject* player) const
{
	int srcCanTakeCount = 0xffff;
	if (fromInventory.type == InventoryLocation::DETACHED)
		srcCanTakeCount = BaseGame::Get()->AllowTakeDetachedInventory(*this, srcItem, player);
	else if (fromInventory.type == InventoryLocation::NODEMETA)
		srcCanTakeCount = BaseGame::Get()->AllowTakeMetadataInventory(*this, srcItem, player);
	else if (fromInventory.type == InventoryLocation::PLAYER)
		srcCanTakeCount = BaseGame::Get()->AllowTakePlayerInventory(*this, srcItem, player);
	else
        LogAssert(false, "invalid take inventory location");
	return srcCanTakeCount;
}

int BaseMoveAction::AllowMove(int tryTakeCount, LogicActiveObject* player) const
{
	int srcCanTakeCount = 0xffff;
	if (fromInventory.type == InventoryLocation::DETACHED)
		srcCanTakeCount = BaseGame::Get()->AllowMoveDetachedInventory(*this, tryTakeCount, player);
	else if (fromInventory.type == InventoryLocation::NODEMETA)
		srcCanTakeCount = BaseGame::Get()->AllowMoveMetadataInventory(*this, tryTakeCount, player);
	else if (fromInventory.type == InventoryLocation::PLAYER)
		srcCanTakeCount = BaseGame::Get()->AllowMovePlayerInventory(*this, tryTakeCount, player);
	else
        LogAssert(false, "invalid move inventory location");
	return srcCanTakeCount;
}

void BaseMoveAction::Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env)
{
	Inventory* invFrom = mgr->GetInventory(fromInventory);
	Inventory* invTo = mgr->GetInventory(toInventory);

	if (!invFrom) 
    {
		LogInformation("BasweMoveAction::apply(): FAIL: source inventory not found: "
            "fromInventory=" + fromInventory.Dump() + ", toInventory=" + toInventory.Dump());
		return;
	}
	if (!invTo) 
    {
		LogInformation("BasweMoveAction::apply(): FAIL: destination inventory not found: "
			"fromInventory=" + fromInventory.Dump() + ", toInventory=" + toInventory.Dump());
		return;
	}

	InventoryList* listFrom = invFrom->GetList(fromList);
	InventoryList* listTo = invTo->GetList(toList);

	/*
		If a list doesn't exist or the source item doesn't exist
	*/
	if (!listFrom) 
    {
		LogInformation("BasweMoveAction::apply(): FAIL: source list not found: "
            "fromInventory=" + fromInventory.Dump() + ", fromList=" + fromList);
		return;
	}
	if (!listTo) 
    {
		LogInformation("BasweMoveAction::apply(): FAIL: destination list not found: "
			"toInventory=" + toInventory.Dump() + ", toList=" + toList);
		return;
	}

	if (moveSomewhere) 
    {
		int16_t oldToItem = toItem;
		uint16_t oldCount = count;
		causedByMoveSomewhere = true;
		moveSomewhere = false;

		LogInformation("BasweMoveAction::apply(): moving item somewhere"
            " msom=" + std::to_string(moveSomewhere) + " count=" + std::to_string(count) +
            " from inv=" + fromInventory.Dump() + " list=" + fromList + " i=" + std::to_string(fromItem) + 
			" to inv=" + toInventory.Dump() + " list=" + toList);

		// Try to add the item to destination list
		short destSize = listTo->GetSize();
		// First try all the non-empty slots
		for (short destItem = 0; destItem < destSize && count > 0; destItem++) 
        {
			if (!listTo->GetItem(destItem).IsEmpty()) 
            {
				toItem = destItem;
				Apply(mgr, player, env);
				LogAssert(moveCount <= count, "invalid destination item");
				count -= moveCount;
			}
		}

		// Then try all the empty ones
		for (short destItem = 0; destItem < destSize && count > 0; destItem++)
        {
			if (listTo->GetItem(destItem).IsEmpty()) 
            {
				toItem = destItem;
				Apply(mgr, player, env);
				count -= moveCount;
			}
		}

		toItem = oldToItem;
		count = oldCount;
		causedByMoveSomewhere = false;
		moveSomewhere = true;
		return;
	}

	if ((uint16_t)toItem > listTo->GetSize()) 
    {
		LogInformation("BaseMoveAction::apply(): FAIL: destination index out of bounds: "
			"toItem=" + std::to_string(toItem) + ", size=" + std::to_string(listTo->GetSize()));
		return;
	}
	/*
		Do not handle rollback if both inventories are that of the same player
	*/
	bool ignoreRollback = (fromInventory.type == InventoryLocation::PLAYER && fromInventory == toInventory);

	/*
		Collect information of endpoints
	*/

	ItemStack srcItem = listFrom->GetItem(fromItem);
	if (count > 0 && count < srcItem.count)
		srcItem.count = count;
	if (srcItem.IsEmpty())
		return;

	int srcCanTakeCount = 0xffff;
	int dstCanPutCount = 0xffff;

	// this is needed for swapping items inside one inventory to work
	ItemStack restitem;
	bool allowSwap = !listTo->ItemFits(toItem, srcItem, &restitem)
		&& restitem.count == srcItem.count && !causedByMoveSomewhere;
	moveCount = srcItem.count - restitem.count;

	// Shift-click: Cannot fill this stack, proceed with next slot
	if (causedByMoveSomewhere && moveCount == 0)
		return;

	if (allowSwap) 
    {
		// Swap will affect the entire stack if it can performed.
		srcItem = listFrom->GetItem(fromItem);
		count = srcItem.count;
	}

	if (fromInventory == toInventory) 
    {
		// Move action within the same inventory
		srcCanTakeCount = AllowMove(srcItem.count, player);

		bool swapExpected = allowSwap;
		allowSwap = allowSwap && (srcCanTakeCount == -1 || srcCanTakeCount >= srcItem.count);
		if (allowSwap) 
        {
			int tryPutCount = listTo->GetItem(toItem).count;
			SwapDirections();
			dstCanPutCount = AllowMove(tryPutCount, player);
			allowSwap = allowSwap && (dstCanPutCount == -1 || dstCanPutCount >= tryPutCount);
			SwapDirections();
		} 
        else 
        {
			dstCanPutCount = srcCanTakeCount;
		}
		if (swapExpected != allowSwap)
			srcCanTakeCount = dstCanPutCount = 0;
	} 
    else 
    {
		// Take from one inventory, put into another
		int srcItemCount = srcItem.count;
        if (causedByMoveSomewhere)
        {
            // When moving somewhere: temporarily use the actual movable stack
            // size to ensure correct callback execution.
            srcItem.count = moveCount;
        }
		dstCanPutCount = AllowPut(srcItem, player);
		srcCanTakeCount = AllowTake(srcItem, player);
        if (causedByMoveSomewhere)
        {
            // Reset source item count
            srcItem.count = srcItemCount;
        }
		bool swapExpected = allowSwap;
		allowSwap = allowSwap
			&& (srcCanTakeCount == -1 || srcCanTakeCount >= srcItem.count)
			&& (dstCanPutCount == -1 || dstCanPutCount >= srcItem.count);
		// A swap is expected, which means that we have to
		// run the "allow" callbacks a second time with swapped inventories
		if (allowSwap) 
        {
			ItemStack dstItem = listTo->GetItem(toItem);
			SwapDirections();

			int srcCanTake = AllowPut(dstItem, player);
			int dstCanPut = AllowTake(dstItem, player);
			allowSwap = allowSwap
				&& (srcCanTake == -1 || srcCanTake >= dstItem.count)
				&& (dstCanPut == -1 || dstCanPut >= dstItem.count);
			SwapDirections();
		}
		if (swapExpected != allowSwap)
			srcCanTakeCount = dstCanPutCount = 0;
	}

	int oldCount = count;

	/* Modify count according to collected data */
	count = srcItem.count;
	if (srcCanTakeCount != -1 && count > srcCanTakeCount)
		count = srcCanTakeCount;
	if (dstCanPutCount != -1 && count > dstCanPutCount)
		count = dstCanPutCount;

	/* Limit according to source item count */
	if (count > listFrom->GetItem(fromItem).count)
		count = listFrom->GetItem(fromItem).count;

	/* If no items will be moved, don't go further */
	if (count == 0) 
    {
		if (causedByMoveSomewhere)
			// Set move count to zero, as no items have been moved
			moveCount = 0;

		// Undo visual prediction.
		if (fromInventory.type == InventoryLocation::PLAYER)
			listFrom->SetModified();

		if (toInventory.type == InventoryLocation::PLAYER)
			listTo->SetModified();

        LogInformation("BaseMoveAction::apply(): move was completely disallowed: count=" + 
            std::to_string(oldCount) + " from inv=" + fromInventory.Dump() + " list=" + 
            fromList + " i=" + std::to_string(fromItem) + " to inv=" + toInventory.Dump() + 
            " list=" + toList + " i=" + std::to_string(toItem));

		return;
	}

	srcItem = listFrom->GetItem(fromItem);
	srcItem.count = count;
	ItemStack fromStackWas = listFrom->GetItem(fromItem);
	ItemStack toStackWas = listTo->GetItem(toItem);

	/*
		Perform actual move

		If something is wrong (source item is empty, destination is the
		same as source), nothing happens
	*/
	bool didSwap = false;
	moveCount = listFrom->MoveItem(fromItem,
		listTo, toItem, count, allowSwap, &didSwap);
	if (causedByMoveSomewhere)
		count = oldCount;
	LogAssert(allowSwap == didSwap, "invalid swap");

	// If source is infinite, reset it's stack
	if (srcCanTakeCount == -1) 
    {
		// For the causedByMoveSomewhere == true case we didn't force-put the item,
		// which guarantees there is no leftover, and code below would duplicate the
		// (not replaced) toStackWas item.
		if (!causedByMoveSomewhere) 
        {
			// If destination stack is of different type and there are leftover
			// items, attempt to put the leftover items to a different place in the
			// destination inventory.
			// The visual-side GUI will try to guess if this happens.
			if (fromStackWas.name != toStackWas.name) 
            {
				for (unsigned int i = 0; i < listTo->GetSize(); i++) 
                {
					if (listTo->GetItem(i).IsEmpty()) {
						listTo->ChangeItem(i, toStackWas);
						break;
					}
				}
			}
		}
		if (moveCount > 0 || didSwap) 
        {
			listFrom->DeleteItem(fromItem);
			listFrom->AddItem(fromItem, fromStackWas);
		}
	}
	// If destination is infinite, reset it's stack and take count from source
	if (dstCanPutCount == -1) 
    {
		listTo->DeleteItem(toItem);
		listTo->AddItem(toItem, toStackWas);
		listFrom->DeleteItem(fromItem);
		listFrom->AddItem(fromItem, fromStackWas);
		listFrom->TakeItem(fromItem, count);
	}

	LogInformation("BaseMoveAction::apply(): moved msom=" + std::to_string(moveSomewhere) + 
        " caused=" + std::to_string(causedByMoveSomewhere) + " count=" + std::to_string(count) + 
        " from inv=" + fromInventory.Dump() + " list=" + fromList + " i=" + std::to_string(fromItem) +
        " to inv=" + toInventory.Dump() + " list=" + toList + " i=" + std::to_string(toItem));

	// If we are inside the move somewhere loop, we don't need to report
	// anything if nothing happened
	if (causedByMoveSomewhere && moveCount == 0)
		return;

	/*
		Record rollback information

	if (!ignoreRollback && game->Rollback()) 
    {
		BaseRollbackManager* rollback = game->Rollback();

		// If source is not infinite, record item take
		if (srcCanTakeCount != -1) 
        {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				fromInventory.Serialize(os);
				loc = os.str();
			}
			action.setModifyInventoryStack(loc, fromList, fromItem, false, srcItem);
			rollback->reportAction(action);
		}
		// If destination is not infinite, record item put
		if (dstCanPutCount != -1) 
        {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				toInventory.Serialize(os);
				loc = os.str();
			}
			action.SetModifyInventoryStack(loc, toList, toItem, true, srcItem);
			rollback->ReportAction(action);
		}
	}
    */

	/*
		Report move to endpoints
	*/

	// Source = destination => move
	if (fromInventory == toInventory) 
    {
		OnMove(count, player);
		if (didSwap) 
        {
			// Item is now placed in source list
			srcItem = listFrom->GetItem(fromItem);
			SwapDirections();
			OnMove(srcItem.count, player);
			SwapDirections();
		}
		mgr->SetInventoryModified(fromInventory);
	} 
    else 
    {
		int srcItemCount = srcItem.count;
        if (causedByMoveSomewhere)
        {
            // When moving somewhere: temporarily use the actual movable stack
            // size to ensure correct callback execution.
            srcItem.count = moveCount;
        }
		OnPutAndOnTake(srcItem, player);
        if (causedByMoveSomewhere)
        {
            // Reset source item count
            srcItem.count = srcItemCount;
        }
		if (didSwap) 
        {
			// Item is now placed in source list
			srcItem = listFrom->GetItem(fromItem);
			SwapDirections();
			OnPutAndOnTake(srcItem, player);
			SwapDirections();
		}
		mgr->SetInventoryModified(toInventory);
		mgr->SetInventoryModified(fromInventory);
	}
}

void BaseMoveAction::Apply(InventoryManager* mgr, Environment* env)
{
	// Optional InventoryAction operation that is run on the visual
	// to make lag less apparent.

	Inventory* invFrom = mgr->GetInventory(fromInventory);
	Inventory* invTo = mgr->GetInventory(toInventory);
	if (!invFrom || !invTo)
		return;

	InventoryLocation currentPlayer;
	currentPlayer.SetCurrentPlayer();
	Inventory* invPlayer = mgr->GetInventory(currentPlayer);
	if (invFrom != invPlayer || invTo != invPlayer)
		return;

	InventoryList* listFrom = invFrom->GetList(fromList);
	InventoryList* listTo = invTo->GetList(toList);
	if (!listFrom || !listTo)
		return;

	if (!moveSomewhere)
		listFrom->MoveItem(fromItem, listTo, toItem, count);
	else
		listFrom->MoveItemSomewhere(fromItem, listTo, count);

	mgr->SetInventoryModified(fromInventory);
	if (invFrom != invTo)
		mgr->SetInventoryModified(toInventory);
}

/*
	BaseDropAction
*/

BaseDropAction::BaseDropAction(std::istream& is)
{
	std::string ts, fl;

	std::getline(is, ts, ' ');
	count = stoi(ts);

	std::getline(is, ts, ' ');
	fromInventory.Deserialize(ts);

	std::getline(is, fl, ' ');
    fromList = fl;

	std::getline(is, ts, ' ');
	fromItem = atoi(ts.c_str());
}

void BaseDropAction::Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env)
{
	Inventory* invFrom = mgr->GetInventory(fromInventory);
	if (!invFrom) 
    {
		LogInformation("FAIL: source inventory not found: fromInventory=" + fromInventory.Dump());
		return;
	}

	InventoryList* listFrom = invFrom->GetList(fromList);

	/*
		If a list doesn't exist or the source item doesn't exist
	*/
	if (!listFrom) 
    {
		LogInformation("FAIL: source list not found: fromInventory=" + fromInventory.Dump());
		return;
	}
	if (listFrom->GetItem(fromItem).IsEmpty()) 
    {
        LogInformation("FAIL: source item not found : fromInventory=" + fromInventory.Dump() + 
			", fromList=" + fromList + " fromItem=" + std::to_string(fromItem));
		return;
	}

	/*
		Do not handle rollback if inventory is player's
	*/
	bool ignoreSrcRollback = (fromInventory.type == InventoryLocation::PLAYER);

	/*
		Collect information of endpoints
	*/

	int takeCount = listFrom->GetItem(fromItem).count;
	if (count != 0 && count < takeCount)
		takeCount = count;
	int srcCanTakeCount = takeCount;

	ItemStack srcItem = listFrom->GetItem(fromItem);
	srcItem.count = takeCount;

	// Run callbacks depending on source inventory
    /*
	switch (fromInventory.type) 
    {
	    case InventoryLocation::DETACHED:
		    srcCanTakeCount = PLAYER_TO_SA(player)->detached_inventory_AllowTake(*this, srcItem, player);
		    break;
	    case InventoryLocation::NODEMETA:
		    srcCanTakeCount = PLAYER_TO_SA(player)->nodemeta_inventory_AllowTake(*this, srcItem, player);
		    break;
	    case InventoryLocation::PLAYER:
		    srcCanTakeCount = PLAYER_TO_SA(player)->player_inventory_AllowTake(*this, srcItem, player);
		    break;
	    default:
		    break;
	}
    */

	if (srcCanTakeCount != -1 && srcCanTakeCount < takeCount)
		takeCount = srcCanTakeCount;

	// Update item due executed callbacks
	srcItem = listFrom->GetItem(fromItem);

	// Drop the item
	ItemStack item1 = listFrom->GetItem(fromItem);
	item1.count = takeCount;
    if (BaseGame::Get()->OnDropItem(item1, player, player->GetBasePosition()))
    {
		int actuallyDroppedCount = takeCount - item1.count;
		if (actuallyDroppedCount == 0) 
        {
			LogInformation("Actually dropped no items");

			// Revert visual prediction.
			if (fromInventory.type == InventoryLocation::PLAYER)
				listFrom->SetModified();
			return;
		}

		// If source isn't infinite
		if (srcCanTakeCount != -1) 
        {
			// Take item from source list
			ItemStack item2 = listFrom->TakeItem(fromItem, actuallyDroppedCount);

			if (item2.count != actuallyDroppedCount)
				LogError("Could not take dropped count of items");
		}

		srcItem.count = actuallyDroppedCount;
		mgr->SetInventoryModified(fromInventory);
	}

    LogInformation("Dropped from inv=" + fromInventory.Dump() + 
		" list=" + fromList + " i=" + std::to_string(fromItem));

	/*
		Report drop to endpoints
	*/
    /*
	switch (fromInventory.type) 
    {
	    case InventoryLocation::DETACHED:
		    PLAYER_TO_SA(player)->detached_inventory_OnTake(*this, srcItem, player);
		    break;
	    case InventoryLocation::NODEMETA:
		    PLAYER_TO_SA(player)->nodemeta_inventory_OnTake(*this, srcItem, player);
		    break;
	    case InventoryLocation::PLAYER:
		    PLAYER_TO_SA(player)->player_inventory_OnTake(*this, srcItem, player);
		    break;
	    default:
		    break;
	}
    */
	/*
		Record rollback information

	if (!ignoreSrcRollback && game->Rollback()) 
    {
		BaseRollbackManager* rollback = game->Rollback();

		// If source is not infinite, record item take
		if (srcCanTakeCount != -1) 
        {
			RollbackAction action;
			std::string loc;
			{
				std::ostringstream os(std::ios::binary);
				fromInventory.Serialize(os);
				loc = os.str();
			}
			action.SetModifyInventoryStack(loc, fromList, fromItem, false, srcItem);
			rollback->ReportAction(action);
		}
	}
    */
}

void BaseDropAction::Apply(InventoryManager* mgr, Environment* env)
{
	// Optional InventoryAction operation that is run on the visual
	// to make lag less apparent.

	Inventory* invFrom = mgr->GetInventory(fromInventory);
	if (!invFrom)
		return;

	InventoryLocation currentPlayer;
	currentPlayer.SetCurrentPlayer();
	Inventory* invPlayer = mgr->GetInventory(currentPlayer);
	if (invFrom != invPlayer)
		return;

	InventoryList* listFrom = invFrom->GetList(fromList);
	if (!listFrom)
		return;

	if (count == 0)
		listFrom->ChangeItem(fromItem, ItemStack());
	else
		listFrom->TakeItem(fromItem, count);

	mgr->SetInventoryModified(fromInventory);
}

/*
	BaseCraftAction
*/

BaseCraftAction::BaseCraftAction(std::istream& is)
{
	std::string ts;

	std::getline(is, ts, ' ');
	count = stoi(ts);

	std::getline(is, ts, ' ');
	craftInventory.Deserialize(ts);
}

void BaseCraftAction::Apply(InventoryManager* mgr, LogicActiveObject* player, Environment* env)
{
	Inventory* invCraft = mgr->GetInventory(craftInventory);

	if (!invCraft) 
    {
		LogInformation("FAIL: inventory not found: craftInventory=" + craftInventory.Dump());
		return;
	}

	InventoryList* listCraft = invCraft->GetList("craft");
	InventoryList* listCraftResult = invCraft->GetList("craftresult");
	InventoryList* listMain = invCraft->GetList("main");

	/*
		If a list doesn't exist or the source item doesn't exist
	*/
	if (!listCraft)
    {
		LogInformation("FAIL: craft list not found: craftInventory=" + craftInventory.Dump());
		return;
	}
	if (!listCraftResult) 
    {
		LogInformation("FAIL: craftresult list not found: craftInventory=" + craftInventory.Dump());
		return;
	}
	if (listCraftResult->GetSize() < 1) 
    {
		LogInformation("FAIL: craftresult list too short: craftInventory=" + craftInventory.Dump());
		return;
	}

	ItemStack crafted;
	ItemStack craftResultItem;
	int countRemaining = count;
	std::vector<ItemStack> outputReplacements;
	GetCraftingResult(invCraft, crafted, outputReplacements, false, env);
    BaseGame::Get()->OnCraftPredictItem(crafted, player, listCraft, craftInventory);
	bool found = !crafted.IsEmpty();

	while (found && listCraftResult->ItemFits(0, crafted)) 
    {
		InventoryList savedCraftList = *listCraft;

		std::vector<ItemStack> temp;
		// Decrement input and add crafting output
		GetCraftingResult(invCraft, crafted, temp, true, env);
		BaseGame::Get()->OnCraftItem(crafted, player, &savedCraftList, craftInventory);
		listCraftResult->AddItem(0, crafted);
		mgr->SetInventoryModified(craftInventory);

		// Add the new replacements to the list
		for (auto& itemstack : temp) 
        {
			for (auto& outputReplacement : outputReplacements) 
            {
				if (itemstack.name == outputReplacement.name) 
                {
					itemstack = outputReplacement.AddItem(itemstack, env->GetItemManager());
					if (itemstack.IsEmpty())
						continue;
				}
			}
			outputReplacements.push_back(itemstack);
		}

		LogInformation(player->GetDescription() + " crafts " + crafted.GetItemString());

		// Decrement counter
		if (countRemaining == 1)
			break;

		if (countRemaining > 1)
			countRemaining--;

		// Get next crafting result
		GetCraftingResult(invCraft, crafted, temp, false, env);
		BaseGame::Get()->OnCraftPredictItem(crafted, player, listCraft, craftInventory);
		found = !crafted.IsEmpty();
	}

	// Put the replacements in the inventory or drop them on the floor, if
	// the inventory is full
	for (auto& outputReplacement : outputReplacements) 
    {
		if (listMain)
			outputReplacement = listMain->AddItem(outputReplacement);
		if (outputReplacement.IsEmpty())
			continue;
		uint16_t count = outputReplacement.count;
		do 
        {
            BaseGame::Get()->OnDropItem(outputReplacement, player, player->GetBasePosition());
			if (count >= outputReplacement.count) 
            {
				LogError("Couldn't drop replacement stack " + outputReplacement.GetItemString() + 
                    " because drop loop didn't decrease count.");

				break;
			}
		} while (!outputReplacement.IsEmpty());
	}

	LogInformation("Crafted craftInventory=" + craftInventory.Dump());
}

void BaseCraftAction::Apply(InventoryManager* mgr, Environment* env)
{
	// Optional InventoryAction operation that is run on the visual
	// to make lag less apparent.
}

// Crafting helper
bool GetCraftingResult(Inventory* inv, ItemStack& result,
    std::vector<ItemStack>& outputReplacements, bool decrementInput, Environment* env)
{
	result.Clear();

	// Get the InventoryList in which we will operate
	InventoryList* clist = inv->GetList("craft");
	if (!clist)
		return false;

	// Mangle crafting grid to an another format
	CraftInput ci;
	ci.method = CRAFT_METHOD_NORMAL;
	ci.width = clist->GetWidth() ? clist->GetWidth() : 3;
	for (uint16_t i=0; i < clist->GetSize(); i++)
		ci.items.push_back(clist->GetItem(i));

	// Find out what is crafted and add it to result item slot
	CraftOutput co;
	bool found = env->GetCraftManager()->GetCraftResult(
			ci, co, outputReplacements, decrementInput, env);
	if (found)
		result.Deserialize(co.item, env->GetItemManager());

	if (found && decrementInput)
    {
		// CraftInput has been changed, apply changes in clist
		for (uint16_t i=0; i < clist->GetSize(); i++)
			clist->ChangeItem(i, ci.items[i]);
	}

	return found;
}


LogicInventoryManager::LogicInventoryManager() : InventoryManager()
{
}

LogicInventoryManager::~LogicInventoryManager()
{
    // Delete detached inventories
    for (auto& detachedInventory : mDetachedInventories)
        delete detachedInventory.second.inventory;
}

Inventory* LogicInventoryManager::GetInventory(const InventoryLocation& loc)
{
    switch (loc.type) 
    {
        case InventoryLocation::UNDEFINED:
        case InventoryLocation::CURRENT_PLAYER:
            break;
        case InventoryLocation::PLAYER: 
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(loc.name.c_str());
            if (!player)
                return NULL;
            PlayerLAO* playerLAO = player->GetPlayerLAO();
            if (!playerLAO)
                return NULL;
            return playerLAO->GetInventory();
        } 
        break;
        case InventoryLocation::NODEMETA: 
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(loc.nodePosition);
            if (!meta)
                return NULL;
            return meta->GetInventory();
        } 
        break;
        case InventoryLocation::DETACHED: 
        {
            auto it = mDetachedInventories.find(loc.name);
            if (it == mDetachedInventories.end())
                return nullptr;
            return it->second.inventory;
        } 
        break;
        default:
            LogAssert(false, "abort"); // abort
            break;
    }
    return NULL;
}

void LogicInventoryManager::SetInventoryModified(const InventoryLocation& loc)
{
    switch (loc.type) 
    {
        case InventoryLocation::UNDEFINED:
            break;
        case InventoryLocation::PLAYER: 
        {
            std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(loc.name.c_str());

            if (!player)
                return;

            player->SetModified(true);
            player->mInventory.SetModified(true);
            // Updates are sent in LogicEnvironment::step()
        } 
        break;
        case InventoryLocation::NODEMETA: 
        {
            MapEditEvent evt;
			evt.type = MEET_BLOCK_NODE_METADATA_CHANGED;
			evt.position = loc.nodePosition;
            mEnvironment->GetMap()->DispatchEvent(evt);
        } 
        break;
        case InventoryLocation::DETACHED: 
        {
            // Updates are sent in LogicEnvironment::step()
        } 
        break;

        default:
            LogAssert(false, "abort"); // abort
            break;
    }
}

Inventory* LogicInventoryManager::CreateDetachedInventory(
    const std::string& name, BaseItemManager* itemMgr, const std::string& player)
{
    if (mDetachedInventories.count(name) > 0) 
    {
        LogInformation("Clearing detached inventory " + name);
        delete mDetachedInventories[name].inventory;
    }
    else 
    {
        LogInformation("Creating detached inventory " + name);
    }

    Inventory* inv = new Inventory(itemMgr);
    LogAssert(inv, "invalid inventory");
    mDetachedInventories[name].inventory = inv;
    if (!player.empty()) 
    {
        mDetachedInventories[name].owner = player;

        if (!mEnvironment)
            return inv; // Mods are not loaded yet, ignore

        std::shared_ptr<LogicPlayer> p = mEnvironment->GetPlayer(name.c_str());

        // if player is connected, send him the inventory
        if (p && p->GetId() != INVALID_ACTOR_ID) 
            mEnvironment->SendDetachedInventory(inv, name, p->GetId());
    }
    else 
    {
        if (!mEnvironment)
            return inv; // Mods are not loaded yet, don't send

        // Inventory is for everybody, broadcast
        mEnvironment->SendDetachedInventory(inv, name, INVALID_ACTOR_ID);
    }

    return inv;
}

bool LogicInventoryManager::RemoveDetachedInventory(const std::string& name)
{
    const auto& invIt = mDetachedInventories.find(name);
    if (invIt == mDetachedInventories.end())
        return false;

    delete invIt->second.inventory;
    const std::string& owner = invIt->second.owner;

    if (!owner.empty()) 
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(owner.c_str());

        if (player && player->GetId() != INVALID_ACTOR_ID)
            mEnvironment->SendDetachedInventory(nullptr, name, player->GetId());

    }
    else 
    {
        // Notify all players about the change
        mEnvironment->SendDetachedInventory(nullptr, name, INVALID_ACTOR_ID);
    }

    mDetachedInventories.erase(invIt);

    return true;
}

bool LogicInventoryManager::CheckDetachedInventoryAccess(
    const InventoryLocation& loc, const std::string& player) const
{
    LogAssert(loc.type == InventoryLocation::DETACHED, "invalid inventory location type");

    const auto& invIt = mDetachedInventories.find(loc.name);
    if (invIt == mDetachedInventories.end())
        return false;

    return invIt->second.owner.empty() || invIt->second.owner == player;
}

void LogicInventoryManager::SendDetachedInventories(const std::string& actorName,
    bool incremental, std::function<void(const std::string &, Inventory *)> ApplyCB)
{
    for (const auto& detachedInventory : mDetachedInventories) 
    {
        const DetachedInventory& dInv = detachedInventory.second;
        if (incremental) 
        {
            if (!dInv.inventory || !dInv.inventory->CheckModified())
                continue;
        }

        // if we are pushing inventories to a specific player
        // we should filter to send only the right inventories
        if (!actorName.empty())
        {
            const std::string& attachedPlayer = dInv.owner;
            if (!attachedPlayer.empty() && actorName != attachedPlayer)
                continue;
        }

        ApplyCB(detachedInventory.first, detachedInventory.second.inventory);
    }
}
