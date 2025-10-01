//========================================================================
// Quake.h : source file for the sample game
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

#ifndef QUAKE_H
#define QUAKE_H

#include "Game/Game.h"
#include "Games/Subgames.h"

#include "Data/MetricsBackend.h"

#include "Games/Actors/PlayerActor.h"

#include "Games/Actors/GrenadeFire.h"
#include "Games/Actors/PlasmaFire.h"
#include "Games/Actors/RocketFire.h"
#include "Games/Actors/AmmoPickup.h"
#include "Games/Actors/ItemPickup.h"
#include "Games/Actors/ArmorPickup.h"
#include "Games/Actors/HealthPickup.h"
#include "Games/Actors/WeaponPickup.h"
#include "Games/Actors/PushTrigger.h"
#include "Games/Actors/TeleporterTrigger.h"

#include "Core/OS/OS.h"

#include "Audio/Sound.h"

#include "Application/Settings.h"

#include "QuakeAIManager.h"

#define CHAT_MESSAGE_LIMIT_PER_10S 10.0f

#define	MAX_SPAWN_POINTS	128
#define	DEFAULT_GRAVITY		800
#define	GIB_HEALTH			-40
#define	ARMOR_PROTECTION	0.66

#define	GENTITYNUM_BITS		10		// don't need to send any more
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communcated over the net need to
// also be in this range
#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)

#define	MAX_MODELS			256		// these are sent over the net as 8 bits
#define	MAX_SOUNDS			256		// so they cannot be blindly increased

#define	CARNAGE_REWARD_TIME	3000
#define REWARD_SPRITE_TIME	2000

// contents flags are seperate bits
// a given brush can contribute multiple content bits

#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define CONTENTS_NOTTEAM1		0x0080
#define CONTENTS_NOTTEAM2		0x0100
#define CONTENTS_NOBOTCLIP		0x0200

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000
//bot specific contents types
#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000
#define CONTENTS_BOTCLIP		0x400000
#define CONTENTS_MOVER			0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

// Entity Flgas
#define	EF_DEAD				0x00000001		// don't draw a foe marker over players with EF_DEAD
#define	EF_TELEPORT_BIT		0x00000004		// toggled every time the origin abruptly changes
#define	EF_AWARD_EXCELLENT	0x00000008		// draw an excellent sprite
#define EF_PLAYER_EVENT		0x00000010
#define	EF_BOUNCE			0x00000010		// for missiles
#define	EF_BOUNCE_HALF		0x00000020		// for missiles
#define	EF_AWARD_GAUNTLET	0x00000040		// draw a gauntlet sprite
#define	EF_NODRAW			0x00000080		// may have an event, but no model (unspawned items)
#define	EF_FIRING			0x00000100		// for lightning gun
#define	EF_KAMIKAZE			0x00000200
#define	EF_MOVER_STOP		0x00000400		// will push otherwise
#define EF_AWARD_CAP		0x00000800		// draw the capture sprite
#define	EF_TALK				0x00001000		// draw a talk balloon
#define	EF_CONNECTION		0x00002000		// draw a connection trouble sprite
#define	EF_VOTED			0x00004000		// already cast a vote
#define	EF_AWARD_IMPRESSIVE	0x00008000		// draw an impressive sprite
#define	EF_AWARD_DEFEND		0x00010000		// draw a defend sprite
#define	EF_AWARD_ASSIST		0x00020000		// draw a assist sprite
#define EF_AWARD_DENIED		0x00040000		// denied
#define EF_TEAMVOTED		0x00080000		// already cast a team vote

#define	LIGHTNING_RANGE		768

#define MACHINEGUN_SPREAD	200
#define	MACHINEGUN_DAMAGE	5
#define	MACHINEGUN_TEAM_DAMAGE	5		// wimpier MG in teamplay

#define DEFAULT_SHOTGUN_SPREAD	700
#define DEFAULT_SHOTGUN_COUNT	11

class BaseEventManager;
class NetworkEventForwarder;

class QuakeLogicThread;

struct MediaInfo
{
	std::wstring path;
	std::string sha1Digest;

	MediaInfo(const std::wstring& aPath = L"",
		const std::string& aSha1Digest = "") :
		path(aPath), sha1Digest(aSha1Digest)
	{

	}
};

enum ItemType 
{
	IT_WEAPON,				// EFX: rotate + upscale + minlight
	IT_AMMO,				// EFX: rotate
	IT_ARMOR,				// EFX: rotate + minlight
	IT_HEALTH,				// EFX: static external sphere + rotating internal
	IT_POWERUP,				// instant on, timer based
							// EFX: rotate + external ring that rotates
	IT_HOLDABLE,			// single use, holdable item
							// EFX: rotate + bob
	IT_COUNT
};


enum PowerupType 
{
	PW_NONE,

	PW_QUAD,
	PW_BATTLESUIT,
	PW_HASTE,
	PW_INVIS,
	PW_REGEN,
	PW_FLIGHT,

	PW_REDFLAG,
	PW_BLUEFLAG,
	PW_NEUTRALFLAG,

	PW_SCOUT,
	PW_GUARD,
	PW_DOUBLER,
	PW_AMMOREGEN,
	PW_INVULNERABILITY,

	PW_NUM_POWERUPS
};

enum EntityType
{
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_BEAM,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_GRAPPLE,				// grapple hooked on wall
	ET_TEAM,

	ET_EVENTS				// any of the EV_* events can be added freestanding
							// by setting eType to ET_EVENTS + eventNum
							// this avoids having to set eFlags and eventNum
};

struct SoundParams
{
	enum Type
	{
		SP_LOCAL,
		SP_POSITIONAL,
		SP_OBJECT
	} type = SP_LOCAL;
	float gain = 1.0f;
	float fade = 0.0f;
	float pitch = 1.0f;
	bool loop = false;
	float maxHearDistance = 700;
	Vector3<float> position = Vector3<float>::Zero();
	ActorId object = INVALID_ACTOR_ID;
	ActorId toPlayer = INVALID_ACTOR_ID;
	ActorId excludePlayer = INVALID_ACTOR_ID;

	Vector3<float> GetPosition(bool* positionExists) const;
};

struct SoundPlaying
{
	SoundParams params;
	SimpleSound sound;
	std::unordered_set<ActorId> actors; // ActorIds
};


class StatBars
{
public:

	StatBars();
	~StatBars();

	void Remove(const std::shared_ptr<PlayerActor>& player);
	void Update(const std::shared_ptr<PlayerActor>& player);
	bool ReplaceHud(HudElement* hud, std::string hudName);
	bool EventHandler(const std::shared_ptr<PlayerActor>& player, std::string eventName);

protected:

	//cache setting
	bool mEnableDamage;

	HudElement* mAmmo;
	HudElement* mArmor;
	HudElement* mHealth;
	HudElement* mScore;

	std::vector<ActorId> mHudIds;
	std::map<ActorId, uint16_t> mScoreIds;
	std::map<ActorId, uint16_t> mHealthIds;
	std::map<ActorId, uint16_t> mArmorIds;
	std::map<ActorId, uint16_t> mAmmoIds;
};

//---------------------------------------------------------------------------------------------------------------------
// QuakeLogic class                        - Chapter 21, page 723
//---------------------------------------------------------------------------------------------------------------------
class QuakeLogic : public GameLogic
{
	friend class QuakeAIManager;
	friend class QuakeLogicThread;

public:
	QuakeLogic();
	virtual ~QuakeLogic();

	// Update
	virtual void SetProxy();

	virtual void SyncActor(const ActorId id, Transform const &transform);

	virtual void ResetViewType();
	virtual void UpdateViewType(const std::shared_ptr<BaseGameView>& pView, bool add);

	virtual void Start(tinyxml2::XMLElement* pLevelData);
	virtual void Stop();

	void Step(float dTime);

	// This is run by logic thread and does the actual processing
	void AsyncStep();

	// Logic Update
	virtual void OnUpdate(float time, float deltaMs);

	// Overloads
	virtual void ChangeState(BaseGameState newState);
	virtual std::shared_ptr<BaseGamePhysic> GetGamePhysics(void) { return mPhysics; }

	Subgame GetGameSpec() { return mGameSpec; }
	std::string GetWorldPath() { return mWorldSpec.mPath; }

	void SendShowFormMessage(ActorId actorId, const std::string& form, const std::string& formName);

	int PlaySound(const SimpleSound& sound, const SoundParams& params, bool ephemeral = false);
	void FadeSound(int handle, float step, float gain);
	void StopSound(int handle);
	void RemoveSounds(const std::vector<int>& soundList);

	//Actors410 
	std::shared_ptr<Actor> GetRandomActor();
	void GetAmmoActors(std::vector<ActorId>& ammos);
	void GetArmorActors(std::vector<ActorId>& armors);
	void GetWeaponActors(std::vector<ActorId>& weapons);
	void GetHealthActors(std::vector<ActorId>& healths);
	void GetAmmoActors(std::vector<std::shared_ptr<Actor>>& ammos);
	void GetArmorActors(std::vector<std::shared_ptr<Actor>>& armors);
	void GetWeaponActors(std::vector<std::shared_ptr<Actor>>& weapons);
	void GetHealthActors(std::vector<std::shared_ptr<Actor>>& healths);
	void GetPlayerActors(std::vector<std::shared_ptr<PlayerActor>>& players);
	void GetExplosionActors(std::vector<std::shared_ptr<Actor>>& explosions);
	void GetFiringActors(std::vector<std::shared_ptr<Actor>>& firings);
	void GetTriggerActors(std::vector<std::shared_ptr<Actor>>& triggers);
	void GetTargetActors(std::vector<std::shared_ptr<Actor>>& targets);

	//Items
	bool CanItemBeGrabbed(const std::shared_ptr<Actor>& item, const std::shared_ptr<PlayerActor>& player);

	//Spawn Points
	void SelectRandomFurthestSpawnPoint(const Vector3<float>& avoidPoint, Transform& transform, bool checkCollision = true);
	void SelectSpawnPoint(const Vector3<float>& avoidPoint, Transform& transform, bool checkCollision = true);
	void SelectNearestSpawnPoint(const Vector3<float>& from, std::shared_ptr<Actor>& spot);
	void SelectRandomSpawnPoint(std::shared_ptr<Actor>& spot, bool checkCollision = true);
	void SelectInitialSpawnPoint(Transform& transform);

	// event delegates
	void GameInitDelegate(BaseEventDataPtr pEventData);
	void SetControlledActorDelegate(BaseEventDataPtr pEventData);
	void RemoveControlledActorDelegate(BaseEventDataPtr pEventData);
	void RequestStartGameDelegate(BaseEventDataPtr pEventData);
	void RemoteClientDelegate(BaseEventDataPtr pEventData);
	void NetworkPlayerActorAssignmentDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerEnterDelegate(BaseEventDataPtr pEventData);
	void PhysicsTriggerLeaveDelegate(BaseEventDataPtr pEventData);
	void PhysicsCollisionDelegate(BaseEventDataPtr pEventData);
	void PhysicsSeparationDelegate(BaseEventDataPtr pEventData);
	void EnvironmentLoadedDelegate(BaseEventDataPtr pEventData);

	void HandleRemoveSoundDelegate(BaseEventDataPtr pEventData);

	void HandleChatMessageDelegate(BaseEventDataPtr pEventData);
	void HandleNotifyPlayerDelegate(BaseEventDataPtr pEventData);

	void SimulateAIGameDelegate(BaseEventDataPtr pEventData);
	void AnalyzeAIGameDelegate(BaseEventDataPtr pEventData);
	void ShowAIGameAnalysisDelegate(BaseEventDataPtr pEventData);
	void ShowAIGameDelegate(BaseEventDataPtr pEventData);
	void SaveAIGameDelegate(BaseEventDataPtr pEventData);

	void SaveAllDelegate(BaseEventDataPtr pEventData);

	void ShowGameSimulationDelegate(BaseEventDataPtr pEventData);
	void ShowGameStateDelegate(BaseEventDataPtr pEventData);

	void SplashDamageDelegate(BaseEventDataPtr pEventData);
	void FireWeaponDelegate(BaseEventDataPtr pEventData);
	void TeleportActorDelegate(BaseEventDataPtr pEventData);
	void SpawnActorDelegate(BaseEventDataPtr pEventData);
	void PushActorDelegate(BaseEventDataPtr pEventData);
	void JumpActorDelegate(BaseEventDataPtr pEventData);
	void MoveActorDelegate(BaseEventDataPtr pEventData);
	void FallActorDelegate(BaseEventDataPtr pEventData);
	void RotateActorDelegate(BaseEventDataPtr pEventData);

	void ChangeWeaponDelegate(BaseEventDataPtr pEventData);

protected:

	void NotifyPlayers(const std::wstring& msg);

	bool CanSendChatMessage() const;
	void SendChatMessage(const std::wstring& message);

	bool GetChatMessage(std::wstring& res);
	void UpdateChat(float deltaMs, const Vector2<unsigned int>& screensize);
	inline void PushToChatQueue(ChatMessage* cec)
	{
		mChatQueue.push(cec);
	}

	void HandleChatMessage(const ChatMessage& message);

	// This returns the answer to the sender of wmessage, or "" if there is none
	std::wstring HandleChat(const std::string& name, std::wstring messageInput, bool checkShoutPriv = false);

	bool HudSetFlags(const std::shared_ptr<PlayerActor>& player, unsigned int flags, unsigned int mask);

	// event registers
	void RegisterPhysicsDelegates(void);
	void RemovePhysicsDelegates(void);
	void RegisterAllDelegates(void);
	void RemoveAllDelegates(void);
	void CreateNetworkEventForwarder(const int socketId);
	void DestroyAllNetworkEventForwarders(void);

	unsigned int GetGameTime() const { return mGameTime; }

	void ReportMaxLagEstimate(float f) { mMaxLagEstimate = f; }
	float GetMaxLagEstimate() { return mMaxLagEstimate; }


	std::shared_ptr<PlayerActor> CreatePlayerActor(const std::string& actorResource,
		tinyxml2::XMLElement* overrides, const Transform* initialTransform = NULL,
		const ActorId serversActorId = INVALID_ACTOR_ID);

	virtual ActorFactory* CreateActorFactory(void);
	virtual LevelManager* CreateLevelManager(void);
	virtual AIManager* CreateAIManager(void);

	void AnalyzeAIGame(unsigned short analysisFrame, unsigned short playerIndex);
	void DestroyAIGameActors();

	void UpdateGameAIState();
	void UpdateGameAI(float deltaMs);
	void UpdateGameAIAnalysis(
		unsigned short tabIndex, unsigned short analysisFrame, unsigned short playerIndex, 
		const std::string& decisionCluster, const std::string& evaluationCluster);
	void UpdateGameAISimulation(unsigned short frame);

	virtual bool LoadGameDelegate(tinyxml2::XMLElement* pRoot);

	bool LoadGameAsync(tinyxml2::XMLElement* pRoot);

	void LoadActors(BspLoader& bspLoader);

	std::shared_ptr<StatBars> mStatBars;

	std::unordered_map<uint16_t, std::string> mFormStateData;

	// Environment mutex (envlock)
	std::mutex mEnvMutex;

	std::recursive_mutex mEnvRecMutex;

	std::list<NetworkEventForwarder*> mNetworkEventForwarders;

private:

	bool GetGameInit();

	bool AddMediaFile(const std::wstring& fileName,
		const std::wstring& filePath, const std::wstring& fileRelativePath,
		std::string* fileData = nullptr, std::string* digest = nullptr);
	void FillMediaCache();
	void SendMediaData();

	std::wstring FormatChatMessage(const std::string& name, const std::string& message);
	bool OnChatMessage(const std::string& name, const std::string& message);

	bool SpotTelefrag(const std::shared_ptr<Actor>& spot);
	void LookAtKiller(
		const std::shared_ptr<Actor>& inflictor,
		const std::shared_ptr<PlayerActor>& player,
		const std::shared_ptr<PlayerActor>& attacker);
	void Die(int damage, MeansOfDeath meansOfDeath,
		const std::shared_ptr<Actor>& inflictor,
		const std::shared_ptr<PlayerActor>& player,
		const std::shared_ptr<PlayerActor>& attacker);
	void Damage(int damage, int dflags, int mod,
		Vector3<float> dir, Vector3<float> point,
		const std::shared_ptr<PlayerActor>& target,
		const std::shared_ptr<Actor>& inflictor,
		const std::shared_ptr<PlayerActor>& attacker);
	void DamageFeedback(const std::shared_ptr<PlayerActor>& player);
	bool LogAccuracyHit(const std::shared_ptr<PlayerActor>& target, 
		const std::shared_ptr<PlayerActor>& attacker);
	bool CanDamage(const std::shared_ptr<PlayerActor>& target, Vector3<float> origin);
	int CheckArmor(std::shared_ptr<PlayerActor> playerActor, int damage, int dflags);
	bool RadiusDamage(float damage, float radius, int mod,
		Vector3<float> origin, const std::shared_ptr<PlayerActor>& attacker);

	void GauntletAttack(const std::shared_ptr<PlayerActor>& player,
		const Vector3<float>& muzzle, const Vector3<float>& forward);
	void BulletFire(const std::shared_ptr<PlayerActor>& player, const Vector3<float>& muzzle, 
		const Vector3<float>& forward, const Vector3<float>& right, const Vector3<float>& up, 
		float spread, int damage);
	void ShotgunFire(const std::shared_ptr<PlayerActor>& player, const Vector3<float>& muzzle, 
		const Vector3<float>& forward, const Vector3<float>& right, const Vector3<float>& up);
	bool ShotgunPellet(const std::shared_ptr<PlayerActor>& player,
		const Vector3<float>& forward, const Vector3<float>& start, const Vector3<float>& end);
	void GrenadeLauncherFire(const std::shared_ptr<PlayerActor>& player, 
		const Vector3<float>& muzzle, const Vector3<float>& forward, const EulerAngles<float>& angles);
	void RocketLauncherFire(const std::shared_ptr<PlayerActor>& player, 
		const Vector3<float>& muzzle, const Vector3<float>& forward, const EulerAngles<float>& angles);
	void PlasmagunFire(const std::shared_ptr<PlayerActor>& player, 
		const Vector3<float>& muzzle, const Vector3<float>& forward, const EulerAngles<float>& angles);
	void RailgunFire(const std::shared_ptr<PlayerActor>& player, 
		const Vector3<float>& muzzle, const Vector3<float>& forward);
	void LightningFire(const std::shared_ptr<PlayerActor>& player, 
		const Vector3<float>& muzzle, const Vector3<float>& forward);

	int PickupAmmo(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<AmmoPickup>& ammo);
	int PickupWeapon(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<WeaponPickup>& weapon);
	int PickupHealth(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<HealthPickup>& health);
	int PickupArmor(const std::shared_ptr<PlayerActor>& player, const std::shared_ptr<ArmorPickup>& armor);

	// Subgame specification
	Subgame mGameSpec;

	// World Spec must be kept in sync!
	WorldSpec mWorldSpec;

	bool mGameInit = false;
	bool mGameAICombat = false;
	bool mGameAISimulation = false;
	AIGame::GameState mGameAIState;

	int mGameDecision = -1;
	int mPlayerEvaluation = -1;

	AIAnalysis::PlayerInput mPlayerInput;
	AIAnalysis::PlayerInput mOtherPlayerInput;

	float mSendRecommendedTimer = 0.0f;
	IntervalLimiter mObjectManagementInterval;

	// Hud Elements logic
	HudElement* mHealthBar;
	HudElement* mArmorBar;

	std::map<ActorId, unsigned int> mHuds;

	// Sounds
	std::unordered_map<int, SoundPlaying> mPlayingSounds;
	int mNextSoundId = 0; // positive values only
	int NextSoundId();

	unsigned int mGameTime = 0;
	// A helper variable for incrementing the latter
	float mGameTimeFractionCounter = 0.0f;

	// An interval for generally sending object positions and stuff
	float mRecommendedSendInterval = 0.1f;
	// Estimate for general maximum lag as determined by logic.
	// Can raise to high values like 15s with eg. map generation mods.
	float mMaxLagEstimate = 0.1f;

	/* Threads */

	// The quake logic mainly operates in this thread
	QuakeLogicThread* mThread = nullptr;

	// media files known
	std::unordered_map<std::wstring, MediaInfo> mMedia;

	unsigned short mMaxChatMessageLength;

	ChatBackend* mChatBackend = nullptr;
	std::queue<std::string> mChatLogBuf;

	std::queue<std::wstring> mOutChatQueue;
	unsigned int mLastChatMessageSent = (unsigned int)std::time(0);
	float mChatMessageAllowance = 5.0f;
	std::queue<ChatMessage*> mChatQueue;

	// Global logic metrics backend
	std::unique_ptr<MetricsBackend> mMetricsBackend;

	// Logic metrics
	MetricCounterPtr mUptimeCounter;
};


class QuakeLogicThread : public Thread
{
	friend class QuakeLogic;

public:

	QuakeLogicThread(QuakeLogic* logic) : Thread("QuakeLogic"), mGameLogic(logic)
	{

	}

	virtual void* Run();

protected:

	tinyxml2::XMLElement* mLevelData;

private:
	QuakeLogic* mGameLogic;
};

#endif