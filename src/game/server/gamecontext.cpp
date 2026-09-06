#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wvarargs"
#endif
#include <cstring>
#include <new>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h> // MapGen
#include <engine/shared/mappath.h>
#include <engine/map.h>
#include <engine/console.h>
#include <engine/platform_events.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/npc.h>
#include <game/challenge/challenge_variant.h>
#include <game/server/pvp_balance.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/weapons/weapon_catalog.h>
#include <game/weapons/weapon_packages.h>
#include <game/weapons/forge.h>
#include "gamemodes/dm.h"
#include "gamemodes/cs.h"
#include "gamemodes/ball.h"
#include "gamemodes/tdm.h"
#include "gamemodes/ctf.h"
#include "gamemodes/invasion.h"
#include "gamemodes/tutorial.h"
#include "gamemodes/horde.h"
#include "gamemodes/extract.h"
#include "gamemodes/base.h"
#include "gamemodes/roam.h"
#include <game/server/roam_mapgen_layout.h>
#include "gamemodes/texasrun.h"

#include <game/server/entities/ball.h>
#include <game/server/entities/block.h>
#include <game/server/entities/pickup.h>
#include <game/server/entities/projectile.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/building.h>
#include <game/server/entities/turret.h>
#include <game/server/entities/teslacoil.h>
#include <game/server/entities/droid.h>

#include <game/server/entities/weapon.h>

#include <game/server/playerdata.h>
#include <game/pve/pve_roguelite.h>
#include <game/server/pve_director.h>
#include <game/server/bosspool.h>
#include <game/server/tutorial_director.h>
#include <game/server/blockentities.h>

#include <game/server/ai_protocol.h>
#include <game/server/ai.h>

#include <game/buildables.h>

#include <stdarg.h>

const int ExplosionDmg = 40;
const int MineExplosionDmg = 20;

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	mem_zero(m_aNpcs, sizeof(m_aNpcs));

	m_BroadcastLockTick = 0;

	m_pController = 0;
	m_pPveDirector = 0;
	m_pTutorialDirector = 0;
	m_ExpeditionReady = false;
	m_ExpeditionSave.Reset();
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;
	m_NumGameVotes = 0;
	m_WinnerVote = -1;
	m_aChallengeContentHash[0] = 0;
	m_ChallengeApi.m_ApiVersion = 0;
	m_ChallengeApi.m_Capabilities = 0;
	m_ChallengeScriptLoaded = false;

	ClearFlameHits();

	m_aMostInterestingPlayer[0] = -1;
	m_aMostInterestingPlayer[1] = -1;

	if(Resetting == NO_RESET)
		m_pVoteOptionHeap = new CHeap();
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	delete m_pPveDirector;
	m_pPveDirector = 0;
	delete m_pTutorialDirector;
	m_pTutorialDirector = 0;
	for(int i = 0; i < MAX_NPCS; i++)
	{
		delete m_aNpcs[i].m_pCharacter;
		m_aNpcs[i].m_pCharacter = 0;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new(this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}

CPlayerSpecData CGameContext::GetPlayerSpecData(int ClientID)
{
	CPlayerSpecData data;
	CCharacter *pCharacter = GetPlayerChar(ClientID);

	if(!pCharacter)
		return data;

	data.m_Kits = pCharacter->m_Kits;
	data.m_WeaponSlot = pCharacter->GetWeaponSlot();

	for(int i = 0; i < 4; i++)
	{
		CWeapon *pWeapon = pCharacter->GetWeapon(i);
		data.m_aWeapon[i] = pWeapon ? pWeapon->GetWeaponSpec() : CWeaponSpec{};
	}

	return data;
}

bool CGameContext::LoadChallengeScript()
{
	m_ChallengeScriptLoaded = false;
	m_ChallengeScript.Deactivate();
	str_copy(m_aChallengeContentHash, "none", sizeof(m_aChallengeContentHash));
	m_ChallengeApi.m_ApiVersion = 0;
	m_ChallengeApi.m_Capabilities = 0;
	if(!g_Config.m_SvChallengeScript[0])
		return true;

	char aPath[1024];
	IOHANDLE File = Storage()->OpenFile(g_Config.m_SvChallengeScript, IOFLAG_READ, IStorage::TYPE_ALL, aPath, sizeof(aPath));
	if(!File && (g_Config.m_SvChallengeScript[0] == '/' ||
				(g_Config.m_SvChallengeScript[0] && g_Config.m_SvChallengeScript[1] == ':')))
	{
		str_copy(aPath, g_Config.m_SvChallengeScript, sizeof(aPath));
		File = io_open(aPath, IOFLAG_READ);
	}
	if(!File)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "challenge", "unable to open challenge script");
		return false;
	}
	const long Size = io_length(File);
	if(Size <= 0 || Size > 1024 * 1024)
	{
		io_close(File);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "challenge", "challenge script size limit exceeded");
		return false;
	}
	char *pSource = (char *)mem_alloc((unsigned)Size, 1);
	const unsigned Read = io_read(File, pSource, (unsigned)Size);
	io_close(File);
	if(Read != (unsigned)Size)
	{
		mem_free(pSource);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "challenge", "unable to read challenge script");
		return false;
	}

	m_ChallengeApi.m_ApiVersion = ModApiCurrentVersion();
	m_ChallengeApi.m_Capabilities = MOD_CAPABILITY_GAMEPLAY_RULES;
	char aError[256];
	const bool Activated = m_ChallengeScript.Activate(
		m_ChallengeApi, (uint32_t)g_Config.m_SvMapGenSeed, aError, sizeof(aError));
	const bool Loaded = Activated && m_ChallengeScript.LoadScript(
		g_Config.m_SvChallengeScript, pSource, (int)Size, aError, sizeof(aError));
	mem_free(pSource);
	if(!Loaded)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "challenge", aError);
		m_ChallengeScript.Deactivate();
		return false;
	}
	str_copy(m_aChallengeContentHash,
		g_Config.m_SvChallengeContentHash[0] ? g_Config.m_SvChallengeContentHash : "none",
		sizeof(m_aChallengeContentHash));
	m_ChallengeScriptLoaded = true;
	return true;
}

void CGameContext::DispatchChallengeEvent(EChallengeScriptEvent Event, int ClientID, int Value)
{
	if(!m_ChallengeScriptLoaded)
		return;
	m_ChallengeScript.SetTick(Server()->Tick());
	char aError[256];
	if(!m_ChallengeScript.Dispatch(Event, ClientID, Value, aError, sizeof(aError)))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "challenge", aError);
		m_ChallengeScriptLoaded = false;
		m_ChallengeScript.Deactivate();
	}
	if(m_ChallengeScriptLoaded && Event != EChallengeScriptEvent::Tick)
	{
		CNetMsg_Sv_ChallengeEvent Msg;
		Msg.m_Event = static_cast<int>(Event);
		Msg.m_ClientID = clamp(ClientID, -1, MAX_CLIENTS - 1);
		Msg.m_Value = Value;
		Msg.m_Tick = Server()->Tick();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	for(int i = 0; i < m_ChallengeScript.CommandCount(); ++i)
	{
		const CChallengeScriptCommand *pCommand = m_ChallengeScript.CommandAt(i);
		if(!pCommand || pCommand->m_Kind != CHALLENGE_COMMAND_ADD_SCORE ||
		   pCommand->m_ClientID < 0 || pCommand->m_ClientID >= MAX_CLIENTS || !m_apPlayers[pCommand->m_ClientID])
			continue;
		m_apPlayers[pCommand->m_ClientID]->m_Score = clamp(
			m_apPlayers[pCommand->m_ClientID]->m_Score + pCommand->m_Arg0, -999999, 999999);
	}
}

void CGameContext::SendChallengeInfo(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CNetMsg_Sv_ChallengeInfo Msg;
	Msg.m_ApiVersion = m_ChallengeScriptLoaded ? m_ChallengeApi.m_ApiVersion : 0;
	Msg.m_pContentHash = m_ChallengeScriptLoaded ? m_aChallengeContentHash : "none";
	Msg.m_FixedSeed = g_Config.m_SvMapGenSeed;
	Msg.m_VariantMask = g_Config.m_SvChallengeVariants;
	Msg.m_Active = m_ChallengeScriptLoaded ? 1 : 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

bool CGameContext::RespawnAlly(vec2 Pos, int Team, int Reviver)
{
	int Current = -1;
	int DeathTick = 0;

	if(m_pController->IsCoop() && Team < 0)
		return false;
	if(m_pPveDirector && !m_pPveDirector->RespawnAllowed())
		return false;

	if(!Collision()->IsTileSolid(Pos.x - 32, Pos.y - 24) && !Collision()->IsTileSolid(Pos.x - 32, Pos.y + 24))
		Pos.x -= 32;
	else if(!Collision()->IsTileSolid(Pos.x + 32, Pos.y - 24) && !Collision()->IsTileSolid(Pos.x + 32, Pos.y + 24))
		Pos.x += 32;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team && !GetPlayerChar(i))
		{
			if(Current < 0 || DeathTick > m_apPlayers[i]->m_DeathTick)
			{
				Current = i;
				DeathTick = m_apPlayers[i]->m_DeathTick;
			}
		}
	}

	if(Current >= 0)
	{
		m_apPlayers[Current]->ForceRespawn(Pos);
		if(Reviver >= 0 && Reviver < MAX_CLIENTS && Reviver != Current && m_apPlayers[Reviver] &&
		   !m_apPlayers[Reviver]->m_IsBot)
			Server()->SendPlatformEvent(Reviver, PLATFORM_EVENT_COOP_RESCUE);
		if(m_pPveDirector && Reviver >= 0 && Reviver < MAX_CLIENTS &&
		   m_pPveDirector->PerkStacks(Reviver, PVE_CARD_NO_ONE_LEFT))
		{
			if(GetPlayerChar(Reviver))
				GetPlayerChar(Reviver)->IncreaseArmor(20);
			if(GetPlayerChar(Current))
				GetPlayerChar(Current)->IncreaseArmor(20);
		}
		return true;
	}

	return false;
}

class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
		return m_apPlayers[ClientID] ? m_apPlayers[ClientID]->GetCharacter() : 0;
	if(IsNpcCoreIndex(ClientID))
		return m_aNpcs[NpcSlotFromCore(ClientID)].m_pCharacter;
	return 0;
}

class CCharacter *CGameContext::GetCoreChar(int Index)
{
	if(Index >= 0 && Index < MAX_CLIENTS)
		return GetPlayerChar(Index);
	if(Index >= MAX_CLIENTS && Index < MAX_CHARACTERS)
		return m_aNpcs[Index - MAX_CLIENTS].m_pCharacter;
	return 0;
}

void CGameContext::CreateBuildingHit(vec2 Pos)
{
	CNetEvent_BuildingHit *pEvent =
		(CNetEvent_BuildingHit *)m_Events.Create(NETEVENTTYPE_BUILDINGHIT, sizeof(CNetEvent_BuildingHit));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateFlameHit(vec2 Pos)
{
	CNetEvent_FlameHit *pEvent =
		(CNetEvent_FlameHit *)m_Events.Create(NETEVENTTYPE_FLAMEHIT, sizeof(CNetEvent_FlameHit));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Damage, int ClientID)
{
	CNetEvent_DamageInd *pEvent =
		(CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_Angle = (int)(Angle * 256.0f + frandom() * 200 - frandom() * 200);
		// pEvent->m_Angle = (int)(Angle*256.0f);
		pEvent->m_Damage = Damage;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateHitConfirm(vec2 Pos, const CAttackSource &Source, int Damage, int TargetType, bool Killed)
{
	if(Source.m_Kind != EAttackSourceKind::PlayerWeapon || !Source.m_HitFeedback || Damage <= 0)
		return;

	const int Owner = Source.m_Owner;
	if(Owner < 0 || Owner >= MAX_CLIENTS || !m_apPlayers[Owner] || m_apPlayers[Owner]->m_IsBot)
		return;

	int64 Mask = CmaskOne(Owner);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && m_apPlayers[i]->m_SpectatorID == Owner)
			Mask |= CmaskOne(i);
	}

	CNetEvent_HitConfirm *pEvent =
		(CNetEvent_HitConfirm *)m_Events.Create(NETEVENTTYPE_HITCONFIRM, sizeof(CNetEvent_HitConfirm), Mask);
	if(!pEvent)
		return;

	pEvent->m_X = (int)Pos.x;
	pEvent->m_Y = (int)Pos.y;
	pEvent->m_Damage = Damage;
	pEvent->m_TargetType = clamp(TargetType, 0, NUM_HIT_TARGETS - 1);
	pEvent->m_Killed = Killed ? 1 : 0;
	pEvent->m_SourceKind = static_cast<int>(Source.m_Kind);
	pEvent->m_SourceType = Source.m_Type;
	pEvent->m_WeaponDefinitionId = static_cast<int>(Source.m_Weapon.m_DefinitionId);
	pEvent->m_WeaponLevel = Source.m_Weapon.m_Level;
}

void CGameContext::CreateVisionBurst(vec2 Pos, int Kind, float Radius)
{
	const int EventKind = clamp(Kind, 0, 1);
	const int EventRadius = clamp((int)Radius, 1, 2048);
	CNetEvent_VisionBurst *pEvent =
		(CNetEvent_VisionBurst *)m_Events.Create(NETEVENTTYPE_VISIONBURST, sizeof(CNetEvent_VisionBurst));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_Kind = EventKind;
		pEvent->m_Radius = EventRadius;
	}

	// The source itself is intentionally not filtered from this pass. A flash
	// is a global light pulse, and the client-side cl_lighting preference must
	// not be able to hide a server-authored gameplay effect.
	const float EffectRadius = clamp(Radius, 1.0f, 2048.0f);
	const int DurationTicks = EventKind == 0 ? (int)(3.0f * Server()->TickSpeed()) :
		(int)(5.0f * Server()->TickSpeed());

	for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
	{
		CPlayer *pPlayer = m_apPlayers[ClientID];
		CCharacter *pCharacter = pPlayer ? pPlayer->GetCharacter() : 0;
		if(!pPlayer || !pCharacter || !pCharacter->IsAlive())
			continue;

		const float Distance = distance(Pos, pCharacter->m_Pos);
		if(Distance > EffectRadius)
			continue;

		// Platforms are deliberately transparent to vision effects. Solid walls,
		// ramps and dynamic blocks still stop the ray, matching gameplay lighting.
		if(Collision()->IntersectLine(Pos,
			pCharacter->m_Pos,
			0,
			0,
			false,
			false,
			true) != 0)
			continue;

		const float Falloff = clamp(1.0f - Distance / EffectRadius, 0.0f, 1.0f);
		if(EventKind == 0)
		{
			// Keep the pulse strongest at the epicentre and shorten it at the edge;
			// the maximum duration is exactly three seconds.
			const int FlashTicks = max(1, (int)(DurationTicks * Falloff));
			const int Alpha = clamp((int)(255.0f * Falloff), 24, 255);
			pPlayer->ApplyFlashEffect(FlashTicks, Alpha);
		}
		else
		{
			pPlayer->ApplyBlindEffect(DurationTicks);
		}

		if(pPlayer->m_pAI)
			pPlayer->m_pAI->SetVisionSuppressed(true);
	}
}

void CGameContext::CreateRepairInd(vec2 Pos)
{
	CNetEvent_Repair *pEvent = (CNetEvent_Repair *)m_Events.Create(NETEVENTTYPE_REPAIR, sizeof(CNetEvent_Repair));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateHammerHit(vec2 Pos)
{
	// create the event
	CNetEvent_HammerHit *pEvent =
		(CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

bool CGameContext::GetRoamSpawnPos(vec2 *Pos)
{
	if(!m_pBlockEntities)
		return false;

	m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, 0, true);

	return m_pBlockEntities->GetSpawn(Pos);
}

int CGameContext::CreateDeathray(vec2 Pos)
{
	// get height
	vec2 To = Pos + vec2(0, 1200);

	Collision()->IntersectLine(Pos, To, 0x0, &To);

	int Height = To.y - Pos.y + 14;

	// create the event
	CNetEvent_Lazer *pEvent = (CNetEvent_Lazer *)m_Events.Create(NETEVENTTYPE_LAZER, sizeof(CNetEvent_Lazer));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_Height = Height;
	}

	return Height;
}

bool CGameContext::BuildableSpot(vec2 Pos)
{
	if(Collision()->GetCollisionAt(Pos.x, Pos.y) & CCollision::COLFLAG_SOLID || !Collision()->CanBuildBlock(Pos))
		return false;

	CEntity *apEnts[8];
	static const int s_aBlockingTypes[] = {
		CGameWorld::ENTTYPE_BUILDING,
		CGameWorld::ENTTYPE_WEAPON,
		CGameWorld::ENTTYPE_PICKUP,
		CGameWorld::ENTTYPE_BLOCK,
	};
	for(unsigned t = 0; t < sizeof(s_aBlockingTypes) / sizeof(s_aBlockingTypes[0]); t++)
	{
		if(m_World.FindEntities(Pos, 32.0f, apEnts, 8, s_aBlockingTypes[t]) > 0)
			return false;
	}

	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pCharacter = GetPlayerChar(i);

		if(pCharacter && abs(Pos.x - pCharacter->m_Pos.x) < 32.0f && abs(Pos.y - pCharacter->m_Pos.y + 10) < 64.0f)
			return false;
	}

	return true;
}

void CGameContext::OnBlockChange(vec2 Pos)
{
	// force characters to update and send the core
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pCharacter = GetPlayerChar(i);

		if(pCharacter && abs(Pos.x - pCharacter->m_Pos.x) < 1000 && abs(Pos.y - pCharacter->m_Pos.y) < 1000)
			pCharacter->m_ForceCoreSend = true;
	}

	// check if buildings are affected
	CBuilding *apEnts[512];
	int Num = m_World.FindEntities(Pos, 100, (CEntity **)apEnts, 512, CGameWorld::ENTTYPE_BUILDING);

	for(int i = 0; i < Num; ++i)
	{
		apEnts[i]->DoFallCheck();
	}
}

bool CGameContext::AddBlock(int Type, vec2 Pos, int Owner, int KitCost)
{
	if(!BuildableSpot(Pos))
		return false;

	// remove existing blocks
	vec2 BPos = vec2(int(Pos.x / 32) * 32, int(Pos.y / 32) * 32);
	CBlock *apEnts[16];
	int Num = m_World.FindBlocks(BPos, ivec2(1, 1), (CEntity **)apEnts, 16);

	for(int i = 0; i < Num; ++i)
	{
		CBlock *pTarget = apEnts[i];
		pTarget->Destroy();
	}

	// add new one
	CBlock *pBlock = new CBlock(&m_World, Type, Pos);
	pBlock->m_PveBuilder = Owner;
	pBlock->m_PveKitCost = KitCost;
	return true;
}

void CGameContext::DamageBlocks(vec2 Pos, int Damage, int Range)
{
	vec2 BPos = vec2(int(Pos.x / 32) * 32, int(Pos.y / 32) * 32);

	CBlock *apEnts[1024];
	int Num = m_World.FindBlocks(BPos, ivec2(1, 1) * Range, (CEntity **)apEnts, 1024);

	for(int i = 0; i < Num; ++i)
	{
		CBlock *pTarget = apEnts[i];

		if(Range > 8)
		{
			float Radius = Range;
			float InnerRadius = Radius * 0.5f;
			vec2 Diff = pTarget->m_Pos - Pos + vec2(16, 16);

			float l = length(Diff);
			l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
			float Dmg = Damage * l;

			if((int)Dmg && Dmg > 0.0f)
				pTarget->TakeDamage((int)Dmg);
		}
		else
			pTarget->TakeDamage(Damage);
	}
}

void CGameContext::CreateEffect(int FX, vec2 Pos)
{
	// create the event
	CNetEvent_FX *pEvent = (CNetEvent_FX *)m_Events.Create(NETEVENTTYPE_FX, sizeof(CNetEvent_FX));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_FX = FX;
	}
}

CWeapon *CGameContext::NewWeapon(const CWeaponSpec &Spec)
{
	return new CWeapon(&m_World, Spec);
}

bool CGameContext::AddBuilding(int Kit, vec2 Pos, int Owner, int PaidCost)
{
	// float OffsetY = -(int(Pos.y)%32) + 12;
	float CheckRange = 40.0f;

	if(!g_Config.m_SvEnableBuilding)
		return false;
	if(PaidCost < 0 && Kit >= 0 && Kit < NUM_BUILDABLES)
		PaidCost = BuildableCost[Kit];

	if(Kit == BUILDABLE_BLOCK1 || Kit == BUILDABLE_BLOCK2)
		CheckRange = 32.0f;

	// check sanity
	/*
	if (!Collision()->GetCollisionAt(Pos.x-24, Pos.y+24)&CCollision::COLFLAG_SOLID ||
		!Collision()->GetCollisionAt(Pos.x+24, Pos.y+24)&CCollision::COLFLAG_SOLID ||
		Collision()->IsForceTile(Pos.x, Pos.y+24) != 0)
		return false;
		*/

	// check for close by buildings
	CBuilding *apEnts[16];
	int Num = m_World.FindEntities(Pos, 32, (CEntity **)apEnts, 16, CGameWorld::ENTTYPE_BUILDING);

	for(int i = 0; i < Num; ++i)
	{
		CBuilding *pTarget = apEnts[i];

		if(distance(pTarget->m_Pos, Pos) < CheckRange)
			return false;
	}

	if(Kit == BUILDABLE_BLOCK1)
		return AddBlock(1, Pos, Owner, PaidCost);

	if(Kit == BUILDABLE_BLOCK2)
		return AddBlock(4, Pos, Owner, PaidCost);

	if(Kit == BUILDABLE_BARREL)
	{
		CBuilding *pBuilding = new CBuilding(&m_World, Pos, BUILDING_BARREL + irandom(3), TEAM_NEUTRAL);
		pBuilding->m_PveBuilder = Owner;
		pBuilding->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_POWERBARREL)
	{
		CBuilding *pBuilding = new CBuilding(&m_World, Pos, BUILDING_POWERBARREL + irandom(2), TEAM_NEUTRAL);
		pBuilding->m_PveBuilder = Owner;
		pBuilding->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_TURRET)
	{
		CBuilding *pBuilding = new CBuilding(&m_World, Pos + vec2(0, 8), BUILDING_STAND, TEAM_NEUTRAL);
		pBuilding->m_PveBuilder = Owner;
		pBuilding->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_LIGHTNINGWALL)
	{
		int Team = m_apPlayers[Owner]->GetTeam();
		if(!m_pController->IsTeamplay())
			Team = m_apPlayers[Owner]->GetCID();

		CBuilding *pBuilding = new CBuilding(&m_World, Pos + vec2(0, -14), BUILDING_LIGHTNINGWALL, Team);
		pBuilding->m_PveBuilder = Owner;
		pBuilding->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_TESLACOIL)
	{
		int Team = m_apPlayers[Owner]->GetTeam();
		if(!m_pController->IsTeamplay())
			Team = m_apPlayers[Owner]->GetCID();

		CTeslacoil *Tesla = new CTeslacoil(&m_World, Pos + vec2(0, +35), Team, Owner);
		Tesla->m_DamageOwner = Owner;
		Tesla->m_PveBuilder = Owner;
		Tesla->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_GENERATOR)
	{
		int Team = m_apPlayers[Owner]->GetTeam();
		if(!m_pController->IsTeamplay())
			Team = m_apPlayers[Owner]->GetCID();

		CBuilding *pBuilding = new CBuilding(&m_World, Pos + vec2(0, -34), BUILDING_GENERATOR, Team);
		pBuilding->m_PveBuilder = Owner;
		pBuilding->m_PveKitCost = PaidCost;
		return true;
	}

	if(Kit == BUILDABLE_FLAMETRAP)
	{
		CBuilding *pFlametrap = new CBuilding(&m_World, Pos + vec2(0, -18), BUILDING_FLAMETRAP, TEAM_NEUTRAL);
		pFlametrap->m_PveBuilder = Owner;
		pFlametrap->m_PveKitCost = PaidCost;

		if(Collision()->IsTileSolid(Pos.x + 32, Pos.y))
		{
			pFlametrap->m_Mirror = true;
			pFlametrap->m_Pos.x += 13;
		}
		else
			pFlametrap->m_Pos.x -= 12;

		return true;
	}

	return false;
}

void CGameContext::ClearFlameHits()
{
	for(int i = 0; i < MAX_CHARACTERS; i++)
		m_aFlameHit[i] = false;
}

void CGameContext::CreateMeleeHit(
	const CAttackSource &Source, float Dmg, vec2 Pos, vec2 Direction, vec2 WeaponPos, float PowerScale)
{
	const int DamageOwner = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponVisualProfile Visual{};
	CWeaponCatalog::TryResolveAttack(
		Source, &Combat, &Visual, m_pController && !m_pController->IsCoop());
	CWeaponDefinition Definition{};
	const bool HasDefinition = Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
							   CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &Definition);
	const bool HammerImpact = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_HAMMER_IMPACT);
	const bool Flamer = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_FLAMER);
	const bool Chainsaw = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_CHAINSAW);
	const bool Tool = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_TOOL);
	float ProximityRadius = Combat.m_MeleeHitRadius;
	float Damage = Combat.m_ProjectileDamage;
	float Knockback = Combat.m_ProjectileKnockback;
	PowerScale = max(0.0f, PowerScale);
	Damage *= PowerScale;
	const float ImpactScale = 0.75f + 0.25f * PowerScale;
	Knockback *= ImpactScale;
	ProximityRadius *= ImpactScale;

	// melee damage mask
	if(!Flamer)
	{
		CCharacter *pChr = GetPlayerChar(DamageOwner);

		if(pChr && pChr->GetMask() == 5)
			Dmg *= 1.5f;
	}

	// AddBlock(1, Pos);

	// for testing the collision
	// CreateBuildingHit(Pos);

	// player collision
	{
		CCharacter *apEnts[MAX_CHARACTERS];
		int Num =
			m_World.FindEntities(Pos, ProximityRadius, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);

		for(int i = 0; i < Num; ++i)
		{
			CCharacter *pTarget = apEnts[i];

			if(pTarget->CoreIndex() == DamageOwner || pTarget->IgnoreCollision())
				continue;

			if(Flamer && Collision()->IntersectLine(Pos, pTarget->m_Pos, 0, 0))
				continue;

			if(m_pController->IsCoop() && !pTarget->m_IsBot && (DamageOwner >= 0 && !IsBot(DamageOwner)))
				continue;

			if(Flamer)
			{
				const int HitIndex = pTarget->CoreIndex();
				if(HitIndex < 0 || HitIndex >= MAX_CHARACTERS)
					continue;
				if(m_aFlameHit[HitIndex])
					continue;

				m_aFlameHit[HitIndex] = true;
			}
			else
			{
				if(Chainsaw || Tool)
					CreateEffect(FX_BLOOD2, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
				else if(Visual.m_RenderType != WRT_SPIN)
				{
					// hammer
					if(HammerImpact)
						CreateEffect(FX_BLOOD3, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
					// swords
					else
						CreateEffect(FX_BLOOD1, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
				}
			}

			float f = Combat.m_FlameAmount;
			if(f > 0.0f)
				pTarget->SetAflame(f, Source);

			if(Visual.m_RenderType == WRT_SPIN)
				pTarget->TakeDamage(Source,
									Damage * Dmg,
									normalize(pTarget->m_Pos - WeaponPos) * Knockback,
									mix(Pos, pTarget->m_Pos + vec2(0, -24), 0.75f));
			else
				pTarget->TakeDamage(Source,
									Damage * Dmg,
									(normalize(pTarget->m_Pos - WeaponPos) + normalize(Direction)) * Knockback * 0.5f,
									Pos);
		}
	}

	if(Tool)
		Damage *= -2;

	if(Flamer)
		DamageBlocks(Pos, 1 + Damage * 0.5f, ProximityRadius * 1.7f);
	else if(Chainsaw)
		DamageBlocks(Pos, Damage * 0.5f, 24 + ProximityRadius);
	else
		DamageBlocks(Pos, Damage * 0.5f, ProximityRadius * 0.9f);

	// buildings
	{
		CBuilding *apEnts[MAX_CLIENTS];
		int Num =
			m_World.FindEntities(Pos, ProximityRadius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_BUILDING);

		for(int i = 0; i < Num; ++i)
		{
			CBuilding *pTarget = apEnts[i];

			// skip own buildings in co-op
			if(m_pController->IsCoop())
			{
				if(pTarget->m_Type == BUILDING_TURRET || pTarget->m_Type == BUILDING_GENERATOR ||
				   pTarget->m_Type == BUILDING_TESLACOIL || pTarget->m_Type == BUILDING_REACTOR)
				{
					if(DamageOwner >= 0 && DamageOwner < MAX_CLIENTS)
					{
						CPlayer *pPlayer = m_apPlayers[DamageOwner];
						if(pTarget->m_Team >= 0 && pPlayer && !pPlayer->m_IsBot && Damage > 0)
							continue;
					}
				}
			}
			else if(m_pController->IsTeamplay())
			{
				if(DamageOwner >= 0 && DamageOwner < MAX_CLIENTS)
				{
					CPlayer *pPlayer = m_apPlayers[DamageOwner];
					if(pPlayer && pPlayer->GetTeam() == pTarget->m_Team && Damage > 0)
						continue;
				}
			}
			else
			{
				if(DamageOwner >= 0 && DamageOwner < MAX_CLIENTS)
				{
					CPlayer *pPlayer = m_apPlayers[DamageOwner];
					if(pPlayer && pPlayer->GetCID() == pTarget->m_Team && Damage > 0)
						continue;
				}
			}

			if(pTarget->m_Collision)
			{
				if(Flamer || Visual.m_RenderType == WRT_SPIN)
					;
				else if(Chainsaw || Tool)
					CreateEffect(FX_BLOOD2, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
				else
					CreateEffect(FX_BLOOD1, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));

				pTarget->TakeDamage(Damage * Dmg, Source, normalize(pTarget->m_Pos - WeaponPos) * Knockback * 0.5f);

				if(Flamer)
					CreateFlameHit((Pos + pTarget->m_Pos) / 2.0f +
								   vec2(frandom() - frandom(), frandom() - frandom()) * 8.0f);
				else
					CreateBuildingHit((Pos + pTarget->m_Pos) / 2.0f);
			}
		}
	}

	if(Source.m_Kind == EAttackSourceKind::Droid)
		return;

	// droids & walkers
	{
		CDroid *apEnts[MAX_CLIENTS];
		int Num =
			m_World.FindEntities(Pos, ProximityRadius, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_DROID);

		for(int i = 0; i < Num; ++i)
		{
			CDroid *pTarget = apEnts[i];

			if(pTarget->m_Health <= 0)
				continue;

			pTarget->TakeDamage(
				normalize(pTarget->m_Pos - WeaponPos) * Knockback * 0.5f, Damage * Dmg, Source, vec2(0, 0));

			if(Flamer || Visual.m_RenderType == WRT_SPIN)
				;
			else if(Chainsaw || Tool)
				CreateEffect(FX_BLOOD2, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
			else
				CreateEffect(FX_BLOOD1, (Pos + pTarget->m_Pos) / 2.0f + vec2(0, -4));
		}
	}
	if(m_pPveDirector)
		m_pPveDirector->OnMeleeAttack(Source, Pos, max(1, (int)(Damage * Dmg + 0.5f)));
}

void CGameContext::CreateProjectile(
	const CAttackSource &Source, int Charge, vec2 Pos, vec2 Direction, vec2 WeaponPos, CBuilding *OwnerBuilding)
{
	const int DamageOwner = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponVisualProfile Visual{};
	CWeaponCatalog::TryResolveAttack(
		Source, &Combat, &Visual, m_pController && !m_pController->IsCoop());
	CWeaponDefinition Definition{};
	const bool HasDefinition = Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
							   CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &Definition);
	const bool IsMelee = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_MELEE);
	const bool Shuriken = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_SHURIKEN);
	const bool Chainsaw = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_CHAINSAW);
	const bool Tool = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_TOOL);
	const bool Claw = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_CLAW);
	const bool Flamer = HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_FLAMER);
	// less damage for bots in co-op
	float Dmg = 1.0f;
	if(m_pController->IsCoop() && Source.m_Kind != EAttackSourceKind::Droid && (DamageOwner < 0 || IsBot(DamageOwner)))
		Dmg = 0.5f;

	vec2 Vel = vec2(0, 0);

	if(GetPlayerChar(DamageOwner))
		Vel = GetPlayerChar(DamageOwner)->GetVel();

	// sword hit
	if(IsMelee)
	{
		const float ChargeRatio = clamp(Charge / 100.0f, 0.0f, 1.0f);
		const float PowerScale = mix(Combat.m_ChargePowerMin, Combat.m_ChargePowerMax, ChargeRatio);
		CreateMeleeHit(Source, Dmg, Pos, Direction, WeaponPos, PowerScale);
		return;
	}

	if(Combat.m_DirectMelee)
	{
		CreateMeleeHit(Source, Dmg, Pos, Direction, WeaponPos);
	}

	if(Shuriken || Chainsaw || Tool || Claw)
	{
		if(Chainsaw)
			Pos += normalize(Direction) * Charge * 5.0f;

		CreateMeleeHit(Source, Dmg, Pos, Direction, WeaponPos);
		return;
	}
	else if(Flamer)
	{
		ClearFlameHits();
		for(int i = 0; i < 4; i++)
		{
			vec2 To = Pos + Direction * i * 58;

			Collision()->IntersectLine(Pos, To, 0x0, &To);
			CreateMeleeHit(Source, Dmg, To, Direction, WeaponPos);

			// to visualize hit points
			// CreateFlameHit(To);
		}
		ClearFlameHits();
		return;
	}

	// define the projectile type
	int HitSound = -1;
	float BulletSpread = Combat.m_ProjectileSpread;
	float Damage = Combat.m_ProjectileDamage;
	float Knockback = Combat.m_ProjectileKnockback;
	float BulletLife = Combat.m_ProjectileLife;
	const float ChargeRatio = clamp(Charge / 100.0f, 0.0f, 1.0f);
	const float ChargeDamage = mix(Combat.m_ChargeDamageMin, Combat.m_ChargeDamageMax, ChargeRatio);
	const float ChargeRange = mix(Combat.m_ChargeRangeMin, Combat.m_ChargeRangeMax, ChargeRatio);
	const int ProjectilePenetration =
		Combat.m_ProjectilePenetration == WEAPON_INFINITE_PENETRATION
			? WEAPON_INFINITE_PENETRATION
			: Combat.m_ProjectilePenetration + static_cast<int>(ChargeRatio * Combat.m_ChargePenetrationMax);
	if(!Combat.m_LaserWeapon)
		BulletLife *= ChargeRange;
	if(HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_CLUSTER) &&
	   Source.m_Weapon.m_Level == WEAPON_CLUSTER_FRAGMENT_LEVEL)
		BulletLife += frandom() * 0.7f;

	int ShotSpread = Combat.m_ShotSpread;

	// laser pistol
	if(HasDefinition && WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_ELECTRIC_GUN))
	{
		new CLaser(
			&m_World, Pos, Direction, 200.0f + Charge * 3.5f, Source, Damage * Dmg * (0.1f + Charge * 0.009f), Charge);
		return;
	}

	if(Combat.m_LaserWeapon)
	{
		const float LaserDamage = Damage * Dmg * ChargeDamage;
		const float LaserRange = Combat.m_LaserRange * ChargeRange;
		const int LaserCharge = Combat.m_ChargeControlsLaser ? Charge : Combat.m_LaserCharge;
		for(int i = 0; i < ShotSpread; i++)
		{
			float Angle = GetAngle(Direction);
			Angle -= (ShotSpread - 1) / 2.0f * pi / 180 * 4;
			Angle += i * pi / 180 * 4;
			Angle += (frandom() - frandom()) * BulletSpread;
			new CLaser(&m_World,
					   Pos,
					   vec2(cosf(Angle), sinf(Angle)),
					   LaserRange,
					   Source,
					   LaserDamage,
					   LaserCharge,
					   ProjectilePenetration);
		}
		return;
	}

	CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
	Msg.AddInt(ShotSpread);

	for(int i = 0; i < ShotSpread; i++)
	{
		float Angle = GetAngle(Direction);
		Angle -= (ShotSpread - 1) / 2.0f * pi / 180 * 4;
		Angle += i * pi / 180 * 4;
		Angle += (frandom() - frandom()) * BulletSpread;

		CProjectile *pProj = new CProjectile(&m_World,
											 Source,
											 Pos,
											 vec2(cosf(Angle), sinf(Angle)),
											 Vel,
											 (int)(Server()->TickSpeed() * BulletLife),
											 Damage * Dmg * ChargeDamage,
											 Knockback,
											 HitSound,
											 ChargeDamage,
											 ProjectilePenetration);

		pProj->m_OwnerBuilding = OwnerBuilding;

		// pack the Projectile and send it to the client Directly
		CNetObj_Projectile p;
		pProj->FillInfo(&p);

		for(unsigned i = 0; i < sizeof(CNetObj_Projectile) / sizeof(int); i++)
			Msg.AddInt(((int *)&p)[i]);
	}

	if(DamageOwner >= 0 && DamageOwner < MAX_CLIENTS)
		Server()->SendMsg(&Msg, 0, DamageOwner);
}

void CGameContext::AmmoFill(vec2 Pos, int Weapon)
{
	CNetEvent_AmmoFill *pEvent =
		(CNetEvent_AmmoFill *)m_Events.Create(NETEVENTTYPE_AMMOFILL, sizeof(CNetEvent_AmmoFill));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_Weapon = Weapon;
	}
}

void CGameContext::Repair(vec2 Pos)
{
	/*
	float CheckRange = 42.0f;

	// check if there's turret base near
	CBuilding *apEnts[16];
	int Num = m_World.FindEntities(Pos, 32, (CEntity**)apEnts, 16, CGameWorld::ENTTYPE_BUILDING);

	for (int i = 0; i < Num; ++i)
	{
		CBuilding *pTarget = apEnts[i];

		if (distance(pTarget->m_Pos, Pos) < CheckRange)
		{
			if (pTarget->Repair())
			{
				CNetEvent_Repair *pEvent = (CNetEvent_Repair *)m_Events.Create(NETEVENTTYPE_REPAIR,
	sizeof(CNetEvent_Repair)); if(pEvent)
				{
					pEvent->m_X = (int)(pTarget->m_Pos.x + Pos.x)/2;
					pEvent->m_Y = (int)(pTarget->m_Pos.y + Pos.y)/2;
				}
			}

			CreateBuildingHit(Pos);
		}
	}
	*/
}

void CGameContext::CreateExplosion(vec2 Pos, const CAttackSource &Source, float DamageScale)
{
	const int Owner = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponVisualProfile Visual{};
	if(!CWeaponCatalog::TryResolveAttack(
		Source, &Combat, &Visual, m_pController && !m_pController->IsCoop()))
		return;
	float Dmg2 = 1.0f;
	const int BaseExplosionDamage = Combat.m_ExplosionDamage;
	const float ExplosionDamage = BaseExplosionDamage * max(0.0f, DamageScale);
	const int ExplosionSound = Visual.m_ExplosionSound;

	if(m_pController->IsCoop() && IsBot(Owner))
		Dmg2 = 0.6f;

	// create the event
	CNetEvent_Explosion *pEvent =
		(CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SourceKind = static_cast<int>(Source.m_Kind);
		pEvent->m_SourceType = Source.m_Type;
		pEvent->m_WeaponDefinitionId = static_cast<int>(Source.m_Weapon.m_DefinitionId);
		pEvent->m_WeaponLevel = Source.m_Weapon.m_Level;
	}

	CWeaponDefinition ExplosionDefinition;
	if(Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
	   CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &ExplosionDefinition) &&
	   ExplosionDefinition.m_aExplosionSound[0])
		CreateWeaponSound(Pos, Source.m_Weapon, 2);
	else if(ExplosionSound)
		CreateSound(Pos, ExplosionSound);

	// deal damage
	if(!ExplosionDamage)
		return;

	CCharacter *apEnts[MAX_CHARACTERS];
	float Radius = Combat.m_ExplosionSize * 0.7f;
	if(m_pPveDirector)
		Radius = m_pPveDirector->ModifyExplosionRadius(Owner, Radius);
	// const float InnerRadius = Radius < 200.0f ? Radius*(0.5f + (200.0f-Radius)/400.0f) : Radius*0.5f;
	const float InnerRadius = Radius * 0.5f;

	DamageBlocks(Pos, ExplosionDamage * 0.5f, Radius * 0.8f);

	int Num = m_World.FindEntities(Pos, Radius, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->m_Pos - Pos - vec2(0, 8);
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = Diff / l;
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Dmg = ExplosionDamage * l;

		if((int)Dmg && Dmg > 0.0f)
			apEnts[i]->TakeDamage(Source, (int)Dmg * Dmg2, ForceDir * Dmg * 0.3f, vec2(0, 0));
	}

	CBuilding *apBuildings[32];
	Num = m_World.FindEntities(Pos, Radius, (CEntity **)apBuildings, 32, CGameWorld::ENTTYPE_BUILDING);
	for(int i = 0; i < Num; i++)
	{
		vec2 Diff = apBuildings[i]->m_Pos - Pos - vec2(0, 8);
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = Diff / l;
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Dmg = ExplosionDamage * l;

		if((int)Dmg && Dmg > 0.0f)
			apBuildings[i]->TakeDamage((int)Dmg * Dmg2, Source, ForceDir * Dmg * 0.3f);
	}

	// ball
	if(m_pController->m_pBall)
	{
		vec2 BPos = m_pController->m_pBall->m_Pos;
		vec2 Diff = BPos - Pos - vec2(0, 8);
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = Diff / l;
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Dmg = ExplosionDamage * l;

		if((int)Dmg && Dmg > 0.0f)
		{
			m_pController->m_pBall->AddForce(ForceDir * Dmg * 0.3f); //
			m_pController->m_LastBallToucher = Owner;
		}
	}

	{
		CPickup *apPickups[64];
		Num = m_World.FindEntities(Pos, Radius, (CEntity **)apPickups, 64, CGameWorld::ENTTYPE_PICKUP);
		for(int i = 0; i < Num; i++)
		{
			vec2 Diff = apPickups[i]->m_Pos - Pos - vec2(0, 8);
			vec2 ForceDir(0, 1);
			float l = length(Diff);
			if(l)
				ForceDir = Diff / l;
			l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
			float Dmg = ExplosionDamage * l;

			if((int)Dmg && Dmg > 0.0f)
				apPickups[i]->AddForce(ForceDir * Dmg * 0.3f); //
		}
	}

	CDroid *apDEnts[MAX_CLIENTS];
	int DNum = m_World.FindEntities(Pos, Radius, (CEntity **)apDEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_DROID);

	for(int i = 0; i < DNum; ++i)
	{
		CDroid *pTarget = apDEnts[i];

		if(pTarget->m_Health <= 0)
			continue;

		vec2 Diff = pTarget->m_Pos - Pos - vec2(0, 8);
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if(l)
			ForceDir = Diff / l;
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Dmg = ExplosionDamage * l;

		if((int)Dmg && Dmg > 0.0f)
			pTarget->TakeDamage(ForceDir * Dmg * 0.3f, (int)Dmg * Dmg2, Source, vec2(0, 0));
	}
}

void CGameContext::SendEffect(int ClientID, int EffectID)
{
	CNetEvent_Effect *pEvent = (CNetEvent_Effect *)m_Events.Create(NETEVENTTYPE_EFFECT, sizeof(CNetEvent_Effect));
	if(pEvent)
	{
		pEvent->m_ClientID = ClientID;
		pEvent->m_EffectID = EffectID;
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int64 Mask)
{
	if(Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent =
		(CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateWeaponSound(vec2 Pos, const CWeaponSpec &Weapon, int Slot, int64 Mask)
{
	if(Slot < 0 || Slot > 2 || !CWeaponCatalog::IsCustom(Weapon))
		return;
	CNetEvent_WeaponSound *pEvent =
		(CNetEvent_WeaponSound *)m_Events.Create(NETEVENTTYPE_WEAPONSOUND, sizeof(CNetEvent_WeaponSound), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_WeaponDefinitionId = static_cast<int>(Weapon.m_DefinitionId);
		pEvent->m_WeaponLevel = Weapon.m_Level;
		pEvent->m_Slot = Slot;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if(Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if(Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if(Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}

bool CGameContext::IsBot(int ClientID)
{
	if(IsNpcCoreIndex(ClientID))
		return m_aNpcs[NpcSlotFromCore(ClientID)].m_Used;
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsBot;
}

bool CGameContext::IsHuman(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;

	if(m_apPlayers[ClientID] && !m_apPlayers[ClientID]->m_pAI)
		return true;

	return false;
}

void CGameContext::SendChatTarget(int To, const char *pText, ...)
{
	if(To >= MAX_CLIENTS)
		return;
	// skip sending to bots
	if(IsBot(To))
		return;

	int Start = (To < 0 ? 0 : To);
	int End = (To < 0 ? MAX_CLIENTS : To + 1);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHATMODE_ALL;
	Msg.m_ClientID = -1;
	Msg.m_TargetID = -1;

	va_list VarArgs;
	va_start(VarArgs, pText);

	char aText[256];
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			va_list Copy;
			va_copy(Copy, VarArgs);
			str_format_args(aText, sizeof(aText), Localize(pText, i), Copy);
			va_end(Copy);
			Msg.m_pMessage = aText;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}

	va_end(VarArgs);
}

void CGameContext::SendChat(int ChatterClientID, int Mode, const char *pText, int TargetID)
{
	char aBuf[256];
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(
			aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Mode != CHATMODE_ALL ? "teamchat" : "chat", aBuf);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = Mode;
	Msg.m_ClientID = ChatterClientID;
	Msg.m_TargetID = -1;
	Msg.m_pMessage = pText;

	if(Mode == CHATMODE_ALL)
	{
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else if(Mode == CHATMODE_TEAM)
	{
		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		int Team = ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS ? m_apPlayers[ChatterClientID]->GetTeam() : -1;
		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && !IsBot(i) && m_apPlayers[i]->GetTeam() == Team)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else if(Mode == CHATMODE_WHISPER)
	{
		Msg.m_TargetID = TargetID;
		if(ChatterClientID >= 0)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
		if(TargetID >= 0 && TargetID != ChatterClientID && m_apPlayers[TargetID])
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, TargetID);
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	if(ClientID < 0 || ClientID >= MAX_CHARACTERS)
		return;

	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::ResetGameVotes()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "GameContext", "Resetting gamevotes");
	Server()->ResetGameVoting();

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPlayerGameVote[i] = -1;

	m_NumGameVotes = 0;
	m_WinnerVote = -1;
	for(int i = 0; i < MAX_GAME_VOTES; i++)
		m_aGameVote[i].m_Valid = false;

	while(m_NumGameVotes < MAX_GAME_VOTES)
	{
		CGameVote Vote;
		if(!Server()->GetGameVote(&Vote, m_pController->CountHumans()))
			break;

		m_aGameVote[m_NumGameVotes] = Vote;
		m_NumGameVotes++;
	}

	SelectRecommendedModes();
}

void CGameContext::SelectRecommendedModes()
{
	for(int i = 0; i < m_NumGameVotes; i++)
	{
		m_aGameVote[i].m_RecommendedRank = 0;
		m_aGameVote[i].m_IsCurrentMode = false;
	}

	if(m_pController && m_pController->m_pGameType[0])
	{
		for(int i = 0; i < m_NumGameVotes; i++)
		{
			if(!m_aGameVote[i].m_Valid || m_aGameVote[i].m_aGameType[0] == 0)
				continue;
			if(str_comp(m_aGameVote[i].m_aGameType, m_pController->m_pGameType) == 0)
			{
				m_aGameVote[i].m_IsCurrentMode = true;
				break;
			}
		}
	}

	const int Count = m_NumGameVotes;
	int aCandidates[MAX_GAME_VOTES];
	int CandidateCount = 0;
	for(int i = 0; i < Count; i++)
		if(m_aGameVote[i].m_Valid && !m_aGameVote[i].m_IsCurrentMode)
			aCandidates[CandidateCount++] = i;

	for(int i = CandidateCount - 1; i > 0; i--)
	{
		const int j = irandom(i + 1);
		const int Tmp = aCandidates[i];
		aCandidates[i] = aCandidates[j];
		aCandidates[j] = Tmp;
	}

	const int Want = min(3, CandidateCount);
	for(int i = 0; i < Want; i++)
		m_aGameVote[aCandidates[i]].m_RecommendedRank = i + 1;
}

void CGameContext::RegisterGameVote(int ClientID, int Vote)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || Vote < 0 || Vote >= m_NumGameVotes || !m_aGameVote[Vote].m_Valid)
		return;

	m_aPlayerGameVote[ClientID] = Vote;

	SendGameVoteStats();
}

void CGameContext::SendGameVoteStats(int ClientID)
{
	int aVotes[MAX_GAME_VOTES] = {0};

	// count
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_aPlayerGameVote[i] >= 0 && m_aPlayerGameVote[i] < m_NumGameVotes)
			aVotes[m_aPlayerGameVote[i]]++;

	for(int i = 0; i < m_NumGameVotes; i++)
	{
		CNetMsg_Sv_GameVoteStatus Msg;
		Msg.m_Index = i;
		Msg.m_Votes = aVotes[i];
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::CalculateVoteWinnerConfig()
{
	if(m_NumGameVotes <= 0)
	{
		m_WinnerVote = -1;
		return;
	}

	int aVotes[MAX_GAME_VOTES] = {0};

	// count
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_aPlayerGameVote[i] >= 0 && m_aPlayerGameVote[i] < m_NumGameVotes)
			aVotes[m_aPlayerGameVote[i]]++;

	int Biggest = 0;

	for(int i = 0; i < m_NumGameVotes; i++)
		if(aVotes[i] > Biggest)
			Biggest = aVotes[i];

	int Tied = 0;
	for(int i = 0; i < m_NumGameVotes; i++)
		if(aVotes[i] == Biggest)
			Tied++;

	int Pick = irandom(Tied);
	for(int i = 0; i < m_NumGameVotes; i++)
	{
		if(aVotes[i] != Biggest)
			continue;
		if(Pick-- == 0)
		{
			m_WinnerVote = i;
			return;
		}
	}
}

/*const char *CGameContext::GetVoteWinnerConfig()
{
	int aVotes[6] = {0, 0, 0, 0, 0, 0};

	// count
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_aPlayerGameVote[i] >= 0 && m_aPlayerGameVote[i] < 6)
			aVotes[m_aPlayerGameVote[i]]++;

	int Biggest = 0;

	for (int i = 0; i < 6; i++)
		if (aVotes[i] > Biggest)
			Biggest = aVotes[i];

	int j = 0;
	int i = rand()%6;

	while (aVotes[i] < Biggest && j++ < 1000)
	{
		i = rand()%6;
	}

	if (!m_aGameVote[i].m_Valid)
		return "reload";
	else
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "exec %s.cfg", m_aGameVote[i].m_aConfig);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "GetVoteWinnerConfig", aBuf);

		return static_cast < const char * > (aBuf);

		//return static_cast < const char * > (aBuf);
	}
}*/

void CGameContext::SendGameVotes(int ClientID)
{
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "GameContext", "Sending gamevotes");
	auto LocalizeDescription = [&](int Vote, int Target, char *pBuf, int BufSize) -> const char *
	{
		const char *pDescription = Localize(m_aGameVote[Vote].m_aDescription, Target);
		if(!m_aGameVote[Vote].m_DisplayLevel)
			return pDescription;

		char aLevel[32];
		str_format(aLevel, sizeof(aLevel), Localize("Level %d", Target), g_Config.m_SvMapGenLevel);
		str_format(pBuf, BufSize, "%s - %s", pDescription, aLevel);
		return pBuf;
	};

	for(int i = 0; i < m_NumGameVotes; i++)
	{
		if(m_aGameVote[i].m_Valid)
		{
			if(ClientID == -1)
			{
				for(int j = 0; j < MAX_CLIENTS; j++)
				{
					if(!m_apPlayers[j])
						continue;

					if(m_apPlayers[j]->m_IsBot)
						continue;

					char aDescription[64];
					CNetMsg_Sv_GameVote Msg;
					Msg.m_pName = Localize(m_aGameVote[i].m_aName, j);
					Msg.m_pDescription = LocalizeDescription(i, j, aDescription, sizeof(aDescription));
					Msg.m_pImage = m_aGameVote[i].m_aImage;
					Msg.m_pPlayers = "";
					Msg.m_Index = i;
					Msg.m_TimeLeft = m_pController->GetVoteTime();
					Msg.m_RecommendedRank = m_aGameVote[i].m_RecommendedRank;
					Msg.m_IsCurrentMode = m_aGameVote[i].m_IsCurrentMode ? 1 : 0;
					Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, j);
				}
			}
			else
			{
				char aDescription[64];
				CNetMsg_Sv_GameVote Msg;
				Msg.m_pName = Localize(m_aGameVote[i].m_aName, ClientID);
				Msg.m_pDescription = LocalizeDescription(i, ClientID, aDescription, sizeof(aDescription));
				Msg.m_pImage = m_aGameVote[i].m_aImage;
				Msg.m_pPlayers = "";
				Msg.m_Index = i;
				Msg.m_TimeLeft = m_pController->GetVoteTime();
				Msg.m_RecommendedRank = m_aGameVote[i].m_RecommendedRank;
				Msg.m_IsCurrentMode = m_aGameVote[i].m_IsCurrentMode ? 1 : 0;
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
			}
			// Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "GameContext", "Sending gamevote");
		}
	}

	/*
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	if(ClientID < 0)
	{
		if(Lock)
			m_BroadcastLockTick = Server()->Tick() + g_Config.m_SvBroadcastLock * Server()->TickSpeed();
	}
	else
	{
		str_copy(m_apPlayers[ClientID]->m_aBroadcast, Lock ? pText : "", sizeof(m_apPlayers[ClientID]->m_aBroadcast));
		m_apPlayers[ClientID]->m_BroadcastLockTick = Lock ? Server()->Tick() : 0;
	}
	*/
}

void CGameContext::SendBroadcast(const char *pText, int ClientID, bool Lock)
{
	if(ClientID >= MAX_CLIENTS || (ClientID >= 0 && !m_apPlayers[ClientID]))
		return;
	CNetMsg_Sv_Broadcast Msg;
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID + 1);

	// only for server demo record
	if(ClientID < 0)
	{
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);
	}

	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Msg.m_pMessage = Localize(pText, i);
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}

	if(ClientID < 0)
	{
		if(Lock)
			m_BroadcastLockTick = Server()->Tick() + g_Config.m_SvBroadcastLock * Server()->TickSpeed();
	}
	else
	{
		str_copy(m_apPlayers[ClientID]->m_aBroadcast,
				 Lock ? Localize(pText, ClientID) : "",
				 sizeof(m_apPlayers[ClientID]->m_aBroadcast));
		m_apPlayers[ClientID]->m_BroadcastLockTick = Lock ? Server()->Tick() : 0;
	}
}

void CGameContext::SendBroadcastFormat(int ClientID, bool Lock, const char *pText, ...)
{
	if(ClientID >= MAX_CLIENTS || (ClientID >= 0 && !m_apPlayers[ClientID]))
		return;
	CNetMsg_Sv_Broadcast Msg;
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID + 1);

	// only for server demo record
	if(ClientID < 0)
	{
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);
	}

	va_list VarArgs;
	va_start(VarArgs, pText);

	char aText[256];
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			va_list Copy;
			va_copy(Copy, VarArgs);
			str_format_args(aText, sizeof(aText), Localize(pText, i), Copy);
			va_end(Copy);
			Msg.m_pMessage = aText;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}

	if(ClientID < 0)
	{
		if(Lock)
			m_BroadcastLockTick = Server()->Tick() + g_Config.m_SvBroadcastLock * Server()->TickSpeed();
	}
	else
	{
		str_copy(m_apPlayers[ClientID]->m_aBroadcast,
				 Lock ? Localize(pText, ClientID) : "",
				 sizeof(m_apPlayers[ClientID]->m_aBroadcast));
		m_apPlayers[ClientID]->m_BroadcastLockTick = Lock ? Server()->Tick() : 0;
	}
	va_end(VarArgs);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if vote time has expired or is invalid
	if(time_get() > m_VoteCloseTime || m_VoteCloseTime < time_get() - time_freq() * 25)
		m_VoteCloseTime = 0;

	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * 25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}

void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Type = 0;
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime &&
	   ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
		(!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}

// Not for now.
void CGameContext::CheckPureTuning()
{
	return;

	/*
	// might not be created yet during start up
	if(!m_pController)服务器
		return;

	bool Pure = false;
	switch(m_pController->m_pGameType[0])
	{
	case 'B': Pure = str_comp(m_pController->m_pGameType, "BALL") == 0; break;
	case 'C': Pure = str_comp(m_pController->m_pGameType, "CTF") == 0; break;
	case 'D': Pure = str_comp(m_pController->m_pGameType, "DM") == 0 || str_comp(m_pController->m_pGameType, "DEF") ==
	0; break; case 'G': Pure = str_comp(m_pController->m_pGameType, "GUN") == 0; break; case 'I': Pure =
	str_comp(m_pController->m_pGameType, "INF") == 0 || str_comp(m_pController->m_pGameType, "INV") == 0; break; case
	'T': Pure = str_comp(m_pController->m_pGameType, "TDM") == 0 || str_comp(m_pController->m_pGameType, "TUT") == 0;
	break; default: break;
	}

	if(Pure)
	{
		CTuningParams p;
		if(mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}*/
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::UpdateSpectators()
{
	bool Found[2] = {false, false};

	// check validity
	for(int i = 0; i < 2; i++)
	{
		if(m_aMostInterestingPlayer[i] >= 0)
		{
			// player left or something
			if(!m_apPlayers[m_aMostInterestingPlayer[i]])
			{
				m_aMostInterestingPlayer[i] = -1;
			}
			else
			{
				// player is a spectator
				if(m_apPlayers[m_aMostInterestingPlayer[i]]->Spectating())
					m_aMostInterestingPlayer[i] = -1;
			}
		}
	}

	// find the most interesting player of both teams
	for(int i = 0; i < MAX_CLIENTS; i++)
	{

		// player and character exists
		if(m_apPlayers[i] && m_apPlayers[i]->m_EnableAutoSpectating && m_apPlayers[i]->GetCharacter() &&
		   m_apPlayers[i]->GetCharacter()->IsAlive() && (!m_apPlayers[i] || !g_Config.m_SvSpectateOnlyHumans))
		{
			int Team = m_apPlayers[i]->GetTeam();

			// team is correct
			if(Team == TEAM_RED || Team == TEAM_BLUE)
			{
				// most interesting player exists
				int Points = -1;
				int Player = m_aMostInterestingPlayer[Team];

				m_apPlayers[i]->m_InterestPoints += frandom();

				if(Player >= 0)
					if(m_apPlayers[Player] && m_apPlayers[Player]->GetCharacter())
						Points = m_apPlayers[Player]->m_InterestPoints;

				if(m_apPlayers[i]->m_InterestPoints > Points)
				{
					m_aMostInterestingPlayer[Team] = i;
					Found[Team] = true;
				}
			}
		}
	}

	// update the spectator views
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// if(m_apPlayers[i] && (m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || !m_apPlayers[i]->GetCharacter()) &&
		// !m_apPlayers[i]->m_IsBot)
		if(m_apPlayers[i] && !m_apPlayers[i]->m_IsBot && m_apPlayers[i]->m_ActionSpectator &&
		   m_apPlayers[i]->Spectating())
		{
			if(!m_apPlayers[i]->m_LastSetSpectatorMode)
				m_apPlayers[i]->m_LastSetSpectatorMode =
					Server()->Tick() - Server()->TickSpeed() * g_Config.m_SvSpectatorUpdateTime;
			else
			{
				if(m_apPlayers[i]->m_LastSetSpectatorMode + Server()->TickSpeed() * g_Config.m_SvSpectatorUpdateTime <
				   Server()->Tick())
				{
					int WantedPlayer = -1;

					/*
					if (!m_pController->IsTeamplay())
					{
						if (m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
						{



						}
					}
					else*/
					{
						int Team = m_apPlayers[i]->GetTeam();

						// get the correct player
						if(Team == TEAM_RED || Team == TEAM_BLUE)
						{
							WantedPlayer = m_aMostInterestingPlayer[Team];

							// update the view
							if(WantedPlayer >= 0 && m_apPlayers[i]->m_SpectatorID != WantedPlayer && Found[Team])
							{
								m_apPlayers[i]->m_LastSetSpectatorMode = Server()->Tick();
								m_apPlayers[i]->m_SpectatorID = WantedPlayer;
								Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "cstt", "Spectator id changed");
							}
						}
					}
				}
			}
		}
	}
}

void CGameContext::SwapTeams()
{
	if(!m_pController->IsTeamplay() || m_pController->IsInfection())
		return;

	SendChatTarget(-1, "Teams were swapped");

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_apPlayers[i]->SetTeam(m_apPlayers[i]->GetTeam() ^ 1, false);
	}

	(void)m_pController->CheckTeamBalance();
}

void CGameContext::SendMusicThreat(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return;

	CCharacter *pChar = GetPlayerChar(ClientID);
	if(!pChar)
	{
		// player is dead - send zero threat
		CNetMsg_Sv_MusicThreat Msg;
		Msg.m_Threat = 0;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		return;
	}

	vec2 PlayerPos = pChar->m_Pos;

	// find all droids near the player
	CDroid *apDroids[256];
	int Num = m_World.FindEntities(PlayerPos, 3500.0f, (CEntity **)apDroids, 256, CGameWorld::ENTTYPE_DROID);

	float TotalThreat = 0.0f;
	for(int i = 0; i < Num; i++)
	{
		CDroid *pDroid = apDroids[i];
		if(!pDroid || pDroid->m_Health <= 0)
			continue;

		float ThreatPower = DroidSoundThreat(pDroid->m_Type);
		float Dist = distance(PlayerPos, pDroid->m_Pos);
		if(Dist < 100.0f)
			Dist = 100.0f;

		float HealthRatio = (float)pDroid->m_Health / (float)pDroid->m_MaxHealth;
		TotalThreat += ThreatPower * HealthRatio / Dist;
	}

	// find nearby AI-controlled enemy characters
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_apPlayers[i] || !m_apPlayers[i]->m_pAI)
			continue;
		if(!m_pController->ArePlayersEnemies(ClientID, i))
			continue;

		CCharacter *pEnemy = GetPlayerChar(i);
		if(!pEnemy || pEnemy->m_HiddenHealth <= 0)
			continue;

		float Dist = distance(PlayerPos, pEnemy->m_Pos);
		if(Dist > 3500.0f)
			continue;
		if(Dist < 100.0f)
			Dist = 100.0f;

		float ThreatPower = m_apPlayers[i]->m_pAI->PowerLevel() / 5.0f;
		float HealthRatio = (float)pEnemy->m_HiddenHealth / (float)pEnemy->m_MaxHealth;
		TotalThreat += ThreatPower * HealthRatio / Dist;
	}

	// P.S.: feel free to change 1000.0f to something better
	int ThreatValue = clamp((int)(TotalThreat * 1000.0f), 0, 255);

	CNetMsg_Sv_MusicThreat Msg;
	Msg.m_Threat = ThreatValue;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.m_Core.ClearImpacts();
	m_World.m_Core.ClearDroids();
	if(m_pPveDirector)
		m_pPveDirector->Tick();
	m_World.Tick();

	// if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}
	TickNpcs();
	DispatchChallengeEvent(EChallengeScriptEvent::Tick);

	// dynamic music threat - send to human players every 30 ticks
	{
		static int s_MusicThreatTick = 0;
		if(++s_MusicThreatTick >= 30)
		{
			s_MusicThreatTick = 0;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_apPlayers[i] && IsHuman(i))
					SendMusicThreat(i);
			}
		}
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
		{
			SendChatTarget(-1, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i] && !IsBot(i))
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || IsBot(i) || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||
					   aVoteChecked[i]) // don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i + 1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || IsBot(i) || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(Yes >= Total / 2 + 1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= (Total + 1) / 2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChatTarget(-1, "Vote passed");

				if(m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChatTarget(-1, "Vote failed");
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i & 1) ? -1 : 1;
			m_apPlayers[MAX_CLIENTS - i - 1]->OnPredictedInput(&Input);
		}
	}
#endif
}

bool CGameContext::AIInputUpdateNeeded(int ClientID)
{
	if(m_apPlayers[ClientID])
		return m_apPlayers[ClientID]->AIInputChanged();

	return false;
}

void CGameContext::UpdateAI()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && IsBot(i))
			m_apPlayers[i]->AITick();
	}
}

/*
enum InputList
{
	INPUT_MOVE = 0,
	INPUT_SHOOT = 4,
	INPUT_JUMP = 3,
	INPUT_HOOK = 5
	INPUT_DOWN = 6

	//1 & 2 vectors for weapon direction
};
*/

void CGameContext::AIUpdateInput(int ClientID, int *Data)
{
	if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_pAI)
		m_apPlayers[ClientID]->m_pAI->UpdateInput(Data);
}

// Server hooks
void CGameContext::AddZombie()
{
	Server()->AddZombie();
}

void CGameContext::GetAISkin(CAISkin *pAISkin, bool PVP, int Level, int WaveGroup)
{
	Server()->GetAISkin(pAISkin, PVP, Level, WaveGroup);
}

void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(m_pTutorialDirector)
		m_pTutorialDirector->OnInput(ClientID, (CNetObj_PlayerInput *)pInput);
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

static void ExpeditionCopyToPlayerData(CPlayerData *pData, const CExpeditionPlayer &Src);

void CGameContext::OnClientEnter(int ClientID)
{
	// world.insert_entity(&players[client_id]);
	if(m_ExpeditionReady && m_apPlayers[ClientID] && !m_apPlayers[ClientID]->m_IsBot)
	{
		const int ColorID = m_apPlayers[ClientID]->GetColorID();
		const int Index = m_ExpeditionSave.FindPlayer(Server()->ClientName(ClientID), ColorID);
		if(Index >= 0)
		{
			CPlayerData *pData = Server()->GetPlayerData(ClientID, ColorID);
			if(pData)
				ExpeditionCopyToPlayerData(pData, m_ExpeditionSave.m_aPlayers[Index]);
		}
	}
	m_apPlayers[ClientID]->Respawn();
	SendChallengeInfo(ClientID);
	if(m_pPveDirector)
		m_pPveDirector->OnClientEnter(ClientID);
	if(m_pTutorialDirector)
		m_pTutorialDirector->OnClientEnter(ClientID);
	SendChatTarget(-1, "'%s' joined the fun", Server()->ClientName(ClientID));

	char aBuf[512];
	str_format(aBuf,
			   sizeof(aBuf),
			   "team_join player='%d:%s' team=%d",
			   ClientID,
			   Server()->ClientName(ClientID),
			   m_apPlayers[ClientID]->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if(str_comp(g_Config.m_SvGametype, "coop") == 0 && g_Config.m_SvMapGen)
	{
		if(!g_Config.m_SvInvFails)
			SendBroadcastFormat(ClientID, false, "Level %d", g_Config.m_SvMapGenLevel);
		else if(g_Config.m_SvInvFails == 1)
			SendBroadcastFormat(ClientID, false, "Level %d - Second try", g_Config.m_SvMapGenLevel);
		else if(g_Config.m_SvInvFails == 6)
			SendBroadcastFormat(ClientID, false, "Level %d - Final attempt", g_Config.m_SvMapGenLevel);
		else
			SendBroadcastFormat(
				ClientID, false, "Level %d - Attempt %d", g_Config.m_SvMapGenLevel, g_Config.m_SvInvFails + 1);
	}

	m_VoteUpdate = true;

	if(m_pController->GameVoting())
	{
		SendGameVotes(ClientID);
		SendGameVoteStats(ClientID);
	}
}

void CGameContext::OnClientConnected(int ClientID, bool AI)
{
	if(!AI && str_comp(g_Config.m_SvGametype, "tutorial") == 0 && m_pController->CountHumans() >= 1)
	{
		Server()->Kick(ClientID, "Tutorial sessions are single-player");
		return;
	}
	// Check which team the player should be on
	int StartTeam = g_Config.m_SvTournamentMode ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	if(m_pController->IsTeamplay() && !m_pController->IsCoop() &&
	   (g_Config.m_SvNoBotTeam == TEAM_RED || g_Config.m_SvNoBotTeam == TEAM_BLUE))
	{
		if(AI && StartTeam == g_Config.m_SvNoBotTeam)
		{
			if(StartTeam == TEAM_RED)
				StartTeam = TEAM_BLUE;
			else if(StartTeam == TEAM_BLUE)
				StartTeam = TEAM_RED;
		}
		else if(!AI && StartTeam != g_Config.m_SvNoBotTeam)
		{
			if(StartTeam == TEAM_RED)
				StartTeam = TEAM_BLUE;
			else if(StartTeam == TEAM_BLUE)
				StartTeam = TEAM_RED;
		}
	}

	if(!AI)
	{
		m_pController->OnPlayerJoin();
	}

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, StartTeam);
	// players[client_id].init(client_id);
	// players[client_id].client_id = client_id;

	m_apPlayers[ClientID]->m_IsBot = AI;
	m_apPlayers[ClientID]->m_TeeInfos.m_IsBot = AI;
	if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		static_cast<CGameControllerRoam *>(m_pController)->ResetRace(ClientID);

	(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
			return;
	}
#endif

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	/* skip motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	*/
	/*
	if (!AI)
	{
		if (m_pController->GameVoting())
			SendGameVotes(ClientID);
	}
	*/
}

bool CGameContext::Shop(CPlayer *pPlayer, int Slot, bool AI)
{
	if(!pPlayer->GetCharacter())
		return false;
	if(m_pPveDirector && !m_pPveDirector->ShopsAllowed())
	{
		CreateSoundGlobal(SOUND_GUI_DENIED1, pPlayer->GetCID());
		return false;
	}

	vec2 Pos = pPlayer->GetCharacter()->m_Pos;

	CBuilding *apEnts[32];
	int Num = m_World.FindEntities(Pos, 400, (CEntity **)apEnts, 32, CGameWorld::ENTTYPE_BUILDING);

	for(int i = 0; i < Num; ++i)
	{
		CBuilding *pTarget = apEnts[i];

		if(pTarget->m_Type == BUILDING_SHOP && abs(Pos.x - pTarget->m_Pos.x) < 100 &&
		   abs(Pos.y - pTarget->m_Pos.y) < 100)
		{
			if(Slot == 4 && (!m_pPveDirector || !m_pPveDirector->PerkStacks(pPlayer->GetCID(), PVE_CARD_PREMIUM_STOCK)))
				continue;
			CWeaponSpec Item = pTarget->GetItem(Slot);

			if(Item.IsValid())
			{
				CResolvedWeaponProfile Profile;
				if(!CWeaponCatalog::TryResolve(Item, &Profile))
					continue;
				int Cost = Profile.m_Combat.m_Cost;
				if(m_pPveDirector)
					Cost = m_pPveDirector->ModifyShopCost(pPlayer->GetCID(), Cost);
				if((!AI || !WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_UPGRADE)) &&
				   pPlayer->GetGold() >= Cost)
				{
					if(pPlayer->GetCharacter()->GiveWeapon(NewWeapon(Item)))
					{
						pPlayer->ReduceGold(Cost);
						if(m_pPveDirector)
							m_pPveDirector->OnGoldSpent(pPlayer->GetCID(), Cost);
						pPlayer->GetCharacter()->SendInventory();
						pTarget->ClearItem(Slot);

						CreateSound(Pos, SOUND_PICKUP_SHOTGUN);
						return true;
					}
					else
						CreateSoundGlobal(SOUND_GUI_DENIED1, pPlayer->GetCID());
				}
				else
					CreateSoundGlobal(SOUND_GUI_DENIED1, pPlayer->GetCID());
			}
		}
	}

	return false;
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return;
	if(m_pPveDirector)
		m_pPveDirector->OnClientDrop(ClientID);
	if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		static_cast<CGameControllerRoam *>(m_pController)->ResetRace(ClientID);
	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pRawMsg)
	{
		if(g_Config.m_Debug)
		{
			char aBuf[256];
			str_format(aBuf,
					   sizeof(aBuf),
					   "dropped weird message '%s' (%d), failed on '%s'",
					   m_NetObjHandler.GetMsgName(MsgID),
					   MsgID,
					   m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_PVEPROGRESS)
		{
			CNetMsg_Cl_PveProgress *pMsg = (CNetMsg_Cl_PveProgress *)pRawMsg;
			if(m_pPveDirector)
				m_pPveDirector->OnProgress(ClientID,
										   pMsg->m_Version,
										   pMsg->m_ResearchPoints,
										   pMsg->m_ResearchMask0,
										   pMsg->m_ResearchMask1,
							   pMsg->m_ResearchMask2,
							   pMsg->m_ResearchMask3,
							   pMsg->m_HighestInvasion,
							   pMsg->m_PreferredCheckpoint);
		}
		else if(MsgID == NETMSGTYPE_CL_PVECHOICE)
		{
			CNetMsg_Cl_PveChoice *pMsg = (CNetMsg_Cl_PveChoice *)pRawMsg;
			if(m_pPveDirector)
				m_pPveDirector->OnChoice(ClientID, pMsg->m_Nonce, pMsg->m_Card);
		}
		else if(MsgID == NETMSGTYPE_CL_PVECONTRACTVOTE)
		{
			CNetMsg_Cl_PveContractVote *pMsg = (CNetMsg_Cl_PveContractVote *)pRawMsg;
			if(m_pPveDirector)
				m_pPveDirector->OnContractVote(ClientID, pMsg->m_Nonce, pMsg->m_Contract);
		}
		else if(MsgID == NETMSGTYPE_CL_PVERESEARCHBUY)
		{
			CNetMsg_Cl_PveResearchBuy *pMsg = (CNetMsg_Cl_PveResearchBuy *)pRawMsg;
			if(m_pPveDirector)
				m_pPveDirector->OnResearchBuy(ClientID, pMsg->m_Nonce, pMsg->m_Card);
		}
		else if(MsgID == NETMSGTYPE_CL_PVEDRONEMODULE)
		{
			CNetMsg_Cl_PveDroneModule *pMsg = (CNetMsg_Cl_PveDroneModule *)pRawMsg;
			if(m_pPveDirector)
				m_pPveDirector->OnDroneModule(ClientID, pMsg->m_Nonce, pMsg->m_Module);
		}
		else if(MsgID == NETMSGTYPE_CL_PVEINVASIONRETRYVOTE)
		{
			CNetMsg_Cl_PveInvasionRetryVote *pMsg = (CNetMsg_Cl_PveInvasionRetryVote *)pRawMsg;
			CGameControllerInvasion *pInvasion = dynamic_cast<CGameControllerInvasion *>(m_pController);
			if(pInvasion)
				pInvasion->OnRetryVote(ClientID, pMsg->m_Nonce, pMsg->m_Choice);
		}
		else if(MsgID == NETMSGTYPE_CL_PVEINVASIONFIELDORDER)
		{
			CNetMsg_Cl_PveInvasionFieldOrder *pMsg = (CNetMsg_Cl_PveInvasionFieldOrder *)pRawMsg;
			CGameControllerInvasion *pInvasion = dynamic_cast<CGameControllerInvasion *>(m_pController);
			if(pInvasion)
				pInvasion->OnFieldOrderVote(ClientID, pMsg->m_Nonce, pMsg->m_Package);
		}
		else if(MsgID == NETMSGTYPE_CL_TUTORIALACTION)
		{
			CNetMsg_Cl_TutorialAction *pMsg = (CNetMsg_Cl_TutorialAction *)pRawMsg;
			if(m_pTutorialDirector)
				m_pTutorialDirector->OnAction(ClientID, pMsg->m_Action, pMsg->m_Nonce, pMsg->m_Value);
		}
		else if(MsgID == NETMSGTYPE_CL_SAY)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChat &&
			   pPlayer->m_LastChat + Server()->TickSpeed() > Server()->Tick())
				return;

			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			int Mode = pMsg->m_Mode;
			int TargetID = pMsg->m_Target;

			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) &&
				   (Code < 0x2028 || Code > 0x202F) && (Code < 0x205F || Code > 0x2064) &&
				   (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) && Code != 0xFEFF &&
				   (Code < 0xFFF9 || Code > 0xFFFC))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 127)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 16 characters per second)
			if(Length == 0 || (g_Config.m_SvSpamprotection && pPlayer->m_LastChat &&
							   pPlayer->m_LastChat + Server()->TickSpeed() * ((15 + Length) / 16) > Server()->Tick()))
				return;

			bool SkipSending = false;

			pPlayer->m_LastChat = Server()->Tick();

			if(strcmp(pMsg->m_pMessage, "/color") == 0)
			{
				SendChatTarget(ClientID,
							   "Body: %d, feet: %d, skin: %d, topper: %d",
							   pPlayer->m_TeeInfos.m_ColorBody,
							   pPlayer->m_TeeInfos.m_ColorFeet,
							   pPlayer->m_TeeInfos.m_ColorSkin,
							   pPlayer->m_TeeInfos.m_ColorTopper);
				SkipSending = true;
			}

			if(strcmp(pMsg->m_pMessage, "/seed") == 0)
			{
				SendChatTarget(ClientID, "Mapgen seed: %d", g_Config.m_SvMapGenSeed);
				SkipSending = true;
			}

			if(strcmp(pMsg->m_pMessage, "/highest") == 0)
			{
				SendChatTarget(ClientID, "Highest level reached on server: %d", Server()->GetHighScore());
				SkipSending = true;
			}

			if(strcmp(pMsg->m_pMessage, "/playercount") == 0)
			{
				SendChatTarget(ClientID, "Number of player profiles in Invasion: %d", Server()->GetPlayerCount());
				SkipSending = true;
			}

			if(strcmp(pMsg->m_pMessage, "/droid") == 0)
			{
				if(pPlayer->GetDroid())
				{
					pPlayer->ToggleDroidControl();
					SendChatTarget(ClientID, "droid control released");
				}
				else if(!pPlayer->GetBody())
					SendChatTarget(ClientID, "need a living character");
				else
				{
					pPlayer->ToggleDroidControl();
					if(pPlayer->GetDroid())
						SendChatTarget(ClientID, "controlling droid");
				}
				SkipSending = true;
			}

			if(!SkipSending)
			{
				if(Mode == CHATMODE_WHISPER)
				{
					if(TargetID < 0 || TargetID >= MAX_CLIENTS || !m_apPlayers[TargetID])
						return;
					SendChat(ClientID, CHATMODE_WHISPER, pMsg->m_pMessage, TargetID);
				}
				else if(Mode == CHATMODE_TEAM)
				{
					SendChat(ClientID, CHATMODE_TEAM, pMsg->m_pMessage);
				}
				else
				{
					SendChat(ClientID, CHATMODE_ALL, pMsg->m_pMessage);
				}
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry &&
			   pPlayer->m_LastVoteTry + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			int64 Now = Server()->Tick();
			pPlayer->m_LastVoteTry = Now;
			if(pPlayer->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
				return;
			}

			if(m_VoteCloseTime)
			{
				SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
				return;
			}

			int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
			if(pPlayer->m_LastVoteCall && Timeleft > 0)
			{
				char aChatmsg[512] = {0};
				str_format(aChatmsg,
						   sizeof(aChatmsg),
						   Localize("You must wait %d seconds before making another vote", ClientID),
						   (Timeleft / Server()->TickSpeed()) + 1);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}

			char aChatmsg[512] = {0};
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			const char *pReason = pMsg->m_Reason[0] ? pMsg->m_Reason : "No reason given";

			if(str_comp_nocase(pMsg->m_Type, "option") == 0)
			{
				CVoteOptionServer *pOption = m_pVoteOptionFirst;

				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
					{
						str_format(aChatmsg,
								   sizeof(aChatmsg),
								   "'%s' called vote to change server option '%s' (%s)",
								   Server()->ClientName(ClientID),
								   pOption->m_aDescription,
								   pReason);
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
				{
					SendChatTarget(ClientID, "'%s' isn't an option on this server", pMsg->m_Value);
					return;
				}
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				if(!g_Config.m_SvVoteKick)
				{
					SendChatTarget(ClientID, "Server does not allow voting to kick players");
					return;
				}

				if(g_Config.m_SvVoteKickMin)
				{
					int PlayerNum = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
						if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
							++PlayerNum;

					if(PlayerNum < g_Config.m_SvVoteKickMin)
					{
						SendChatTarget(
							ClientID, "Kick voting requires %d players on the server", g_Config.m_SvVoteKickMin);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_Value);
				if(KickID < 0 || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if(KickID == ClientID)
				{
					SendChatTarget(ClientID, "You can't kick yourself");
					return;
				}
				if(Server()->IsAuthed(KickID))
				{
					SendChatTarget(ClientID, "You can't kick admins");
					char aBufKick[128];
					str_format(aBufKick,
							   sizeof(aBufKick),
							   Localize("'%s' called for vote to kick you", ClientID),
							   Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				str_format(aChatmsg,
						   sizeof(aChatmsg),
						   "'%s' called for vote to kick '%s' (%s)",
						   Server()->ClientName(ClientID),
						   Server()->ClientName(KickID),
						   pReason);
				str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
				if(!g_Config.m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
				}
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!g_Config.m_SvVoteSpectate)
				{
					SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
					return;
				}

				int SpectateID = str_toint(pMsg->m_Value);
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] ||
				   m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget(ClientID, "Invalid client id to move");
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatTarget(ClientID, "You can't move yourself");
					return;
				}

				str_format(aChatmsg,
						   sizeof(aChatmsg),
						   "'%s' called for vote to move '%s' to spectators (%s)",
						   Server()->ClientName(ClientID),
						   Server()->ClientName(SpectateID),
						   pReason);
				str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
			}

			// do nothing
			if(str_comp(aCmd, "null") == 0)
			{
				return;
			}

			if(aCmd[0])
			{
				SendChat(-1, CHATMODE_ALL, aChatmsg);
				StartVote(aDesc, aCmd, pReason);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				m_VoteCreator = ClientID;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(!m_VoteCloseTime)
				return;

			if(pPlayer->m_Vote == 0)
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(!pMsg->m_Vote)
					return;

				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if(pPlayer->GetTeam() == pMsg->m_Team ||
			   (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam &&
				pPlayer->m_LastSetTeam + Server()->TickSpeed() * 1 > Server()->Tick()))
				return;

			pPlayer->m_LastSetTeam = Server()->Tick();
			if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
				m_VoteUpdate = true;

			pPlayer->SetTeam(pMsg->m_Team);
			pPlayer->m_TeamChangeTick = Server()->Tick();

			/*
			if(pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams)
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				SendBroadcast("Teams are locked", ClientID);
				return;
			}
			*/

			/*
			if(pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick())/Server()->TickSpeed();
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft/60,
			TimeLeft%60); SendBroadcast(aBuf, ClientID); return;
			}


			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if(m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
				{
					pPlayer->m_LastSetTeam = Server()->Tick();
					if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					pPlayer->SetTeam(pMsg->m_Team);
					//(void)m_pController->CheckTeamBalance();
					pPlayer->m_TeamChangeTick = Server()->Tick();
				}
				//else
				//	SendBroadcast("Teams must be balanced, please join other team", ClientID);
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed",
			Server()->MaxClients()-g_Config.m_SvSpectatorSlots); SendBroadcast(aBuf, ClientID);
			}
			*/
		}
		else if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			pPlayer->m_ActionSpectator = false;

			if((pPlayer->GetTeam() != TEAM_SPECTATORS && !g_Config.m_SvSurvivalMode) ||
			   pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID ||
			   (g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode &&
				pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() * 1 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			if(pMsg->m_SpectatorID != SPEC_FREEVIEW &&
			   (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
				SendChatTarget(ClientID, "Invalid spectator id used");
			else
				pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
		}
		else if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo &&
			   pPlayer->m_LastChangeInfo + Server()->TickSpeed() * 5 > Server()->Tick())
				return;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set infos
			char aOldName[MAX_NAME_LENGTH];
			str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
			Server()->SetClientName(ClientID, pMsg->m_pName);
			if(str_comp(aOldName, Server()->ClientName(ClientID)) != 0)
				SendChatTarget(-1, "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));

			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_TopperName, pMsg->m_pTopper, sizeof(pPlayer->m_TeeInfos.m_TopperName));
			str_copy(pPlayer->m_TeeInfos.m_EyeName, pMsg->m_pEye, sizeof(pPlayer->m_TeeInfos.m_EyeName));
			str_copy(pPlayer->m_TeeInfos.m_HeadName, pMsg->m_pHead, sizeof(pPlayer->m_TeeInfos.m_HeadName));
			str_copy(pPlayer->m_TeeInfos.m_BodyName, pMsg->m_pBody, sizeof(pPlayer->m_TeeInfos.m_BodyName));
			str_copy(pPlayer->m_TeeInfos.m_HandName, pMsg->m_pHand, sizeof(pPlayer->m_TeeInfos.m_HandName));
			str_copy(pPlayer->m_TeeInfos.m_FootName, pMsg->m_pFoot, sizeof(pPlayer->m_TeeInfos.m_FootName));
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->m_TeeInfos.m_ColorTopper = pMsg->m_ColorTopper;
			pPlayer->m_TeeInfos.m_BloodColor = pMsg->m_BloodColor;
			pPlayer->m_TeeInfos.m_ColorSkin = pMsg->m_ColorSkin;
			str_copy(
				pPlayer->m_aLanguage, Localization()->GetLanguageCode(pMsg->m_Language), sizeof(pPlayer->m_aLanguage));

			m_pController->OnPlayerInfoChange(pPlayer);
		}
		else if(MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastEmote &&
			   pPlayer->m_LastEmote + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();

			if((pMsg->m_Emoticon == EMOTICON_EYES || pMsg->m_Emoticon == EMOTICON_HEARTS) && pPlayer->GetCharacter())
				pPlayer->GetCharacter()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

			if((pMsg->m_Emoticon == EMOTICON_SPLATTEE || pMsg->m_Emoticon == EMOTICON_DEVILTEE ||
				pMsg->m_Emoticon == EMOTICON_ZOMG) &&
			   pPlayer->GetCharacter())
				pPlayer->GetCharacter()->SetEmote(EMOTE_ANGRY, Server()->Tick() + Server()->TickSpeed());

			SendEmoticon(ClientID, pMsg->m_Emoticon);
		}
		else if(MsgID == NETMSGTYPE_CL_DROPWEAPON && !m_World.m_Paused)
		{
			pPlayer->DropWeapon();
		}
		else if(MsgID == NETMSGTYPE_CL_SELECTITEM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SelectItem *pMsg = (CNetMsg_Cl_SelectItem *)pRawMsg;
			pPlayer->SelectItem(pMsg->m_Item);
		}
		else if(MsgID == NETMSGTYPE_CL_INVENTORYACTION)
		{
			CNetMsg_Cl_InventoryAction *pMsg = (CNetMsg_Cl_InventoryAction *)pRawMsg;
			switch(pMsg->m_Type)
			{
				case INVENTORYACTION_ROLL:
					pPlayer->InventoryRoll(pMsg->m_Slot);
					break;
				case INVENTORYACTION_DROP:
					pPlayer->DropItem(pMsg->m_Slot, vec2(pMsg->m_Item1, pMsg->m_Item2));
					break;
				case INVENTORYACTION_SWAP:
					pPlayer->SwapItem(pMsg->m_Item1, pMsg->m_Item2);
					break;
				case INVENTORYACTION_COMBINE:
					pPlayer->CombineItem(pMsg->m_Item1, pMsg->m_Item2, pMsg->m_Slot);
					break;
				case INVENTORYACTION_TAKEPART:
					break;
				case INVENTORYACTION_SHOP:
					Shop(pPlayer, pMsg->m_Slot);
					break;
				default:
					return;
			};
		}
		else if(MsgID == NETMSGTYPE_CL_USEKIT && !m_World.m_Paused)
		{
			CNetMsg_Cl_UseKit *pMsg = (CNetMsg_Cl_UseKit *)pRawMsg;
			pPlayer->UseKit(pMsg->m_Kit, vec2(pMsg->m_X, pMsg->m_Y));
		}
		else if(MsgID == NETMSGTYPE_CL_VOTEGAMEMODE)
		{
			CNetMsg_Cl_VoteGameMode *pMsg = (CNetMsg_Cl_VoteGameMode *)pRawMsg;
			RegisterGameVote(ClientID, pMsg->m_Vote);
		}
		else if(MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if(pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * 1 > Server()->Tick())
				return;

			pPlayer->m_LastKill = Server()->Tick();
			pPlayer->KillCharacter(CAttackSource::World(WEAPON_SELF, ClientID));
		}
	}
	else
	{
		// bots skip sending this info
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			// limit players to 4 in invasion
			if(m_pController->IsCoop() && m_pController->CountHumans() > 16)
				Server()->Kick(ClientID, "Server full - max 16 players in co-op modes");

			if(pPlayer->m_IsReady)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			str_copy(pPlayer->m_TeeInfos.m_TopperName, pMsg->m_pTopper, sizeof(pPlayer->m_TeeInfos.m_TopperName));
			str_copy(pPlayer->m_TeeInfos.m_EyeName, pMsg->m_pEye, sizeof(pPlayer->m_TeeInfos.m_EyeName));
			str_copy(pPlayer->m_TeeInfos.m_HeadName, pMsg->m_pHead, sizeof(pPlayer->m_TeeInfos.m_HeadName));
			str_copy(pPlayer->m_TeeInfos.m_BodyName, pMsg->m_pBody, sizeof(pPlayer->m_TeeInfos.m_BodyName));
			str_copy(pPlayer->m_TeeInfos.m_HandName, pMsg->m_pHand, sizeof(pPlayer->m_TeeInfos.m_HandName));
			str_copy(pPlayer->m_TeeInfos.m_FootName, pMsg->m_pFoot, sizeof(pPlayer->m_TeeInfos.m_FootName));
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->m_TeeInfos.m_ColorTopper = pMsg->m_ColorTopper;
			pPlayer->m_TeeInfos.m_ColorSkin = pMsg->m_ColorSkin;
			pPlayer->m_TeeInfos.m_BloodColor = pMsg->m_BloodColor;
			str_copy(
				pPlayer->m_aLanguage, Localization()->GetLanguageCode(pMsg->m_Language), sizeof(pPlayer->m_aLanguage));

			m_pController->OnPlayerInfoChange(pPlayer);

			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			CNetMsg_Sv_VoteOptionListAdd OptionMsg;
			int NumOptions = 0;
			OptionMsg.m_pDescription0 = "";
			OptionMsg.m_pDescription1 = "";
			OptionMsg.m_pDescription2 = "";
			OptionMsg.m_pDescription3 = "";
			OptionMsg.m_pDescription4 = "";
			OptionMsg.m_pDescription5 = "";
			OptionMsg.m_pDescription6 = "";
			OptionMsg.m_pDescription7 = "";
			OptionMsg.m_pDescription8 = "";
			OptionMsg.m_pDescription9 = "";
			OptionMsg.m_pDescription10 = "";
			OptionMsg.m_pDescription11 = "";
			OptionMsg.m_pDescription12 = "";
			OptionMsg.m_pDescription13 = "";
			OptionMsg.m_pDescription14 = "";
			CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
			while(pCurrent)
			{
				switch(NumOptions++)
				{
					case 0:
						OptionMsg.m_pDescription0 = pCurrent->m_aDescription;
						break;
					case 1:
						OptionMsg.m_pDescription1 = pCurrent->m_aDescription;
						break;
					case 2:
						OptionMsg.m_pDescription2 = pCurrent->m_aDescription;
						break;
					case 3:
						OptionMsg.m_pDescription3 = pCurrent->m_aDescription;
						break;
					case 4:
						OptionMsg.m_pDescription4 = pCurrent->m_aDescription;
						break;
					case 5:
						OptionMsg.m_pDescription5 = pCurrent->m_aDescription;
						break;
					case 6:
						OptionMsg.m_pDescription6 = pCurrent->m_aDescription;
						break;
					case 7:
						OptionMsg.m_pDescription7 = pCurrent->m_aDescription;
						break;
					case 8:
						OptionMsg.m_pDescription8 = pCurrent->m_aDescription;
						break;
					case 9:
						OptionMsg.m_pDescription9 = pCurrent->m_aDescription;
						break;
					case 10:
						OptionMsg.m_pDescription10 = pCurrent->m_aDescription;
						break;
					case 11:
						OptionMsg.m_pDescription11 = pCurrent->m_aDescription;
						break;
					case 12:
						OptionMsg.m_pDescription12 = pCurrent->m_aDescription;
						break;
					case 13:
						OptionMsg.m_pDescription13 = pCurrent->m_aDescription;
						break;
					case 14:
					{
						OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
						OptionMsg.m_NumOptions = NumOptions;
						Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
						OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
						NumOptions = 0;
						OptionMsg.m_pDescription1 = "";
						OptionMsg.m_pDescription2 = "";
						OptionMsg.m_pDescription3 = "";
						OptionMsg.m_pDescription4 = "";
						OptionMsg.m_pDescription5 = "";
						OptionMsg.m_pDescription6 = "";
						OptionMsg.m_pDescription7 = "";
						OptionMsg.m_pDescription8 = "";
						OptionMsg.m_pDescription9 = "";
						OptionMsg.m_pDescription10 = "";
						OptionMsg.m_pDescription11 = "";
						OptionMsg.m_pDescription12 = "";
						OptionMsg.m_pDescription13 = "";
						OptionMsg.m_pDescription14 = "";
					}
				}
				pCurrent = pCurrent->m_pNext;
			}
			if(NumOptions > 0)
			{
				OptionMsg.m_NumOptions = NumOptions;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
			}

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReady = true;
			CNetMsg_Sv_ReadyToEnter m;
			m.m_pWeaponContentHash = CWeaponCatalog::OfficialContentHash();
			Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
		}
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pSelf->m_pController->IsGameOver())
		return;

	if(!pSelf->m_pPveDirector || !pSelf->m_pPveDirector->TogglePauseAfterIntermission())
		pSelf->m_World.m_Paused ^= 1;
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CHATMODE_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick =
		pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	pSelf->SendChatTarget(-1, pSelf->m_pController->GetTeamMoveAllMessage(Team));

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pController->IsTeamplay() || pSelf->m_pController->IsInfection())
		return;

	int PlayerTeam = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			++PlayerTeam;
	PlayerTeam = (PlayerTeam + 1) / 2;

	pSelf->SendChatTarget(-1, "Teams were shuffled");

	/*
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			if(CounterRed == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
			else if(CounterBlue == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
			else
			{
				if(irandom(2))
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
					++CounterBlue;
				}
				else
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
					++CounterRed;
				}
			}
		}
	}
	*/

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	if(pSelf->m_LockTeams)
		pSelf->SendChatTarget(-1, "Teams were locked");
	else
		pSelf->SendChatTarget(-1, "Teams were unlocked");
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if(pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription && *pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// remove the option from the linked list (heap entry kept until reset)
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption->m_pNext;
	else
		pSelf->m_pVoteOptionFirst = pOption->m_pNext;
	if(pOption->m_pNext)
		pOption->m_pNext->m_pPrev = pOption->m_pPrev;
	else
		pSelf->m_pVoteOptionLast = pOption->m_pPrev;
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason =
		pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				pSelf->SendChatTarget(-1, "admin forced server option '%s' (%s)", pValue, pReason);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if(!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] ||
		   pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		pSelf->SendChatTarget(
			-1, "admin moved '%s' to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);

		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
}

void CGameContext::ReloadMap()
{
	Console()->ExecuteLine("reload");
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConEndRound(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "round ending");
	pSelf->m_pController->EndRound();
}

void CGameContext::ConWeaponList(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	char aLine[256];
	for(int Index = 0; Index < CWeaponCatalog::DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(!CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition))
			continue;
		str_format(aLine,
				   sizeof(aLine),
				   "id=%d stable=%s source=%s",
				   (int)Definition.m_Id,
				   Definition.m_aStableId,
				   Definition.m_Custom ? Definition.m_aPackageId : "official");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon", aLine);
	}
}

void CGameContext::ConWeaponValidate(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
							"weapon",
							CWeaponCatalog::Validate() ? "weapon registry is valid"
													   : "weapon registry validation failed");
}

void CGameContext::ConWeaponSpawn(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	const int ClientId = pResult->GetInteger(0);
	const int Level = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	CWeaponSpec Spec;
	CCharacter *pCharacter = ClientId >= 0 && ClientId < MAX_CLIENTS ? pSelf->GetPlayerChar(ClientId) : 0;
	if(!pCharacter || !CWeaponCatalog::TryFromStableId(pResult->GetString(1), Level, &Spec))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD, "weapon", "usage failed: weapon_spawn <client-id> <stable-id> [level]");
		return;
	}
	pSelf->m_pController->DropWeapon(pCharacter->m_Pos, vec2(0, -8), pSelf->NewWeapon(Spec));
}

void CGameContext::ConWeaponReload(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
		if(pSelf->m_apPlayers[ClientId])
		{
			pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD, "weapon", "weapon_reload requires an empty server");
			return;
		}
	char aRoot[1024], aError[256];
	pSelf->Storage()->GetCompletePath(IStorage::TYPE_SAVE, "workshop", aRoot, sizeof(aRoot));
	if(!WeaponPackagesLoadCollection(
		   aRoot, g_Config.m_SvModIds, pSelf->NetVersion(), g_Config.m_SvModHash, aError, sizeof(aError)))
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon", aError);
	else
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD, "weapon", "weapon definitions reloaded; restarting map");
		pSelf->Console()->ExecuteLine("reload");
	}
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if(!pSelf->m_VoteCloseTime)
		return;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;

	pSelf->SendChatTarget(-1, "admin forced vote %s", pResult->GetString(0));

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult,
											 void *pUserData,
											 IConsole::FCommandCallback pfnCallback,
											 void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		/*
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(pSelf->m_apPlayers[i] && !pSelf->IsBot(i))
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			*/
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register(
		"restart", "?i", CFGFLAG_SERVER | CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Register("end_round", "", CFGFLAG_SERVER, ConEndRound, this, "Ends the current round");
	Console()->Register("weapon_list", "", CFGFLAG_SERVER, ConWeaponList, this, "Lists registered Lua weapons");
	Console()->Register(
		"weapon_validate", "", CFGFLAG_SERVER, ConWeaponValidate, this, "Validates the Lua weapon registry");
	Console()->Register(
		"weapon_spawn", "is?i", CFGFLAG_SERVER, ConWeaponSpawn, this, "Spawns a weapon near a client by stable ID");
	Console()->Register(
		"weapon_reload", "", CFGFLAG_SERVER, ConWeaponReload, this, "Reloads data-only weapon Mods on an empty server");

	// Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGameContext::ActivateBlockEntities(int x)
{
	if(!m_pBlockEntities)
		return;

	CMapPath *pPath = m_Collision.GetMapPath();
	if(pPath)
	{
		int Center = 0;
		bool FoundPlayer = false;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = m_apPlayers[i];
			if(!pPlayer || !pPlayer->GetCharacter())
				continue;
			const int PX = (int)pPlayer->GetCharacter()->m_Pos.x / 32;
			const int PY = (int)pPlayer->GetCharacter()->m_Pos.y / 32;
			const CMapPathPlacementData *pPlayerPlacement = pPath->PlacementAtWorldTile(PX, PY);
			if(!pPlayerPlacement)
				continue;
			const int Course = pPlayerPlacement->m_CourseIndex;
			for(int Index = max(0, Course - 2); Index <= min(pPath->PlacementCount() - 1, Course + 2); Index++)
				m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, Index, true);
			if(!FoundPlayer || Course > Center)
				Center = Course;
			FoundPlayer = true;
		}
		if(!FoundPlayer)
		{
			const int TileX = x / 32;
			for(int SampleY = -pPath->Info().m_ChunkHeight * 4; SampleY <= pPath->Info().m_ChunkHeight * 4;
				SampleY += max(1, pPath->Info().m_ChunkHeight / 2))
			{
				const CMapPathPlacementData *pPlacement = pPath->PlacementAtWorldTile(TileX, SampleY);
				if(pPlacement)
				{
					Center = pPlacement->m_CourseIndex;
					break;
				}
			}
			for(int Index = max(0, Center - 2); Index <= min(pPath->PlacementCount() - 1, Center + 2); Index++)
				m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, Index, true);
		}
		m_Collision.GenerateWaypointsAround(Center);
		return;
	}

	m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, x / 32, false);
	m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, (x - 1000) / 32, true);
	m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, (x + 1000) / 32, true);

	const int ChunkSize = m_Collision.GetChunkSize();
	if(ChunkSize > 0)
	{
		int MinTileX = x / 32;
		int MaxTileX = MinTileX;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = m_apPlayers[i];
			if(!pPlayer || pPlayer->m_IsBot || !pPlayer->GetCharacter())
				continue;
			const int PlayerTileX = (int)pPlayer->GetCharacter()->m_Pos.x / 32;
			MinTileX = min(MinTileX, PlayerTileX);
			MaxTileX = max(MaxTileX, PlayerTileX);
		}
		const int Margin = ChunkSize * 3;
		m_Collision.GenerateWaypointsAround((MinTileX + MaxTileX) / 2);
		m_Collision.PruneMapChunks(MinTileX - Margin, MaxTileX + Margin);
		m_pBlockEntities = m_pBlockEntities->FreeOutside(this, MinTileX - Margin, MaxTileX + Margin);
	}
}

void CGameContext::CreateEntitiesForBlock(int block)
{
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);
	CMapPath *pPath = m_Collision.GetMapPath();

	if(pPath)
	{
		const CMapPathPlacementData *pPlacement = pPath->Placement(block);
		if(!pPlacement)
			return;
		const int ChunkW = pPath->Info().m_ChunkWidth;
		const int ChunkH = pPath->Info().m_ChunkHeight;
		const int AtlasCol = pPlacement->m_TemplateIndex % pPath->Info().m_AtlasColumns;
		const int AtlasRow = pPlacement->m_TemplateIndex / pPath->Info().m_AtlasColumns;
		const int AtlasOriginX = AtlasCol * ChunkW;
		const int AtlasOriginY = AtlasRow * ChunkH;
		const int WorldOriginX = pPlacement->m_GridX * ChunkW;
		const int WorldOriginY = pPlacement->m_GridY * ChunkH;
		if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		{
			CGameControllerRoam *pRoam = static_cast<CGameControllerRoam *>(m_pController);
			CBlockEntities *pNode = m_pBlockEntities ? m_pBlockEntities->GetBlockEntities(this, block, false) : 0;
			if(pPlacement->m_CourseIndex == 0 && pNode)
			{
				for(int i = 0; i < 4; i++)
					pNode->AddSpawnLocal(vec2(WorldOriginX + RoamMapGen::SpawnLocalX(i), WorldOriginY + RoamMapGen::SpawnLocalY()));
			}
			if(pPlacement->m_CourseIndex > 0)
			{
				int RespawnX, RespawnY;
				RoamMapGen::EntryRespawnLocal(pPlacement->m_EntryDir, &RespawnX, &RespawnY);
				const vec2 Respawn((WorldOriginX + RespawnX) * 32.0f + 16.0f, (WorldOriginY + RespawnY) * 32.0f + 16.0f);
				const RoamMapGen::CTileAabb B = RoamMapGen::RaceGateLocalAabb(pPlacement->m_EntryDir, false);
				const vec2 Min((WorldOriginX + B.m_MinX) * 32.0f, (WorldOriginY + B.m_MinY) * 32.0f);
				const vec2 Max((WorldOriginX + B.m_MaxX + 1) * 32.0f, (WorldOriginY + B.m_MaxY + 1) * 32.0f);
				pRoam->RegisterRaceGate(ENTITY_CHECKPOINT, Min, Max, pPlacement->m_CourseIndex, Respawn);
			}
			if(pPlacement->m_CourseIndex == pPath->PlacementCount() - 1)
			{
				const RoamMapGen::CTileAabb B = RoamMapGen::RaceGateLocalAabb(pPlacement->m_EntryDir, true);
				const vec2 Min((WorldOriginX + B.m_MinX) * 32.0f, (WorldOriginY + B.m_MinY) * 32.0f);
				const vec2 Max((WorldOriginX + B.m_MaxX + 1) * 32.0f, (WorldOriginY + B.m_MaxY + 1) * 32.0f);
				const vec2 Respawn((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
				pRoam->RegisterRaceGate(ENTITY_FINISH, Min, Max, pPlacement->m_CourseIndex, Respawn);
			}

			// Trap candidates live in template metadata, but activation is based on
			// course position so early placements remain damage-free even when a
			// visual template is reused later.
			const int ActiveHazards = RoamMapGen::ActiveHazardCount(
				pPlacement->m_CourseIndex, pPath->PlacementCount(), RoamMapGen::TemplateSpec(pPlacement->m_TemplateIndex).m_Finish ? 0 : 2);
			if(ActiveHazards > 0)
			{
				RoamMapGen::CTemplateGrid Grid;
				RoamMapGen::GenerateTemplate(RoamMapGen::TemplateSpec(pPlacement->m_TemplateIndex), &Grid);
				for(int Hazard = 0; Hazard < min(Grid.m_HazardCount, ActiveHazards); Hazard++)
				{
					const RoamMapGen::CHazardSpec &H = Grid.m_aHazards[Hazard];
					const vec2 Pos((WorldOriginX + H.m_X) * 32.0f + 16.0f, (WorldOriginY + H.m_Y) * 32.0f + 16.0f);
					if(H.m_Type == RoamMapGen::HAZARD_SAW || H.m_Type == RoamMapGen::HAZARD_FLAME)
					{
						const int Type = H.m_Type == RoamMapGen::HAZARD_SAW ? BUILDING_SAWBLADE : BUILDING_FLAMETRAP;
						CBuilding *pHazard = new CBuilding(&m_World, Pos, Type, TEAM_NEUTRAL);
						pHazard->m_NonBlockingHazard = true;
						pHazard->m_Collision = false;
						pHazard->m_CanMove = false;
						pHazard->m_Moving = false;
						if(H.m_Type == RoamMapGen::HAZARD_FLAME)
							pHazard->m_Mirror = Grid.Solid(H.m_X + 1, H.m_Y);
					}
					else
						pRoam->OnEntity(ENTITY_LAZER, Pos);
				}
			}
		}

		for(int ly = 0; ly < ChunkH; ly++)
			for(int lx = 0; lx < ChunkW; lx++)
			{
				const int AtlasX = AtlasOriginX + lx;
				const int AtlasY = AtlasOriginY + ly;
				if(AtlasX < 0 || AtlasY < 0 || AtlasX >= pTileMap->m_Width || AtlasY >= pTileMap->m_Height)
					continue;
				const int Index = pTiles[AtlasY * pTileMap->m_Width + AtlasX].m_Index;
				const int WorldTileX = WorldOriginX + lx;
				const int WorldTileY = WorldOriginY + ly;
				if(Index - ENTITY_OFFSET == ENTITY_SPAWN && m_pBlockEntities)
				{
					CBlockEntities *pNode = m_pBlockEntities->GetBlockEntities(this, block, false);
					if(pNode->AddSpawnLocal(vec2(WorldTileX, WorldTileY)))
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", "Spawn created");
				}
				else if(Index - ENTITY_OFFSET == ENTITY_ENEMYSPAWN)
					m_pController->AddEnemy(vec2(WorldTileX * 32.0f + 16.0f, WorldTileY * 32.0f + 16.0f));
				else if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(WorldTileX * 32.0f + 16.0f, WorldTileY * 32.0f + 16.0f);
					if(str_comp(g_Config.m_SvGametype, "roam") == 0 &&
					   (Index - ENTITY_OFFSET == ENTITY_CHECKPOINT || Index - ENTITY_OFFSET == ENTITY_FINISH))
						continue; // Path race markers are placement entities, never atlas entities.
					else
						m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
				}
			}
		return;
	}

	int OffX = block * m_Collision.GetChunkSize();

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < m_Collision.GetChunkSize(); x++)
		{
			int xx = m_Collision.GetModularPos(x + OffX);
			int Index = pTiles[y * pTileMap->m_Width + xx].m_Index;

			if(Index - ENTITY_OFFSET == ENTITY_SPAWN && m_pBlockEntities)
			{
				if(m_pBlockEntities->AddSpawn(vec2((x + OffX), y)))
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", "Spawn created");
				else
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", "Spawn creation failure");
			}
			else if(Index - ENTITY_OFFSET == ENTITY_ENEMYSPAWN)
				m_pController->AddEnemy(vec2((x + OffX) * 32.0f + 16.0f, y * 32.0f + 16.0f));
			else if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos((x + OffX) * 32.0f + 16.0f, y * 32.0f + 16.0f);
				m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
			}
		}
	}
}

bool CGameContext::StoreEntity(int ObjType, int Type, int Subtype, int x, int y)
{
	if(!m_Collision.IsMapModular())
		return false;

	m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, x / 32, false);
	m_pBlockEntities->StoreEntity(ObjType, Type, Subtype, x, y);

	return true;
}

void CGameContext::RestoreEntity(int ObjType, int Type, int Subtype, int x, int y)
{
	m_pController->RestoreEntity(ObjType, Type, Subtype, x, y);
}

static void ExpeditionCopyFromPlayerData(CExpeditionPlayer *pDst, const CPlayerData *pData, const char *pName, int ColorID)
{
	mem_zero(pDst, sizeof(*pDst));
	str_copy(pDst->m_aName, pName, sizeof(pDst->m_aName));
	pDst->m_ColorID = ColorID;
	pDst->m_Kits = pData->m_Kits;
	pDst->m_Armor = pData->m_Armor;
	pDst->m_Score = pData->m_Score;
	pDst->m_Gold = pData->m_Gold;
	for(int i = 0; i < EXPEDITION_WEAPON_SLOTS; i++)
	{
		pDst->m_aWeaponDefinitionId[i] = pData->m_aWeaponDefinitionId[i];
		pDst->m_aWeaponLevel[i] = pData->m_aWeaponLevel[i];
		pDst->m_aWeaponAmmo[i] = pData->m_aWeaponAmmo[i];
	}
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		pDst->m_aPvePerks[i] = pData->m_aPvePerks[i];
	pDst->m_PveChoices = pData->m_PveChoices;
	pDst->m_PveUsedContracts = pData->m_PveUsedContracts;
	pDst->m_PveInvasionFloors = pData->m_PveInvasionFloors;
	pDst->m_PvePendingArmor = pData->m_PvePendingArmor;
	pDst->m_PvePendingKits = pData->m_PvePendingKits;
	pDst->m_PvePendingAmmo = pData->m_PvePendingAmmo;
	pDst->m_PveLegendaryCard = pData->m_PveLegendaryCard;
	for(int i = 0; i < 4; i++)
		pDst->m_aPveWeaponResources[i] = pData->m_aPveWeaponResources[i];
	pDst->m_PveBarrier = pData->m_PveBarrier;
	pDst->m_PveDroneModule = pData->m_PveDroneModule;
	pDst->m_PveDeathlessFloors = pData->m_PveDeathlessFloors;
}

static void ExpeditionCopyToPlayerData(CPlayerData *pData, const CExpeditionPlayer &Src)
{
	pData->m_Kits = Src.m_Kits;
	pData->m_Armor = Src.m_Armor;
	pData->m_Score = Src.m_Score;
	pData->m_Gold = Src.m_Gold;
	for(int i = 0; i < EXPEDITION_WEAPON_SLOTS; i++)
	{
		pData->m_aWeaponDefinitionId[i] = Src.m_aWeaponDefinitionId[i];
		pData->m_aWeaponLevel[i] = Src.m_aWeaponLevel[i];
		pData->m_aWeaponAmmo[i] = Src.m_aWeaponAmmo[i];
	}
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		pData->m_aPvePerks[i] = Src.m_aPvePerks[i];
	pData->m_PveChoices = Src.m_PveChoices;
	pData->m_PveUsedContracts = Src.m_PveUsedContracts;
	pData->m_PveInvasionFloors = Src.m_PveInvasionFloors;
	pData->m_PvePendingArmor = Src.m_PvePendingArmor;
	pData->m_PvePendingKits = Src.m_PvePendingKits;
	pData->m_PvePendingAmmo = Src.m_PvePendingAmmo;
	pData->m_PveLegendaryCard = Src.m_PveLegendaryCard;
	for(int i = 0; i < 4; i++)
		pData->m_aPveWeaponResources[i] = Src.m_aPveWeaponResources[i];
	pData->m_PveBarrier = Src.m_PveBarrier;
	pData->m_PveDroneModule = Src.m_PveDroneModule;
	pData->m_PveDeathlessFloors = Src.m_PveDeathlessFloors;
	pData->m_PveRunMode = PVE_MODE_INVASION;
}

void CGameContext::WriteExpeditionSave(bool SnapshotCharacters)
{
	if(!CExpeditionSaveStorage::SlotValid(g_Config.m_SvExpeditionSlot) || !m_pStorage)
		return;
	if(!m_ExpeditionReady)
		CExpeditionSaveStorage::Load(m_pStorage, g_Config.m_SvExpeditionSlot, &m_ExpeditionSave);
	m_ExpeditionSave.m_Floor = max(1, g_Config.m_SvMapGenLevel);
	m_ExpeditionSave.m_Seed = max(1, g_Config.m_SvMapGenSeed);
	if(m_pPveDirector)
		m_ExpeditionSave.m_UsedContracts = m_pPveDirector->UsedContracts();
	if(!SnapshotCharacters)
		m_ExpeditionSave.m_NumPlayers = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot)
			continue;
		if(SnapshotCharacters && pPlayer->GetCharacter())
			pPlayer->SaveData();
		CPlayerData *pData = Server()->GetPlayerData(i, pPlayer->GetColorID());
		if(!pData)
			continue;
		CExpeditionPlayer Row;
		ExpeditionCopyFromPlayerData(&Row, pData, Server()->ClientName(i), pPlayer->GetColorID());
		Row.m_PveUsedContracts = m_ExpeditionSave.m_UsedContracts;
		m_ExpeditionSave.UpsertPlayer(Row);
	}
	CExpeditionSaveStorage::Save(m_pStorage, g_Config.m_SvExpeditionSlot, m_ExpeditionSave);
	m_ExpeditionReady = true;
	dbg_msg("expedition",
			"saved slot %d floor %d players %d",
			g_Config.m_SvExpeditionSlot,
			m_ExpeditionSave.m_Floor,
			m_ExpeditionSave.m_NumPlayers);
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	dbg_assert(CWeaponCatalog::Validate(), "weapon catalog validation failed");
	dbg_assert(CForge::Validate(), "forge recipe validation failed");
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>(); // MapGen
	m_pLocalization = Kernel()->RequestInterface<ILocalization>();
	if(CExpeditionSaveStorage::SlotValid(g_Config.m_SvExpeditionSlot) && m_pStorage)
	{
		const EExpeditionLoadResult Result =
			CExpeditionSaveStorage::Load(m_pStorage, g_Config.m_SvExpeditionSlot, &m_ExpeditionSave);
		if(Result == EXPEDITION_LOAD_OK)
		{
			g_Config.m_SvMapGenLevel = m_ExpeditionSave.m_Floor;
			g_Config.m_SvMapGenSeed = m_ExpeditionSave.m_Seed;
			g_Config.m_SvMapGenRandSeed = 0;
			g_Config.m_SvInvasionUseCheckpoint = 0;
			m_ExpeditionReady = true;
			dbg_msg("expedition",
					"loaded slot %d floor %d seed %d players %d",
					g_Config.m_SvExpeditionSlot,
					m_ExpeditionSave.m_Floor,
					m_ExpeditionSave.m_Seed,
					m_ExpeditionSave.m_NumPlayers);
		}
		else
			dbg_msg("expedition", "slot %d load result %d", g_Config.m_SvExpeditionSlot, Result);
	}
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	m_pBlockEntities = 0;

	// if(!data) // only load once
	// data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);
	m_MapGen.Init(&m_Layers, &m_Collision, m_pStorage); // MapGen

	// reset everything here
	// world = new GAMEWORLD;
	// players = new CPlayer[MAX_CLIENTS];

	// select gametype
	if(str_comp(g_Config.m_SvGametype, "ctf") == 0)
		m_pController = new CGameControllerCTF(this);
	else if(str_comp(g_Config.m_SvGametype, "def") == 0)
		m_pController = new CGameControllerCS(this);
	else if(str_comp(g_Config.m_SvGametype, "tdm") == 0)
		m_pController = new CGameControllerTDM(this);
	else if(str_comp(g_Config.m_SvGametype, "inf") == 0)
		m_pController = new CGameControllerTexasRun(this);
	else if(str_comp(g_Config.m_SvGametype, "base") == 0)
		m_pController = new CGameControllerBase(this);
	else if(str_comp(g_Config.m_SvGametype, "tutorial") == 0)
		m_pController = new CGameControllerTutorial(this);
	else if(str_comp(g_Config.m_SvGametype, "coop") == 0)
		m_pController = new CGameControllerInvasion(this);
	else if(str_comp(g_Config.m_SvGametype, "horde") == 0)
		m_pController = new CGameControllerHorde(this);
	else if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		m_pController = new CGameControllerExtract(this);
	else if(str_comp(g_Config.m_SvGametype, "ball") == 0)
		m_pController = new CGameControllerBall(this);
	else if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		m_pController = new CGameControllerRoam(this);
	else
		m_pController = new CGameControllerDM(this);

	// Apply challenge variants once, after the controller has set its baseline
	// config (variants override it, e.g. no building).
	ApplyChallengeVariants(this);
	ApplyPvpModeBalance(this);
	LoadChallengeScript();

	// Tutorial forge/build need kits; restore building if a challenge cleared it.
	if(str_comp(g_Config.m_SvGametype, "tutorial") == 0 &&
	   TutorialChapterForcesBuilding(g_Config.m_SvTutorialChapter))
		g_Config.m_SvEnableBuilding = 1;

	m_pPveDirector = new CPveDirector(this);
	if(m_ExpeditionReady)
		m_pPveDirector->ImportUsedContracts(m_ExpeditionSave.m_UsedContracts);
	if(str_comp(g_Config.m_SvGametype, "tutorial") == 0)
		m_pTutorialDirector = new CTutorialDirector(this);

	// if (str_comp(g_Config.m_SvGametype, "coop") != 0)
	//	Server()->ResetPlayerData();

	// MapGen
	// if (str_comp(g_Config.m_SvGametype, "coop") == 0 && g_Config.m_SvMapGen && !m_pServer->m_MapGenerated)
	if(g_Config.m_SvMapGen && !m_pServer->m_MapGenerated)
	{
		m_MapGen.FillMap();
		SaveMap("");

		str_copy(g_Config.m_SvMap, "generated", sizeof(g_Config.m_SvMap));
		m_pServer->m_MapGenerated = true;
	}

	// setup core world
	// for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	/*
	num_spawn_points[0] = 0;
	num_spawn_points[1] = 0;
	num_spawn_points[2] = 0;
	*/

	// create entities for non-modular maps
	if(!m_Collision.IsMapModular())
	{
		for(int y = 0; y < pTileMap->m_Height; y++)
		{
			for(int x = 0; x < pTileMap->m_Width; x++)
			{
				int Index = pTiles[y * pTileMap->m_Width + x].m_Index;

				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
				}
			}
		}
	}
	//
	else
	{
		CMapPath *pPath = m_Collision.GetMapPath();
		if(pPath)
			m_pBlockEntities = new CBlockEntities(this, 0, 1, 0);
		else
			m_pBlockEntities = new CBlockEntities(this, 0, m_Collision.GetChunkSize(), 0);

		ActivateBlockEntities(0);
		if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		{
			if(pPath)
			{
				for(int Index = 0; Index < pPath->PlacementCount(); Index++)
					m_pBlockEntities = m_pBlockEntities->GetBlockEntities(this, Index, true);
			}
			else
			{
				const int ChunkSize = m_Collision.GetChunkSize();
				for(int WorldChunk = 1; WorldChunk <= 3; WorldChunk++)
					ActivateBlockEntities(WorldChunk * ChunkSize * 32 + 16);
			}
		}
	}

	if(str_comp(g_Config.m_SvGametype, "roam") == 0)
	{
		// During the first mapgen pass the source atlas has no persisted Path yet;
		// the server immediately reloads generated.map.  Validate only the loaded
		// Path instance so a successful generation does not emit a false error.
		if(!g_Config.m_SvMapGen || m_Collision.GetMapPath())
		{
			vec2 SpawnPos;
			static_cast<CGameControllerRoam *>(m_pController)->FinalizeCourse(GetRoamSpawnPos(&SpawnPos));
		}
	}

	// game.world.insert_entity(game.Controller);

	// SetupVotes(-1);

	/*
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	m_pVoteOptionHeap->Reset();
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	*/
}

void CGameContext::OnShutdown()
{
	KickBots();

	if(m_pBlockEntities)
	{
		CBlockEntities::DestroyChain(m_pBlockEntities);
		m_pBlockEntities = 0;
	}

	delete m_pPveDirector;
	m_pPveDirector = 0;
	delete m_pTutorialDirector;
	m_pTutorialDirector = 0;
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	if(ClientID != -1 && m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsBot)
		return;

	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() &&
	   mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	if(m_pPveDirector)
		m_pPveDirector->Snap(ClientID);
	m_Events.Snap(ClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}

	SnapNpcs(ClientID);

	if(m_ChallengeScriptLoaded)
	{
		const CChallengeScriptState &State = m_ChallengeScript.State();
		for(int Owner = 0; Owner < MAX_CLIENTS; ++Owner)
		{
			if(!m_apPlayers[Owner])
				continue;
			CNetObj_ChallengeRuntime *pRuntime = static_cast<CNetObj_ChallengeRuntime *>(
				Server()->SnapNewItem(NETOBJTYPE_CHALLENGERUNTIME, Owner, sizeof(CNetObj_ChallengeRuntime)));
			if(!pRuntime)
				continue;
			pRuntime->m_Tick = State.m_Tick;
			pRuntime->m_RandomState = (int)State.m_RandomState;
			pRuntime->m_State0 = State.m_aGlobal[0];
			pRuntime->m_State1 = State.m_aGlobal[1];
			pRuntime->m_State2 = State.m_aGlobal[2];
			pRuntime->m_State3 = State.m_aGlobal[3];
			pRuntime->m_State4 = State.m_aGlobal[4];
			pRuntime->m_State5 = State.m_aGlobal[5];
			pRuntime->m_State6 = State.m_aGlobal[6];
			pRuntime->m_State7 = State.m_aGlobal[7];
			pRuntime->m_PlayerState0 = State.m_aPlayer[Owner][0];
			pRuntime->m_PlayerState1 = State.m_aPlayer[Owner][1];
			pRuntime->m_PlayerState2 = State.m_aPlayer[Owner][2];
			pRuntime->m_PlayerState3 = State.m_aPlayer[Owner][3];
			pRuntime->m_Checksum = (int)State.m_Checksum;
			pRuntime->m_Owner = Owner;
		}
	}
}
void CGameContext::OnPreSnap()
{
}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true;
}

const char *CGameContext::GameType()
{
	return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : "";
}
const char *CGameContext::Version()
{
	return GAME_VERSION;
}
const char *CGameContext::NetVersion()
{
	return GAME_NETVERSION;
}

IGameServer *CreateGameServer()
{
	return new CGameContext;
}

void CGameContext::KickBots()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "engine", "Kicking bots...");

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsBot(i))
			Server()->Kick(i, "");
	}

	for(int i = 0; i < MAX_NPCS; i++)
	{
		if(!m_aNpcs[i].m_Used)
			continue;
		if(m_aNpcs[i].m_pCharacter)
		{
			if(m_aNpcs[i].m_pCharacter->IsAlive())
				m_aNpcs[i].m_pCharacter->Die(CAttackSource::World(WEAPON_WORLD, -1), true);
			delete m_aNpcs[i].m_pCharacter;
			m_aNpcs[i].m_pCharacter = 0;
		}
		m_aNpcs[i].m_Used = false;
		m_aNpcs[i].m_Spawning = false;
		m_aNpcs[i].m_ToBeKicked = false;
	}
}

void CGameContext::KickBot(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return;

	if(m_apPlayers[ClientID]->GetCharacter())
		m_apPlayers[ClientID]->GetCharacter()->Die(CAttackSource::World(WEAPON_WORLD, ClientID), true);

	if(IsBot(ClientID))
		Server()->Kick(ClientID, "");
}

void CGameContext::KickOneBot(int Team)
{
	for(int i = MAX_NPCS - 1; i >= 0; i--)
	{
		if(!m_aNpcs[i].m_Used)
			continue;
		if(Team >= TEAM_RED && Team <= TEAM_BLUE && m_aNpcs[i].m_Team != Team)
			continue;

		if(m_aNpcs[i].m_pCharacter)
		{
			if(m_aNpcs[i].m_pCharacter->IsAlive())
				m_aNpcs[i].m_pCharacter->Die(CAttackSource::World(WEAPON_WORLD, -1), true);
			delete m_aNpcs[i].m_pCharacter;
			m_aNpcs[i].m_pCharacter = 0;
		}
		m_aNpcs[i].m_Used = false;
		m_aNpcs[i].m_Spawning = false;
		m_aNpcs[i].m_ToBeKicked = false;
		return;
	}
}

void CGameContext::AddBot()
{
	bool aUsed[MAX_NPCS];
	for(int i = 0; i < MAX_NPCS; i++)
		aUsed[i] = m_aNpcs[i].m_Used;
	const int Slot = NpcAllocSlot(aUsed, MAX_NPCS);
	if(Slot < 0)
		return;

	int Team = TEAM_BLUE;
	if(m_pController && !m_pController->IsCoop())
		Team = m_pController->GetAutoTeam(-1);

	mem_zero(&m_aNpcs[Slot], sizeof(m_aNpcs[Slot]));
	m_aNpcs[Slot].m_Used = true;
	m_aNpcs[Slot].m_Team = Team;
	m_aNpcs[Slot].m_Spawning = true;
	m_aNpcs[Slot].m_RespawnTick = Server()->Tick();
	TrySpawnNpc(Slot);
}

void CGameContext::TrySpawnNpc(int Slot)
{
	if(Slot < 0 || Slot >= MAX_NPCS)
		return;

	CNpcSlot *pSlot = &m_aNpcs[Slot];
	if(!pSlot->m_Used || pSlot->m_pCharacter || !m_pController)
		return;

	vec2 SpawnPos = vec2(0, 0);
	if(!m_pController->CanSpawn(pSlot->m_Team, &SpawnPos, true))
		return;

	CCharacter *pChr = new(NpcCoreIndex(Slot)) CCharacter(&m_World);
	pChr->SpawnNpc(Slot, pSlot->m_Team, SpawnPos);
	pSlot->m_pCharacter = pChr;
	pSlot->m_Spawning = false;
	CreatePlayerSpawn(SpawnPos);
}

void CGameContext::SnapNpcs(int SnappingClient)
{
	(void)SnappingClient;
	for(int i = 0; i < MAX_NPCS; i++)
	{
		if(!m_aNpcs[i].m_Used)
			continue;

		CNetObj_NpcInfo *pInfo = static_cast<CNetObj_NpcInfo *>(
			Server()->SnapNewItem(NETOBJTYPE_NPCINFO, i, sizeof(CNetObj_NpcInfo)));
		if(!pInfo)
			continue;

		const char *pName = m_aNpcs[i].m_AISkin.m_aName;
		if(m_aNpcs[i].m_pCharacter && m_aNpcs[i].m_pCharacter->m_AISkin.m_aName[0])
			pName = m_aNpcs[i].m_pCharacter->m_AISkin.m_aName;
		StrToInts(&pInfo->m_Name0, 4, pName);
		pInfo->m_Score = m_aNpcs[i].m_Score;
		pInfo->m_Team = m_aNpcs[i].m_Team;
	}
}

void CGameContext::TickNpcs()
{
	for(int i = 0; i < MAX_NPCS; i++)
	{
		CNpcSlot *pSlot = &m_aNpcs[i];
		if(!pSlot->m_Used)
			continue;

		if(pSlot->m_pCharacter && pSlot->m_pCharacter->ToBeKicked())
			pSlot->m_ToBeKicked = true;

		if(pSlot->m_pCharacter && !pSlot->m_pCharacter->IsAlive())
		{
			if(g_Config.m_SvSurvivalMode)
				pSlot->m_RespawnTick = Server()->Tick();
			else
				pSlot->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvRespawnDelay;
			delete pSlot->m_pCharacter;
			pSlot->m_pCharacter = 0;
			pSlot->m_Spawning = !pSlot->m_ToBeKicked;
		}

		if(pSlot->m_ToBeKicked)
		{
			if(pSlot->m_pCharacter)
			{
				if(pSlot->m_pCharacter->IsAlive())
					pSlot->m_pCharacter->Die(CAttackSource::World(WEAPON_WORLD, -1), true);
				delete pSlot->m_pCharacter;
				pSlot->m_pCharacter = 0;
			}
			pSlot->m_Used = false;
			pSlot->m_Spawning = false;
			pSlot->m_ToBeKicked = false;
			continue;
		}

		if(pSlot->m_Spawning && pSlot->m_RespawnTick <= Server()->Tick())
			TrySpawnNpc(i);
	}
}

void CGameContext::TriggerBotAI(int TriggerLevel)
{
	for(int i = 0; i < MAX_NPCS; i++)
	{
		CCharacter *pChr = m_aNpcs[i].m_pCharacter;
		if(pChr && pChr->m_pAI)
			pChr->m_pAI->Trigger(TriggerLevel);
	}
}

int CGameContext::CountBots(bool SkipSpecialTees)
{
	int n = 0;

	for(int i = 0; i < MAX_NPCS; i++)
	{
		if(!m_aNpcs[i].m_Used)
			continue;
		if(SkipSpecialTees)
		{
			CCharacter *pChr = m_aNpcs[i].m_pCharacter;
			if(pChr && pChr->m_pAI && pChr->m_pAI->m_Special < 0)
				n++;
		}
		else
			n++;
	}

	return n;
}

/*
int CGameContext::CountHumans()
{
	int n = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsHuman(i))
			n++;
	}

	return n;
}
*/

int CGameContext::CountBotsAlive(bool SkipSpecialTees)
{
	int n = 0;

	for(int i = 0; i < MAX_NPCS; i++)
	{
		CCharacter *pChr = m_aNpcs[i].m_pCharacter;
		if(!pChr || !pChr->IsAlive())
			continue;
		if(SkipSpecialTees)
		{
			if(pChr->m_pAI && pChr->m_pAI->m_Special < 0)
				n++;
		}
		else
			n++;
	}

	return n;
}

int CGameContext::CountHumansAlive()
{
	int n = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsHuman(i) && m_apPlayers[i]->GetCharacter() && m_apPlayers[i]->GetCharacter()->IsAlive())
			n++;
	}

	return n;
}

int CGameContext::DistanceToHuman(vec2 Pos)
{
	int MinDist = 10000;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && !m_apPlayers[i]->m_IsBot && m_apPlayers[i]->GetCharacter())
		{
			if(distance(Pos, m_apPlayers[i]->GetCharacter()->m_Pos) < MinDist)
				MinDist = distance(Pos, m_apPlayers[i]->GetCharacter()->m_Pos);
		}
	}

	return MinDist;
}

vec2 CGameContext::GetNearHumanSpawnPos(bool AllowVision)
{
	int n = 0;
	vec2 ReturnPos = Collision()->GetRandomWaypointPos();
	int Dist = 100000;

	while(n++ < 50)
	{
		vec2 Pos = Collision()->GetRandomWaypointPos();
		if(Collision()->IsInFluid(Pos.x, Pos.y) || Collision()->IsInFluid(Pos.x, Pos.y + 32.0f))
			continue;

		bool Valid = true;
		int MinDist = 10000;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && !m_apPlayers[i]->m_IsBot && m_apPlayers[i]->GetCharacter())
			{
				vec2 PlayerPos = m_apPlayers[i]->GetCharacter()->m_Pos;

				if(!AllowVision && abs(Pos.x - PlayerPos.x) < 1200 && abs(Pos.x - PlayerPos.x) < 900)
				{
					Valid = false;
					break;
				}
				else
				{

					if(distance(Pos, PlayerPos) < MinDist)
						MinDist = distance(Pos, PlayerPos);
				}
			}
		}

		if(Valid)
		{
			if(MinDist < 1800)
				return Pos;

			if(MinDist < Dist)
			{
				Dist = MinDist;
				ReturnPos = Pos;
			}
		}
	}
	return ReturnPos;
}

vec2 CGameContext::GetFarHumanSpawnPos(bool AllowVision)
{
	int n = 0;
	vec2 ReturnPos = Collision()->GetRandomWaypointPos();
	int Dist = 1;

	while(n++ < 50)
	{
		vec2 Pos = Collision()->GetRandomWaypointPos();
		if(Collision()->IsInFluid(Pos.x, Pos.y) || Collision()->IsInFluid(Pos.x, Pos.y + 32.0f))
			continue;

		bool Valid = true;
		int MaxDist = 1;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && !m_apPlayers[i]->m_IsBot && m_apPlayers[i]->GetCharacter())
			{
				vec2 PlayerPos = m_apPlayers[i]->GetCharacter()->m_Pos;

				if(!AllowVision && abs(Pos.x - PlayerPos.x) < 1200 && abs(Pos.x - PlayerPos.x) < 900)
				{
					Valid = false;
					break;
				}
				else
				{

					if(distance(Pos, PlayerPos) > MaxDist)
						MaxDist = distance(Pos, PlayerPos);
				}
			}
		}

		if(Valid)
		{
			if(MaxDist > 3000)
				return Pos;

			if(MaxDist > Dist)
			{
				Dist = MaxDist;
				ReturnPos = Pos;
			}
		}
	}
	return ReturnPos;
}

vec2 CGameContext::GetFarSafeStandPos(vec2 From)
{
	CCollision *pCol = Collision();
	vec2 Best = vec2(0, 0);
	float BestScore = -1.0f;
	const int Count = pCol->WaypointCount();
	for(int i = 0; i < Count; i++)
	{
		vec2 Raw = pCol->GetWaypointPos(i);
		if(Raw.x == 0.0f && Raw.y == 0.0f)
			continue;
		vec2 Cand = pCol->SnapToStandPos(Raw);
		if(!pCol->IsSafeStandPos(Cand))
			continue;
		float Score = (From.x == 0.0f && From.y == 0.0f) ? Cand.x : distance(Cand, From);
		if(Score > BestScore)
		{
			BestScore = Score;
			Best = Cand;
		}
	}
	return Best;
}

// MapGen
void CGameContext::SaveMap(const char *path)
{
	IMap *pMap = Layers()->Map();
	if(!pMap)
		return;

	CDataFileWriter fileWrite;
	char aMapFile[512];
	// str_format(aMapFile, sizeof(aMapFile), "maps/%s_%d.map", Server()->GetMapName(), g_Config.m_SvMapGenSeed);
	str_format(aMapFile, sizeof(aMapFile), "maps/generated.map");

	// Map will be saved to current dir, not to ~/.ninslash/maps or to data/maps, so we need to create a dir for it
	Storage()->CreateFolder("maps", IStorage::TYPE_SAVE);

	fileWrite.SaveMap(Storage(),
		pMap->GetFileReader(),
		aMapFile,
		0,
		0,
		m_MapGen.GetModularInfo(),
		m_MapGen.GetModularRules(),
		m_MapGen.GetPathInfo(),
		m_MapGen.GetPathPlacements());

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Map saved in '%s'!", aMapFile);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

const char *CGameContext::Localize(const char *pText, int ClientID)
{
	if(!m_apPlayers[ClientID])
		return pText;

	return Localization()->Localize(m_apPlayers[ClientID]->m_aLanguage, pText);
}

/*
Server-side localization keys (see data/server/languages/en-template.json):
Quest start/completion strings, game-vote names/descriptions, team-move messages, and all SendBroadcast/SendChatTarget
literals. Run scripts/check_localization.py to verify coverage.
*/
