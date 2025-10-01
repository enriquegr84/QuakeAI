//========================================================================
// MinecraftEvents.cpp : defines game-specific events for Minecraft
//
// Part of the GameEngine Application
//
// GameEngine is the sample application that encapsulates much of the source code
// discussed in "Game Coding Complete - 4th Edition" by Mike McShaffry and David
// "Rez" Graham, published by Charles River Media. 
// ISBN-10: 1133776574 | ISBN-13: 978-1133776574
//
// If this source code has found it's way to you, and you think it has helped you
// in any way, do the authors a favor and buy a new copy of the book - there are 
// detailed explanations in it that compliment this code well. Buy a copy at Amazon.com
// by clicking here: 
//    http://www.amazon.com/gp/product/1133776574/ref=olp_product_details?ie=UTF8&me=&seller=
//
// There's a companion web site at http://www.mcshaffry.com/GameCode/
// 
// The source code is managed and maintained through Google Code: 
//    http://code.google.com/p/GameEngine/
//
// (c) Copyright 2012 Michael L. McShaffry and David Graham
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser GPL v3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
// http://www.gnu.org/licenses/lgpl-3.0.txt for more details.
//
// You should have received a copy of the GNU Lesser GPL v3
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//========================================================================

#include "MinecraftStd.h"
#include "MinecraftEvents.h"

const BaseEventType EventDataDeleteContentStore::skEventType(0x38ddd9ce);
const BaseEventType EventDataOpenContentStore::skEventType(0xd38d7005);
const BaseEventType EventDataOpenGameSelection::skEventType(0xeb4d364c);
const BaseEventType EventDataChangeGameSelection::skEventType(0xe8eba3f5);

const BaseEventType EventDataGameUIUpdate::skEventType(0x1002ded2);
const BaseEventType EventDataSetActorController::skEventType(0xbe5e3388);

const BaseEventType EventDataHudAdd::skEventType(0x42d77272);
const BaseEventType EventDataHudRemove::skEventType(0x7abffd92);
const BaseEventType EventDataHudChange::skEventType(0xf0a27a5d);
const BaseEventType EventDataHudSetFlags::skEventType(0x5b6ad2fa);
const BaseEventType EventDataHudSetParam::skEventType(0x5cd6fcdf);
const BaseEventType EventDataHudSetSky::skEventType(0x26f77de3);
const BaseEventType EventDataHudSetSun::skEventType(0x60ebc06a);
const BaseEventType EventDataHudSetMoon::skEventType(0xabe5ec99);
const BaseEventType EventDataHudSetStars::skEventType(0x590922f4);

const BaseEventType EventDataSetClouds::skEventType(0xa4673c7a);
const BaseEventType EventDataTimeOfDay::skEventType(0xfd7a1cdc);
const BaseEventType EventDataOverrideDayNightRatio::skEventType(0xf956626b);

const BaseEventType EventDataShowForm::skEventType(0xa568b495);
const BaseEventType EventDataDeathScreen::skEventType(0xc5087f71);

const BaseEventType EventDataInitChat::skEventType(0x4aa9711);
const BaseEventType EventDataUpdateChat::skEventType(0x6402659e);

const BaseEventType EventDataMovement::skEventType(0xbafd47f9);
const BaseEventType EventDataInteract::skEventType(0x6a4bcb2f);

const BaseEventType EventDataGameInit::skEventType(0x8fa1bc35);
const BaseEventType EventDataGameReady::skEventType(0x65faf628);

const BaseEventType EventDataPlayerHP::skEventType(0x2410cecb);
const BaseEventType EventDataPlayerBreath::skEventType(0xd5e85910);
const BaseEventType EventDataPlayerItem::skEventType(0xaedd26b1);
const BaseEventType EventDataPlayerSpeed::skEventType(0x7e07e012);
const BaseEventType EventDataPlayerPosition::skEventType(0x33fb9e3c);
const BaseEventType EventDataPlayerInventoryForm::skEventType(0xb0eb2252);
const BaseEventType EventDataPlayerEyeOffset::skEventType(0x6a8bd11e);
const BaseEventType EventDataPlayerAnimations::skEventType(0x9ee33730);
const BaseEventType EventDataPlayerRespawn::skEventType(0xab1a2d1a);
const BaseEventType EventDataPlayerRegainGround::skEventType(0xd8d198f8);
const BaseEventType EventDataPlayerJump::skEventType(0x78a7bffb);
const BaseEventType EventDataPlayerMove::skEventType(0x8fe2f220);
const BaseEventType EventDataPlayerDamage::skEventType(0x9d2c82a0);
const BaseEventType EventDataPlayerFallingDamage::skEventType(0xa88d20e6);

const BaseEventType EventDataPlaySoundAt::skEventType(0xa5c092cc);
const BaseEventType EventDataPlaySoundType::skEventType(0x6033d808);
const BaseEventType EventDataStopSound::skEventType(0x590a1c23);
const BaseEventType EventDataFadeSound::skEventType(0x1e817604);
const BaseEventType EventDataRemoveSounds::skEventType(0x9b35f587);

const BaseEventType EventDataDeleteParticleSpawner::skEventType(0x3554250d);
const BaseEventType EventDataAddParticleSpawner::skEventType(0xf0efa637);
const BaseEventType EventDataSpawnParticle::skEventType(0x7d1dc2f9);

const BaseEventType EventDataViewBobbingStep::skEventType(0x5d15439e);
const BaseEventType EventDataCameraPunchLeft::skEventType(0x31b0cb56);
const BaseEventType EventDataCameraPunchRight::skEventType(0xac27aa0a);

const BaseEventType EventDataMapNodeRemove::skEventType(0x4aa00389);
const BaseEventType EventDataMapNodeAdd::skEventType(0xb843631b);
const BaseEventType EventDataMapNodeDug::skEventType(0x8ebd6ec0);

const BaseEventType EventDataActiveObjectRemoveAdd::skEventType(0xccbedc76);
const BaseEventType EventDataActiveObjectMessages::skEventType(0x587cd384);

const BaseEventType EventDataChangePassword::skEventType(0x8dbcf93a);
const BaseEventType EventDataChangeVolume::skEventType(0xf1fbe2b5);
const BaseEventType EventDataChangeMenu::skEventType(0x1cdcf9b7);

const BaseEventType EventDataSaveBlockData::skEventType(0x3df68e46);
const BaseEventType EventDataHandleBlockData::skEventType(0x7a0a1cbd);
const BaseEventType EventDataDeletedBlocks::skEventType(0x97752712);
const BaseEventType EventDataGotBlocks::skEventType(0xd115c340);

const BaseEventType EventDataHandleNodes::skEventType(0x97833760);
const BaseEventType EventDataHandleNodeMetaChanged::skEventType(0xb8203118);
const BaseEventType EventDataHandleNodeMetaFields::skEventType(0x2f4f1cab);

const BaseEventType EventDataHandleItems::skEventType(0x29048287);
const BaseEventType EventDataHandleInventory::skEventType(0x48c6dac3);
const BaseEventType EventDataHandleInventoryFields::skEventType(0xed4bd1d7);
const BaseEventType EventDataHandleInventoryAction::skEventType(0x37c13ff7);
const BaseEventType EventDataHandleDetachedInventory::skEventType(0x12d78c85);

const BaseEventType EventDataHandleMedia::skEventType(0xc3fbf20a);