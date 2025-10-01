/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "Database.h"

#include "../Games/Actors/LogicPlayer.h"
#include "../Games/Actors/PlayerLAO.h"

#include "Core/Logger/Logger.h"

#include <fstream>

#include <cassert>

/****************
 * Black magic! *
 ****************
 * The position hashing is very messed up.
 * It's a lot more complicated than it looks.
 */

static inline short UnsignedToSigned(short i, short maxPositive)
{
	if (i < maxPositive)
		return i;

	return i - (maxPositive * 2);
}


// Modulo of a negative number does not work consistently in C
static inline int64_t PythonModulo(int64_t i, short mod)
{
	if (i >= 0)
		return i % mod;

	return mod - ((-i) % mod);
}


int64_t MapDatabase::GetBlockAsInteger(const Vector3<short> &pos)
{
	return (int64_t) pos[2] * 0x1000000 +
		(int64_t) pos[1] * 0x1000 +
		(int64_t) pos[0];
}


Vector3<short> MapDatabase::GetIntegerAsBlock(int64_t i)
{
    Vector3<short> pos;
	pos[0] = UnsignedToSigned((short)PythonModulo(i, 4096), 2048);
	i = (i - pos[0]) / 4096;
	pos[1] = UnsignedToSigned((short)PythonModulo(i, 4096), 2048);
	i = (i - pos[1]) / 4096;
	pos[2] = UnsignedToSigned((short)PythonModulo(i, 4096), 2048);
	return pos;
}


MapDatabase::MapDatabase(const std::string& savedir, const std::string& dbname) :
    mSavedir(savedir), mDBname(dbname)
{

}


bool MapDatabase::SaveBlock(const Vector3<short>& pos, const std::string& data)
{
    int64_t blockPos = GetBlockAsInteger(pos);
    CerealTypes::Block block;

    if (mLookupData.find(blockPos) != mLookupData.end())
    {
        std::vector<CerealTypes::Block>::iterator itBlock;
        for (itBlock = mData.blocks.begin(); itBlock != mData.blocks.end(); itBlock++)
        {
            CerealTypes::Block& bl = (*itBlock);
            if (bl.position == blockPos)
                bl.blob = data;
        }

        mLookupData[blockPos] = data;
        return true;
    }

    block.position = blockPos;
    block.blob = data;
    mData.blocks.push_back(block);
    mLookupData[blockPos] = data;
    return true;
}

void MapDatabase::LoadBlock(const Vector3<short>& pos, std::string* block)
{
    int64_t blockPos = GetBlockAsInteger(pos);

    *block = "";
    std::map<int64_t, std::string>::iterator itData = mLookupData.find(blockPos);
    if (itData != mLookupData.end())
        *block = (*itData).second;
}

void MapDatabase::SaveMap(const std::string& path)
{
    std::ofstream os(path, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    archive(mData);
}

void MapDatabase::LoadMap(const std::string& path)
{
    std::ifstream is(path, std::ios::binary);
    if (is.fail())
    {
        LogError(strerror(errno));
        return;
    }
    cereal::BinaryInputArchive archive(is);
    archive(mData);

    for (CerealTypes::Block block : mData.blocks)
        mLookupData[block.position] = block.blob;
}

bool MapDatabase::DeleteBlock(const Vector3<short>& pos)
{
    int64_t blockPos = GetBlockAsInteger(pos);

    std::vector<CerealTypes::Block>::iterator itBlock;
    for (itBlock = mData.blocks.begin(); itBlock != mData.blocks.end(); itBlock++)
    {
        CerealTypes::Block& bl = (*itBlock);
        if (bl.position == blockPos)
        {
            mData.blocks.erase(itBlock);
            mLookupData.erase(blockPos);
            return true;
        }
    }

    return false;
}

void MapDatabase::ListAllLoadableBlocks(std::vector<Vector3<short>>& dst)
{
    for (CerealTypes::Block block : mData.blocks)
        dst.push_back(GetIntegerAsBlock(block.position));
}

void PlayerDatabase::SavePlayer(PlayerLAO* playerLAO)
{
    LogAssert(playerLAO, "invalid player");

    const Vector3<float>& pos = playerLAO->GetBasePosition();

    std::string playerName = playerLAO->GetPlayer()->GetName();
    auto itPlayer =  std::find_if(mData.players.begin(), mData.players.end(), 
        [&playerName](CerealTypes::Player const& player) { return playerName == player.name; });
    CerealTypes::Player playerData;
    if (itPlayer == mData.players.end())
    {
        playerData = CerealTypes::Player();
        mData.players.push_back(playerData);
    }
    else CerealTypes::Player playerData = (*itPlayer);

    playerData.name = playerLAO->GetPlayer()->GetName();
    playerData.pitch = playerLAO->GetLookPitch();
    playerData.yaw = playerLAO->GetRotation()[1];
    playerData.posx = pos[0];
    playerData.posy = pos[1];
    playerData.posz = pos[2];
    playerData.health = playerLAO->GetHP();
    playerData.breath = playerLAO->GetBreath();

    playerData.inventories.clear();
    std::vector<const InventoryList*> inventoryLists = playerLAO->GetInventory()->GetLists();
    for (unsigned int i = 0; i < inventoryLists.size(); i++)
    {
        const InventoryList* list = inventoryLists[i];

        CerealTypes::Inventory playerInventory;
        playerInventory.id = i;
        playerInventory.width = list->GetWidth();
        playerInventory.name = list->GetName();
        playerInventory.size = list->GetSize();

        for (unsigned int j = 0; j < list->GetSize(); j++) 
        {
            std::ostringstream os;
            list->GetItem(j).Serialize(os);
            std::string itemStr = os.str();

            CerealTypes::Item item;
            item.slotid = j;
            item.data = itemStr;
            playerInventory.items.push_back(item);
        }

        playerData.inventories.push_back(playerInventory);
    }

    const StringMap& attrs = playerLAO->GetMeta().GetStrings();
    for (const auto& attr : attrs)
    {
        CerealTypes::Metadata metadata;
        metadata.name = attr.first;
        metadata.value = attr.second;
        playerData.metadatas.push_back(metadata);
    }
}

bool PlayerDatabase::LoadPlayer(PlayerLAO* playerLAO)
{
    std::string playerName = playerLAO->GetPlayer()->GetName();
    auto itPlayer = std::find_if(mData.players.begin(), mData.players.end(),
        [&playerName](CerealTypes::Player const& player) { return playerName == player.name; });
    if (itPlayer == mData.players.end())
        return false;

    CerealTypes::Player playerData = (*itPlayer);
    playerLAO->SetLookPitch(playerData.pitch);
    playerLAO->SetPlayerYaw(playerData.yaw);
    playerLAO->SetBasePosition(
        Vector3<float>{playerData.posx, playerData.posy, playerData.posz});
    playerLAO->SetHPRaw(std::min(playerLAO->GetHP(), (uint16_t)0xFFFF));
    playerLAO->SetBreath(std::min(playerLAO->GetBreath(), (uint16_t)0xFFFF), false);

    for (CerealTypes::Inventory playerInventory : playerData.inventories)
    {
        InventoryList *invList = playerLAO->GetPlayer()->mInventory.AddList(playerInventory.name, playerInventory.size);
        invList->SetWidth(playerInventory.width);

        for (CerealTypes::Item invItem : playerInventory.items)
        {
            ItemStack stack;
            stack.Deserialize(invItem.data);
            invList->ChangeItem(invItem.slotid, stack);
        }
    }

    for (CerealTypes::Metadata metadata : playerData.metadatas)
        playerLAO->GetMeta().SetString(metadata.name, metadata.value);

    return true;
}

void PlayerDatabase::SavePlayers(const std::string& path)
{
    std::ofstream os(path, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    archive(mData);
}

void PlayerDatabase::LoadPlayers(const std::string& path)
{
    std::ifstream is(path, std::ios::binary);
    if (is.fail())
    {
        LogError(strerror(errno));
        return;
    }
    cereal::BinaryInputArchive archive(is);
    archive(mData);
}

bool PlayerDatabase::RemovePlayer(PlayerLAO* playerLAO)
{
    std::vector<CerealTypes::Player>::iterator itPlayer;
    for (itPlayer = mData.players.begin(); itPlayer != mData.players.end(); itPlayer++)
    {
        CerealTypes::Player pl = (*itPlayer);
        if (pl.name == playerLAO->GetPlayer()->GetName())
        {
            mData.players.erase(itPlayer);
            return true;
        }
    }

    return false;
}

void PlayerDatabase::ListPlayers(std::vector<std::string>& res)
{
    for (CerealTypes::Player player : mData.players)
        res.push_back(player.name);
}


bool AuthDatabase::GetAuth(const std::string& name, AuthEntry &res)
{
    return true;
}

bool AuthDatabase::SaveAuth(const AuthEntry& authEntry)
{
    return true;
}

bool AuthDatabase::CreateAuth(AuthEntry& authEntry)
{
    return true;
}

bool AuthDatabase::DeleteAuth(const std::string& name)
{
    return true;
}

void AuthDatabase::ListNames(std::vector<std::string>& res)
{

}

void AuthDatabase::Reload()
{

}


