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

#include "../Games/Actors/PlayerActor.h"

#include "Game/Actor/TransformComponent.h"

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

void PlayerDatabase::SavePlayer(const std::shared_ptr<PlayerActor>& player)
{
    ActorId playerId = player->GetId();
    auto itPlayer =  std::find_if(mData.players.begin(), mData.players.end(), 
        [&playerId](CerealTypes::Player const& player) { return playerId == player.id; });
    CerealTypes::Player playerData;
    if (itPlayer == mData.players.end())
    {
        playerData = CerealTypes::Player();
        mData.players.push_back(playerData);
    }
    else CerealTypes::Player playerData = (*itPlayer);

    std::shared_ptr<TransformComponent> pPlayerTransform(
        player->GetComponent<TransformComponent>(TransformComponent::Name).lock());
    EulerAngles<float> viewAngles;
    viewAngles.mAxis[1] = 1;
    viewAngles.mAxis[2] = 2;
    pPlayerTransform->GetTransform().GetRotation(viewAngles);

    playerData.id = player->GetId();
    playerData.pitch = viewAngles.mAngle[1];
    playerData.yaw = viewAngles.mAngle[2];
    playerData.posx = pPlayerTransform->GetPosition()[0];
    playerData.posy = pPlayerTransform->GetPosition()[1];
    playerData.posz = pPlayerTransform->GetPosition()[2];
    playerData.health = player->GetState().stats[STAT_HEALTH];
    playerData.armor = player->GetState().stats[STAT_ARMOR];

    playerData.inventories.clear();
    CerealTypes::Inventory playerInventory;
    playerInventory.id = 0;
    playerInventory.name = "weapons";
    playerInventory.size = MAX_WEAPONS;
    playerData.inventories.push_back(playerInventory);
    for (unsigned int i = 0; i < MAX_WEAPONS; i++)
    {
        if (player->GetState().stats[STAT_WEAPONS] & (1 << i))
        {
            CerealTypes::Item item;
            item.id = i;
            item.amount = player->GetState().ammo[i];
            playerInventory.items.push_back(item);
        }
    }
}

bool PlayerDatabase::LoadPlayer(const std::shared_ptr<PlayerActor>& player)
{
    ActorId playerId = player->GetId();
    auto itPlayer = std::find_if(mData.players.begin(), mData.players.end(),
        [&playerId](CerealTypes::Player const& player) { return playerId == player.id; });
    if (itPlayer == mData.players.end())
        return false;

    std::shared_ptr<TransformComponent> pPlayerTransform(
        player->GetComponent<TransformComponent>(TransformComponent::Name).lock());

    CerealTypes::Player playerData = (*itPlayer);
    EulerAngles<float> viewAngles;
    viewAngles.mAxis[1] = 1;
    viewAngles.mAxis[2] = 2;
    viewAngles.mAngle[1] = playerData.pitch;
    viewAngles.mAngle[2] = playerData.yaw;
    pPlayerTransform->GetTransform().SetRotation(viewAngles);
    pPlayerTransform->SetPosition(
        Vector3<float>{playerData.posx, playerData.posy, playerData.posz});
    player->GetState().stats[STAT_HEALTH] = playerData.health;
    player->GetState().stats[STAT_ARMOR] = playerData.armor;

    CerealTypes::Inventory inventory = playerData.inventories[0];
    for (CerealTypes::Item item : inventory.items)
    {
        player->GetState().stats[STAT_WEAPONS] |= (1 << item.id);
        player->GetState().ammo[item.id] = item.amount;
    }

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

bool PlayerDatabase::RemovePlayer(const std::shared_ptr<PlayerActor>& player)
{
    std::vector<CerealTypes::Player>::iterator itPlayer;
    for (itPlayer = mData.players.begin(); itPlayer != mData.players.end(); itPlayer++)
    {
        CerealTypes::Player pl = (*itPlayer);
        if (pl.id == player->GetId())
        {
            mData.players.erase(itPlayer);
            return true;
        }
    }

    return false;
}

void PlayerDatabase::ListPlayers(std::vector<ActorId>& res)
{
    for (CerealTypes::Player player : mData.players)
        res.push_back(player.id);
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


