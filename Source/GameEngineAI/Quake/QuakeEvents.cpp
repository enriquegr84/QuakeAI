//========================================================================
// QuakeEvents.cpp : defines game-specific events for Quake
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
//    http://code.google.com/p/gamecode4/
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

#include "QuakeStd.h"
#include "QuakeEvents.h"

const BaseEventType EventDataDeleteContentStore::skEventType(0xb2324eb2);
const BaseEventType EventDataOpenContentStore::skEventType(0x7f6e1d09);
const BaseEventType EventDataOpenGameSelection::skEventType(0x9f4c3871);
const BaseEventType EventDataChangeGameSelection::skEventType(0xa0d1e6d9);

const BaseEventType EventDataShowForm::skEventType(0xb1753432);

const BaseEventType EventDataInitChat::skEventType(0xe44a7188);
const BaseEventType EventDataUpdateChat::skEventType(0xd9aeb777);

const BaseEventType EventDataGameInit::skEventType(0xa88b0c6);
const BaseEventType EventDataGameReady::skEventType(0x2186f971);

const BaseEventType EventDataPlaySoundAt::skEventType(0xc4d06192);
const BaseEventType EventDataPlaySoundType::skEventType(0x2693bb0d);
const BaseEventType EventDataStopSound::skEventType(0x56266141);
const BaseEventType EventDataFadeSound::skEventType(0xd5ded2b6);
const BaseEventType EventDataRemoveSounds::skEventType(0xbbdd20a1);

const BaseEventType EventDataSaveAll::skEventType(0xfb92d693);
const BaseEventType EventDataChangeVolume::skEventType(0xf968e78c);
const BaseEventType EventDataChangeMenu::skEventType(0x413d0bf5);

const BaseEventType EventDataHandleMedia::skEventType(0x7d84febb);

const BaseEventType EventDataFireWeapon::skEventType(0x1b15b6a7);
const BaseEventType EventDataChangeWeapon::skEventType(0xcee385cc);

const BaseEventType EventDataSplashDamage::skEventType(0x23e5bdcb);
const BaseEventType EventDataDeadActor::skEventType(0xaf50e7db);
const BaseEventType EventDataTeleportActor::skEventType(0xe2a39f17);
const BaseEventType EventDataSpawnActor::skEventType(0x92f851da);
const BaseEventType EventDataPushActor::skEventType(0x47fda8e7);
const BaseEventType EventDataJumpActor::skEventType(0xfeee009e);
const BaseEventType EventDataMoveActor::skEventType(0xeeaa0a40);
const BaseEventType EventDataFallActor::skEventType(0x47d33df3);
const BaseEventType EventDataRotateActor::skEventType(0xed6973fe);

const BaseEventType EventDataClear::skEventType(0x970c61d);
const BaseEventType EventDataRemoveArc::skEventType(0x61649f05);
const BaseEventType EventDataRemoveArcType::skEventType(0xd1f03d64);
const BaseEventType EventDataRemoveNode::skEventType(0x3ffa4396);
const BaseEventType EventDataHighlightNode::skEventType(0xba614a68);
const BaseEventType EventDataEditMapNode::skEventType(0xf370d95b);
const BaseEventType EventDataShowMapNode::skEventType(0x591ea56d);

const BaseEventType EventDataChangeAnalysisFrame::skEventType(0xd6a1d469);
const BaseEventType EventDataShowGameSimulation::skEventType(0xe7dd903f);
const BaseEventType EventDataShowGameState::skEventType(0xdf8ca2);

const BaseEventType EventDataEditPathing::skEventType(0xc37cc8a6);
const BaseEventType EventDataShowPathing::skEventType(0x1af3f6ee);
const BaseEventType EventDataCreatePathing::skEventType(0x92c31d5d);
const BaseEventType EventDataCreatePathingMap::skEventType(0xce4860a1);
const BaseEventType EventDataCreatePathingNode::skEventType(0x37f528e9);
const BaseEventType EventDataSimulateExploring::skEventType(0x8ffb7ec9);
const BaseEventType EventDataSimulatePathing::skEventType(0xe917e522);
const BaseEventType EventDataSimulateAIGame::skEventType(0x95308432);
const BaseEventType EventDataAnalyzeAIGame::skEventType(0x11131e45);
const BaseEventType EventDataShowAIGameAnalysis::skEventType(0xd3924190);
const BaseEventType EventDataShowAIGame::skEventType(0x29eaa0c5);
const BaseEventType EventDataSaveAIGame::skEventType(0xdd5c01fb);

const BaseEventType EventDataPlayDuelCombat::skEventType(0x5d83116f);

const BaseEventType EventDataNodeVisibility::skEventType(0x13e7b61b);
const BaseEventType EventDataNodeConnection::skEventType(0xb0f0f0f3);
const BaseEventType EventDataArcConnection::skEventType(0x667969e7);

const BaseEventType EventDataSaveMap::skEventType(0x5d651f12);
const BaseEventType EventDataEditMap::skEventType(0x11f2ac24);
const BaseEventType EventDataShowMap::skEventType(0x1fc7f3db);
const BaseEventType EventDataCreateMap::skEventType(0xd362239b);
const BaseEventType EventDataCreatePath::skEventType(0x6068a353);

const BaseEventType EventDataGameplayUIUpdate::skEventType(0x1002ded2);
const BaseEventType EventDataSetControlledActor::skEventType(0xbe5e3388);
const BaseEventType EventDataRemoveControlledActor::skEventType(0xc420e670);