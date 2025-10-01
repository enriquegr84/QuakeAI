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


#ifndef DATABASE_H
#define DATABASE_H

#include "GameEngineStd.h"

#include "Mathematic/Algebra/Vector3.h"

#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>

class PlayerLAO;

namespace CerealTypes
{
    struct Block
    {
        int64_t position;
        std::string blob;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(position, blob);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(position, blob);
        }
    };

    struct Map
    {
        std::vector<Block> blocks;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(blocks);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(blocks);
        }
    };

    struct Item
    {
        int slotid;
        std::string data;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(slotid, data);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(slotid, data);
        }
    };

    struct Inventory
    {
        int id;
        int size;
        int width;
        std::string name;
        std::vector<Item> items;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(id, size, width, name, items);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(id, size, width, name, items);
        }
    };

    struct Metadata
    {
        std::string name;
        std::string value;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(name, value);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(name, value);
        }
    };

    struct Player
    {
        std::string name;
        float pitch, yaw;
        float posx, posy, posz;
        int health, breath;
        std::vector<Inventory> inventories;
        std::vector<Metadata> metadatas;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(name, pitch, yaw, posx, posy, posz, health, breath, inventories, metadatas);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(name, pitch, yaw, posx, posy, posz, health, breath, inventories, metadatas);
        }
    };

    struct Players
    {
        std::vector<Player> players;

        template <class Archive>
        void save(Archive& ar) const
        {
            ar(players);
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(players);
        }
    };
}

class Database
{
public:
    virtual void BeginSave() { }
    virtual void EndSave() { }
	virtual bool Initialized() const { return true; }
};

class MapDatabase : public Database
{
public:
    MapDatabase(const std::string& savedir, const std::string& dbname);
	virtual ~MapDatabase() = default;

	virtual bool SaveBlock(const Vector3<short>& pos, const std::string& data);
	virtual void LoadBlock(const Vector3<short>& pos, std::string* block);
    virtual void LoadMap(const std::string& path);
    virtual void SaveMap(const std::string& path);
	virtual bool DeleteBlock(const Vector3<short>& pos);

	static int64_t GetBlockAsInteger(const Vector3<short>& pos);
	static Vector3<short> GetIntegerAsBlock(int64_t i);

	virtual void ListAllLoadableBlocks(std::vector<Vector3<short>>& dst);

private:

    std::string mSavedir = "";
    std::string mDBname = "";

    //set data
    CerealTypes::Map mData;
    std::map<int64_t, std::string> mLookupData;
};

class PlayerDatabase
{
public:
	virtual ~PlayerDatabase() = default;

	virtual void SavePlayer(PlayerLAO* player);
	virtual bool LoadPlayer(PlayerLAO* player);
    virtual void SavePlayers(const std::string& path);
    virtual void LoadPlayers(const std::string& path);
	virtual bool RemovePlayer(PlayerLAO* player);
	virtual void ListPlayers(std::vector<std::string>& res);

private:

    //set data
    CerealTypes::Players mData;
};

struct AuthEntry
{
	uint64_t id;
	std::string name;
    std::string password;
    std::vector<std::string> privileges;
	int64_t lastLogin;
};

class AuthDatabase
{
public:
	virtual ~AuthDatabase() = default;

	virtual bool GetAuth(const std::string& name, AuthEntry &res);
	virtual bool SaveAuth(const AuthEntry& authEntry);
	virtual bool CreateAuth(AuthEntry& authEntry);
	virtual bool DeleteAuth(const std::string& name);
	virtual void ListNames(std::vector<std::string>& res);
	virtual void Reload();
};

#endif