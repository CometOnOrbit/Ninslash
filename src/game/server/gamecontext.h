

#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>
#include <engine/storage.h> // MapGen

#include <game/layers.h>
#include <game/voting.h>
#include <game/weapons/weapon_catalog.h>
#include <game/challenge/challenge_script_runtime.h>
#include <game/pve/expedition_save.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "blockentities.h"
#include "player.h"
#include "mapgen.h"
#include "aiskin.h"
#include "gamevote.h"

#include <engine/localization.h>

struct CAttackSource;

/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/

struct CPlayerSpecData
{
	CPlayerSpecData()
	{
		m_Kits = 0;
		m_WeaponSlot = 0;

		for(int i = 0; i < 4; i++)
			m_aWeapon[i] = {};
	}

	int m_WeaponSlot;
	CWeaponSpec m_aWeapon[4];
	int m_Kits;
};

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	// MapGen
	CMapGen m_MapGen;
	IStorage *m_pStorage;
	ILocalization *m_pLocalization;

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConSwapTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConShuffleTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConLockTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConEndRound(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponList(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponValidate(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponSpawn(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponReload(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult,
										  void *pUserData,
										  IConsole::FCommandCallback pfnCallback,
										  void *pCallbackUserData);
	static void ConMapsList(IConsole::IResult *pResult, void *pUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;

  public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	// MapGen
	CLayers *Layers() { return &m_Layers; }
	IStorage *Storage() const { return m_pStorage; }
	CMapGen *MapGen() { return &m_MapGen; }
	ILocalization *Localization() { return m_pLocalization; }

	bool GetRoamSpawnPos(vec2 *Pos);
	void WriteExpeditionSave(bool SnapshotCharacters = true);

	CExpeditionSave m_ExpeditionSave;
	bool m_ExpeditionReady;

	CBlockEntities *m_pBlockEntities;
	void CreateEntitiesForBlock(int block);
	void ActivateBlockEntities(int x);

	bool StoreEntity(int ObjType, int Type, int Subtype, int x, int y);
	void RestoreEntity(int ObjType, int Type, int Subtype, int x, int y);

	CGameContext();
	~CGameContext();

	void Clear();

	void ReloadMap();

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];

	struct CNpcSlot
	{
		class CCharacter *m_pCharacter;
		bool m_Used;
		bool m_ToBeKicked;
		bool m_Spawning;
		int m_Team;
		int m_RespawnTick;
		int m_Score;
		CAISkin m_AISkin;
	};
	CNpcSlot m_aNpcs[MAX_NPCS];
	void TickNpcs();
	void TrySpawnNpc(int Slot);
	void SnapNpcs(int SnappingClient);
	void TriggerBotAI(int TriggerLevel);

	IGameController *m_pController;
	class CPveDirector *m_pPveDirector;
	class CTutorialDirector *m_pTutorialDirector;
	CGameWorld m_World;

	CPlayerSpecData GetPlayerSpecData(int ClientID);

	CChallengeScriptRuntime m_ChallengeScript;
	char m_aChallengeContentHash[65];
	CModApiDescriptor m_ChallengeApi;
	bool m_ChallengeScriptLoaded;

	bool LoadChallengeScript();
	void DispatchChallengeEvent(EChallengeScriptEvent Event, int ClientID = -1, int Value = 0);
	void SendChallengeInfo(int ClientID);

	int m_aMostInterestingPlayer[2];

	void UpdateSpectators();

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);
	class CCharacter *GetCoreChar(int Index);
	class CPlayer *GetClientPlayer(int ClientID)
	{
		if(ClientID < 0 || ClientID >= MAX_CLIENTS)
			return 0;
		return m_apPlayers[ClientID];
	}

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason);
	void EndVote();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientID);

	int m_VoteCreator;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	enum
	{
		VOTE_ENFORCE_UNKNOWN = 0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateFlameHit(vec2 Pos);
	void CreateBuildingHit(vec2 Pos);
	void CreateDamageInd(vec2 Pos, float AngleMod, int Damage, int ClientID);
	void CreateHitConfirm(vec2 Pos, const CAttackSource &Source, int Damage, int TargetType, bool Killed);
	// Detonate a server-authoritative flash/blind grenade. The event is
	// broadcast for the one-frame light pulse while each affected player keeps
	// its own status in CPlayer and the regular snapshot.
	void CreateVisionBurst(vec2 Pos, int Kind, float Radius);
	void CreateRepairInd(vec2 Pos);
	void CreateExplosion(vec2 Pos, const CAttackSource &Source, float DamageScale = 1.0f);
	void SendEffect(int ClientID, int EffectID);
	void CreateHammerHit(vec2 Pos);
	void CreateEffect(int FX, vec2 Pos);
	int CreateDeathray(vec2 Pos);
	void CreatePlayerSpawn(vec2 Pos);
	void CreateDeath(vec2 Pos, int Who);
	void CreateSound(vec2 Pos, int Sound, int64 Mask = -1);
	void CreateWeaponSound(vec2 Pos, const CWeaponSpec &Weapon, int Slot, int64 Mask = -1);
	void CreateSoundGlobal(int Sound, int Target = -1);

	void SendMusicThreat(int ClientID);

	bool BuildableSpot(vec2 Pos);
	bool AddBlock(int Type, vec2 Pos, int Owner = -1, int KitCost = 0);
	void DamageBlocks(vec2 Pos, int Damage, int Range);
	void OnBlockChange(vec2 Pos);

	class CWeapon *NewWeapon(const CWeaponSpec &Spec);

	bool RespawnAlly(vec2 Pos, int Team, int Reviver);

	bool AddBuilding(int Kit, vec2 Pos, int Owner, int PaidCost = -1);

	bool Shop(class CPlayer *pPlayer, int Slot, bool AI = false);

	void CreateProjectile(const CAttackSource &Source,
						  int Charge,
						  vec2 Pos,
						  vec2 Direction,
						  vec2 WeaponPos,
						  class CBuilding *OwnerBuilding = 0);
	void CreateMeleeHit(
		const CAttackSource &Source, float Dmg, vec2 Pos, vec2 Direction, vec2 WeaponPos, float PowerScale = 1.0f);

	void ClearFlameHits();

	bool m_aFlameHit[MAX_CHARACTERS];

	void Repair(vec2 Pos);
	void AmmoFill(vec2 Pos, int Weapon);

	enum
	{
		CHAT_ALL = -2,
		CHAT_SPEC = -1,
		CHAT_RED = 0,
		CHAT_BLUE = 1
	};

	// network
	void SendChatTarget(int To, const char *pText, ...);
	void SendChat(int ClientID, int Mode, const char *pText, int TargetID = -1);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendBroadcast(const char *pText, int ClientID, bool Lock = false);
	void SendBroadcastFormat(int ClientID, bool Lock, const char *pText, ...);
	void SendGameVotes(int ClientID = -1);
	void SelectRecommendedModes();

	void ResetGameVotes();

	int m_WinnerVote;
	int m_NumGameVotes;
	CGameVote m_aGameVote[MAX_GAME_VOTES];
	int m_aPlayerGameVote[MAX_CLIENTS];

	void RegisterGameVote(int ClientID, int Vote);
	void SendGameVoteStats(int ClientID = -1);
	// const char *GetVoteWinnerConfig();
	void CalculateVoteWinnerConfig();

	//
	void CheckPureTuning();
	void SendTuningParams(int ClientID);

	//
	void SwapTeams();

	//
	void UpdateAI();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnShutdown();

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual void GetAISkin(CAISkin *pAISkin, bool PVP, int Level = 1, int WaveGroup = 0);
	virtual void AddZombie();
	virtual bool AIInputUpdateNeeded(int ClientID);
	virtual void AIUpdateInput(int ClientID, int *Data);

	int CountBots(bool SkipSpecialTees = false);
	int CountBotsAlive(bool SkipSpecialTees = false);
	// int CountHumans();
	int CountHumansAlive();

	virtual void OnClientConnected(int ClientID, bool AI = false);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID, const char *pReason);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual bool IsClientReady(int ClientID);
	virtual bool IsClientPlayer(int ClientID);

	virtual const char *GameType();
	virtual const char *Version();
	virtual const char *NetVersion();

	// MapGen
	virtual void SaveMap(const char *path);

	vec2 GetNearHumanSpawnPos(bool AllowVision = false);
	vec2 GetFarHumanSpawnPos(bool AllowVision = false);
	vec2 GetFarSafeStandPos(vec2 From);
	int DistanceToHuman(vec2 Pos);

	void AddBot();
	void KickBots();
	void KickBot(int ClientID);
	void KickOneBot(int Team = -1);

	bool IsBot(int ClientID);
	bool IsHuman(int ClientID);

	int m_BroadcastLockTick;

	const char *Localize(const char *pText, int ClientID);
};

inline int64 CmaskAll()
{
	return -1;
}
inline int64 CmaskOne(int ClientID)
{
	return (int64)(1ULL << ClientID);
}
inline int64 CmaskAllExceptOne(int ClientID)
{
	return CmaskAll() ^ CmaskOne(ClientID);
}
inline bool CmaskIsSet(int64 Mask, int ClientID)
{
	return (Mask & CmaskOne(ClientID)) != 0;
}
#endif
