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

#include "Games.h"

#include "Environment/LogicEnvironment.h"

#include "Actors/EntityLAO.h"

#include "../MinecraftEvents.h"

#include "../Utils/PointedThing.h"

#include "Forms/Death.h"

#include "Application/Settings.h"

bool StringToEnum(const EnumString* spec, int& result, const std::string& str)
{
    const EnumString* esp = spec;
    while (esp->str)
    {
        if (!strcmp(str.c_str(), esp->str))
        {
            result = esp->num;
            return true;
        }
        esp++;
    }
    return false;
}

unsigned int AddHud(LogicPlayer* player)
{
    //Fixed offset in config file
    Vector2<float> offset{ 0, -101 };

    //Dirty trick to avoid collision with Minetest's status text (e.g. “Volume changed to 0%”)
    if (offset[1] >= -167 && offset[1] <= -156)
        offset[1] = -181;

    HudElement* form = new HudElement();
    form->type = HudElementType::HUD_ELEM_TEXT;
    form->position = { 0.5f, 1.f };
    form->offset = offset;
    form->align = { 0.f, 0.f };
    form->number = 0xFFFFFF;

    unsigned int id = player->AddHud(form);
    EventManager::Get()->QueueEvent(std::make_shared<EventDataHudAdd>(
        id, (uint8_t)form->type, form->position, form->name, form->scale, form->text,
        form->number, form->item, form->direction, form->align, form->offset,
        form->worldPosition, form->size, form->zIndex, form->text2));

    return id;
}

void ChangeHud(unsigned int id, HudElement* elem, const std::string& statstr)
{
    if (elem)
    {
        int statint;
        HudElementStat stat = HUD_STAT_NUMBER;
        stat = StringToEnum(ESHudElementStat, statint, statstr) ? (HudElementStat)statint : stat;

        void* value = nullptr;
        switch (stat)
        {
            case HUD_STAT_POS:
                value = &elem->position;
                break;
            case HUD_STAT_NAME:
                value = &elem->name;
                break;
            case HUD_STAT_SCALE:
                value = &elem->scale;
                break;
            case HUD_STAT_TEXT:
                value = &elem->text;
                break;
            case HUD_STAT_NUMBER:
                value = &elem->number;
                break;
            case HUD_STAT_ITEM:
                if (elem->type == HUD_ELEM_WAYPOINT && statstr == "precision")
                    elem->item++;
                value = &elem->item;
                break;
            case HUD_STAT_DIR:
                value = &elem->direction;
                break;
            case HUD_STAT_ALIGN:
                value = &elem->align;
                break;
            case HUD_STAT_OFFSET:
                value = &elem->offset;
                break;
            case HUD_STAT_WORLD_POS:
                value = &elem->worldPosition;
                break;
            case HUD_STAT_SIZE:
                value = &elem->size;
                break;
            case HUD_STAT_Z_INDEX:
                value = &elem->zIndex;
                break;
            case HUD_STAT_TEXT2:
                value = &elem->text2;
                break;
        }

        EventManager::Get()->QueueEvent(std::make_shared<EventDataHudChange>(id, stat, value));
    }
}

StatBars::StatBars()
{
    //cache setting
    mEnableDamage = Settings::Get()->GetBool("enable_damage");

    mHealthBar = new HudElement();
    mHealthBar->type = HUD_ELEM_STATBAR;
    mHealthBar->position = { 0.5f, 1.f };
    mHealthBar->text = "heart.png";
    mHealthBar->text2 = "heart_gone.png";
    mHealthBar->number = PLAYER_MAX_HP_DEFAULT;
    mHealthBar->item = PLAYER_MAX_HP_DEFAULT;
    mHealthBar->direction = 0;
    mHealthBar->size = { 24, 24 };
    mHealthBar->offset = { (-10 * 24) - 25, -(48 + 24 + 16) };

    mBreathBar = new HudElement();
    mBreathBar->type = HUD_ELEM_STATBAR;
    mBreathBar->position = { 0.5f, 1.f };
    mBreathBar->text = "bubble.png";
    mBreathBar->text2 = "bubble_gone.png";
    mBreathBar->number = PLAYER_MAX_BREATH_DEFAULT;
    mBreathBar->item = PLAYER_MAX_BREATH_DEFAULT;
    mBreathBar->direction = 0;
    mBreathBar->size = { 24, 24 };
    mBreathBar->offset = { 25, -(48 + 24 + 16) };
}

StatBars:: ~StatBars() 
{
    delete mHealthBar;
    delete mBreathBar;
}

unsigned int StatBars::ScaleToDefault(PlayerLAO* player, std::string field)
{
    if (field == "breath")
    {
        //Scale "hp" or "breath" to the default dimensions
        unsigned short maxBreath = std::max((unsigned short)PLAYER_MAX_BREATH_DEFAULT,
            std::max(player->AccessObjectProperties()->breathMax, player->GetBreath()));
        return player->GetBreath() / maxBreath * PLAYER_MAX_BREATH_DEFAULT;
    }
    else
    {
        //Scale "hp" or "breath" to the default dimensions
        unsigned short maxHP = std::max((unsigned short)PLAYER_MAX_HP_DEFAULT,
            std::max(player->AccessObjectProperties()->hpMax, player->GetHP()));
        return player->GetHP() / maxHP * PLAYER_MAX_HP_DEFAULT;
    }
}

void StatBars::Update(PlayerLAO* playerLAO)
{
    LogicPlayer* player = playerLAO->GetPlayer();
    if (player->GetName() == "")
        return;

    if (std::find(mHudIds.begin(), mHudIds.end(), player->GetName()) == mHudIds.end())
    {
        mHudIds.push_back(player->GetName());
        //flags are not transmitted to visual on connect, we need to make sure
        //our current flags are transmitted by sending them actively
        unsigned int flags = 0;
        unsigned int mask = 0;

        const EnumString* esp = ESHudBuiltinElement;
        for (int i = 0; esp[i].str; i++)
        {
            flags |= esp[i].num;
            mask |= esp[i].num;
        }

        unsigned int setFlags = flags;
        setFlags &= ~(HUD_FLAG_HEALTHBAR_VISIBLE | HUD_FLAG_BREATHBAR_VISIBLE);
        
        EventManager::Get()->QueueEvent(std::make_shared<EventDataHudSetFlags>(mask, setFlags));

        player->mHudFlags &= ~mask;
        player->mHudFlags |= flags;
    }

    if ((player->mHudFlags & HUD_FLAG_HEALTHBAR_VISIBLE) && mEnableDamage && !playerLAO->IsImmortal())
    {
        unsigned int number = ScaleToDefault(playerLAO, "hp");
        if (mHealthBarIds.find(player->GetName()) == mHealthBarIds.end())
        {
            HudElement* elem = new HudElement();
            elem->type = mHealthBar->type;
            elem->name = mHealthBar->name;
            elem->scale = mHealthBar->scale;
            elem->text = mHealthBar->text;
            elem->number = number;
            if (elem->type == HUD_ELEM_WAYPOINT)
            {
                // waypoints reuse the item field to store precision, item = precision + 1
                elem->item = mHealthBar->item;
            }
            else elem->item = mHealthBar->item;
            elem->direction = mHealthBar->direction;
            elem->size = { mHealthBar->size[0], mHealthBar->size[1] };
            elem->align = { mHealthBar->align[0], mHealthBar->align[1] };
            elem->offset = { mHealthBar->offset[0], mHealthBar->offset[1] };
            elem->position = { mHealthBar->position[0], mHealthBar->position[1] };
            elem->worldPosition = { mHealthBar->worldPosition[0], mHealthBar->worldPosition[1] };
            elem->zIndex = mHealthBar->zIndex;
            elem->text2 = mHealthBar->text2;

            mHealthBarIds[player->GetName()] = player->AddHud(elem);
            EventManager::Get()->QueueEvent(std::make_shared<EventDataHudAdd>(
                mHealthBarIds[player->GetName()], (uint8_t)elem->type, elem->position, 
                elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction, 
                elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
        }
        else
        {
            unsigned int id = mHealthBarIds[player->GetName()];
            HudElement* elem = player->GetHud(id);
            if (elem)
                ChangeHud(id, elem, "number");
        }
    }
    else if (mHealthBarIds.find(player->GetName()) != mHealthBarIds.end())
    {
        HudElement* removeHud = player->RemoveHud(mHealthBarIds[player->GetName()]);
        if (removeHud)
        {
            delete removeHud;

            EventManager::Get()->QueueEvent(
                std::make_shared<EventDataHudRemove>(mHealthBarIds[player->GetName()]));
        }
        mHealthBarIds.erase(player->GetName());
    }

    bool showBreathBar =
        (player->mHudFlags & HUD_FLAG_BREATHBAR_VISIBLE) && mEnableDamage && !playerLAO->IsImmortal();
    if (showBreathBar && playerLAO->GetBreath() <= playerLAO->AccessObjectProperties()->breathMax)
    {
        unsigned int number = ScaleToDefault(playerLAO, "breath");
        if (mBreathBarIds.find(player->GetName()) == mBreathBarIds.end() &&
            playerLAO->GetBreath() <= playerLAO->AccessObjectProperties()->breathMax)
        {
            HudElement* elem = new HudElement();
            elem->type = mBreathBar->type;
            elem->name = mBreathBar->name;
            elem->scale = mBreathBar->scale;
            elem->text = mBreathBar->text;
            elem->number = number;
            if (elem->type == HUD_ELEM_WAYPOINT)
            {
                // waypoints reuse the item field to store precision, item = precision + 1
                elem->item = mBreathBar->item;
            }
            else elem->item = mBreathBar->item;
            elem->direction = mBreathBar->direction;
            elem->size = { mBreathBar->size[0], mBreathBar->size[1] };
            elem->align = { mBreathBar->align[0], mBreathBar->align[1] };
            elem->offset = { mBreathBar->offset[0], mBreathBar->offset[1] };
            elem->position = { mBreathBar->position[0], mBreathBar->position[1] };
            elem->worldPosition = { mBreathBar->worldPosition[0], mBreathBar->worldPosition[1] };
            elem->zIndex = mBreathBar->zIndex;
            elem->text2 = mBreathBar->text2;

            mBreathBarIds[player->GetName()] = player->AddHud(elem);
            EventManager::Get()->QueueEvent(std::make_shared<EventDataHudAdd>(
                mBreathBarIds[player->GetName()], (uint8_t)elem->type, elem->position,
                elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
                elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
        }
        else if (mBreathBarIds.find(player->GetName()) != mBreathBarIds.end())
        {
            unsigned int id = mBreathBarIds[player->GetName()];
            HudElement* elem = player->GetHud(id);
            if (elem)
                ChangeHud(id, elem, "number");
        }
    }

    if (mBreathBarIds.find(player->GetName()) != mBreathBarIds.end() &&
        (!showBreathBar || playerLAO->GetBreath() == playerLAO->AccessObjectProperties()->breathMax))
    {
        //core.after(1, function(player_name, breath_bar), name, hud.id_breathbar)
        HudElement* removeHud = player->RemoveHud(mBreathBarIds[player->GetName()]);
        if (removeHud)
        {
            delete removeHud;

            EventManager::Get()->QueueEvent(
                std::make_shared<EventDataHudRemove>(mBreathBarIds[player->GetName()]));
        }
        mBreathBarIds.erase(player->GetName());
    }
}

void StatBars::Cleanup(PlayerLAO* playerLAO)
{
    LogicPlayer* player = playerLAO->GetPlayer();
    if (player->GetName() == "")
        return;

    mHealthBarIds.erase(player->GetName());
    mBreathBarIds.erase(player->GetName());
    mHudIds.erase(std::remove(mHudIds.begin(), mHudIds.end(), player->GetName()), mHudIds.end());
}

bool StatBars::EventHandler(PlayerLAO* playerLAO, std::string eventName)
{
    LogicPlayer* player = playerLAO->GetPlayer();
    if (player->GetName() == "" ||
        std::find(mHudIds.begin(), mHudIds.end(), player->GetName()) == mHudIds.end())
    {
        return false;
    }

    if (eventName == "health_changed")
    {
        Update(playerLAO);

        if (mHealthBarIds.find(player->GetName()) != mHealthBarIds.end())
            return true;
    }

    if (eventName == "breath_changed")
    {
        Update(playerLAO);

        if (mBreathBarIds.find(player->GetName()) != mBreathBarIds.end())
            return true;
    }
    if (eventName == "hud_changed" || eventName == "properties_changed")
    {
        Update(playerLAO);
        return true;
    }

    return false;
}

bool StatBars::ReplaceHud(HudElement* hud, std::string hudName)
{
    return false;
}

BaseGame* BaseGame::mGame = NULL;

BaseGame* BaseGame::Get(void)
{
    LogAssert(BaseGame::mGame, "Game doesn't exist");
    return BaseGame::mGame;
}

BaseGame::BaseGame(LogicEnvironment* env) : mEnvironment(env)
{
    if (BaseGame::mGame)
    {
        LogError("Attempting to create two global game! \
					The old one will be destroyed and overwritten with this one.");
        delete BaseGame::mGame;
    }

    mStatBars = std::make_shared<StatBars>();

    //If item_entity_ttl is not set, enity will have default life time
    mTimeToLive = Settings::Get()->GetFloat("item_entity_ttl");
    mGravity = Settings::Get()->GetFloat("movement_gravity");

    mCheckForFallingNeighbors = {
        {-1, -1, 0},
        {1, -1, 0},
        {0, -1, -1},
        {0, -1, 1},
        {0, -1, 0},
        {-1, 0, 0},
        {1, 0, 0},
        {0, 0, 1},
        {0, 0, -1},
        {0, 0, 0},
        {0, 1, 0}};

    mFacedirToEuler = {
        {0.f, 0.f, 0.f},
        {0.f, -(float)GE_C_HALF_PI, 0.f},
        {0.f, (float)GE_C_PI, 0.f},
        {0.f, (float)GE_C_HALF_PI, 0.f},
        {-(float)GE_C_HALF_PI, (float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_PI, (float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_HALF_PI, (float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {0.f, (float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_HALF_PI, -(float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {0.f, -(float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {-(float)GE_C_HALF_PI, -(float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_PI, -(float)GE_C_HALF_PI, (float)GE_C_HALF_PI},
        {0.f, 0.f, (float)GE_C_HALF_PI},
        {-(float)GE_C_HALF_PI, 0.f, (float)GE_C_HALF_PI},
        {(float)GE_C_PI, 0.f, (float)GE_C_HALF_PI},
        {(float)GE_C_HALF_PI, 0.f, (float)GE_C_HALF_PI},
        {(float)GE_C_PI, (float)GE_C_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_HALF_PI, (float)GE_C_PI, (float)GE_C_HALF_PI},
        {0.f, (float)GE_C_PI, (float)GE_C_HALF_PI},
        {-(float)GE_C_HALF_PI, (float)GE_C_PI, (float)GE_C_HALF_PI},
        {(float)GE_C_PI, (float)GE_C_PI, 0.f},
        {(float)GE_C_PI, -(float)GE_C_HALF_PI, 0.f},
        {(float)GE_C_PI, 0.f, 0.f},
        {(float)GE_C_PI, (float)GE_C_HALF_PI, 0.f}
    };

    mWallmountedToDirection = {
        {0, 1, 0},
        {0, -1, 0},
        {1, 0, 0},
        {-1, 0, 0},
        {0, 0, 1},
        {0, 0, -1},
    };

    BaseGame::mGame = this;
}

BaseGame::~BaseGame()
{
    if (BaseGame::mGame == this)
        BaseGame::mGame = nullptr;
}

void BaseGame::OnDeath()
{
    /*
        core.register_on_death(function()
    */
    std::string form = "size[11,5.5]bgcolor[#320000b4;true]"
        "label[4.85,1.35;You died"
        "]button_exit[4,3;3,0.5;btn_respawn;Respawn]";
    std::string formName = "Death";

    EventManager::Get()->QueueEvent(std::make_shared<EventDataShowForm>(form, formName));
}

bool BaseGame::RotateAndPlace(ItemStack& stack, LogicActiveObject* placer,
    const PointedThing& pointed, bool infiniteStacks, bool invertWall, bool preventAfterPlace)
{
    /*
        function core.rotate_and_place(itemstack, placer, pointed_thing,
                infinitestacks, orient_flags, prevent_after_place)
    */

    MapNode underNode = mEnvironment->GetMap()->GetNode(pointed.nodeUndersurface);
    if (underNode.GetContent() == CONTENT_IGNORE)
        return false;

    const ContentFeatures& cFeaturesUnder = mEnvironment->GetNodeManager()->Get(underNode);
    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(placer->GetId());
    if (!player->GetPlayerControl().sneak && cFeaturesUnder.rightClickable)
        if (OnRightClickNode(pointed.nodeUndersurface, underNode))
            return true;

    float pitch = player->GetPlayerLAO()->GetRadLookPitchDep();
    float yaw = player->GetPlayerLAO()->GetRadYawDep();
    Vector3<int> lookDirection{ 
        (int)round(std::cos(pitch) * std::cos(yaw)), 
        (int)round(std::sin(pitch)), 
        (int)round(std::cos(pitch) * std::sin(yaw)) };
    uint8_t faceDirection = DirectionToFaceDirection(lookDirection);

    bool isWall = pointed.nodeAbovesurface[1] == pointed.nodeUndersurface[1];
    bool isCeiling = !isWall && (pointed.nodeAbovesurface[1] < pointed.nodeUndersurface[1]);

    if (cFeaturesUnder.buildableTo)
        isWall = false;

    if (invertWall)
        isWall = !isWall;

    Vector4<int> dirs1 = { 9, 18, 7, 12 };
    Vector4<int> dirs2 = { 20, 23, 22, 21 };
    uint8_t param2 = faceDirection;
    if (isWall)
        param2 = dirs1[faceDirection];
    else if (isCeiling)
        param2 = dirs2[faceDirection];

    ItemStack oldItemStack, newItemStack;
    oldItemStack.Deserialize(stack.name, mEnvironment->GetItemManager());
    newItemStack = ItemPlaceNode(stack, placer, pointed, param2, preventAfterPlace);
    return true;
}

void BaseGame::ReportMetadataChange(MapNodeMetadata* meta, const Vector3<short>& pos, const std::string& name)
{
    MapEditEvent evt;
    evt.type = MEET_BLOCK_NODE_METADATA_CHANGED;
    evt.position = pos;
    evt.IsPrivateChange = meta && meta->IsPrivate(name);
    mEnvironment->GetMap()->DispatchEvent(evt);
}


void BaseGame::HandleNodeDrops(const Vector3<short>& pos, const std::vector<std::string>& drops, PlayerLAO* digger)
{
    /*
        function core.handle_node_drops(pos, drops, digger)
    */

    // Add dropped items to object's inventory
    Inventory* inventory = digger->GetInventory();

    for (std::string droppedItem : drops)
    {
        ItemStack item;
        item.Deserialize(droppedItem, mEnvironment->GetItemManager());
        if (inventory)
        {
            ItemStack leftOver = inventory->AddItem("main", item);
            if (leftOver.count != item.count)
            {
                // Inform other things that the inventory has changed
                mEnvironment->GetInventoryManager()->SetInventoryModified(digger->GetInventoryLocation());
            }
            item = leftOver;
        }

        if (!item.IsEmpty())
        {
            Vector3<float> itemPos{
                (float)pos[0] + Randomizer::FRand() / 2.f - 0.25f,
                (float)pos[1] + Randomizer::FRand() / 2.f - 0.25f,
                (float)pos[2] + Randomizer::FRand() / 2.f - 0.25f };
            itemPos *= BS;

            if (item.IsEmpty() || !item.IsKnown(mEnvironment->GetItemManager()))
                continue;

            EntityLAO* obj = new EntityLAO(mEnvironment, itemPos, "__builtin:item", "");
            obj->GetInventoryLocation();
            int objId = mEnvironment->AddActiveObject(obj);
            // If failed to add, return nothing (reads as nil)
            if (objId == 0)
                continue;

            // If already deleted (can happen in on_activate), return nil
            if (obj->IsGone())
                continue;

            SetItem(obj, droppedItem);
        }
    }
}


bool BaseGame::DigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger)
{
    /*
        function core.node_dig(pos, node, digger)
    */

    if (node.GetContent() == CONTENT_IGNORE)
        return false;

    // Copy pos because the callback could modify it
    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (!cFeatures.diggable)
    {
        LogInformation("Tried to dig " + cFeatures.name + "which is not diggable" + 
            std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));
        return false;
    }

    if (IsProtected(pos, digger->GetPlayer()))
    {
        LogInformation("Tried to dig " + cFeatures.name + "at protected position" +
            std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));
        return false;
    }

    LogInformation("Player digs " + cFeatures.name + "at " +
        std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));

    ItemStack wielded;
    digger->GetWieldedItem(&wielded);

    std::vector<std::string> drops;
    GetNodeDrops(node, wielded.name, drops);
    Item wieldedItem = wielded.GetDefinition(mEnvironment->GetItemManager());
    ToolCapabilities toolCap = wielded.GetToolCapabilities(mEnvironment->GetItemManager());
    DigParams digParams = GetDigParams(cFeatures.groups, &toolCap);

    // Wear out tool
    if (Settings::Get()->GetBool("creative_mode"))
        wielded.AddWear(digParams.wear, mEnvironment->GetItemManager());
    digger->SetWieldedItem(wielded);

    // Handle drops
    HandleNodeDrops(pos, drops, digger);

    // Remove node and update
    mEnvironment->RemoveNode(pos);

    // Play sound if it was done by a player
    if (cFeatures.soundDug.Exists())
    {
        SoundParams params;
        params.type = SoundParams::SP_POSITIONAL;
        params.position = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
        params.excludePlayer = digger->GetPlayer()->GetName();
        mEnvironment->PlaySound(cFeatures.soundDug, params, false);
    }

    return true;
}

void BaseGame::DropAttachedNode(const Vector3<short>& pos)
{
    /*
        local function drop_attached_node(p)
    */

    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() != CONTENT_IGNORE)
    {
        std::vector<std::string> drops;
        GetNodeDrops(node, "", drops);
        const ContentFeatures& cFeatures =  mEnvironment->GetNodeManager()->Get(node);
        /*
        if (cFeatures.soundFall.Exists())
        {
            SoundParams params;
            params.type = SoundParams::SP_POSITIONAL;
            params.position = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
            mEnvironment->PlaySound(cFeatures.soundFall, params, true);
        }
        */

        mEnvironment->RemoveNode(pos);
        for (std::string droppedItem : drops)
        {
            ItemStack itemStack;
            itemStack.Deserialize(droppedItem, mEnvironment->GetItemManager());
            if (itemStack.IsEmpty() || !itemStack.IsKnown(mEnvironment->GetItemManager()))
                continue;

            Vector3<float> itemPos{
                (float)pos[0] + Randomizer::FRand()/2.f - 0.25f,
                (float)pos[1] + Randomizer::FRand()/2.f - 0.25f,
                (float)pos[2] + Randomizer::FRand()/2.f - 0.25f};
            itemPos *= BS;

            EntityLAO* obj = new EntityLAO(mEnvironment, itemPos, "__builtin:item", "");
            int objId = mEnvironment->AddActiveObject(obj);
            // If failed to add, return nothing (reads as nil)
            if (objId == 0)
                continue;

            // If already deleted (can happen in on_activate), return nil
            if (obj->IsGone())
                continue;

            SetItem(obj, droppedItem);
        }
    }
}

bool BaseGame::CheckAttachedNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        function builtin_shared.check_attached_node(p, n)
    */
    Vector3<short> dir = Vector3<short>::Zero();
    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.paramType2 == CPT2_WALLMOUNTED || cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
    {
        /*
            The fallback vector here is in case 'wallmounted to dir' is nil due
            to voxelmanip placing a wallmounted node without resetting a
            pre - existing param2 value that is out - of - range for wallmounted.
            The fallback vector corresponds to param2 = 0.
        */
        dir = mWallmountedToDirection[node.param2 % 8];
    }
    else dir[1] = -1;

    Vector3<short> newPos = pos + dir;
    MapNode newNode = mEnvironment->GetMap()->GetNode(newPos);
    if (newNode.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeaturesNew = mEnvironment->GetNodeManager()->Get(newNode);
        if (!cFeaturesNew.walkable)
            return false;
    }

    return true;
}

bool BaseGame::ConvertToFallingNode(const Vector3<short>& nodePos, const MapNode& node)
{
    /*
        local function convert_to_falling_node(pos, node)
    */
    Vector3<float> pos = Vector3<float>{ (float)nodePos[0], (float)nodePos[1], (float)nodePos[2] } * BS;
    EntityLAO* obj = new EntityLAO(mEnvironment, pos, "__builtin:falling_node", "");
    int objId = mEnvironment->AddActiveObject(obj);
    // If failed to add, return nothing (reads as nil)
    if (objId == 0)
        return false;

    // If already deleted (can happen in on_activate), return nil
    if (obj->IsGone())
        return false;

    // remember node level, the entities' set_node() uses this
    /*
        node.level = core.get_node_level(pos)
        local meta = core.get_meta(pos)
        local metatable = meta and meta:to_table() or {}
    */
    if (node.GetContent() != CONTENT_IGNORE)
    {
        /*
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.soundFall.Exists())
        {
            SoundParams params;
            params.type = SoundParams::SP_POSITIONAL;
            params.position = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
            mEnvironment->PlaySound(cFeatures.soundFall, params, true);
        }
        */
        // Create node
        mEnvironment->SetNode(nodePos, node);

        mEnvironment->RemoveNode(nodePos);
        return true;
    } 
    else return false;
}

bool BaseGame::CheckSingleForFalling(const Vector3<short>& nodePos)
{
    /*
        function core.check_single_for_falling(p)
    */
    MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (ItemGroupGet(cFeatures.groups, "FallingNode"))
        {
            Vector3<short> posBottom = { nodePos[0], (short)(nodePos[1] - 1), nodePos[2] };
            // Only spawn falling node if node below is loaded
            MapNode nodeBottom = mEnvironment->GetMap()->GetNode(posBottom);
            if (nodeBottom.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeaturesBottom = mEnvironment->GetNodeManager()->Get(nodeBottom);
                bool same = cFeatures.name == cFeaturesBottom.name;
                // Let leveled nodes fall if it can merge with the bottom node
                if (same && cFeaturesBottom.paramType2 == CPT2_LEVELED)
                {
                    if (nodeBottom.GetLevel(mEnvironment->GetNodeManager()) <
                        nodeBottom.GetMaxLevel(mEnvironment->GetNodeManager()))
                    {
                        ConvertToFallingNode(nodePos, node);
                        return true;
                    }
                }

                // Otherwise only if the bottom node is considered "fall through"
                if (!same && (!cFeaturesBottom.walkable || cFeaturesBottom.buildableTo) &&
                    (ItemGroupGet(cFeatures.groups, "Float") == 0 || cFeaturesBottom.liquidType == LIQUID_NONE))
                {
                    ConvertToFallingNode(nodePos, node);
                    return true;
                }
            }
        }

        if (ItemGroupGet(cFeatures.groups, "AttachedNode"))
        {
            if (!CheckAttachedNode(nodePos, node))
            {
                DropAttachedNode(nodePos);
                return true;
            }
        }
    }
    return false;
}

void BaseGame::CheckForFalling(const Vector3<short>& pos)
{
    /*
        function core.check_for_falling(p)
    */

    // Round pos to prevent falling entities to get stuck.
    Vector3<short> nodePos = pos; // p = vector.round(p)

    /* 
        We make a stack, and manually maintain size for performance.
        Stored in the stack, we will maintain tables with pos, and
        last neighbor visited.This way, when we get back to each
        node, we know which directions we have already walked, and
        which direction is the next to walk.
    */
    std::map<int, std::pair<Vector3<short>, int>> stack;
    int node = 0;
    
    // The neighbor order we will visit from our table.
    int visit = 0;

    while (true)
    {
        //Push current pos onto the stack.
        stack[node] = std::pair<Vector3<short>, int>{nodePos, visit};
        // Select next node from neighbor list.
        nodePos += mCheckForFallingNeighbors[visit];
        
        // Now we check out the node.If it is in need of an update,
        //it will let us know in the return value(true = updated).
        if (!CheckSingleForFalling(nodePos))
        {
            /*
                If we don't need to "recurse" (walk) to it then pop
                our previous pos off the stack and continue from there,
                with the v value we were at when we last were at that node
            */
            do
            {
                std::pair<Vector3<short>, int> pop = stack[node];
                nodePos = pop.first;
                visit = pop.second;

                stack[node] = std::pair<Vector3<short>, int>(Vector3<short>::Zero(), 0);
                // If there's nothing left on the stack, and no
                // more sides to walk to, we're done and can exit
                if (node == 0 && visit == 10)
                    return;

                node -= 1;

            } while (visit >= 10);

            //The next round walk the next neighbor in list.
            visit += 1;
        }
        else
        {
            // If we did need to walk the neighbor, then
            // start walking it from the walk order start(1),
            // and not the order we just pushed up the stack.
            visit = 0;
        }

        node += 1;
    }
}

bool BaseGame::TryPlaceEntity(unsigned int id, MapNode node, Vector3<short> nodePos)
{
    /*
    core.register_entity(":__builtin:falling_node"
        try_place = function(self, bcp, bcn)
    */

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);

    //Add levels if dropped on same leveled node
    if (cFeatures.paramType2 == CPT2_LEVELED && mFallingEntitiesNode[id] == cFeatures.name)
    {
        uint8_t addLevel = node.GetLevel(mEnvironment->GetNodeManager());
        if (addLevel == 0)
            addLevel = cFeatures.leveled;

        uint8_t nodeLevel = node.AddLevel(mEnvironment->GetNodeManager(), addLevel);
        mEnvironment->SetNode(nodePos, node);
        if (nodeLevel < addLevel)
        {
            return true;
        }
        else if (cFeatures.buildableTo)
        {
            //Node level has already reached max, don't place anything
            return true;
        }
    }

    // Decide if we are replacing the node or placing on top
    Vector3<short> newPos = nodePos;
    if (cFeatures.buildableTo && (!mFallingEntitiesFloats[id] || cFeatures.liquidType == LIQUID_NONE))
        mEnvironment->RemoveNode(nodePos);
    else 
        newPos[1] += 1;

    // Check what's here
    MapNode newNode = mEnvironment->GetMap()->GetNode(newPos);
    if (node.GetContent() != CONTENT_IGNORE)
    {
        //If it's not air or liquid, remove node and replace it with it's drops
        const ContentFeatures& cFeaturesNew = mEnvironment->GetNodeManager()->Get(newNode);
        if (cFeaturesNew.name != "air" && cFeaturesNew.liquidType == LIQUID_NONE)
        {
            if (!cFeaturesNew.buildableTo)
            {
                //OnDigNode(newPos, newNode);
                //If it's still there, it might be protected
                MapNode checkNode = mEnvironment->GetMap()->GetNode(newPos);
                const ContentFeatures& cFeaturesCheck = mEnvironment->GetNodeManager()->Get(checkNode);
                if (cFeaturesCheck.name == cFeaturesNew.name)
                    return false;
            }
            else mEnvironment->RemoveNode(newPos);
        }

        // Create node
        mEnvironment->SetNode(newPos, newNode);
        
        if (cFeaturesNew.soundPlace.Exists())
        {
            SoundParams params;
            params.type = SoundParams::SP_POSITIONAL;
            params.position = Vector3<float>{ (float)newPos[0], (float)newPos[1], (float)newPos[2] } * BS;
            mEnvironment->PlaySound(cFeaturesNew.soundPlace, params, false);
        }
        CheckForFalling(newPos);
    }
    return true;
}

bool BaseGame::TryMergeWithEntity(ItemStack ownStack, EntityLAO* objectLao, EntityLAO* entityLao)
{
    /*
        core.register_entity(":__builtin:item"
            try_merge_with = function(self, own_stack, object, entity)
    */

    if (mEntitiesActive[objectLao->GetId()] == mEntitiesActive[entityLao->GetId()])
    {
        //Can not merge with itself
        return false;
    }

    ItemStack itemStack;
    itemStack.Deserialize(mEntitiesItemString[entityLao->GetId()], mEnvironment->GetItemManager());
    if (ownStack.name != itemStack.name ||
        ownStack.metadata != itemStack.metadata ||
        ownStack.wear != itemStack.wear ||
        ownStack.FreeSpace(mEnvironment->GetItemManager()) == 0)
    {
        // Can not merge different or full stack
        return false;
    }

    if (itemStack.count + ownStack.count > itemStack.GetStackMax(mEnvironment->GetItemManager()))
        return false;

    // Merge the remote stack into this one
    Vector3<float> objPosition = objectLao->GetBasePosition() / BS;
    objPosition[1] += (itemStack.count / itemStack.GetStackMax(mEnvironment->GetItemManager())) * 0.15f;
    objectLao->MoveTo(objPosition * BS, false);

    mEntitiesActive[objectLao->GetId()] = 0; // Handle as new entity
    ownStack.count = itemStack.count + ownStack.count;
    SetItem(objectLao, mEntitiesItemString[objectLao->GetId()]);

    mEntitiesItemString[entityLao->GetId()] = "";
    Remove(objectLao);
    return true;
}

void BaseGame::EnablePhysicsEntity(EntityLAO* lao)
{
    /*
        core.register_entity(":__builtin:item"
            enable_physics = function(self)
    */
    if (!lao->AccessObjectProperties()->physical)
    {
        lao->AccessObjectProperties()->physical = true;
        lao->SetVelocity(Vector3<float>::Zero());
        lao->SetAcceleration(Vector3<float>{ 0, -mGravity, 0 } *BS);
    }
}

void BaseGame::DisablePhysicsEntity(EntityLAO* lao)
{
    /*
        core.register_entity(":__builtin:item"
            disable_physics = function(self)
    */
    if (lao->AccessObjectProperties()->physical)
    {
        lao->AccessObjectProperties()->physical = false;
        lao->SetVelocity(Vector3<float>::Zero());
        lao->SetAcceleration(Vector3<float>::Zero());
    }
}

bool BaseGame::AddEntity(EntityLAO* lao, const char* name)
{ 
    return true; 
}

std::string BaseGame::GetStaticDataEntity(const EntityLAO* lao) const
{
    std::stringstream os;
    if (lao)
    {
        if (lao->GetName() == "__builtin:item")
        {
            /*
                core.register_entity(":__builtin:item"
                    get_staticdata = function(self)
            */
            json root;
            root["itemstring"] = mEntitiesItemString.find(lao->GetId())->second;
            root["age"] = mEntitiesActive.find(lao->GetId())->second;
            os << root;
        }
        else if (lao->GetName() == "__builtin:falling_node")
        {
            /*
                core.register_entity(":__builtin:falling_node"
                    get_staticdata = function(self)
            */
            json root;
            root["node"] = mFallingEntitiesNode.find(lao->GetId())->second;
            root["meta"] = mFallingEntitiesMeta.find(lao->GetId())->second;
            os << root;
        }
    }

    return "return " + os.str();
}

void BaseGame::OnActivateEntity(EntityLAO* lao,
    const std::string& staticData, unsigned int dTime)
{ 
    if (lao->GetName() == "__builtin:item")
    {
        /*
            core.register_entity(":__builtin:item",
                on_activate = function(self, staticdata, dtime_s)
        */
        if (staticData.substr(0, std::string("return").length()) == "return")
        {
            std::string str = staticData.substr(std::string("return").length());
            StringReplace(str, "]=", ":");
            StringReplace(str, "] =", ":");
            StringReplace(str, "[", "");
            StringReplace(str, "]", "");
            StringReplace(str, "(", "[");
            StringReplace(str, ")", "]");

            json root;
            std::stringstream(str) >> root;
            mEntitiesItemString[lao->GetId()] = root["itemstring"];
            mEntitiesActive[lao->GetId()] = root["age"] + (float)dTime;
        }
        else
        {
            mEntitiesItemString[lao->GetId()] = staticData;
            mEntitiesActive[lao->GetId()] = 0;
        }

        ItemGroupList armorGroup;
        armorGroup["Immortal"] = 1;
        lao->SetArmorGroups(armorGroup);
        lao->SetVelocity(Vector3<float>{ 0, 2, 0 } * BS);
        lao->SetAcceleration(Vector3<float>{ 0, -mGravity, 0 } * BS);

        SetItem(lao, mEntitiesItemString[lao->GetId()]);
    }
    else if (lao->GetName() == "__builtin:falling_node")
    {
        /*
            core.register_entity(":__builtin:falling_node"
                on_activate = function(self, staticdata)
        */
        mEntitiesActive[lao->GetId()] = 0;

        ItemGroupList armorGroup;
        armorGroup["Immortal"] = 1;
        lao->SetArmorGroups(armorGroup);
        lao->SetVelocity(Vector3<float>{ 0, 2, 0 } * BS);
        lao->SetAcceleration(Vector3<float>{ 0, -mGravity , 0 } * BS);

        std::string str = staticData;
        if (staticData.substr(0, std::string("return").length()) == "return")
        {
            str = staticData.substr(std::string("return").length());
            StringReplace(str, "]=", ":");
            StringReplace(str, "] =", ":");
            StringReplace(str, "[", "");
            StringReplace(str, "]", "");
            StringReplace(str, "(", "[");
            StringReplace(str, ")", "]");

            json root;
            std::stringstream(str) >> root;
            mFallingEntitiesNode[lao->GetId()] = root["node"];
            mFallingEntitiesMeta[lao->GetId()] = root["meta"];
        }
        else
        {
            mFallingEntitiesNode[lao->GetId()] = staticData;
            mFallingEntitiesMeta[lao->GetId()] = staticData;
        }

        Vector3<float> pos = lao->GetBasePosition() / BS;
        Vector3<short> nodePos = Vector3<short>{ (short)pos[0], (short)pos[1], (short)pos[2] };
        MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
        if (node.GetContent() == CONTENT_IGNORE)
        {
            // Don't allow unknown nodes to fall
            LogInformation("Unknown falling node removed at " + 
                std::to_string(pos[0]) + " " + std::to_string(pos[1]) + " " + std::to_string(pos[2]));
            Remove(lao);
            return;
        }

        //Cache whether we're supposed to float on water
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        mFallingEntitiesFloats[lao->GetId()] = ItemGroupGet(cFeatures.groups, "Float");

        //Set entity visuals
        if (cFeatures.drawType == NDT_TORCHLIKE || cFeatures.drawType == NDT_SIGNLIKE)
        {
            std::vector<std::string> textures;
            std::string tile = cFeatures.tile[0].name;
            if (cFeatures.drawType == NDT_TORCHLIKE && cFeatures.paramType2 == CPT2_WALLMOUNTED)
                tile = cFeatures.tile[1].name;
            if (cFeatures.drawType == NDT_TORCHLIKE)
                textures = { "(" + tile + ")^[transformFX", tile };
            else
                textures = { tile, "(" + tile + ")^[transformFX" };

            lao->AccessObjectProperties()->isVisible = true;
            lao->AccessObjectProperties()->visual = "upright_sprite";
            lao->AccessObjectProperties()->textures = textures;
            lao->AccessObjectProperties()->visualSize = 
            { cFeatures.visualScale, cFeatures.visualScale, cFeatures.visualScale };
            lao->AccessObjectProperties()->glow = cFeatures.lightSource;
        }
        else if (cFeatures.drawType != NDT_AIRLIKE)
        {
            std::string itemString = cFeatures.name;
            if (cFeatures.paramType2 == CPT2_COLOR || cFeatures.paramType2 == CPT2_COLORED_FACEDIR ||
                cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED || cFeatures.paramType2 == CPT2_COLORED_DEGROTATE)
            {
                ItemStack itemStack;
                itemStack.Deserialize(itemString, mEnvironment->GetItemManager());
                itemStack.metadata.SetString("palette_index", std::to_string(node.param2));

                itemString = itemStack.GetItemString();
            }

            //FIXME: solution needed for paramtype2 == "leveled"
            Vector3<float> visualSize = Vector3<float>::Zero();;
            if (cFeatures.visualScale)
            {
                visualSize = Vector3<float>{
                    cFeatures.visualScale, cFeatures.visualScale, cFeatures.visualScale} * 0.667f;
            }

            lao->AccessObjectProperties()->isVisible = true;
            lao->AccessObjectProperties()->wieldItem = itemString;
            lao->AccessObjectProperties()->visualSize = visualSize;
            lao->AccessObjectProperties()->glow = cFeatures.lightSource;
        }

        //Set collision box (certain nodeboxes only for now)
        if (cFeatures.drawType == NDT_NODEBOX &&
            cFeatures.nodeBox.fixed.empty() == false &&
            (cFeatures.nodeBox.type == NodeBoxType::NODEBOX_FIXED ||
            cFeatures.nodeBox.type == NodeBoxType::NODEBOX_LEVELED ||
            cFeatures.nodeBox.type == NodeBoxType::NODEBOX_CONNECTED))
        {
            BoundingBox<float> box = cFeatures.nodeBox.fixed.front();
            if (cFeatures.paramType2 == CPT2_LEVELED && node.GetLevel(mEnvironment->GetNodeManager()) > 0)
                box.mMaxEdge[2] = -0.5f + node.GetLevel(mEnvironment->GetNodeManager()) / 64;
            lao->AccessObjectProperties()->collisionBox = box;
            lao->AccessObjectProperties()->selectionBox = box;
        }

        //Rotate entity
        bool rotate = false;
        if (cFeatures.drawType == NDT_TORCHLIKE)
        {
            //yaw rotation
            if (cFeatures.paramType2 == CPT2_WALLMOUNTED)
                lao->SetRotation(Vector3<float>{0.f, 45.f, 0.f});
            else
                lao->SetRotation(Vector3<float>{0.f, -45.f, 0.f});
        }
        else if (node.param2 != 0 || cFeatures.drawType == NDT_NODEBOX || cFeatures.drawType == NDT_MESH)
        {
            ItemStack itemStack;
            itemStack.Deserialize(cFeatures.name, mEnvironment->GetItemManager());
            const Item& item = itemStack.GetDefinition(mEnvironment->GetItemManager());
            if (item.wieldImage.empty()) rotate = true;
        }
        if (rotate || 
            cFeatures.drawType == NDT_SIGNLIKE || cFeatures.drawType == NDT_MESH ||
            cFeatures.drawType == NDT_NORMAL || cFeatures.drawType == NDT_NODEBOX)
        {
            if (cFeatures.paramType2 == CPT2_FACEDIR || cFeatures.paramType2 == CPT2_COLORED_FACEDIR)
            {
                uint8_t faceDir = node.param2 % 32;
                //Get rotation from a precalculated lookup table
                lao->SetRotation(mFacedirToEuler[faceDir] * (float)GE_C_RAD_TO_DEG);
            }
            else if (cFeatures.paramType2 ==  CPT2_WALLMOUNTED || cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
            {
                uint8_t rot = node.param2 % 8;
                Vector3<float> yawPitchRoll = Vector3<float>::Zero();
                if (cFeatures.drawType == NDT_NODEBOX || cFeatures.drawType == NDT_MESH)
                {
                    if (rot == 0)
                        yawPitchRoll = { (float)GE_C_HALF_PI, 0.f, 0.f };
                    else if (rot == 1)
                        yawPitchRoll = { -(float)GE_C_HALF_PI, (float)GE_C_PI, 0.f };
                    else if (rot == 2)
                        yawPitchRoll = { 0.f,(float)GE_C_HALF_PI, 0.f };
                    else if (rot == 3)
                        yawPitchRoll = { 0.f, -(float)GE_C_HALF_PI, 0.f };
                    else if (rot == 4)
                        yawPitchRoll = { 0.f, (float)GE_C_PI, 0.f };
                }
                else
                {
                    if (rot == 1)
                        yawPitchRoll = { (float)GE_C_PI, (float)GE_C_PI, 0.f };
                    else if (rot == 2)
                        yawPitchRoll = { (float)GE_C_HALF_PI, (float)GE_C_HALF_PI, 0.f };
                    else if (rot == 3)
                        yawPitchRoll = { (float)GE_C_HALF_PI, -(float)GE_C_HALF_PI, 0.f };
                    else if (rot == 4)
                        yawPitchRoll = { (float)GE_C_HALF_PI, -(float)GE_C_PI, 0.f };
                    else if (rot == 5)
                        yawPitchRoll = { (float)GE_C_HALF_PI, 0.f, 0.f };
                }

                if (cFeatures.drawType == NDT_SIGNLIKE)
                {
                    yawPitchRoll[0] -= (float)GE_C_HALF_PI;
                    if (rot == 0)
                        yawPitchRoll[1] += (float)GE_C_HALF_PI;
                    else if (rot == 1)
                        yawPitchRoll[1] -= (float)GE_C_HALF_PI;
                }
                else if (cFeatures.drawType == NDT_MESH ||
                    cFeatures.drawType == NDT_NORMAL ||
                    cFeatures.drawType == NDT_NODEBOX)
                {
                    if (rot == 0 || rot == 1)
                        yawPitchRoll[2] += (float)GE_C_PI;
                    else
                        yawPitchRoll[1] += (float)GE_C_PI;
                }
                lao->SetRotation(yawPitchRoll * (float)GE_C_RAD_TO_DEG);
            }
        }
    }
    else
    {
        LogWarning("Invalid object activation");
        return;
    }
}

void BaseGame::SetItem(EntityLAO* lao, const std::string& itemString)
{
    ItemStack item;
    item.Deserialize(itemString, mEnvironment->GetItemManager());
    mEntitiesItemString[lao->GetId()] = itemString;
    if (item.name.empty())
    {
        // Item not yet known
        return;
    }

    uint16_t stackMax = mEnvironment->GetItemManager()->Get(itemString).stackMax;
    uint16_t stackCount = std::min(item.count, stackMax);
    float size = 0.2f + 0.1f * (float)(std::cbrt(stackCount / stackMax));

    lao->AccessObjectProperties()->isVisible = true;
    lao->AccessObjectProperties()->visual = "wielditem";
    lao->AccessObjectProperties()->visualSize = { size, size, size };
    lao->AccessObjectProperties()->collisionBox = { -size, -size, -size, size, size, size };
    lao->AccessObjectProperties()->selectionBox = { -size, -size, -size, size, size, size };
    lao->AccessObjectProperties()->automaticRotate = (float)GE_C_PI * 0.5f * 0.2f / size;
    lao->AccessObjectProperties()->wieldItem = itemString;
    lao->AccessObjectProperties()->glow = 0;

    Vector3<float> pos = lao->GetBasePosition() / BS;
    Vector3<short> nodePos = Vector3<short>{ (short)pos[0], (short)pos[1], (short)pos[2] };
    MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        lao->AccessObjectProperties()->glow = (char)floor(cFeatures.lightSource / 2 + 0.5f);
    }
}

void BaseGame::RemoveEntity(EntityLAO* lao)
{
    mEntitiesActive.erase(lao->GetId());
    mEntitiesMoving.erase(lao->GetId());
    mEntitiesItemString.erase(lao->GetId());
    mEntitiesForceOut.erase(lao->GetId());
    mEntitiesForceOutStart.erase(lao->GetId());

    mFallingEntitiesFloats.erase(lao->GetId());
    mFallingEntitiesNode.erase(lao->GetId());
    mFallingEntitiesMeta.erase(lao->GetId());
}

void BaseGame::Remove(EntityLAO* lao)
{
    if (lao->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
        return;

    lao->ClearChildAttachments();
    lao->ClearParentAttachment();
    lao->MarkForRemoval();
}

void BaseGame::OnStepEntity(EntityLAO* lao, float dTime, const CollisionMoveResult* moveResult)
{
    if (mEntitiesActive.find(lao->GetId()) == mEntitiesActive.end())
        return;

    if (lao->GetName() == "__builtin:item")
    {
        /*
            core.register_entity(":__builtin:item"
                on_step = function(self, dtime, moveResult)
        */

        mEntitiesActive[lao->GetId()] += dTime;
        if (mTimeToLive > 0 && mEntitiesActive[lao->GetId()] > mTimeToLive)
        {
            Remove(lao);
            return;
        }

        Vector3<float> pos = lao->GetBasePosition() / BS;
        pos[1] += lao->AccessObjectProperties()->collisionBox.mMinEdge[1] - 0.05f;

        Vector3<short> nodePos;
        nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));

        MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
        if (node.GetContent() == CONTENT_IGNORE)
        {
            //Delete in 'ignore' nodes
            Remove(lao);
            return;
        }

        if (mEntitiesForceOut.find(lao->GetId()) != mEntitiesForceOut.end())
        {
            /* This code runs after the entity got a push from the is_stuck code.
            It makes sure the entity is entirely outside the solid node*/
            BoundingBox<float> colBox = lao->AccessObjectProperties()->collisionBox;
            Vector3<float> forceOutStart = mEntitiesForceOutStart[lao->GetId()];
            Vector3<float> forceOut = mEntitiesForceOut[lao->GetId()];

            bool ok = (forceOut[0] > 0 && pos[0] + colBox.mMinEdge[0] > forceOutStart[0] + 0.5f) ||
                (forceOut[1] > 0 && pos[1] + colBox.mMinEdge[1] > forceOutStart[1] + 0.5f) ||
                (forceOut[2] > 0 && pos[2] + colBox.mMinEdge[2] > forceOutStart[2] + 0.5f) ||
                (forceOut[0] < 0 && pos[0] + colBox.mMaxEdge[0] < forceOutStart[0] - 0.5f) ||
                (forceOut[2] < 0 && pos[2] + colBox.mMaxEdge[2] < forceOutStart[2] - 0.5f);
            if (ok)
            {
                //Item was successfully forced out
                mEntitiesForceOut.erase(lao->GetId());
                mEntitiesForceOutStart.erase(lao->GetId());
                EnablePhysicsEntity(lao);
            }
        }

        if (!lao->AccessObjectProperties()->physical)
            return; //Don't do anything

        LogAssert(moveResult,
            "Collision info missing, this is caused by an out-of-date/buggy mod or game");

        if (!moveResult->collides)
        {
            //future TODO : items should probably decelerate in air
            return;
        }

        // Push item out when stuck inside solid node
        bool isStuck = false;
        pos = lao->GetBasePosition() / BS;
        nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));

        node = mEnvironment->GetMap()->GetNode(nodePos);
        if (node.GetContent() != CONTENT_IGNORE)
        {
            const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
            isStuck = cFeatures.walkable &&
                cFeatures.collisionBox.type == NodeBoxType::NODEBOX_REGULAR &&
                cFeatures.nodeBox.type == NodeBoxType::NODEBOX_REGULAR;
        }

        if (isStuck)
        {
            Vector3<short> shootDir = Vector3<short>::Zero();
            std::vector<Vector3<short>> orders{
                {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1} };

            //Check which one of the 4 sides is free
            for (Vector3<short> order : orders)
            {
                node = mEnvironment->GetMap()->GetNode(nodePos + order);
                if (node.GetContent() != CONTENT_IGNORE)
                {
                    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
                    if (!cFeatures.walkable)
                    {
                        shootDir = order;
                        break;
                    }
                }
            }

            //If none of the 4 sides is free, check upwards
            if (shootDir == Vector3<short>::Zero())
            {
                node = mEnvironment->GetMap()->GetNode(nodePos + Vector3<short>::Unit(1));
                if (node.GetContent() != CONTENT_IGNORE)
                    shootDir = Vector3<short>::Unit(1);
            }

            if (shootDir != Vector3<short>::Zero())
            {
                //Set new item moving speed accordingly
                DisablePhysicsEntity(lao);

                Vector3<float> newVector = Vector3<float>{
                    (float)shootDir[0], (float)shootDir[1], (float)shootDir[2] } *3.f;
                lao->SetVelocity(newVector * BS);

                mEntitiesForceOut[lao->GetId()] = newVector;
                mEntitiesForceOutStart[lao->GetId()] = Vector3<float>{
                    (float)nodePos[0], (float)nodePos[1], (float)nodePos[2] };
                return;
            }
        }

        node = { CONTENT_IGNORE }; //ground node we're colliding with
        if (moveResult->touchingGround)
        {
            for (CollisionInfo colInfo : moveResult->collisions)
            {
                if (colInfo.axis == CollisionAxis::COLLISION_AXIS_Y)
                {
                    nodePos[0] = (short)(colInfo.node[0] + (colInfo.node[0] > 0 ? 1.f / 2 : -1.f / 2));
                    nodePos[1] = (short)(colInfo.node[1] + (colInfo.node[1] > 0 ? 1.f / 2 : -1.f / 2));
                    nodePos[2] = (short)(colInfo.node[2] + (colInfo.node[2] > 0 ? 1.f / 2 : -1.f / 2));
                    node = mEnvironment->GetMap()->GetNode(nodePos);
                    break;
                }
            }
        }

        //Slide on slippery nodes
        bool keepMovement = false;
        if (node.GetContent() != CONTENT_IGNORE)
        {
            const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
            int slippery = ItemGroupGet(cFeatures.groups, "Slippery");
            Vector3<float> vel = lao->GetVelocity();
            if (slippery != 0 && (abs(vel[0]) > 0.1f || abs(vel[2]) > 0.1f))
            {
                // Horizontal deceleration
                float factor = std::min(4 / (slippery + 4) * dTime, 1.f);
                lao->SetVelocity(Vector3<float>{vel[0] * (1 - factor), 0.f, vel[2] * (1 - factor)} * BS);
                keepMovement = true;
            }
        }

        if (!keepMovement)
            lao->SetVelocity(Vector3<float>::Zero());

        if (mEntitiesMoving[lao->GetId()] == keepMovement)
        {
            //Do not update anything until the moving state changes
            return;
        }
        mEntitiesMoving[lao->GetId()] = keepMovement;

        //Only collect items if not moving
        if (mEntitiesMoving[lao->GetId()])
            return;

        //Collect the items around to merge with
        ItemStack ownStack;
        ownStack.Deserialize(mEntitiesItemString[lao->GetId()], mEnvironment->GetItemManager());
        if (ownStack.FreeSpace(mEnvironment->GetItemManager()) == 0)
            return;

        float radius = BS;
        std::vector<LogicActiveObject*> sObjects;
        auto includeObjCb = [](LogicActiveObject* obj) { return !obj->IsGone(); };
        mEnvironment->GetObjectsInsideRadius(sObjects, pos, radius, includeObjCb);
        for (LogicActiveObject* obj : sObjects)
        {
            if (obj->GetType() == ACTIVEOBJECT_TYPE_ENTITY)
            {
                EntityLAO* entity = (EntityLAO*)obj;
                if (entity && entity->GetName() == "__builtin:item")
                {
                    TryMergeWithEntity(ownStack, lao, entity);
                    ownStack.Deserialize(mEntitiesItemString[lao->GetId()], mEnvironment->GetItemManager());
                    if (ownStack.FreeSpace(mEnvironment->GetItemManager()) == 0)
                        return;
                }
            }
        }
    }
    else if (lao->GetName() == "__builtin:falling_node")
    {
        /*
            core.register_entity(":__builtin:falling_node"
                on_step = function(self, dtime, moveResult)
        */

        //Fallback code since collision detection can't tell us
        //about liquids(which do not collide)
        if (mFallingEntitiesFloats[lao->GetId()])
        {
            Vector3<float> pos = lao->GetBasePosition() / BS;
            pos[1] -= 0.7f;

            Vector3<short> nodePos;
            nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
            MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
            if (node.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
                if (cFeatures.liquidType != LiquidType::LIQUID_NONE)
                {
                    if (TryPlaceEntity(lao->GetId(), node, nodePos))
                    {
                        Remove(lao);
                        return;
                    }
                }
            }
        }

        LogAssert(moveResult, "Invalid collision info");
        if (!moveResult->collides)
            return; // do nothing

        MapNode collisionMapNode;
        Vector3<short> collisionMapNodePos;

        CollisionInfo collisionNode;
        bool isCollisionNode = false;

        CollisionInfo collisionPlayer;
        bool isCollisionPlayer = false;
        if (moveResult->touchingGround)
        {
            for (CollisionInfo collision : moveResult->collisions)
            {
                if (collision.type == COLLISION_OBJECT)
                {
                    if (collision.axis == COLLISION_AXIS_Y && collision.object &&
                        collision.object->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
                    {
                        collisionPlayer = collision;
                        isCollisionPlayer = true;
                    }
                }
                else if (collision.axis == COLLISION_AXIS_Y)
                {
                    collisionMapNodePos[0] = (short)(collision.node[0] + (collision.node[0] > 0 ? 1.f / 2 : -1.f / 2));
                    collisionMapNodePos[1] = (short)(collision.node[1] + (collision.node[1] > 0 ? 1.f / 2 : -1.f / 2));
                    collisionMapNodePos[2] = (short)(collision.node[2] + (collision.node[2] > 0 ? 1.f / 2 : -1.f / 2));
                    collisionMapNode = mEnvironment->GetMap()->GetNode(collisionMapNodePos);
                    
                    collisionNode = collision;
                    isCollisionNode = true;
                    break;
                }
            }
        }

        if (!isCollisionNode)
        {
            //We're colliding with something, but not the ground. Irrelevant to us.
            if (isCollisionPlayer)
            {
                //Continue falling through players by moving a little into their collision box
                //TODO: this hack could be avoided in the future if objects could choose who to collide with
                Vector3<float> vel = lao->GetVelocity();
                vel[1] = collisionPlayer.oldSpeed[1];
                lao->SetVelocity(vel);

                Vector3<float> pos = lao->GetBasePosition() / BS;
                pos -= Vector3<float>{0.f, 0.5f, 0.f};
                lao->SetPosition(pos * BS);
            }
            return;
        }
        else if (collisionMapNode.GetContent() == CONTENT_IGNORE)
        {
            //Delete on contact with ignore at world edges
            Remove(lao);
            return;
        }

        bool failure = false;
        Vector3<float> nodePos = lao->GetBasePosition() / BS;
        Vector3<float> distance = Vector3<float>{
            abs(nodePos[0] - (float)collisionNode.node[0]),
            abs(nodePos[1] - (float)collisionNode.node[1]),
            abs(nodePos[2] - (float)collisionNode.node[2])
        };
        if (distance[0] >= 1 || distance[2] >= 1)
        {
            //We're colliding with some part of a node that's sticking out
            //Since we don't want to visually teleport, drop as item
            failure = true;
        }
        else if (distance[1] >= 2)
        {
            // Doors consist of a hidden top nodeand a bottom node that is the actual door. 
            // Despite the top node being solid, the moveresult almost always indicates collision 
            // with the bottom node. Compensate for this by checking the top node
            collisionMapNodePos = collisionNode.node;
            collisionMapNodePos[1] += 1;

            collisionMapNodePos[0] = (short)(collisionMapNodePos[0] + (collisionMapNodePos[0] > 0 ? 1.f / 2 : -1.f / 2));
            collisionMapNodePos[1] = (short)(collisionMapNodePos[1] + (collisionMapNodePos[1] > 0 ? 1.f / 2 : -1.f / 2));
            collisionMapNodePos[2] = (short)(collisionMapNodePos[2] + (collisionMapNodePos[2] > 0 ? 1.f / 2 : -1.f / 2));
            collisionMapNode = mEnvironment->GetMap()->GetNode(collisionMapNodePos);

            failure = true; //Unexpected
            if (collisionMapNode.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(collisionMapNode);
                if (cFeatures.walkable)
                    failure = false;
            }
        }

        //Try to actually place ourselves
        if (!failure)
        {
            if (collisionMapNode.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(collisionMapNode);
                failure = !TryPlaceEntity(lao->GetId(), collisionMapNode, collisionMapNodePos);
            }
        }

        if (failure)
        {
            Vector3<float> pos = lao->GetBasePosition() / BS;
            Vector3<short> nodePos;
            nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));

            MapNode node = mEnvironment->GetMap()->GetNode(nodePos);
            std::vector<std::string> drops;
            GetNodeDrops(node, "", drops);
            for (std::string item : drops)
            {
                ItemStack itemStack;
                itemStack.Deserialize(item, mEnvironment->GetItemManager());
                if (itemStack.IsEmpty() || !itemStack.IsKnown(mEnvironment->GetItemManager()))
                    continue;

                EntityLAO* obj = new EntityLAO(mEnvironment, lao->GetBasePosition(), "__builtin:item", "");
                int objId = mEnvironment->AddActiveObject(obj);
                // If failed to add, return nothing (reads as nil)
                if (objId == 0)
                    continue;

                // If already deleted (can happen in on_activate), return nil
                if (obj->IsGone())
                    continue;

                SetItem(obj, item);
            }
        }
    }
}

void BaseGame::GetPropertiesEntity(EntityLAO* lao, ObjectProperties* prop)
{ 
    prop->hpMax = 0;
    if (lao->GetName() == "__builtin:item")
    {
        /*
            core.register_entity(":__builtin:item",
                initial_properties
        */
        prop->hpMax = 1;
        prop->physical = true;
        prop->collideWithObjects = false;
        prop->collisionBox = BoundingBox<float>{ -0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f };
        prop->selectionBox = BoundingBox<float>{ -0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f };
        prop->textures = { "" };
        prop->visual = "wielditem";
        prop->visualSize = { 0.4f, 0.4f, 0.4f };
        prop->isVisible = false;
    }
    else if (lao->GetName() == "__builtin:falling_node")
    {
        /*
            core.register_entity(":__builtin:falling_node",
                initial_properties
        */
        prop->physical = true;
        prop->collideWithObjects = true;
        prop->collisionBox = BoundingBox<float>{ -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
        prop->selectionBox = BoundingBox<float>{ -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
        prop->textures.clear();
        prop->visual = "item";
        prop->visualSize = { 0.667f, 0.667f, 0.667f };
        prop->isVisible = false;
    }
    else
    {
        LogWarning("Invalid object initialization");
        return;
    }

    if (prop->hpMax < lao->GetHP())
    {
        PlayerHPChangeReason reason(PlayerHPChangeReason::SET_HP);
        lao->SetHP(prop->hpMax, reason);
        if (lao->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
            mEnvironment->SendPlayerHPOrDie((PlayerLAO*)lao, reason);
    }

    if (lao->GetType() == ACTIVEOBJECT_TYPE_PLAYER)
    {
        PlayerLAO* playerLAO = (PlayerLAO*)lao;
        if (prop->breathMax < playerLAO->GetBreath())
            playerLAO->SetBreath(prop->breathMax);
    }
}


bool BaseGame::OnPunchEntity(EntityLAO* lao, LogicActiveObject* puncher, 
    float timeFromLastPunch, const ToolCapabilities* toolcap, Vector3<float> dir, short damage)
{
    /*
        core.register_entity(":__builtin:item"
            on_punch = function(self, hitter)
    */

    if (puncher->GetInventory() && mEntitiesItemString[lao->GetId()] != "")
    {
        ItemStack itemStack;
        itemStack.Deserialize(mEntitiesItemString[lao->GetId()], mEnvironment->GetItemManager());
        InventoryList* list = puncher->GetInventory()->GetList("main");
        if (list)
        {
            ItemStack leftOver = list->AddItem(itemStack);
            if (leftOver.count != itemStack.count)
            {
                // Inform other things that the inventory has changed
                mEnvironment->GetInventoryManager()->SetInventoryModified(puncher->GetInventoryLocation());
            }

            if (!leftOver.IsEmpty())
            {
                SetItem(lao, leftOver.GetItemString());
                return false;
            }
        }
    }

    mEntitiesItemString[lao->GetId()] = "";
    Remove(lao);
    return false;
}

bool BaseGame::OnDeathEntity(EntityLAO* lao, LogicActiveObject* killer)
{ 
    return false; 
}

void BaseGame::OnRightClickEntity(EntityLAO* lao, LogicActiveObject* clicker)
{ 

}
void BaseGame::OnAttachChildEntity(LogicActiveObject* lao, LogicActiveObject* child)
{ 

}

void BaseGame::OnDetachChildEntity(LogicActiveObject* lao, LogicActiveObject* child)
{ 

}

void BaseGame::OnDetachEntity(LogicActiveObject* lao, LogicActiveObject* parent)
{ 

}

void BaseGame::OnMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{

}

void BaseGame::OnPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

void BaseGame::OnTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

int BaseGame::AllowMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    return count;
}

int BaseGame::AllowPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return stack.count;
}

int BaseGame::AllowTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return stack.count;
}

void BaseGame::OnMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{

}

void BaseGame::OnPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

void BaseGame::OnTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

int BaseGame::AllowMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    // If node doesn't exist, we don't know what callback to call
    MapNode node = mEnvironment->GetMap()->GetNode(ma.toInventory.nodePosition);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    return count;
}

int BaseGame::AllowPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    // If node doesn't exist, we don't know what callback to call
    MapNode node = mEnvironment->GetMap()->GetNode(ma.toInventory.nodePosition);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    return stack.count;
}

int BaseGame::AllowTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    // If node doesn't exist, we don't know what callback to call
    MapNode node = mEnvironment->GetMap()->GetNode(ma.fromInventory.nodePosition);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    return stack.count;
}

void BaseGame::OnMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{

}

void BaseGame::OnPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

void BaseGame::OnTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{

}

int BaseGame::AllowMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    return count;
}

int BaseGame::AllowPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return stack.count;
}

int BaseGame::AllowTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return stack.count;
}

Inventory* BaseGame::CreateDetachedInventory(const std::string& name, const std::string& player)
{
    /*
        core.detached_inventories[name] = stuff
        return core.create_detached_inventory_raw(name, player_name)
    */

    DetachedInventory detachedInventory;
    mDetachedInventories[name] = detachedInventory;
    Inventory* inv = mEnvironment->GetInventoryManager()->CreateDetachedInventory(
        name, mEnvironment->GetItemManager(), player);
    if (inv)
    {
        InventoryLocation loc;
        loc.SetDetached(name);
        //InvRef::create(L, loc);
    }
    return inv;
}

void BaseGame::RemoveDetachedInventory(const std::string& name)
{
    /*
        core.detached_inventories[name] = nil
        return core.remove_detached_inventory_raw(name)
    */
    mEnvironment->GetInventoryManager()->RemoveDetachedInventory(name);
}

void BaseGame::OnRecieveFieldsPlayer(PlayerLAO* player, const std::string& formName, const StringMap& fields)
{
    //Chat command
    if (formName != "__builtin:help_cmds" || fields.find("quit") != fields.end())
        return;
/*
    // Send
    std::shared_ptr<EventDataNotifyActor>
        pNotifyActorEvent(new EventDataNotifyActor(player->GetId(), text));
    BaseEventManager::Get()->QueueEvent(pNotifyActorEvent);
*/
}

void BaseGame::OnLeavePlayer(PlayerLAO* player)
{
    /*
        local player_name = player:get_player_name()
        core.send_leave_message(player_name, timed_out)
    */

    std::wstring announcement = L"Player has left the game";

    // Send
    std::shared_ptr<EventDataNotifyActor>
        pNotifyActorEvent(new EventDataNotifyActor(player->GetId(), announcement));
    BaseEventManager::Get()->QueueEvent(pNotifyActorEvent);
}

void BaseGame::OnJoinPlayer(PlayerLAO* player)
{
    /*
        local player_name = player:get_player_name()
        core.send_join_message(player_name)
    */

    std::wstring announcement = L"Player has joined the game";

    // Send
    std::shared_ptr<EventDataNotifyActor>
        pNotifyActorEvent(new EventDataNotifyActor(player->GetId(), announcement));
    BaseEventManager::Get()->QueueEvent(pNotifyActorEvent);
}

void BaseGame::OnDiePlayer(PlayerLAO* player, const PlayerHPChangeReason& reason)
{

}


void BaseGame::OnRightClickPlayer(PlayerLAO* player, LogicActiveObject* clicker)
{

}

bool BaseGame::AfterPlaceNode(const Vector3<short>& pos, 
    std::shared_ptr<LogicPlayer> player, const Item& item)
{
    return false;
}

void BaseGame::OnPlaceNode(const PointedThing& pointed, const Item& item)
{
    /*
        local function on_placenode(p, node)
            core.check_for_falling(p)
        end
    */

    //falling
    if (item.name == "__builtin:falling_node")
        CheckForFalling(pointed.nodeUndersurface);
}

void BaseGame::OnUseNode(const Vector3<short>& pos, const MapNode& node)
{

}

bool BaseGame::OnPunch(const Vector3<short>& pos, const MapNode& node)
{
    /*
        local function on_punchnode(p, node)
            core.check_for_falling(p)
        end
    */

    // falling
    CheckForFalling(pos);

    return false;
}

bool BaseGame::OnPunchNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        local function on_punchnode(p, node)
            core.check_for_falling(p)
        end
    */

    // falling
    CheckForFalling(pos);

    return false;
}

bool BaseGame::OnFallNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        local function on_dignode(p, node)
            core.check_for_falling(p)
        end
    */

    // falling
    CheckForFalling(pos);

    return false;
}

bool BaseGame::OnDigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger)
{
    /*
        local function on_dignode(p, node, digger)
            core.node_dig(p, node, digger)
        end
    */

    // digging
    bool dig = DigNode(pos, node, digger);

    // falling
    CheckForFalling(pos);
    return dig;
}

bool BaseGame::CanDigNode(const Vector3<short>& pos, const MapNode& node)
{
    return false;
}

bool BaseGame::OnTimerNode(const Vector3<short>& pos, const MapNode& node, float dTime)
{
    return false;
}

void BaseGame::OnDestructNode(const Vector3<short>& pos, const MapNode& node)
{ 

}

void BaseGame::AfterDestructNode(const Vector3<short>& pos, const MapNode& node)
{ 

}

void BaseGame::OnConstructNode(const Vector3<short>& pos, const MapNode& node)
{ 

}

bool BaseGame::OnRightClickNode(const Vector3<short>& pos, const MapNode& node)
{
    if (node.GetContent() != CONTENT_IGNORE)
    {
        //If it's not air or liquid, remove node and replace it with it's drops
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        return cFeatures.rightClickable;
    }
    return false;
}

void BaseGame::OnRecieveFieldsNode(const Vector3<short>& pos,
    const std::string& formName, const StringMap& fields, UnitLAO* sender)
{

}

bool BaseGame::OnEventPlayer(PlayerLAO* player, const std::string& type)
{
    return mStatBars->EventHandler(player, type);
}

float BaseGame::CalculateKnockback(PlayerLAO* player, LogicActiveObject* hitter, 
    float timeFromLastPunch, const ToolCapabilities* toolcap, Vector3<float> dir, float distance, short damage)
{
    /*
        core.calculate_knockback(player, hitter, time_from_last_punch, tool_capabilities, dir, distance, damage)
    */

    if (damage == 0 || player->IsImmortal())
        return 0.f;

    float m = 8.f;
    //solve m - m * e ^ (k * 4) = 4 for k
    float k = -0.17328f;
    float res = m - m * std::exp(k * damage);
    if (distance < 2.f)
        res = res * 1.1f; // more knockback when closer
    else if (distance > 4.f)
        res = res * 0.9f; // less when far away
    return res;
}

bool BaseGame::OnPunchPlayer(PlayerLAO* player, LogicActiveObject* hitter,
    float timeFromLastPunch, const ToolCapabilities* toolcap, Vector3<float> dir, short damage)
{
    /*
        core.register_on_punchplayer(function(player, hitter, time_from_last_punch, tool_capabilities, unused_dir, damage)
    */

    if (player->GetHP() == 0)
        return true; // RIP

    //Adds eye offset to one but not the other so the direction is slightly off, calculate it ourselves
    Vector3<float> direction = player->GetBasePosition() - hitter->GetBasePosition();
    float distance = Length(direction);
    Normalize(direction);

    float knockback = CalculateKnockback(player, hitter, timeFromLastPunch, toolcap, dir, distance, damage);
    Vector3<float> knockbackDirection = direction * knockback;

    if (knockback < 1.f)
        return true; //barely noticeable, so don't even send

    mEnvironment->AddVelocity(player, knockbackDirection * BS);
    return true;
}

int BaseGame::OnHPChangePlayer(PlayerLAO* player, int hpChange, const PlayerHPChangeReason& reason)
{
    return 0;
}

bool BaseGame::OnSecondaryUseItem(ItemStack& itemStack, LogicActiveObject* user, const PointedThing& pointed)
{
    /*
        function core.item_secondary_use(itemstack, placer)
	        return itemstack
        end
    */
    return false;
}

bool BaseGame::OnUseItem(const ItemStack& itemStack, const PointedThing& pointed)
{
    return false;
}

bool BaseGame::OnUseItem(ItemStack& itemStack, LogicActiveObject* user, const PointedThing& pointed)
{
    return false;
}

bool BaseGame::OnPlaceItem(ItemStack& itemStack, LogicActiveObject* placer, const PointedThing& pointed)
{
    /*
        function core.item_place(itemstack, placer, pointed_thing, param2)
    */

    // Call on_rightclick if the pointed node defines it
    if (pointed.type == POINTEDTHING_NODE)
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(placer->GetId());
        if (!player->GetPlayerControl().sneak)
        {
            MapNode node = mEnvironment->GetMap()->GetNode(pointed.nodeUndersurface);
            if (OnRightClickNode(pointed.nodeUndersurface, node))
                return true;
        }
    }

    // Place if node, otherwise do nothing
    Item item = itemStack.GetDefinition(mEnvironment->GetItemManager());
    if (item.type == ITEM_NODE)
    {
        uint8_t param2 = 0;
        ItemPlaceNode(itemStack, placer, pointed, param2);
        return true;
    }
    return false;
}

bool BaseGame::OnCraftItem(ItemStack& itemStack, LogicActiveObject* user,
    const InventoryList* oldCraftGrid, const InventoryLocation& craftInv)
{
    return false;
}

bool BaseGame::OnCraftPredictItem(ItemStack& itemStack, LogicActiveObject* user,
    const InventoryList* oldCraftGrid, const InventoryLocation& craftInv)
{
    return false;
}

bool BaseGame::OnDropItem(ItemStack& itemStack, LogicActiveObject* dropper, Vector3<float> pos)
{
    /*
        function core.item_drop(itemstack, dropper, pos)
    */

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(dropper->GetId());
    if (player)
        pos[1] += 1.2f * BS;
    ItemStack item = itemStack.TakeItem(itemStack.count);
    if (item.IsEmpty() || !item.IsKnown(mEnvironment->GetItemManager()))
        return false;

    EntityLAO* obj = new EntityLAO(mEnvironment, pos, "__builtin:item", "");
    int objId = mEnvironment->AddActiveObject(obj);
    // If failed to add, return nothing (reads as nil)
    if (objId == 0)
        return false;

    // If already deleted (can happen in on_activate), return nil
    if (obj->IsGone())
        return false;

    SetItem(obj, item.GetItemString());
    if (obj)
    {
        //dropper is player
        if (player)
        {
            float pitch = player->GetPlayerLAO()->GetRadLookPitchDep();
            float yaw = player->GetPlayerLAO()->GetRadYawDep();
            Vector3<float> dir{
                std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw) };
            dir[0] *= 2.9f;
            dir[1] *= 2.9f;
            dir[1] += 2;
            dir[2] *= 2.9f;
            obj->SetVelocity(dir * BS);
        }
        return true;
    }

    //If we reach this, adding the object to the environment failed
    return false;
}

uint8_t BaseGame::DirectionToFaceDirection(const Vector3<int>& dir, bool is6Dir)
{
    /*
        function core.dir_to_facedir(dir, is6d)
    */

    //account for y if requested
    if (is6Dir && abs(dir[1]) > abs(dir[0]) && abs(dir[1]) > abs(dir[2]))
    {
        // from above
        if (dir[1] < 0)
        {
            if (abs(dir[0]) > abs(dir[2]))
            {
                if (dir[0] < 0)
                    return 19;
                else
                    return 13;
            }
            else
            {
                if (dir[2] < 0)
                    return 10;
                else
                    return 4;
            }
        }
        else
        {
            //from below
            if (abs(dir[0]) > abs(dir[2]))
            {
                if (dir[0] < 0)
                    return 15;
                else
                    return 17;
            }
            else
            {
                if (dir[2] < 0)
                    return 6;
                else
                    return 8;
            }
        }
    }
    else if (abs(dir[0]) > abs(dir[2]))
    {
        // otherwise, place horizontally
        if (dir[0] < 0)
            return 3;
        else
            return 1;
    }
    else
    {
        if (dir[2] < 0)
            return 2;
        else
            return 0;
    }
}


uint8_t BaseGame::DirectionToWallmounted(const Vector3<int>& dir)
{
    /*
        function core.dir_to_wallmounted(dir)
    */
    if (abs(dir[1]) > std::max(abs(dir[0]), abs(dir[2])))
    {
        if (dir[1] < 0)
            return 1;
        else
            return 0;
    }
    else if (abs(dir[0]) > abs(dir[2]))
    {
        if (dir[0] < 0)
            return 3;
        else
            return 2;
    }
    else
    {
        if (dir[2] < 0)
            return 5;
        else
            return 4;
    }
}

bool BaseGame::IsProtected(const Vector3<short>& placeTo, LogicPlayer* player)
{
    /*
        function core.is_protected(pos, name)
    */
    return false;
}

ItemStack BaseGame::ItemPlaceNode(ItemStack& itemStack, LogicActiveObject* placer,
    const PointedThing& pointed, uint8_t param2, bool preventAfterPlace)
{
    /*
        function core.item_place_node(itemstack, placer, pointed_thing, param2, prevent_after_place)
    */

    Item item = itemStack.GetDefinition(mEnvironment->GetItemManager());
    if (item.type != ITEM_NODE || pointed.type != POINTEDTHING_NODE)
        return itemStack;

    MapNode oldNodeUnder = mEnvironment->GetMap()->GetNode(pointed.nodeUndersurface);
    MapNode oldNodeAbove = mEnvironment->GetMap()->GetNode(pointed.nodeAbovesurface);

    if (oldNodeUnder.GetContent() == CONTENT_IGNORE || oldNodeAbove.GetContent() == CONTENT_IGNORE)
    {
        LogWarning("Player tried to place node in uploaded position " +
            std::to_string(pointed.nodeAbovesurface[0]) + "," + 
            std::to_string(pointed.nodeAbovesurface[1]) + "," + 
            std::to_string(pointed.nodeAbovesurface[2]));
        return itemStack;
    }

    const ContentFeatures& cFeaturesOldUnder = mEnvironment->GetNodeManager()->Get(oldNodeUnder);
    const ContentFeatures& cFeaturesOldAbove = mEnvironment->GetNodeManager()->Get(oldNodeAbove);

    if (!cFeaturesOldUnder.buildableTo && !cFeaturesOldAbove.buildableTo)
    {
        LogWarning("Player tried to place node in invalid position " +
            std::to_string(pointed.nodeAbovesurface[0]) + "," +
            std::to_string(pointed.nodeAbovesurface[1]) + "," +
            std::to_string(pointed.nodeAbovesurface[2]) + " replacing " + cFeaturesOldAbove.name);
        return itemStack;
    }

    //Place above pointed node
    Vector3<short> placeTo = pointed.nodeAbovesurface;

    // If node under is buildable_to, place into it instead(eg.snow)
    if (cFeaturesOldUnder.buildableTo)
    {
        LogInformation("Node under is buildable to");
        placeTo = pointed.nodeUndersurface;
    }

    if (IsProtected(placeTo, mEnvironment->GetPlayer(placer->GetId()).get()))
    {
        LogWarning("Player tried to place " + item.name + " at protected position " +
          std::to_string(placeTo[0]) + "," + std::to_string(placeTo[1]) + "," + std::to_string(placeTo[2]));
        return itemStack;
    }

    MapNode oldNode = mEnvironment->GetMap()->GetNode(placeTo);

    uint16_t id = mEnvironment->GetNodeManager()->GetId(item.name);
    MapNode newNode = MapNode(id, 0, param2);
    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(newNode);

    //Calculate direction for wall mounted stuff like torches and signs
    if (item.placeParam2)
    {
        newNode.param2 = item.placeParam2;
    }
    else if (cFeatures.paramType2 == CPT2_WALLMOUNTED || cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
    {
        Vector3<int> dir = Vector3<int>{
            pointed.nodeUndersurface[0] - pointed.nodeAbovesurface[0],
            pointed.nodeUndersurface[1] - pointed.nodeAbovesurface[1],
            pointed.nodeUndersurface[2] - pointed.nodeAbovesurface[2]
        };
        newNode.param2 = DirectionToWallmounted(dir);
        //Calculate the direction for furnacesand chestsand stuff
    }
    else if (cFeatures.paramType2 == CPT2_FACEDIR || cFeatures.paramType2 == CPT2_COLORED_FACEDIR)
    {
        Vector3<float> pos = placer->GetBasePosition() / BS;

        Vector3<short> placerPos;
        placerPos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
        placerPos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
        placerPos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
        Vector3<int> dir = Vector3<int>{
            pointed.nodeAbovesurface[0] - placerPos[0],
            pointed.nodeAbovesurface[1] - placerPos[1],
            pointed.nodeAbovesurface[2] - placerPos[2]
        };
        newNode.param2 = DirectionToFaceDirection(dir);
        LogInformation("facedir: " + std::to_string(newNode.param2));
    }
    
    std::string paletteIndex = itemStack.metadata.GetString("palette_index");
    if (!paletteIndex.empty() && !item.placeParam2)
    {
        short colorDivisor = 0;
        if (cFeatures.paramType2 == CPT2_COLOR)
            colorDivisor = 1;
        else if (cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
            colorDivisor = 8;
        else if (cFeatures.paramType2 == CPT2_COLORED_FACEDIR)
            colorDivisor = 32;
        else if (cFeatures.paramType2 == CPT2_COLORED_DEGROTATE)
            colorDivisor = 32;

        if (colorDivisor)
        {
            uint32_t color = atoi(paletteIndex.c_str()) / colorDivisor;
            uint32_t other = newNode.param2 % colorDivisor;
            newNode.SetParam2(color * colorDivisor + other);
        }
    }

    //Check if the node is attached and if it can be placed there
    if (ItemGroupGet(cFeatures.groups, "AttachedNode") && !CheckAttachedNode(placeTo, newNode))
    {
        LogWarning("Attached node " + item.name + " can not be place at position " +
            std::to_string(placeTo[0]) + "," + std::to_string(placeTo[1]) + "," + std::to_string(placeTo[2]));
        return itemStack;
    }

    LogInformation("Player places node " + item.name + " at position " +
        std::to_string(placeTo[0]) + "," + std::to_string(placeTo[1]) + "," + std::to_string(placeTo[2]));

    // Add node and update
    mEnvironment->SetNode(placeTo, newNode);

    std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(placer->GetId());
    if (player && cFeatures.soundPlace.Exists())
    {
        SoundParams params;
        params.type = SoundParams::SP_POSITIONAL;
        params.position = Vector3<float>{ (float)placeTo[0], (float)placeTo[1], (float)placeTo[2] } * BS;
        params.excludePlayer = player->GetName();
        mEnvironment->PlaySound(cFeatures.soundPlace, params, true);
    }

    if (!preventAfterPlace)
        if (AfterPlaceNode(placeTo, player, item))
            itemStack.TakeItem(1);
    return itemStack;
}

bool BaseGame::EatItem(uint16_t hpChange, LogicActiveObject* user, 
    ItemStack& itemStack, const PointedThing& pointed)
{
    /*
        function core.item_eat(hp_change, replace_with_item)
        function core.do_item_eat(hp_change, replace_with_item, itemstack, user, pointed_thing)
    */

    ItemStack taken = itemStack.TakeItem(1);
    if (!taken.IsEmpty())
    {
        PlayerHPChangeReason reason(PlayerHPChangeReason::SET_HP);
        user->SetHP(user->GetHP() + hpChange, reason);
        /*
        Item item = itemStack.GetDefinition(mEnvironment->GetItemManager());
        if (item.soundEat.Exists())
        {
            Vector3<float> pos = user->GetBasePosition();
            SoundParams params;
            params.maxHearDistance = 16.f;
            params.type = SoundParams::SP_POSITIONAL;
            params.position = Vector3<float>{ pos[0], pos[1], pos[2] };
            mEnvironment->PlaySound(item.soundEat, params, true);
        }
        */
        return true;
    }
    return false;
}


void BaseGame::RegisterItem(tinyxml2::XMLElement* pData)
{

}


void BaseGame::Step(float dTime)
{
    SampleStep(dTime);
}

void BaseGame::SampleStep(float dTime)
{

}

void BaseGame::GetNodeDrops(MapNode node, std::string toolName, std::vector<std::string>& drops)
{
    /*
        function core.get_node_drops(node, toolname)
    */
    node.GetLevel(mEnvironment->GetNodeManager());
    
    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);

        //-- get color, if there is color (otherwise nil)
        uint8_t paletteIndex = 0;
        if (cFeatures.paramType2 == CPT2_COLOR || cFeatures.paramType2 == CPT2_COLORED_FACEDIR ||
            cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED || cFeatures.paramType2 == CPT2_COLORED_DEGROTATE)
        {
            paletteIndex = node.param2;
            if (cFeatures.paramType2 == CPT2_COLORED_FACEDIR)
                paletteIndex = (uint8_t)floor(node.param2 / 32) * 32;
            else if (cFeatures.paramType2 == CPT2_COLORED_WALLMOUNTED)
                paletteIndex = (uint8_t)floor(node.param2 / 8) * 8;
            else if (cFeatures.paramType2 == CPT2_COLORED_DEGROTATE)
                paletteIndex = (uint8_t)floor(node.param2 / 32) * 32;
        }
        // paramtype2 == "color" requires no modification.

        if (cFeatures.drops.empty())
        {
            //default drop
            if (paletteIndex)
            {
                ItemStack itemStack;
                itemStack.Deserialize(cFeatures.name, mEnvironment->GetItemManager());
                itemStack.metadata.SetString("palette_index", std::to_string(paletteIndex));
                drops.push_back(itemStack.GetItemString());
            }
            else drops.push_back(cFeatures.name);
        }
        else
        {
            //Extended drop table
            for (auto drop : cFeatures.drops)
            {
                if (drop.first.empty())
                    continue;

                if (!drop.second.empty())
                {
                    short rarity = atoi(drop.second.c_str());
                    bool goodRarity = rarity < 1 || rand() % rarity == 0;
                    if (goodRarity)
                    {
                        ItemStack itemStack;
                        itemStack.Deserialize(drop.first, mEnvironment->GetItemManager());
                        itemStack.metadata.SetString("palette_index", std::to_string(paletteIndex));
                        drops.push_back(itemStack.GetItemString());
                    }
                }
                else
                {
                    ItemStack itemStack;
                    itemStack.Deserialize(drop.first, mEnvironment->GetItemManager());
                    drops.push_back(itemStack.GetItemString());
                }
            }
        }
    }
}

TutorialGame::TutorialGame(LogicEnvironment* env) : BaseGame(env)
{
    AreasInit();
    
    LoadState();

    mLocationsOrder = {
        "intro", "jumpup", "pointing1", "items", "eat", "craft1", "repair", "smelt", "mine", 
        "build", "swim", "dive", "viscosity", "waterfall", "health", "sneak", "goodbye"};

    mLocationsPosition["intro"] = { 42, 0.5f, 28 };
    mLocationsPosition["jumpup"] = { 64, 0.5f, 30 };
    mLocationsPosition["ladder"] = { 70, 0.5f, 37 };
    mLocationsPosition["swim"] = { 85, 0.5f, 50 };
    mLocationsPosition["dive"] = { 59, 0.5f, 62 };
    mLocationsPosition["sneak"] = { 33, 0.5f, 41 };
    mLocationsPosition["eat"] = { 67, -3.5f, 60 };
    mLocationsPosition["health"] = { 50, 0.5f, 58 };
    mLocationsPosition["viscosity"] = { 44, 0.5f, 53 };
    mLocationsPosition["waterfall"] = { 40, 0.5f, 81 };
    mLocationsPosition["pointing1"] = { 89, 0.5f, 62 };
    mLocationsPosition["items"] = { 70, 0.5f, 65 };
    mLocationsPosition["craft1"] = { 74, 0.5f, 59 };
    mLocationsPosition["repair"] = { 80, 0.5f, 59 };
    mLocationsPosition["smelt"] = { 78, 4.5f, 63 };
    mLocationsPosition["mine"] = { 79, 0.5f, 75 };
    mLocationsPosition["build"] = { 66, 0.5f, 83 };
    mLocationsPosition["goodbye"] = { 22.5f, 0.5f, 73 };

    mLocationsLookAt["intro"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["jumpup"] = { (float)GE_C_PI * 1.5f, (float)GE_C_PI * 0.2f };
    mLocationsLookAt["ladder"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["swim"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["dive"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["sneak"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["eat"] = { 0.f, 0.f };
    mLocationsLookAt["health"] = { 0.f, 0.f };
    mLocationsLookAt["viscosity"] = { 0.f, (float)GE_C_PI * 0.2f };
    mLocationsLookAt["waterfall"] = { 0.f, 0.f };
    mLocationsLookAt["pointing1"] = { (float)GE_C_HALF_PI, 0.f };
    mLocationsLookAt["items"] = { (float)GE_C_PI, 0.f };
    mLocationsLookAt["craft1"] = { (float)GE_C_PI * 1.5f, 0.f };
    mLocationsLookAt["repair"] = { (float)GE_C_PI, 0.f };
    mLocationsLookAt["smelt"] = { (float)GE_C_PI * 1.5f, 0.f };
    mLocationsLookAt["mine"] = { (float)GE_C_PI * 0.2f, 0.f };
    mLocationsLookAt["build"] = { (float)GE_C_PI, 0.f };
    mLocationsLookAt["goodbye"] = { (float)GE_C_HALF_PI, 0.f };

}

void TutorialGame::LoadState()
{
    std::string data;
    std::string path = mEnvironment->GetWorldPath() + "/tutorialdata.mt";
    std::ifstream is(path);
    if (!is.good())
    {
        LogInformation("Game: Failed to open " + path);
        throw SerializationError("Couldn't load tutorial data");
    }

    std::getline(is, data);
    if (!data.empty())
    {
        data = data.substr(std::string("return").length());
        std::stringstream(data) >> mState;
    }
}

void TutorialGame::SaveState()
{
    std::string data;
    std::string path = mEnvironment->GetWorldPath() + "/tutorialdata.mt";

    std::stringstream os;
    os << mState;
    if (!FileSystem::Get()->SafeWriteToFile(path, "return " + os.str()))
    {
        LogError("TutorialGame::SaveState(): Failed to write in " + path);
        throw SerializationError("Couldn't save env meta");
    }
    else
    {
        LogInformation("TutorialGame::SaveState(): Succeded to write in " + path);
    }
}

void TutorialGame::Step(float dTime)
{
    BaseGame::Step(dTime);

    TutorialStep(dTime);
    BackgroundMusicStep(dTime);
    PlayerStep(dTime);
    WieldedItemStep(dTime);
    AreasStep(dTime);
}

bool TutorialGame::AfterPlaceNode(const Vector3<short>& pos, 
    std::shared_ptr<LogicPlayer> player, const Item& item)
{
    /*
        minetest.register_node("default:apple", {
            after_place_node = function(pos, placer, itemstack)
    */
    BaseGame::AfterPlaceNode(pos, player, item);

    if (item.name == "default:apple")
    {
        if (player)
        {
            uint8_t param2 = 1;
            uint16_t id = mEnvironment->GetNodeManager()->GetId(item.name);
            MapNode node = MapNode(id, 0, param2);
            mEnvironment->SetNode(pos, node);
            return true;
        }
    }
    return false;
}

void TutorialGame::OnPlaceNode(const PointedThing& pointed, const Item& item)
{
    /*
        core.register_node(":ignore",
            on_place = function(itemstack, placer, pointed_thing)
    */
    BaseGame::OnPlaceNode(pointed, item);

    if (item.name == ":ignore")
    {
        std::shared_ptr<EventDataChatMessage>
            pSendChatMessageEvent(new EventDataChatMessage(L"You can't place 'ignore' nodes!"));
        BaseEventManager::Get()->QueueEvent(pSendChatMessageEvent);
    }
}

void TutorialGame::OnUseNode(const Vector3<short>& pos, const MapNode& node)
{
    BaseGame::OnUseNode(pos, node);
}

bool TutorialGame::OnPunch(const Vector3<short>& pos, const MapNode& node)
{
    /*
        minetest.register_node("arrow_signs:wall_right", on_punch = function(pos)
        minetest.register_node("arrow_signs:wall_left", on_punch = function(pos)
        minetest.register_node("arrow_signs:wall_up", on_punch = function(pos)
        minetest.register_node("arrow_signs:wall_down", on_punch = function(pos)
    */
    BaseGame::OnPunch(pos, node);

    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "arrow_signs:wall_right" ||
            cFeatures.name == "arrow_signs:wall_left" ||
            cFeatures.name == "arrow_signs:wall_up" ||
            cFeatures.name == "arrow_signs:wall_down")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", "");
            ReportMetadataChange(meta, pos, "formspec");
        }
    }

    return true;
}

bool TutorialGame::OnPunchNode(const Vector3<short>& pos, const MapNode& node)
{
    return BaseGame::OnPunchNode(pos, node);
}

bool TutorialGame::OnFallNode(const Vector3<short>& pos, const MapNode& node)
{
    return BaseGame::OnFallNode(pos, node);
}

bool TutorialGame::OnDigNode(const Vector3<short>& pos, const MapNode& node, PlayerLAO* digger)
{
    return BaseGame::OnDigNode(pos, node, digger);
}

bool TutorialGame::CanDigNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        minetest.register_node("default:chest", can_dig = function(pos,player)
        minetest.register_node("default:furnace", can_dig = function(pos,player)
        minetest.register_node("default:furnace_active", can_dig = function(pos,player)
    */
    BaseGame::CanDigNode(pos, node);

    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "default:chest")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            Inventory* inv = meta->GetInventory();
            InventoryList* list = inv ? inv->GetList("main") : nullptr;
            return list ? list->GetUsedSlots() == 0 : true;
        }
        else if(cFeatures.name == "default:furnace" || cFeatures.name == "default:furnace_active")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            Inventory* inv = meta->GetInventory();
            if (inv)
            {
                InventoryList* list = inv->GetList("fuel");
                if (list && list->GetUsedSlots() > 0)
                    return false;
                list = inv->GetList("dst");
                if (list && list->GetUsedSlots() > 0)
                    return false;
                list = inv->GetList("src");
                if (list && list->GetUsedSlots() > 0)
                    return false;
            }
            return true;
        }
    }
    return false;
}

bool TutorialGame::OnTimerNode(const Vector3<short>& pos, const MapNode& node, float dTime)
{
    /*
        minetest.register_node("tutorial:itemspawner",
            on_timer = function(pos, elapsed)
    */
    BaseGame::OnTimerNode(pos, node, dTime);

    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "tutorial:itemspawner")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            if (atoi(meta->GetString("configged").c_str()) == 0)
                return false;

            std::string offsetString = meta->GetString("offset");
            StringReplace(offsetString, "(", "");
            StringReplace(offsetString, ")", "");

            int offsetIndex = 0;
            Vector3<float> offset;
            for (auto offsetValue : StringSplit(offsetString, ','))
            {
                offset[offsetIndex] = (float)atof(offsetValue.c_str());
                offsetIndex++;
            }

            std::string itemString = meta->GetString("itemstring");
            Vector3<float> spawnPosition = Vector3<float>{
                pos[0]+offset[0], pos[1]+offset[1], pos[2]+offset[2]} * BS;

            float radius = BS;
            std::vector<LogicActiveObject*> sObjects;
            auto includeObjCb = [](LogicActiveObject* obj) { return !obj->IsGone(); };
            mEnvironment->GetObjectsInsideRadius(sObjects, spawnPosition, radius, includeObjCb);
            for (LogicActiveObject* obj : sObjects)
            {
                if (obj->GetType() == ACTIVEOBJECT_TYPE_ENTITY)
                {
                    EntityLAO* entity = (EntityLAO*)obj;
                    if (entity && entity->GetName() == "__builtin:item" && 
                        mEntitiesItemString[entity->GetId()] == itemString)
                    {
                        // Remove node when item was spawned successfully. So it doesn't get in the way.
                        mEnvironment->RemoveNode(pos);
                        return true;
                    }
                }
            }

            std::vector<std::string> items = { itemString };
            for (std::string item : items)
            {
                ItemStack itemStack;
                itemStack.Deserialize(item, mEnvironment->GetItemManager());
                if (itemStack.IsEmpty() || !itemStack.IsKnown(mEnvironment->GetItemManager()))
                    continue;
                
                Vector3<float> itemPos = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
                EntityLAO* obj = new EntityLAO(mEnvironment, itemPos, "__builtin:item", "");
                int objId = mEnvironment->AddActiveObject(obj);
                // If failed to add, return nothing (reads as nil)
                if (objId == 0)
                    continue;

                // If already deleted (can happen in on_activate), return nil
                if (obj->IsGone())
                    continue;

                SetItem(obj, item);
            }
            mEnvironment->GetMap()->SetNodeTimer(NodeTimer(1.f, 0.f, pos));
        }
    }
    return true;
}

void TutorialGame::OnDestructNode(const Vector3<short>& pos, const MapNode& node)
{
    BaseGame::OnDestructNode(pos, node);
}

void TutorialGame::AfterDestructNode(const Vector3<short>& pos, const MapNode& node)
{
    BaseGame::AfterDestructNode(pos, node);
}

void TutorialGame::OnConstructNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        minetest.register_node("arrow_signs:wall_right", on_construct = function(pos)
        minetest.register_node("arrow_signs:wall_left", on_construct = function(pos)
        minetest.register_node("arrow_signs:wall_up", on_construct = function(pos)
        minetest.register_node("arrow_signs:wall_down", on_construct = function(pos)
        minetest.register_node("tutorial:cup_gold", on_construct = tutorial.goldinfo,
        minetest.register_node("tutorial:cup_diamond", on_construct = tutorial.diamondinfo,
        minetest.register_node("supplemental:loudspeaker", on_construct = set_loudspeaker_infotext,
        minetest.register_node("tutorial:itemspawner", on_construct = function(pos)
        minetest.register_node("default:chest", on_construct = function(pos)
    */

    BaseGame::OnConstructNode(pos, node);

    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "tutorial:itemspawner")
        {
            std::string form = "size[12,6]"
                "label[-0.15,-0.3;Item spawner]"
                "field[0,1;10,1;offset;Offset;(0,0,0)]"
                "field[0,2;10,1;itemstring;Itemstring;]"
                "button_exit[4.5,5.5;3,1;close;Close]";

            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", form);
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "Item spawner (inactive)");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "tutorial:cup_gold")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("infotext", "This golden cup has been awarded for finishing the tutorial.");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "tutorial:cup_diamond")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("infotext", "This diamond cup has been awarded for collecting all hidden diamonds.");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "supplemental:loudspeaker")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("infotext", "Loudspeaker (rightclick to toggle music)");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "default:chest")
        {
            std::string guiBG = "bgcolor[#080808BB;true]";
            std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
            std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";
            std::string guiControls = "\\[Left click\\]: Take/drop stack\n\\[Right click\\]: "
                "Take half stack / drop 1 item\n\\[Middle click\\]: Take/drop 10 items\n\\[Esc\\] or \\[I\\]: Close";

            std::string form = "size[8,10.6]" + guiBG + guiBGImg + guiSlots +
                "label[0,-0.2;Chest inventory:]"
                "list[current_name;main;0,0.3;8,4;]"
                "label[0,4.35;Player inventory:]"
                "list[current_player;main;0,4.85;8,1;]"
                "list[current_player;main;0,6.08;8,3;8]"
                "listring[current_name;main]"
                "listring[current_player;main]"
                "label[0,9.1;" + guiControls + "]"
                "image[0,4.85;1,1;gui_hb_bg.png]"
                "image[1,4.85;1,1;gui_hb_bg.png]"
                "image[2,4.85;1,1;gui_hb_bg.png]"
                "image[3,4.85;1,1;gui_hb_bg.png]"
                "image[4,4.85;1,1;gui_hb_bg.png]"
                "image[5,4.85;1,1;gui_hb_bg.png]"
                "image[6,4.85;1,1;gui_hb_bg.png]"
                "image[7,4.85;1,1;gui_hb_bg.png]";

            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", form);
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "Chest (Rightclick to open)");
            ReportMetadataChange(meta, pos, "infotext");
            Inventory* inv = meta->GetInventory();
            if (inv == nullptr)
                return;

            InventoryList* list = inv->GetList("main");
            if (!list)
                inv->AddList("main", 8 * 4);
            else 
                list->SetSize(8 * 4);
        }
        else if (cFeatures.name == "arrow_signs:wall_right")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", "field[text;;${text}]");
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "\"\"");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "arrow_signs:wall_left")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", "field[text;;${text}]");
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "\"\"");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "arrow_signs:wall_up")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", "field[text;;${text}]");
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "\"\"");
            ReportMetadataChange(meta, pos, "infotext");
        }
        else if (cFeatures.name == "arrow_signs:wall_down")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
            meta->SetString("formspec", "field[text;;${text}]");
            ReportMetadataChange(meta, pos, "formspec");
            meta->SetString("infotext", "\"\"");
            ReportMetadataChange(meta, pos, "infotext");
        }
    }
}

bool TutorialGame::OnRightClickNode(const Vector3<short>& pos, const MapNode& node)
{
    /*
        minetest.register_node("supplemental:loudspeaker",
            on_rightclick = function(pos, node, clicker)
        minetest.register_node("tutorial:waterfall_on",
            on_rightclick = function(pos, node, clicker, itemstack, pointed_thing)
        minetest.register_node("tutorial:waterfall_off",
            on_rightclick = function(pos, node, clicker, itemstack, pointed_thing)
        minetest.register_node("tutorial:day",
            on_rightclick = function(pos, node, clicker, itemstack, pointed_thing)
        minetest.register_node("tutorial:night",
            on_rightclick = function(pos, node, clicker, itemstack, pointed_thing)
    */
    BaseGame::OnRightClickNode(pos, node);

    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "tutorial:day")
        {
            // A special switch which, when flipped, exchanges day for night and vice versa.
            float timeOfDay = 0.f;
            LogAssert(timeOfDay >= 0.f && timeOfDay <= 1.f, "Time of day value must be between 0 and 1");
            int timeOfDayMH = (int)(timeOfDay * 24000.f);
            // This should be set directly in the environment but currently 
            // such changes aren't immediately sent to the clients, so call the server instead.
            mEnvironment->SetTimeOfDay(timeOfDayMH);

            uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:night");
            MapNode nodeNight = MapNode(id, 0, 0);
            mEnvironment->SetNode(pos, nodeNight);
            return true;
        }
        else if (cFeatures.name == "tutorial:night")
        {
            // A special switch which, when flipped, exchanges day for night and vice versa.
            float timeOfDay = 0.5f;
            LogAssert(timeOfDay >= 0.f && timeOfDay <= 1.f, "Time of day value must be between 0 and 1");
            int timeOfDayMH = (int)(timeOfDay * 24000.f);
            // This should be set directly in the environment but currently 
            // such changes aren't immediately sent to the clients, so call the server instead.
            mEnvironment->SetTimeOfDay(timeOfDayMH);

            uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:day");
            MapNode nodeDay = MapNode(id, 0, 0);
            mEnvironment->SetNode(pos, nodeDay);
            return true;
        }
        else if (cFeatures.name == "tutorial:waterfall_on")
        {
            // A special switch which "activates" and "deactivates" the waterfall in Tutorial World.
            // It only works on a prepared map!
            Vector3<short> worldPos = { 0, 5, 86 };
            for (short x = 33; x <= 46; x++)
            {
                worldPos[0] = x;

                uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:wall");
                MapNode nodeWall = MapNode(id, 0, 0);
                mEnvironment->SetNode(worldPos, nodeWall);
            }
            uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:waterfall_off");
            MapNode nodeWaterfallOff = MapNode(id, 0, 0);
            mEnvironment->SetNode(Vector3<short>{30, 7, 91}, nodeWaterfallOff);
            nodeWaterfallOff = MapNode(id, 0, 0);
            mEnvironment->SetNode(Vector3<short>{40, 2, 86}, nodeWaterfallOff);
            return true;
        }
        else if (cFeatures.name == "tutorial:waterfall_off")
        {
            // A special switch which "activates" and "deactivates" the waterfall in Tutorial World.
            // It only works on a prepared map!
            Vector3<short> worldPos = { 0, 5, 86 };
            for (short x = 33; x <= 46; x++)
            {
                worldPos[0] = x;
                mEnvironment->RemoveNode(worldPos);
            }
            uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:waterfall_on");
            MapNode nodeWaterfallOn = MapNode(id, 0, 0);
            mEnvironment->SetNode(Vector3<short>{30, 7, 91}, nodeWaterfallOn);
            nodeWaterfallOn = MapNode(id, 0, 0);
            mEnvironment->SetNode(Vector3<short>{40, 2, 86}, nodeWaterfallOn);
            return true;
        }
        else if (cFeatures.name == "supplemental:loudspeaker")
        {
            if (mSongPlaying)
            {
                //mEnvironment->StopSound();
                //clicker->GetMeta().SetString("play_music", "0");
            }
            else
            {
                //mEnvironment->PlaySound();
                //clicker->GetMeta().SetString("play_music", "1");
            }
            return true;
        }
    }
    return false;
}


void TutorialGame::OnRecieveFieldsNode(const Vector3<short>& pos,
    const std::string& formName, const StringMap& fields, UnitLAO* sender)
{
    /*
        minetest.register_node("arrow_signs:wall_right",
            on_receive_fields = function(pos, formname, fields, sender)
        minetest.register_node("arrow_signs:wall_left",
            on_receive_fields = function(pos, formname, fields, sender)
        minetest.register_node("arrow_signs:wall_up",
            on_receive_fields = function(pos, formname, fields, sender)
        minetest.register_node("arrow_signs:wall_down",
            on_receive_fields = function(pos, formname, fields, sender)
        minetest.register_node("tutorial:itemspawner",
            on_receive_fields = function(pos, formname, fields, sender)
    */
    BaseGame::OnRecieveFieldsNode(pos, formName, fields, sender);

    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() != CONTENT_IGNORE)
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        if (cFeatures.name == "tutorial:itemspawner")
        {
            std::string offset = "(0,0,0)";
            auto offsetIt = fields.find("offset");
            if (offsetIt != fields.end() || !(*offsetIt).second.empty())
                offset = (*offsetIt).second;

            auto itemStringIt = fields.find("itemstring");
            if (itemStringIt != fields.end())
            {
                MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
                meta->SetString("offset", offset);
                ReportMetadataChange(meta, pos, "offset");
                meta->SetString("configged", "1");
                ReportMetadataChange(meta, pos, "configged");
                meta->SetString("itemstring", (*itemStringIt).second);
                ReportMetadataChange(meta, pos, "itemstring");
                meta->SetString("formspec", "");
                ReportMetadataChange(meta, pos, "formspec");
                meta->SetString("infotext", "");
                ReportMetadataChange(meta, pos, "infotext");
            }
        }
        else if (cFeatures.name == "arrow_signs:wall_right" ||
            cFeatures.name == "arrow_signs:wall_left" ||
            cFeatures.name == "arrow_signs:wall_up" ||
            cFeatures.name == "arrow_signs:wall_down")
        {
            MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);

            std::string text;
            auto textIt = fields.find("text");
            if (textIt != fields.end())
                text = (*textIt).second;

            LogInformation("Player wrote " + text + " to sign at " +
                std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));
            meta->SetString("text", text);
            ReportMetadataChange(meta, pos, "text");
            StringReplace(text, "/", "\"\n\"");
            StringReplace(text, "|", "\"\n\"");
            meta->SetString("infotext", text);
            ReportMetadataChange(meta, pos, "infotext");

            std::vector<std::string> lines = StringSplit(text, '\n');
            if (lines.size() > 5)
            {
                std::shared_ptr<EventDataChatMessage>
                    pSendChatMessageEvent(new EventDataChatMessage(
                        L"\tInformation: \nYou've written more than 5 lines. "
                        L"\n it may be that not all lines are displayed. \n Please remove the last entry"));
                BaseEventManager::Get()->QueueEvent(pSendChatMessageEvent);
            }
        }
    }
}

bool TutorialGame::OnUseItem(ItemStack& itemStack, LogicActiveObject* user, const PointedThing& pointed)
{
    /*
         minetest.register_node("default:apple", {
             on_use = minetest.item_eat(1),
     */

    BaseGame::OnUseItem(itemStack, user, pointed);

    Item item = itemStack.GetDefinition(mEnvironment->GetItemManager());
    if (item.name == "default:apple")
        return EatItem(1, user, itemStack, pointed);
    return false;
}

bool TutorialGame::OnPlaceItem(ItemStack& item, LogicActiveObject* placer, const PointedThing& pointed)
{
    /*
        core.register_item(":unknown",  on_place = core.item_place,
        minetest.register_node("default:tree", on_place = minetest.rotate_node
    */

    if (item.name == "default:tree")
    {
        std::shared_ptr<LogicPlayer> player = mEnvironment->GetPlayer(placer->GetId());
        bool invertWall = player->GetPlayerControl().sneak ? true : false;
        bool infiniteStacks = Settings::Get()->GetBool("creative_mode");
        return RotateAndPlace(item, placer, pointed, infiniteStacks, invertWall, true);
    }
    return BaseGame::OnPlaceItem(item, placer, pointed);
}


void TutorialGame::OnMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    BaseGame::OnMovePlayerInventory(ma, count, player);
}

void TutorialGame::OnPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    BaseGame::OnPutPlayerInventory(ma, stack, player);
}

void TutorialGame::OnTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    BaseGame::OnTakePlayerInventory(ma, stack, player);
}

int TutorialGame::AllowMovePlayerInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    return BaseGame::AllowMovePlayerInventory(ma, count, player);
}

int TutorialGame::AllowPutPlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return BaseGame::AllowPutPlayerInventory(ma, stack, player);
}

int TutorialGame::AllowTakePlayerInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return BaseGame::AllowTakePlayerInventory(ma, stack, player);
}

void TutorialGame::OnMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:chest", {
            on_metadata_inventory_move = function(pos, from_list, from_index, to_list, to_index, count, player)
    */
    BaseGame::OnMoveMetadataInventory(ma, count, player);

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.fromInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:chest")
    {
        LogInformation("action : moves stuff in chest at " +
            std::to_string(pos[0]) + " " + std::to_string(pos[1]) + " " + std::to_string(pos[2]));
    }
}

void TutorialGame::OnPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:chest",
            on_metadata_inventory_put = function(pos, listname, index, stack, player)
    */
    BaseGame::OnPutMetadataInventory(ma, stack, player);

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.toInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:chest")
    {
        LogInformation("action : moves stuff to chest at " +
            std::to_string(pos[0]) + " " + std::to_string(pos[1]) + " " + std::to_string(pos[2]));
    }
}

void TutorialGame::OnTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:chest",
            on_metadata_inventory_take = function(pos, listname, index, stack, player)
    */
    BaseGame::OnTakeMetadataInventory(ma, stack, player);

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.fromInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:chest")
    {
        LogInformation("action : moves stuff from chest at " +
            std::to_string(pos[0]) + " " + std::to_string(pos[1]) + " " + std::to_string(pos[2]));
    }
}

int TutorialGame::AllowMoveMetadataInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:furnace_active",
            allow_metadata_inventory_move = function(pos, from_list, from_index, to_list, to_index, count, player)
        minetest.register_node("default:furnace",
            allow_metadata_inventory_move = function(pos, from_list, from_index, to_list, to_index, count, player)
    */

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.toInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:furnace" || cFeatures.name == "default:furnace_active")
    {
        if (IsProtected(pos, mEnvironment->GetPlayer(player->GetId()).get()))
            return 0;

        MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
        Inventory* inv = meta->GetInventory();
        InventoryList* list = inv ? inv->GetList(ma.fromList) : nullptr;
        ItemStack stack;
        if (list != nullptr && ma.fromItem >= 0 && ma.fromItem < (int)list->GetSize())
            stack = list->GetItem(ma.fromItem);
        if (ma.toList == "fuel")
        {
            CraftInput input(CRAFT_METHOD_FUEL, 1, { stack });
            CraftOutput output;
            std::vector<ItemStack> outputReplacements;
            bool got = mEnvironment->GetCraftManager()->GetCraftResult(
                input, output, outputReplacements, true, mEnvironment);
            if (output.time != 0.f)
            {
                list = inv->GetList("src");
                if (list && list->GetUsedSlots() == 0)
                {
                    meta->SetString("infotext", "Empty furnace (Rightclick to examine)");
                    ReportMetadataChange(meta, pos, "infotext");
                }
                return count;
            }
            else return 0;
        }
        else if (ma.toList == "src")
        {
            return count;
        }
        else if (ma.toList == "dst")
        {
            return 0;
        }
    }
    return BaseGame::AllowMoveMetadataInventory(ma, count, player);
}

int TutorialGame::AllowPutMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:furnace",
            allow_metadata_inventory_put = function(pos, listname, index, stack, player)
        minetest.register_node("default:furnace_active",
            allow_metadata_inventory_put = function(pos, listname, index, stack, player)
    */

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.toInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:furnace" || cFeatures.name == "default:furnace_active")
    {
        if (IsProtected(pos, mEnvironment->GetPlayer(player->GetId()).get()))
            return 0;

        MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
        Inventory* inv = meta->GetInventory();
        if (ma.toList == "fuel")
        {
            CraftInput input(CRAFT_METHOD_FUEL, 1, { stack });
            CraftOutput output;
            std::vector<ItemStack> outputReplacements;
            bool got = mEnvironment->GetCraftManager()->GetCraftResult(
                input, output, outputReplacements, true, mEnvironment);
            if (output.time != 0.f)
            {
                InventoryList* list = inv ? inv->GetList("src") : nullptr;
                if (list && list->GetUsedSlots() == 0)
                {
                    meta->SetString("infotext", "Empty furnace (Rightclick to examine)");
                    ReportMetadataChange(meta, pos, "infotext");
                }
                return stack.count;
            }
            else return 0;
        }
        else if (ma.toList == "src")
        {
            return stack.count;
        }
        else if (ma.toList == "dst")
        {
            return 0;
        }
    }
    return BaseGame::AllowPutMetadataInventory(ma, stack, player);
}

int TutorialGame::AllowTakeMetadataInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    /*
        minetest.register_node("default:furnace",
            allow_metadata_inventory_take = function(pos, listname, index, stack, player)
        minetest.register_node("default:furnace_active",
            allow_metadata_inventory_take = function(pos, listname, index, stack, player)
    */

    // If node doesn't exist, we don't know what callback to call
    Vector3<short> pos = ma.fromInventory.nodePosition;
    MapNode node = mEnvironment->GetMap()->GetNode(pos);
    if (node.GetContent() == CONTENT_IGNORE)
        return 0;

    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
    if (cFeatures.name == "default:furnace" || cFeatures.name == "default:furnace_active")
    {
        if (IsProtected(pos, mEnvironment->GetPlayer(player->GetId()).get()))
            return 0;

        return stack.count;
    }
    return BaseGame::AllowTakeMetadataInventory(ma, stack, player);
}

void TutorialGame::OnMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    BaseGame::OnMoveDetachedInventory(ma, count, player);
}

void TutorialGame::OnPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    BaseGame::OnPutDetachedInventory(ma, stack, player);
}

void TutorialGame::OnTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    BaseGame::OnTakeDetachedInventory(ma, stack, player);
}

int TutorialGame::AllowMoveDetachedInventory(const MoveAction& ma, int count, LogicActiveObject* player)
{
    return BaseGame::AllowMoveDetachedInventory(ma, count, player);
}

int TutorialGame::AllowPutDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return BaseGame::AllowPutDetachedInventory(ma, stack, player);
}

int TutorialGame::AllowTakeDetachedInventory(const MoveAction& ma, const ItemStack& stack, LogicActiveObject* player)
{
    return BaseGame::AllowTakeDetachedInventory(ma, stack, player);
}

void TutorialGame::OnGenerateEnvironment(Vector3<short> minp, Vector3<short> maxp, unsigned int blockseed)
{

}

void TutorialGame::AreasInit()
{
    // Load the areas table from the save file
    std::string path = mEnvironment->GetWorldPath() + "/areas.dat";
    std::ifstream is(path);
    if (!is.good())
    {
        LogInformation("Game: Failed to open " + path);
        throw SerializationError("Couldn't load tutorial area data");
    }

    mAreaStore.Deserialize(is);
}

void TutorialGame::TutorialStep(float dTime)
{
    mStepTimer += dTime;
    if (mStepTimer > 2.f)
    {
        for (std::shared_ptr<LogicPlayer> player : mEnvironment->GetPlayers())
        {
            std::string name = player->GetName();

            {
                Inventory* inv = player->GetPlayerLAO()->GetInventory();
                bool stateChanged = false;

                if (!mState["first_gold"])
                {
                    ItemStack goldStack;
                    goldStack.Deserialize("default:gold_ingot 1", mEnvironment->GetItemManager());
                    InventoryList* list = inv ? inv->GetList("main") : nullptr;
                    if (list->ContainsItem(goldStack, false))
                    {
                        std::string firstGold = "You have collected your first gold ingot. Those will help you to keep"
                            " track in this tutorial.,There are 13 gold ingots in this tutorial.,,There is a gold ingot"
                            " at every important station. If you collected all ingots\\,, you are done with the tutorial\\,"
                            " but collecting the gold ingots is not mandatory.";
                        std::string form = "size[12,6]"
                            "label[-0.15,-0.3;Gold ingots in the tutorial]"
                            "tablecolumns[text]"
                            "tableoptions[background=#000000;highlight=#000000;border=false]"
                            "table[0,0.25;12,5.2;text_table;" + firstGold + "]"
                            "button_exit[4.5,5.5;3,1;close;Close]";
                        mEnvironment->SendShowFormMessage(player->GetId(), form, "tutorial_dialog");
                        mState["first_gold"] = true;
                        stateChanged = true;
                    }
                }

                if (!mState["last_gold"])
                {
                    ItemStack goldStack;
                    goldStack.Deserialize("default:gold_ingot " + std::to_string(mGold), mEnvironment->GetItemManager());
                    InventoryList* list = inv ? inv->GetList("main") : nullptr;
                    if (list->ContainsItem(goldStack, false))
                    {
                        std::string lastGold = "You have collected all the gold ingots in this tutorial.,,This means you"
                            " have now travelled to each station. If you read and understood everything, you have learned"
                            " everything which can be learned from this tutorial.,,If this is the case\\, you are finished"
                            " with this tutorial and can leave now.But feel free to stay in this world to explore the area"
                            " a bit further.,, You may also want to visit the Good - Bye room\\, which has a few more"
                            " informational signs with supplemental information\\, but nothing of is is essential or"
                            " gameplay - relevant. If you want to stay\\, you leave later by pressing [Esc] to open the pause"
                            " menu and then return to the main menu or quit Minetest.]";
                        std::string form = "size[12,6]"
                            "label[-0.15,-0.3;You've finished the tutorial!]"
                            "tablecolumns[text]"
                            "tableoptions[background=#000000;highlight=#000000;border=false]"
                            "table[0,0.25;12,5.2;creative_text;" + lastGold + "]"
                            "button_exit[0.5,5.5;3,1;close;Continue]"
                            "button_exit[4.5,5.5;3,1;leave;Leave tutorial]"
                            "button_exit[8.5,5.5;3,1;gotoend;Go to Good-Bye room]";
                        mEnvironment->SendShowFormMessage(player->GetId(), form, "tutorial_last_gold");

                        uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:cup_gold");
                        MapNode node = MapNode(id, 0, 0);
                        mEnvironment->SetNode({19,2,72}, node);
                        mState["last_gold"] = true;
                        stateChanged = true;
                    }
                }

                if (!mState["first_diamond"])
                {
                    ItemStack diamondStack;
                    diamondStack.Deserialize("default:diamond 1", mEnvironment->GetItemManager());
                    InventoryList* list = inv ? inv->GetList("main") : nullptr;
                    if (list->ContainsItem(diamondStack, false))
                    {
                        std::string firstDiamond = "Great, you have found and collected a hidden diamond! In Tutorial World\\," 
                            " there are 12 hidden diamonds.Can you find them all ? The first diamond may have been easy to" 
                            " collect\\, but the remaining 11 diamonds probably won't be that easy.,,If you manage to find" 
                            " them all\\, you will be awarded a symbolic prize.";
                        std::string form = "size[12,6]"
                            "label[-0.15,-0.3;You found a hidden diamond!]"
                            "tablecolumns[text]"
                            "tableoptions[background=#000000;highlight=#000000;border=false]"
                            "table[0,0.25;12,5.2;text_table;" + firstDiamond + "]"
                            "button_exit[4.5,5.5;3,1;close;Close]";
                        mEnvironment->SendShowFormMessage(player->GetId(), form, "tutorial_dialog");
                        mState["first_diamond"] = true;
                        stateChanged = true;
                    }
                }

                if (!mState["last_diamond"])
                {
                    ItemStack diamondStack;
                    diamondStack.Deserialize("default:diamond " + std::to_string(mDiamonds), mEnvironment->GetItemManager());
                    InventoryList* list = inv ? inv->GetList("main") : nullptr;
                    if (list->ContainsItem(diamondStack, false))
                    {
                        std::string lastDiamond = "Congratulations!, You have collected all the diamonds of Tutorial World!,,"
                            " To recognize this achievement\\, you have been awarded with a diamond cup.It has been placed in"
                            " the Good - Bye Room for you.]";
                        std::string form = "size[12,6]"
                            "label[-0.15,-0.3;You have collected all hidden diamonds!]"
                            "tablecolumns[text]"
                            "tableoptions[background=#000000;highlight=#000000;border=false]"
                            "table[0,0.25;12,5.2;last_diamond_text;" + lastDiamond + "]"
                            "button_exit[2.5,5.5;3,1;close;Continue]"
                            "button_exit[6.5,5.5;3,1;gotoend;Go to Good-Bye room]";
                        mEnvironment->SendShowFormMessage(player->GetId(), form, "tutorial_last_diamond");

                        uint16_t id = mEnvironment->GetNodeManager()->GetId("tutorial:cup_diamond");
                        MapNode node = MapNode(id, 0, 0);
                        mEnvironment->SetNode({ 19,2,74 }, node);
                        mState["last_diamond"] = true;
                        stateChanged = true;
                    }
                }

                if (stateChanged)
                {
                    // save state
                    //SaveState();
                }
            }
        }
    }
}

void TutorialGame::BackgroundMusicStep(float dTime)
{
    if (mSongPlaying)
    {
        if (mSongTimeLeft <= 0)
        {
            //mEnvironment->StopSound();
            mSongTimeNext = mSongPauseBetween;
        }
        else mSongTimeLeft -= dTime;
    }
    else if (mSongTimeNext)
    {
        if (mSongTimeNext <= 0)
        {
            //mEnvironment->PlaySound();
        }
        else mSongTimeNext -= dTime;
    }
}

void TutorialGame::PlayerStep(float dTime)
{
    // Check each player and apply animations
    for (std::shared_ptr<LogicPlayer> player : mEnvironment->GetPlayers())
    {
        //Apply animations based on what the player is doing
        if (player->GetPlayerLAO()->GetHP())
        {
            //Determine if the player is walking
            if (player->GetPlayerControl().up || player->GetPlayerControl().down ||
                player->GetPlayerControl().left || player->GetPlayerControl().right)
            {
                if (player->GetPlayerControl().dig)
                {
                    float frameSpeed;
                    std::map<std::string, Vector2<short>> animationFrames;
                    player->GetLocalAnimations(&animationFrames, &frameSpeed);
                    if (player->GetPlayerControl().sneak) frameSpeed /= 2.f;
                    Vector2<float> frameRange = {
                        (float)animationFrames["walk_mine"][0], (float)animationFrames["walk_mine"][1] };
                    player->GetPlayerLAO()->SetAnimation(frameRange, frameSpeed, 0.f, false);
                }
                else //if (player->GetPlayerControl().place)
                {
                    float frameSpeed;
                    std::map<std::string, Vector2<short>> animationFrames;
                    player->GetLocalAnimations(&animationFrames, &frameSpeed);
                    if (player->GetPlayerControl().sneak) frameSpeed /= 2.f;
                    Vector2<float> frameRange = {
                        (float)animationFrames["walk"][0], (float)animationFrames["walk"][1] };
                    player->GetPlayerLAO()->SetAnimation(frameRange, frameSpeed, 0.f, false);
                }
            }
            else if (player->GetPlayerControl().dig)
            {
                float frameSpeed;
                std::map<std::string, Vector2<short>> animationFrames;
                player->GetLocalAnimations(&animationFrames, &frameSpeed);
                Vector2<float> frameRange = {
                    (float)animationFrames["mine"][0], (float)animationFrames["mine"][1] };
                player->GetPlayerLAO()->SetAnimation(frameRange, frameSpeed, 0.f, false);
            }
            else //if (player->GetPlayerControl().place)
            {
                float frameSpeed;
                std::map<std::string, Vector2<short>> animationFrames;
                player->GetLocalAnimations(&animationFrames, &frameSpeed);
                if (player->GetPlayerControl().sneak) frameSpeed /= 2.f;
                Vector2<float> frameRange = {
                    (float)animationFrames["stand"][0], (float)animationFrames["stand"][1] };
                player->GetPlayerLAO()->SetAnimation(frameRange, frameSpeed, 0.f, false);
            }
        }
        else
        {
            float frameSpeed;
            std::map<std::string, Vector2<short>> animationFrames;
            player->GetLocalAnimations(&animationFrames, &frameSpeed);
            Vector2<float> frameRange = {
                (float)animationFrames["lay"][0], (float)animationFrames["lay"][1] };
            player->GetPlayerLAO()->SetAnimation(frameRange, frameSpeed, 0.f, false);
        }
    }
}

void TutorialGame::WieldedItemStep(float dTime)
{
    float dLimit = 3.f;  //HUD element will be hidden after this many seconds

    for (std::shared_ptr<LogicPlayer> player : mEnvironment->GetPlayers())
    {
        std::string name = player->GetName();

        ItemStack wieldStack;
        player->GetWieldedItem(&wieldStack, nullptr);
        std::string wieldName = wieldStack.name;
        unsigned int wieldIndex = player->GetWieldIndex();

        if (mDeltaTimes.find(name) != mDeltaTimes.end() && mDeltaTimes[name] < dLimit)
        {
            mDeltaTimes[name] += dTime;
            if (mDeltaTimes[name] > dLimit && mHuds.find(name) != mHuds.end())
            {
                unsigned int id = mHuds[name];
                HudElement* elem = player->GetHud(id);
                elem->text = "";
                ChangeHud(id, elem, "text");
            }
        }

        //Update HUD when wielded item or wielded index changed
        if (wieldName != mWield[name] || wieldIndex != mWieldIndex[name])
        {
            mWield[name] = wieldName;
            mWieldIndex[name] = wieldIndex;
            mDeltaTimes[name] = 0;

            if (mHuds.find(name) != mHuds.end())
            {
                Item item = mEnvironment->GetItemManager()->Get(wieldName);
                ItemStackMetadata meta = wieldStack.metadata;

                /* Get description.Order of preference :
                    description from metadata
                    description from item definition
                    itemstring
                 */
                std::string desc = meta.GetString("description");
                if (desc == "")
                    desc = item.description;
                if (desc == "")
                    desc = wieldName;

				//Cut off item description after first newline
                size_t firstNewLine = desc.find_first_of('\n');
                if (firstNewLine)
                    desc = desc.substr(0, firstNewLine - 1);

                unsigned int id = mHuds[name];
                HudElement* elem = player->GetHud(id);
                elem->text = desc;
                ChangeHud(id, elem, "text");
            }
        }
    }
}

//Returns a list of areas and number of areas that include the provided position
std::vector<Area*> TutorialGame::GetAreasAtPosition(const Vector3<float>& pos)
{
    std::vector<Area*> areas;
    Vector3<short> position{ (short)round(pos[0]), (short)round(pos[1]), (short)round(pos[2])};
    mAreaStore.GetAreasForPosition(&areas, position);
    return areas;
}

void TutorialGame::AreasStep(float dTime)
{
    for (std::shared_ptr<LogicPlayer> player : mEnvironment->GetPlayers())
    {
        std::string name = player->GetName();
        Vector3<float> pos = player->GetPlayerLAO()->GetBasePosition() / BS;

        std::string areaString;
        for (Area* area : GetAreasAtPosition(pos))
        {
            if (!area->hidden)
                areaString += "You are here " + area->name + "\n";
        }

        if (mAreasHuds.find(name) == mAreasHuds.end())
        {
            HudElement* elem = new HudElement();
            elem->type = HUD_ELEM_TEXT;
            elem->name = "Areas";
            elem->scale = {200, 1};
            elem->text = areaString;
            elem->number = 0xFFFFFF;
            elem->align = { -1, 1 };
            elem->offset = { -12, 17 };
            elem->position = { 1, 0.25f };
            elem->worldPosition = { 0.f, 0.f };

            mAreasHuds[name] = player->AddHud(elem);
            EventManager::Get()->QueueEvent(std::make_shared<EventDataHudAdd>(
                mAreasHuds[player->GetName()], (uint8_t)elem->type, elem->position,
                elem->name, elem->scale, elem->text, elem->number, elem->item, elem->direction,
                elem->align, elem->offset, elem->worldPosition, elem->size, elem->zIndex, elem->text2));
        }
        else
        {
            HudElement* elem = player->GetHud(mAreasHuds[name]);
            if (elem->text != areaString)
            {
                elem->text = areaString;
                ChangeHud(mAreasHuds[name], elem, "text");
            }
        }
    }
}

void TutorialGame::RegisterItem(tinyxml2::XMLElement* pData)
{
    BaseGame::RegisterItem(pData);

    tinyxml2::XMLNode* pNode = pData->Parent();
    tinyxml2::XMLElement* pActorElement = pNode->ToElement();
    mCaptions[pActorElement->Attribute("name")] = pActorElement->Attribute("description");

    tinyxml2::XMLElement* pText= pData->FirstChildElement("Text");
    if (pText)
        mTexts[pActorElement->Attribute("name")] = pText->FirstChild()->Value();
}

void TutorialGame::UpdateCreativeInventory(PlayerLAO* player, const std::string& filter, unsigned short tabId)
{
    InventoryLocation loc;
    loc.SetDetached("creative_" + std::string(player->GetPlayer()->GetName()));

    Inventory* inv = mEnvironment->GetInventoryManager()->GetInventory(loc);

    //mPlayerInventorySize[player->GetId()] = size;
}

void TutorialGame::SetCreativeForm(PlayerLAO* player, unsigned short startIndex, unsigned short pageNum, unsigned short tabId)
{
    std::string playerName = player->GetPlayer()->GetName();
    std::string filter = mPlayerInventoryFilter[player->GetId()];
    uint8_t pageMax = (uint8_t)floor((mPlayerInventorySize[player->GetId()] - 1) / (3 * 8) + 1);

    std::string guiBG = "bgcolor[#080808BB;true]";
    std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
    std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";

    std::string form = "[[size[8, 8.6]"
        "image[4.06, 3.4; 0.8, 0.8; creative_trash_icon.png]"
        "list[current_player; main; 0, 4.7; 8, 1;]"
        "list[current_player; main; 0, 5.85; 8, 3; 8]"
        "list[detached:creative_trash; main; 4, 3.3; 1, 1;]"
        "tablecolumns[color; text; color; text]"
        "tableoptions[background = #00000000; highlight = #00000000; border = false]"
        "button[5.4, 3.2; 0.8, 0.9; creative_prev; <]"
        "button[7.25, 3.2; 0.8, 0.9; creative_next; >]"
        "button[2.1, 3.4; 0.8, 0.5; creative_search; ? ]"
        "button[2.75, 3.4; 0.8, 0.5; creative_clear; X]"
        "tooltip[creative_search;]]Search[[]"
        "tooltip[creative_clear;]]Reset[[]"
        "listring[current_player; main]]]"
        "field[0.3,3.5;2.2,1;creative_filter;;" + filter + "]"
        "listring[detached:creative_" + playerName + ";main]"
        "tabheader[0,0;creative_tabs;Crafting,All,Nodes,Tools,Items;" + std::to_string(tabId) + ";true;false]"
        "list[detached:creative_" + playerName + ";main;0,0;8,3;" + std::to_string(startIndex) + "]"
        "table[6.05,3.35;1.15,0.5;pagenum;#FFFF00," + std::to_string(pageNum) + ",#FFFFFF,/ " + std::to_string(pageMax) + "]"
        "image[0,4.7;1,1;gui_hb_bg.png]"
        "image[1,4.7;1,1;gui_hb_bg.png]"
        "image[2,4.7;1,1;gui_hb_bg.png]"
        "image[3,4.7;1,1;gui_hb_bg.png]"
        "image[4,4.7;1,1;gui_hb_bg.png]"
        "image[5,4.7;1,1;gui_hb_bg.png]"
        "image[6,4.7;1,1;gui_hb_bg.png]"
        "image[7,4.7;1,1;gui_hb_bg.png]" + guiBG + guiBGImg + guiSlots;

    player->GetPlayer()->mInventoryForm = form;
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerInventoryForm>(
        player->GetPlayer()->GetId(), player->GetPlayer()->mInventoryForm));
}

void TutorialGame::SetCraftingForm(PlayerLAO* player)
{
    std::string playerName = player->GetPlayer()->GetName();
    std::string filter = mPlayerInventoryFilter[player->GetId()];
    uint8_t pageMax = (uint8_t)floor((mPlayerInventorySize[player->GetId()] - 1) / (3 * 8) + 1);

    std::string guiBG = "bgcolor[#080808BB;true]";
    std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
    std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";

    std::string form = "[[size[8, 8.6]"
        "list[current_player; craft; 2, 0.75; 3, 3;]"
        "list[current_player; craftpreview; 6, 1.75; 1, 1;]"
        "list[current_player; main; 0, 4.7; 8, 1;]"
        "list[current_player; main; 0, 5.85; 8, 3; 8]"
        "list[detached:creative_trash; main; 0, 2.75; 1, 1;]"
        "image[0.06, 2.85; 0.8, 0.8; creative_trash_icon.png]"
        "image[5, 1.75; 1, 1; gui_furnace_arrow_bg.png^ [transformR270]"
        "tabheader[0, 0; creative_tabs;]]Crafting,All,Nodes,Tools,Items[[; 1; true; false]"
        "listring[current_player; main]"
        "listring[current_player; craft]]]"
        "image[0,4.7;1,1;gui_hb_bg.png]"
        "image[1,4.7;1,1;gui_hb_bg.png]"
        "image[2,4.7;1,1;gui_hb_bg.png]"
        "image[3,4.7;1,1;gui_hb_bg.png]"
        "image[4,4.7;1,1;gui_hb_bg.png]"
        "image[5,4.7;1,1;gui_hb_bg.png]"
        "image[6,4.7;1,1;gui_hb_bg.png]"
        "image[7,4.7;1,1;gui_hb_bg.png]" + guiBG + guiBGImg + guiSlots;

    player->GetPlayer()->mInventoryForm = form;
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerInventoryForm>(
        player->GetPlayer()->GetId(), player->GetPlayer()->mInventoryForm));
}

void TutorialGame::OnRecieveFieldsPlayer(PlayerLAO* player, const std::string& formName, const StringMap& fields)
{
    BaseGame::OnRecieveFieldsPlayer(player, formName, fields);

    if (fields.find("leave") != fields.end())
    {
        LogInformation("You have voluntarily exited the tutorial.");
        return;
    }
    else if (fields.find("teleport") != fields.end())
    {
        std::string form = "size[10,10]"
            "label[0,0;Select teleport destination:]";

        std::string caption, id;
        unsigned int x = 0, y = 1;
        for (unsigned int i = 0; i < mLocationsOrder.size(); i++)
        {
            id = mLocationsOrder[i];
            if (id == "goodbye")
                caption = "Good-Bye room";
            else
                caption = mCaptions["tutorial:sign_" + id];

            form += "button_exit[" + std::to_string(x) + "," + 
                std::to_string(y) + ";5,1;teleport_" + id + ";" + caption + "]";
            y++;
            if (y > 9)
            {
                y = 1;
                x += 5;
            }
        }
        mEnvironment->SendShowFormMessage(player->GetId(), form, "tutorial_teleport");
        return;
    }
    else if (fields.find("gotoend") != fields.end())
    {
        //tutorial.teleport(player, tutorial.locations.goodbye[1], tutorial.locations.goodbye[2])
        player->SetPosition(mLocationsPosition["goodbye"] * BS);
        player->SetPlayerYawAndSend(mLocationsLookAt["goodbye"][0] * (float)GE_C_RAD_TO_DEG);
        player->SetLookPitchAndSend(mLocationsLookAt["goodbye"][1] * (float)GE_C_RAD_TO_DEG);
        return;
    }

    if (formName == "tutorial_teleport")
    {
        for (auto location : mLocationsPosition)
        {
            if (fields.find("teleport_" + location.first) != fields.end())
            {
                player->SetPosition(location.second * BS);
                player->SetPlayerYawAndSend(mLocationsLookAt[location.first][0] * (float)GE_C_RAD_TO_DEG);
                player->SetLookPitchAndSend(mLocationsLookAt[location.first][1] * (float)GE_C_RAD_TO_DEG);
                return;
            }
        }
    }

    //Sound
    if (fields.find("togglemusic") != fields.end())
    {
        if (mSongPlaying)
        {
            //mEnvironment->StopSound();
            player->GetMeta().SetString("play_music", "0");
        }
        else
        {
            //mEnvironment->PlaySound();
            player->GetMeta().SetString("play_music", "1");
        }
    }

    //Creative
    if (!formName.empty() || !Settings::Get()->GetBool("creative_mode"))
        return;

    std::string playerName = player->GetPlayer()->GetName();
    std::string filter = mPlayerInventoryFilter[player->GetId()];
    unsigned int startIndex = mPlayerInventoryStartIndex[player->GetId()];
    unsigned int tabId = mPlayerInventoryTabId[player->GetId()];
    unsigned int size = mPlayerInventorySize[player->GetId()];

    if (fields.find("quit") != fields.end())
    {
        if (tabId == 1)
            SetCraftingForm(player);
    }
    else if (fields.find("creative_tabs") != fields.end())
    {
        auto tabIt = fields.find("creative_tabs");
        unsigned short tab = atoi((*tabIt).second.c_str());
        if (tab == 1)
        {
            SetCraftingForm(player);
        }
        else
        {
            UpdateCreativeInventory(player, filter, tabId);
            SetCreativeForm(player, 0, 1, tab);
        }
    }
    else if (fields.find("creative_clear") != fields.end())
    {
        mPlayerInventoryFilter[player->GetId()] = "";
        UpdateCreativeInventory(player, "", tabId);
        SetCreativeForm(player, 0, 1, tabId);
    }
    else if (fields.find("creative_search") != fields.end())
    {
        auto filterIt = fields.find("creative_filter");
        filter = ToLowerString((*filterIt).second);
        mPlayerInventoryFilter[player->GetId()] = filter;
        UpdateCreativeInventory(player, filter, tabId);
        SetCreativeForm(player, 0, 1, tabId);
    }
    else
    {
        if (fields.find("creative_prev") != fields.end())
        {
            startIndex -= 3 * 8;
            if (startIndex < 0)
            {
                startIndex = size - (size % (3 * 8));
                if (size == startIndex)
                    startIndex = std::max((unsigned int)0, size - (3 * 8));
            }
        }
        else if (fields.find("creative_next") != fields.end())
        {
            startIndex -= 3 * 8;
            if (startIndex >= size)
                startIndex = 0;
        }

        SetCreativeForm(player, startIndex, startIndex / (3 * 8) + 1, tabId);
    }
}

void TutorialGame::OnLeavePlayer(PlayerLAO* playerLAO)
{
    BaseGame::OnLeavePlayer(playerLAO);

    mWield.erase(playerLAO->GetPlayer()->GetName());
    mWieldIndex.erase(playerLAO->GetPlayer()->GetName());

    mAreasHuds.erase(playerLAO->GetPlayer()->GetName());
}

void TutorialGame::OnJoinPlayer(PlayerLAO* playerLAO)
{
    BaseGame::OnJoinPlayer(playerLAO);

    std::shared_ptr<VisualComponent> pMesh =
        playerLAO->GetPlayer()->GetComponent<VisualComponent>(VisualComponent::Name).lock();
    playerLAO->AccessObjectProperties()->visual = "mesh";
    playerLAO->AccessObjectProperties()->visualSize = { 1.f, 1.f, 1.f };
    playerLAO->AccessObjectProperties()->mesh = pMesh->GetMeshes().front();
    playerLAO->AccessObjectProperties()->textures = pMesh->GetTextures();

    LogAssert(playerLAO->GetPlayer(), "invalid player");

    float frameSpeed;
    std::map<std::string, Vector2<short>> animationFrames;
    playerLAO->GetPlayer()->GetLocalAnimations(&animationFrames, &frameSpeed);
    Vector2<float> frameRange = {
        (float)animationFrames["stand"][0], (float)animationFrames["stand"][1] };
    playerLAO->SetAnimation(frameRange, frameSpeed, 0.f, true);
    //default.player_set_animation(player, "stand") 

    std::vector<Vector2<short>> frames;
    for (auto animationFrame : animationFrames)
        frames.push_back(animationFrame.second);
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerAnimations>(playerLAO->GetPlayer()->GetId(), frameSpeed, frames));

    // set GUI
    if (playerLAO->GetPlayer()->GetId() == INVALID_ACTOR_ID)
        return;

    std::string guiBG = "bgcolor[#080808BB;true]";
    std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
    std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";
    std::string guiControls = "\\[Left click\\]: Take/drop stack\n\\[Right click\\]: "
        "Take half stack / drop 1 item\n\\[Middle click\\]: Take/drop 10 items\n\\[Esc\\] or \\[I\\]: Close";

    std::string guiSuvivalForm = "size[8,10]" + guiBG + guiBGImg + guiSlots +
        "button[-0.1,-0.3;3,1;teleport;Teleport]" +
        "label[0,3.75;Player inventory:]" +
        "list[current_player;main;0,4.25;8,1;]" +
        "list[current_player;main;0,5.5;8,3;8]" +
        "label[0,8.5;" + guiControls + "]" +
        "label[2.75,-0.1;Crafting grid:]" +
        "list[current_player;craft;2.75,0.5;3,3;]" +
        "label[6.75,0.9;Output slot:]" +
        "list[current_player;craftpreview;6.75,1.5;1,1;]" +
        "image[5.75,1.5;1,1;gui_furnace_arrow_bg.png^[transformR270]" +
        "listring[current_player;main]" +
        "listring[current_player;craft]" +
        "image[0,4.25;1,1;gui_hb_bg.png]" +
        "image[1,4.25;1,1;gui_hb_bg.png]" +
        "image[2,4.25;1,1;gui_hb_bg.png]" +
        "image[3,4.25;1,1;gui_hb_bg.png]" +
        "image[4,4.25;1,1;gui_hb_bg.png]" +
        "image[5,4.25;1,1;gui_hb_bg.png]" +
        "image[6,4.25;1,1;gui_hb_bg.png]" +
        "image[7,4.25;1,1;gui_hb_bg.png]";

    playerLAO->GetPlayer()->mInventoryForm = guiSuvivalForm;
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataPlayerInventoryForm>(
        playerLAO->GetPlayer()->GetId(), playerLAO->GetPlayer()->mInventoryForm));

    playerLAO->GetPlayer()->SetHotbarImage("gui_hotbar.png");
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetParam>(HUD_PARAM_HOTBAR_IMAGE, "gui_hotbar.png"));

    playerLAO->GetPlayer()->SetHotbarSelectedImage("gui_hotbar_selected.png");
    EventManager::Get()->QueueEvent(
        std::make_shared<EventDataHudSetParam>(HUD_PARAM_HOTBAR_SELECTED_IMAGE, "gui_hotbar_selected.png"));

    mHuds[playerLAO->GetPlayer()->GetName()] = AddHud(playerLAO->GetPlayer());

    ItemStack selectedItem;
    playerLAO->GetWieldedItem(&selectedItem, nullptr);
    mWield[playerLAO->GetPlayer()->GetName()] = selectedItem.name;
    mWieldIndex[playerLAO->GetPlayer()->GetName()] = playerLAO->GetWieldIndex();

    mStatBars->Update(playerLAO);

    if (!Settings::Get()->GetBool("creative_mode"))
        return;

    mPlayerInventorySize[playerLAO->GetId()] = 0;
    mPlayerInventoryFilter[playerLAO->GetId()] = "";
    mPlayerInventoryStartIndex[playerLAO->GetId()] = 1;
    mPlayerInventoryTabId[playerLAO->GetId()] = 2;
    UpdateCreativeInventory(playerLAO, "", 2);
    SetCreativeForm(playerLAO, 0, 1, 2);
}


void TutorialGame::OnActionABM(ABM* abm, Vector3<short> pos, MapNode node,
    unsigned int activeObjectCount, unsigned int activeObjectCountWider)
{
    /*
    minetest.register_abm({
        nodenames = {"default:sapling"},
        action = function(pos, node)

        nodenames = {"default:junglesapling"},
        action = function(pos, node)

        nodenames = {"default:lava_flowing"},,
        action = function(pos, node, active_object_count, active_object_count_wider)

        nodenames = {"default:lava_source"},
        action = function(pos, node, active_object_count, active_object_count_wider)

        nodenames = {"default:chest"},
        action = function(pos,node,active_object_count, active_object_count_wider)

        nodenames = {"default:furnace","default:furnace_active"},
        action = function(pos, node, active_object_count, active_object_count_wider)

        nodenames = {"group:arrowsign"},
        action = function(pos, node, active_object_count, active_object_cound_wider)
    )
    */
    std::vector<std::string>::const_iterator itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "group:TutorialSign");
    if (itContent != abm->GetTriggerContents().end())
    {
        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(node);
        std::string form = "size[12,6]"
            "label[-0.15,-0.3;" + mCaptions[cFeatures.name] + "]" +
            "tablecolumns[text]" +
            "tableoptions[background=#000000;highlight=#000000;border=false]" +
            "table[0,0.25;12,5.2;infosign_text;" + mTexts[cFeatures.name] + "]" +
            "button_exit[4.5,5.5;3,1;close;Close]";
        /*
        MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(p);
        meta->SetString("formspec", form);
        ReportMetadataChange(meta, p, "formspec");
        */
        return;
    }

    itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:sapling");
    if (itContent != abm->GetTriggerContents().end())
    {
        Vector3<short> nodePos;
        nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
        MapNode nodeSapling = mEnvironment->GetMap()->GetNode(nodePos);
        if (node.GetContent() == CONTENT_IGNORE)
            return;

        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(nodeSapling);
        if (ItemGroupGet(cFeatures.groups, "Soil") == 0)
            return;

        LogInformation("A sapling grows into a tree at " + 
            std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));
        return;
    }

    itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:junglesapling");
    if (itContent != abm->GetTriggerContents().end())
    {
        Vector3<short> nodePos;
        nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
        nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
        MapNode nodeJungleSapling = mEnvironment->GetMap()->GetNode(nodePos);
        if (node.GetContent() == CONTENT_IGNORE)
            return;

        const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(nodeJungleSapling);
        if (ItemGroupGet(cFeatures.groups, "Soil") == 0)
            return;

        LogInformation("A jungle sapling grows into a tree at " +
            std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]));
        return;
    }

    itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:lava_flowing");
    if (itContent != abm->GetTriggerContents().end())
    {
        uint16_t id = mEnvironment->GetNodeManager()->GetId("default:stone");
        MapNode nodeStone = MapNode(id, 0, 0);
        mEnvironment->GetMap()->SetNode(pos, nodeStone);

        SoundParams params;
        params.type = SoundParams::SP_POSITIONAL;
        params.position = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
        params.gain = 0.25f;
        EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlaySoundAt>(
            "default_cool_lava", params.position, params.gain, params.pitch, params.loop));
        return;
    }

    itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:lava_source");
    if (itContent != abm->GetTriggerContents().end())
    {
        uint16_t id = mEnvironment->GetNodeManager()->GetId("default:obsidian");
        MapNode nodeStone = MapNode(id, 0, 0);
        mEnvironment->GetMap()->SetNode(pos, nodeStone);

        SoundParams params;
        params.type = SoundParams::SP_POSITIONAL;
        params.position = Vector3<float>{ (float)pos[0], (float)pos[1], (float)pos[2] } * BS;
        params.gain = 0.25f;
        EventManager::Get()->TriggerEvent(std::make_shared<EventDataPlaySoundAt>(
            "default_cool_lava", params.position, params.gain, params.pitch, params.loop));
        return;
    }

    itContent = std::find(
        abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:furnace");
    if (itContent == abm->GetTriggerContents().end())
    {
        itContent = std::find(
            abm->GetTriggerContents().begin(), abm->GetTriggerContents().end(), "default:furnace_active");
    }
    if (itContent != abm->GetTriggerContents().end())
    {
        MapNodeMetadata* meta = mEnvironment->GetMap()->GetMapNodeMetadata(pos);
        if (meta->GetString("fuel_totaltime").empty())
        {
            meta->SetString("fuel_totaltime", "0.0");
            ReportMetadataChange(meta, pos, "fuel_totaltime");
        }

        if (meta->GetString("fuel_time").empty())
        {
            meta->SetString("fuel_time", "0.0");
            ReportMetadataChange(meta, pos, "fuel_time");
        }

        if (meta->GetString("src_totaltime").empty())
        {
            meta->SetString("src_totaltime", "0.0");
            ReportMetadataChange(meta, pos, "src_totaltime");
        }

        if (meta->GetString("src_time").empty())
        {
            meta->SetString("src_time", "0.0");
            ReportMetadataChange(meta, pos, "src_time");
        }


        bool wasActive = false;
        float fuelTime = (float)atof(meta->GetString("fuel_time").c_str());
        float fuelTotalTime = (float)atof(meta->GetString("fuel_totaltime").c_str());
        float srcTime = (float)atof(meta->GetString("src_time").c_str());
        float srcTotalTime = (float)atof(meta->GetString("src_totaltime").c_str());

        if (fuelTime < fuelTotalTime)
        {
            wasActive = true;
            fuelTime += 1.f;
            srcTime += 1.f;
            meta->SetString("fuel_time", std::to_string(fuelTime));
            ReportMetadataChange(meta, pos, "fuel_time");
            meta->SetString("src_time", std::to_string(srcTime));
            ReportMetadataChange(meta, pos, "src_time");

            Inventory* inv = meta->GetInventory();
            InventoryList* srcList = inv ? inv->GetList("src") : nullptr;
            if (srcList)
            {
                std::vector<ItemStack> items;
                for (unsigned int i = 0; i < srcList->GetSize(); i++)
                    items.push_back(srcList->GetItem(i));
                CraftInput input(CRAFT_METHOD_COOKING, 1, items);
                CraftOutput output;
                std::vector<ItemStack> outputReplacements;
                bool cooked = mEnvironment->GetCraftManager()->GetCraftResult(
                    input, output, outputReplacements, true, mEnvironment);
                if (cooked && !output.item.empty() && srcTime > output.time)
                {
                    //check if there's room for output in "dst" list
                    InventoryList* dstList = inv ? inv->GetList("dst") : nullptr;
                    if (dstList)
                    {
                        ItemStack cookedItem;
                        cookedItem.Deserialize(output.item, mEnvironment->GetItemManager());
                        if (dstList->RoomForItem(cookedItem))
                        {
                            // Put result in "dst" list
                            ItemStack leftOver = inv->AddItem("dst", cookedItem);
                            if (leftOver.count != cookedItem.count)
                            {
                                // Inform other things that the inventory has changed
                                //mEnvironment->GetInventoryManager()->SetInventoryModified();
                            }

                            // Take stuff from "src" list
                            InventoryList* srcList = inv->GetList("src");
                            if (srcList != nullptr && srcList->GetSize())
                                srcList->ChangeItem(0, input.items.front());
                        }
                        meta->SetString("src_time", "0.0");
                        ReportMetadataChange(meta, pos, "src_time");
                    }
                }
            }
        }

        fuelTime = (float)atof(meta->GetString("fuel_time").c_str());
        fuelTotalTime = (float)atof(meta->GetString("fuel_totaltime").c_str());
        if (fuelTime < fuelTotalTime)
        {
            int percent = (fuelTime / fuelTotalTime) * 100;
            meta->SetString("infotext",
                "Active furnace (Flame used: " + std::to_string(percent) + "%) (Rightclick to examine)");
            ReportMetadataChange(meta, pos, "infotext");

            Vector3<short> nodePos;
            nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
            MapNode nodeCheck = mEnvironment->GetMap()->GetNode(nodePos);
            if (nodeCheck.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(nodeCheck);
                if (cFeatures.name != "default:furnace_active")
                {
                    uint16_t id = mEnvironment->GetNodeManager()->GetId("default:furnace_active");
                    MapNode nodeFurnance = MapNode(id, node.param1, node.param2);
                    mEnvironment->SwapNode(nodePos, nodeFurnance);
                }
            }

            int itemPercent = 0;
            float srcTime = (float)atof(meta->GetString("src_time").c_str());

            Inventory* inv = meta->GetInventory();
            InventoryList* srcList = inv ? inv->GetList("src") : nullptr;
            if (srcList)
            {
                std::vector<ItemStack> items;
                for (unsigned int i = 0; i < srcList->GetSize(); i++)
                    items.push_back(srcList->GetItem(i));
                CraftInput input(CRAFT_METHOD_COOKING, 1, items);
                CraftOutput output;
                std::vector<ItemStack> outputReplacements;
                bool cooked = mEnvironment->GetCraftManager()->GetCraftResult(
                    input, output, outputReplacements, true, mEnvironment);
                if (cooked)
                    itemPercent = (srcTime / output.time) * 100;

                std::string guiBG = "bgcolor[#080808BB;true]";
                std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
                std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";
                std::string guiControls = "\\[Left click\\]: Take/drop stack\n\\[Right click\\]: "
                    "Take half stack / drop 1 item\n\\[Middle click\\]: Take/drop 10 items\n\\[Esc\\] or \\[I\\]: Close";

                std::string form = "size[8,9.8]" + guiBG + guiBGImg + guiSlots +
                    "label[-0.1,-0.3;This furnace is active and constantly burning its fuel.]"
                    "label[2.25,0.1;Source:]"
                    "list[current_name;src;2.25,0.5;1,1;]"
                    "label[2.25,2.5;Fuel:]"
                    "list[current_name;fuel;2.25,2.9;1,1;]"
                    "label[2.25,1.3;Flame:]"
                    "image[2.25,1.7;1,1;default_furnace_fire_bg.png^[lowpart:" +
                    std::to_string(100 - percent) + ":default_furnace_fire_fg.png]" +
                    "label[3.75,1.3;Progress:]"
                    "image[3.75,1.7;1,1;gui_furnace_arrow_bg.png^[lowpart:" +
                    std::to_string(itemPercent) + ":gui_furnace_arrow_fg.png^[transformR270]" +
                    "label[5.75,0.70;Output slots:]"
                    "list[current_name;dst;5.75,1.16;2,2;]"
                    "label[0,3.75;Player inventory:]"
                    "list[current_player;main;0,4.25;8,1;]"
                    "list[current_player;main;0,5.5;8,3;8]"
                    "listring[current_name;dst]"
                    "listring[current_player;main]"
                    "listring[current_name;src]"
                    "listring[current_player;main]"
                    "label[0,8.5;" + guiControls + "]"
                    "image[0,4.25;1,1;gui_hb_bg.png]" +
                    "image[1,4.25;1,1;gui_hb_bg.png]" +
                    "image[2,4.25;1,1;gui_hb_bg.png]" +
                    "image[3,4.25;1,1;gui_hb_bg.png]" +
                    "image[4,4.25;1,1;gui_hb_bg.png]" +
                    "image[5,4.25;1,1;gui_hb_bg.png]" +
                    "image[6,4.25;1,1;gui_hb_bg.png]" +
                    "image[7,4.25;1,1;gui_hb_bg.png]";
                meta->SetString("formspec", form);
                ReportMetadataChange(meta, pos, "formspec");
            }
            return;
        }

        Inventory* inv = meta->GetInventory();
        InventoryList* srcList = inv ? inv->GetList("src") : nullptr;
        std::string srcItem;
        if (srcList)
        {
            std::vector<ItemStack> items;
            for (unsigned int i = 0; i < srcList->GetSize(); i++)
                items.push_back(srcList->GetItem(i));
            CraftInput input(CRAFT_METHOD_COOKING, 1, items);
            CraftOutput output;
            std::vector<ItemStack> outputReplacements;
            bool cooked = mEnvironment->GetCraftManager()->GetCraftResult(
                input, output, outputReplacements, true, mEnvironment);
            srcItem = output.item;
        }

        InventoryList* fuelList = inv ? inv->GetList("fuel") : nullptr;
        ItemStack fuelItem;
        fuelTime = 0.f;
        if (fuelList)
        {
            std::vector<ItemStack> items;
            for (unsigned int i = 0; i < fuelList->GetSize(); i++)
                items.push_back(fuelList->GetItem(i));
            CraftInput input(CRAFT_METHOD_FUEL, 1, items);
            CraftOutput output;
            std::vector<ItemStack> outputReplacements;
            bool cooked = mEnvironment->GetCraftManager()->GetCraftResult(
                input, output, outputReplacements, true, mEnvironment);
            fuelTime = output.time;
            fuelItem = input.items.front();
        }

        if (fuelTime <= 0.f)
        {
            meta->SetString("infotext", "Furnace without fuel (Rightclick to examine)");
            ReportMetadataChange(meta, pos, "infotext");

            Vector3<short> nodePos;
            nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
            nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
            MapNode nodeCheck = mEnvironment->GetMap()->GetNode(nodePos);
            if (nodeCheck.GetContent() != CONTENT_IGNORE)
            {
                const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(nodeCheck);
                if (cFeatures.name != "default:furnace")
                {
                    uint16_t id = mEnvironment->GetNodeManager()->GetId("default:furnace");
                    MapNode nodeFurnance = MapNode(id, node.param1, node.param2);
                    mEnvironment->SwapNode(nodePos, nodeFurnance);
                }
            }

            std::string guiBG = "bgcolor[#080808BB;true]";
            std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
            std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";
            std::string guiControls = "\\[Left click\\]: Take/drop stack\n\\[Right click\\]: "
                "Take half stack / drop 1 item\n\\[Middle click\\]: Take/drop 10 items\n\\[Esc\\] or \\[I\\]: Close";

            std::string form = "size[8,9.8]" + guiBG + guiBGImg + guiSlots +
                "label[-0.1,-0.3;This furnace is inactive. Please read the sign above.]"
                "label[2.25,0.1;Source:]"
                "list[current_name;src;2.25,0.5;1,1;]"
                "label[2.25,2.5;Fuel:]"
                "list[current_name;fuel;2.25,2.9;1,1;]"
                "label[2.25,1.3;Flame:]"
                "image[2.25,1.7;1,1;default_furnace_fire_bg.png]"
                "label[3.75,1.3;Progress:]"
                "image[3.75,1.7;1,1;gui_furnace_arrow_bg.png^[transformR270]"
                "label[5.75,0.70;Output slots:]"
                "list[current_name;dst;5.75,1.16;2,2;]"
                "label[0,3.75;Player inventory:]"
                "list[current_player;main;0,4.25;8,1;]"
                "list[current_player;main;0,5.5;8,3;8]"
                "listring[current_name;dst]"
                "listring[current_player;main]"
                "listring[current_name;src]"
                "listring[current_player;main]"
                "label[0,8.5;" + guiControls + "]"
                "image[0,4.25;1,1;gui_hb_bg.png]" +
                "image[1,4.25;1,1;gui_hb_bg.png]" +
                "image[2,4.25;1,1;gui_hb_bg.png]" +
                "image[3,4.25;1,1;gui_hb_bg.png]" +
                "image[4,4.25;1,1;gui_hb_bg.png]" +
                "image[5,4.25;1,1;gui_hb_bg.png]" +
                "image[6,4.25;1,1;gui_hb_bg.png]" +
                "image[7,4.25;1,1;gui_hb_bg.png]";
            meta->SetString("formspec", form);
            ReportMetadataChange(meta, pos, "formspec");
            return;
        }

        if (srcItem.empty())
        {
            if (wasActive)
            {
                meta->SetString("infotext", "Empty furnace (Rightclick to examine)");
                ReportMetadataChange(meta, pos, "infotext");

                Vector3<short> nodePos;
                nodePos[0] = (short)(pos[0] + (pos[0] > 0 ? 1.f / 2 : -1.f / 2));
                nodePos[1] = (short)(pos[1] + (pos[1] > 0 ? 1.f / 2 : -1.f / 2));
                nodePos[2] = (short)(pos[2] + (pos[2] > 0 ? 1.f / 2 : -1.f / 2));
                MapNode nodeCheck = mEnvironment->GetMap()->GetNode(nodePos);
                if (nodeCheck.GetContent() != CONTENT_IGNORE)
                {
                    const ContentFeatures& cFeatures = mEnvironment->GetNodeManager()->Get(nodeCheck);
                    if (cFeatures.name != "default:furnace")
                    {
                        uint16_t id = mEnvironment->GetNodeManager()->GetId("default:furnace");
                        MapNode nodeFurnance = MapNode(id, node.param1, node.param2);
                        mEnvironment->SwapNode(nodePos, nodeFurnance);
                    }
                }

                std::string guiBG = "bgcolor[#080808BB;true]";
                std::string guiBGImg = "background[5,5;1,1;gui_formbg.png;true]";
                std::string guiSlots = "listcolors[#00000069;#5A5A5A;#141318;#30434C;#FFF]";
                std::string guiControls = "\\[Left click\\]: Take/drop stack\n\\[Right click\\]: "
                    "Take half stack / drop 1 item\n\\[Middle click\\]: Take/drop 10 items\n\\[Esc\\] or \\[I\\]: Close";

                std::string form = "size[8,9.8]" + guiBG + guiBGImg + guiSlots +
                    "label[-0.1,-0.3;This furnace is inactive. Please read the sign above.]"
                    "label[2.25,0.1;Source:]"
                    "list[current_name;src;2.25,0.5;1,1;]"
                    "label[2.25,2.5;Fuel:]"
                    "list[current_name;fuel;2.25,2.9;1,1;]"
                    "label[2.25,1.3;Flame:]"
                    "image[2.25,1.7;1,1;default_furnace_fire_bg.png]"
                    "label[3.75,1.3;Progress:]"
                    "image[3.75,1.7;1,1;gui_furnace_arrow_bg.png^[transformR270]"
                    "label[5.75,0.70;Output slots:]"
                    "list[current_name;dst;5.75,1.16;2,2;]"
                    "label[0,3.75;Player inventory:]"
                    "list[current_player;main;0,4.25;8,1;]"
                    "list[current_player;main;0,5.5;8,3;8]"
                    "listring[current_name;dst]"
                    "listring[current_player;main]"
                    "listring[current_name;src]"
                    "listring[current_player;main]"
                    "label[0,8.5;" + guiControls + "]"
                    "image[0,4.25;1,1;gui_hb_bg.png]" +
                    "image[1,4.25;1,1;gui_hb_bg.png]" +
                    "image[2,4.25;1,1;gui_hb_bg.png]" +
                    "image[3,4.25;1,1;gui_hb_bg.png]" +
                    "image[4,4.25;1,1;gui_hb_bg.png]" +
                    "image[5,4.25;1,1;gui_hb_bg.png]" +
                    "image[6,4.25;1,1;gui_hb_bg.png]" +
                    "image[7,4.25;1,1;gui_hb_bg.png]";
                meta->SetString("formspec", form);
                ReportMetadataChange(meta, pos, "formspec");
            }
            return;
        }

        meta->SetString("fuel_totaltime", std::to_string(fuelTime));
        ReportMetadataChange(meta, pos, "fuel_totaltime");
        meta->SetString("fuel_time", "0");
        ReportMetadataChange(meta, pos, "fuel_time");

        fuelList = inv ? inv->GetList("fuel") : nullptr;
        if (fuelList != nullptr && fuelList->GetSize())
            fuelList->ChangeItem(0, fuelItem);
    }
}

Inventory* TutorialGame::CreateDetachedInventory(const std::string& name, const std::string& player)
{
    DetachedInventory detachedInventory;
    if (name == "creative_trash")
    {
        detachedInventory.allowPut = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            int count = 0;
            if (Settings::Get()->GetBool("creative_mode"))
                count = stack.count;

            return count;
        };

        detachedInventory.onPut = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            if (invList != NULL && index >= 0 && index < (int)invList->GetSize()) 
            {
                invList->ChangeItem(index, stack);
                return true;
            }
            else return false;
        };
    }
    else
    {
        detachedInventory.allowMove = [this](Inventory* inv, InventoryList* fromList, unsigned int fromIndex,
            InventoryList* toList, unsigned int toIndex, int count, const std::string& player)
        {
            if (Settings::Get()->GetBool("creative_mode"))
                return count;
            else
                return 0;
        };

        detachedInventory.allowPut = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            return 0;
        };

        detachedInventory.allowTake = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            if (Settings::Get()->GetBool("creative_mode"))
                return -1;
            else
                return 0;
        };

        detachedInventory.onMove = [this](Inventory* inv, InventoryList* fromList, unsigned int fromIndex,
            InventoryList* toList, unsigned int toIndex, int count, const std::string& player)
        {
            return true;
        };

        detachedInventory.onPut = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            return true;
        };

        detachedInventory.onTake = [this](Inventory* inv, InventoryList* invList,
            unsigned int index, const ItemStack& stack, const std::string& player)
        {
            if (!stack.IsEmpty())
            {
                LogInformation("action " + player + " takes " + stack.name + " from creative inventory");
            }
            return true;
        };
    }

    mDetachedInventories[name] = detachedInventory;
    Inventory* inv = mEnvironment->GetInventoryManager()->CreateDetachedInventory(
        name, mEnvironment->GetItemManager(), player);
    if (inv)
    {
        InventoryLocation loc;
        loc.SetDetached(name);
        //InvRef::create(L, loc);
    }
    return inv;
}

void TutorialGame::RemoveDetachedInventory(const std::string& name)
{
    /*
        core.detached_inventories[name] = nil
        return core.remove_detached_inventory_raw(name)
    */
    mEnvironment->GetInventoryManager()->RemoveDetachedInventory(name);
}