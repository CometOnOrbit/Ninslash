#include <engine/shared/config.h>

#include <game/collision.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/turret.h>
#include <game/server/entities/weapon.h>
#include <game/nodes_collision.h>

#include "nodes.h"

namespace
{
constexpr int NODE_MAX_TECH_LEVEL = 3;
constexpr int NODE_POWER_REACTOR_RANGE = 750;
constexpr int NODE_POWER_REPEATER_RANGE = 500;

bool ValidTeam(int Team)
{
	return Team == TEAM_RED || Team == TEAM_BLUE;
}

bool NodesAmmoMatches(const CWeapon *pWeapon, int Type)
{
	if(!pWeapon)
		return false;
	const CWeaponCombatProfile &Combat = pWeapon->GetWeaponProfile().m_Combat;
	switch(Type)
	{
		case NODES_AMMO_SHOTGUN:
			return Combat.m_FiringType == WFT_PROJECTILE && !Combat.m_LaserWeapon && !Combat.m_ExplosiveProjectile;
		case NODES_AMMO_GRENADE:
			return Combat.m_ExplosiveProjectile || Combat.m_FiringType == WFT_THROW;
		case NODES_AMMO_LASER:
			return Combat.m_LaserWeapon;
		default:
			return false;
	}
}

bool RefillNodesAmmo(CCharacter *pCharacter, int Type)
{
	if(!pCharacter)
		return false;
	bool Refilled = false;
	for(int Slot = 0; Slot < NUM_SLOTS; ++Slot)
	{
		CWeapon *pWeapon = pCharacter->GetWeapon(Slot);
		if(!NodesAmmoMatches(pWeapon, Type) || pWeapon->GetAmmo() >= pWeapon->m_MaxAmmo)
			continue;
		const int AmmoBefore = pWeapon->GetAmmo();
		pWeapon->IncreaseAmmo(max(1, pWeapon->m_MaxAmmo / 3));
		Refilled |= pWeapon->GetAmmo() > AmmoBefore;
	}
	return Refilled;
}
}

CGameControllerNodes::CGameControllerNodes(CGameContext *pGameServer)
	: IGameController(pGameServer)
{
	m_pGameType = "Nodes";
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_BUILD | GAMEFLAG_NODES;
	g_Config.m_SvDisablePVP = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvTimelimit = 0;
	g_Config.m_SvScorelimit = 0;
	m_Runtime.Reset(g_Config.m_SvNodesStartBuildpoints);
	StartRound();
}

void CGameControllerNodes::PostReset()
{
	ResetNodes();
}

void CGameControllerNodes::ResetNodes()
{
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
	{
		while(m_Runtime.Buildings().Count(Team) > 0)
			DestroyBuilding(m_Runtime.Buildings().At(Team, m_Runtime.Buildings().Count(Team) - 1), true);
	}
	m_Runtime.Reset(g_Config.m_SvNodesStartBuildpoints);
	m_Runtime.PrepareBootstrap();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i])
		{
			LeaveSpawnQueue(GameServer()->m_apPlayers[i]);
		}
	}
	for(int i = 0; i < m_Runtime.InitialBuildings().Count(); ++i)
	{
		const CNodesInitialBuilding &Initial = m_Runtime.InitialBuildings().At(i);
		BuildNodesForTeam(Initial.m_Pos, Initial.m_Type, Initial.m_Team, -1);
	}
	m_Runtime.Activate();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || !ValidTeam(pPlayer->GetTeam()))
			continue;
		pPlayer->m_RespawnTick = Server()->Tick();
		pPlayer->Respawn();
		EnterSpawnQueue(pPlayer);
	}
}

bool CGameControllerNodes::OnEntity(int Index, vec2 Pos)
{
	int Type = -1;
	int Team = TEAM_RED;
	if(Index == NODES_ENTITY_REACTOR_RED)
	{
		Type = NODES_REACTOR;
		Team = TEAM_RED;
	}
	else if(Index == NODES_ENTITY_SPAWN_RED)
	{
		Type = NODES_SPAWN;
		Team = TEAM_RED;
	}
	else if(Index == NODES_ENTITY_REACTOR_BLUE)
	{
		Type = NODES_REACTOR;
		Team = TEAM_BLUE;
	}
	else if(Index == NODES_ENTITY_SPAWN_BLUE)
	{
		Type = NODES_SPAWN;
		Team = TEAM_BLUE;
	}
	else if(Index == NODES_ENTITY_CRATE_SPAWN)
		return true;
	else if(Index == ENTITY_SPAWN_RED)
	{
		Type = NODES_SPAWN;
		Team = TEAM_RED;
	}
	else if(Index == ENTITY_SPAWN_BLUE)
	{
		Type = NODES_SPAWN;
		Team = TEAM_BLUE;
	}

	if(Type >= 0)
	{
		m_Runtime.InitialBuildings().Add(Pos, Team, Type);
		return true;
	}
	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerNodes::BuildNodes(vec2 Pos, int Type, int Owner)
{
	if(Owner < 0 || Owner >= MAX_CLIENTS || !GameServer()->m_apPlayers[Owner])
		return false;
	const bool Built = BuildNodesForTeam(Pos, Type, GameServer()->m_apPlayers[Owner]->GetTeam(), Owner);
	if(Built)
		GameServer()->CreateSound(Pos, SOUND_BUILD);
	return Built;
}

bool CGameControllerNodes::AlignBuildingToGround(vec2 Pos, const CNodesBuildingInfo &Info, vec2 *pAlignedPos) const
{
	if(!pAlignedPos)
		return false;
	CCollision *pCollision = GameServer()->Collision();
	if(NodesBuildingFits(pCollision, Pos, Info))
	{
		*pAlignedPos = Pos;
		return true;
	}
	return NodesBuildingFindGround(pCollision, Pos, Info, pAlignedPos, nullptr);
}

bool CGameControllerNodes::ValidBuildPosition(vec2 Pos, const CNodesBuildingInfo &Info) const
{
	return NodesBuildingFits(GameServer()->Collision(), Pos, Info);
}

bool CGameControllerNodes::BuildNodesForTeam(vec2 Pos, int Type, int Team, int Owner)
{
	if(Type < 0 || Type >= NODES_BUILDING_COUNT || Owner < -1 || Owner >= MAX_CLIENTS || !ValidTeam(Team))
		return false;
	if(m_Runtime.Buildings().Count(Team) >= NODES_MAX_BUILDINGS)
	{
		if(Owner >= 0)
			GameServer()->SendChatTarget(Owner, "Your team has reached the building limit");
		return false;
	}
	if(Owner >= 0 && !m_Runtime.BuildCooldowns().Ready(Owner, Server()->Tick()))
	{
		GameServer()->SendChatTarget(Owner, "Please wait before building again");
		return false;
	}
	const CNodesBuildingInfo &Info = g_aNodesBuildingInfo[Type];
	vec2 BuildPos = Pos;
	if(!AlignBuildingToGround(Pos, Info, &BuildPos))
	{
		if(Owner >= 0)
			GameServer()->SendChatTarget(Owner, "This location is not a valid building position");
		return false;
	}
	if(Owner >= 0 && m_Runtime.Economy().TechLevel(Team) < Info.m_TechLevel)
	{
		GameServer()->SendChatTarget(Owner, "Your team needs a higher tech level for this building");
		return false;
	}
	if(Owner >= 0 && m_Runtime.Economy().BuildPoints(Team) < Info.m_Price)
	{
		GameServer()->SendChatTarget(Owner, "Your team has insufficient build points");
		return false;
	}
	if(Type != NODES_REACTOR && !HasReactor(Team))
	{
		if(Owner >= 0)
			GameServer()->SendChatTarget(Owner, "Build a reactor first");
		return false;
	}
	if(Type == NODES_REACTOR && HasReactor(Team))
	{
		if(Owner >= 0)
			GameServer()->SendChatTarget(Owner, "Your team can only have one reactor");
		return false;
	}
	if(Owner >= 0 && Type == NODES_TELEPORT)
	{
		int Teleporters = 0;
		for(int i = 0; i < m_Runtime.Buildings().Count(Team); ++i)
			if(const CBuilding *pExisting = m_Runtime.Buildings().At(Team, i); pExisting && pExisting->NodesType() == NODES_TELEPORT)
				++Teleporters;
		if(Teleporters >= 2)
		{
			GameServer()->SendChatTarget(Owner, "Your team can only have two teleporters");
			return false;
		}
	}
	if(!ValidBuildPosition(BuildPos, Info))
	{
		if(Owner >= 0)
			GameServer()->SendChatTarget(Owner, "This location is not a valid building position");
		return false;
	}
	for(int OtherTeam = TEAM_RED; OtherTeam <= TEAM_BLUE; ++OtherTeam)
	{
		for(int i = 0; i < m_Runtime.Buildings().Count(OtherTeam); ++i)
		{
			CBuilding *pOther = m_Runtime.Buildings().At(OtherTeam, i);
			if(!pOther || pOther->NodesHealth() <= 0)
				continue;
			if(distance(BuildPos, pOther->m_Pos) < Info.m_Radius + pOther->m_ProximityRadius)
			{
				if(Owner >= 0)
					GameServer()->SendChatTarget(Owner, "This location is blocked by another building");
				return false;
			}
			if(OtherTeam != Team && distance(BuildPos, pOther->m_Pos) < g_Config.m_SvNodesEnemyBuildDistance)
			{
				if(Owner >= 0)
					GameServer()->SendChatTarget(Owner, "You are building too close to the enemy base");
				return false;
			}
		}
	}
	CBuilding *pBuilding = new CBuilding(&GameServer()->m_World, BuildPos, Type, Team, Owner, Owner < 0 ? Info.m_MaxHealth : 1, Owner < 0, Owner < 0);
	if(Type == NODES_TURRET_GUN || Type == NODES_TURRET_SHOTGUN)
	{
		delete pBuilding;
		const CWeaponSpec Weapon = Type == NODES_TURRET_GUN
			? CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1)
			: CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL1);
		CTurret *pTurret = new CTurret(&GameServer()->m_World, BuildPos, Team, GameServer()->NewWeapon(Weapon));
		pBuilding = pTurret;
		pBuilding->m_NodesMode = true;
		pBuilding->m_NodesType = Type;
		pBuilding->m_NodesOwner = Owner;
		pBuilding->m_NodesMaxHealth = Info.m_MaxHealth;
		pBuilding->m_NodesHealth = Owner < 0 ? Info.m_MaxHealth : 1;
		pBuilding->m_NodesAlive = Owner < 0;
		pBuilding->m_NodesPower = false;
		pBuilding->m_NodesFree = Owner < 0;
		pBuilding->m_NodesDeconstruction = false;
		pBuilding->m_NodesDestroyed = false;
		pBuilding->m_Life = pBuilding->m_NodesHealth;
		pBuilding->m_MaxLife = pBuilding->m_NodesMaxHealth;
	}
	if(Owner >= 0 && Type != NODES_REACTOR && Type != NODES_REPEATER && !IsPowered(pBuilding))
	{
		GameServer()->SendChatTarget(Owner, "This location is not powered by a reactor or repeater");
		pBuilding->m_NodesDestroyed = true;
		GameServer()->m_World.DestroyEntity(pBuilding);
		return false;
	}
	if(!m_Runtime.Buildings().Add(pBuilding, Team))
	{
		GameServer()->m_World.DestroyEntity(pBuilding);
		return false;
	}
	if(Owner >= 0)
	{
		m_Runtime.Economy().Spend(Team, Info.m_Price);
		m_Runtime.BuildCooldowns().SetReadyTick(Owner, Server()->Tick() + g_Config.m_SvNodesBuildDelay * Server()->TickSpeed());
	}
	return true;
}

bool CGameControllerNodes::PrepareNodesTurret(CBuilding *pBuilding)
{
	if(!pBuilding || !pBuilding->IsNodesBuilding())
		return true;
	const bool Powered = IsPowered(pBuilding);
	pBuilding->SetNodesPower(Powered);
	pBuilding->SetNodesAnimationFrame(NodesBuildingAnimationFrame(pBuilding->NodesType(), Powered, pBuilding->NodesAlive(), Server()->Tick()));
	if(!pBuilding->NodesAlive())
	{
		if(Powered)
		{
			pBuilding->SetNodesHealth(pBuilding->NodesHealth() + 2);
			if(pBuilding->NodesHealth() >= pBuilding->NodesMaxHealth())
				pBuilding->SetNodesAlive(true);
		}
		return false;
	}
	return Powered;
}

void CGameControllerNodes::TickBuilding(CBuilding *pBuilding)
{
	if(!pBuilding || pBuilding->NodesHealth() <= 0)
		return;
	const int Team = pBuilding->m_Team;
	const int Type = pBuilding->NodesType();
	const bool Powered = IsPowered(pBuilding);
	pBuilding->SetNodesPower(Powered);
	pBuilding->SetNodesAnimationFrame(NodesBuildingAnimationFrame(Type, Powered, pBuilding->NodesAlive(), Server()->Tick()));
	if(!pBuilding->NodesAlive())
	{
		if(Powered || Type == NODES_REACTOR)
		{
			pBuilding->SetNodesHealth(min(pBuilding->NodesMaxHealth(), pBuilding->NodesHealth() + 2));
			if(pBuilding->NodesHealth() >= pBuilding->NodesMaxHealth())
				pBuilding->SetNodesAlive(true);
		}
		return;
	}
	if(!Powered && Type != NODES_REACTOR && Type != NODES_REPEATER)
		return;
	if(Type == NODES_AMMO_SHOTGUN || Type == NODES_AMMO_GRENADE || Type == NODES_AMMO_LASER)
	{
		if(pBuilding->m_NodesAttackTick > Server()->Tick())
			return;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			CCharacter *pCharacter = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pCharacter || pPlayer->GetTeam() != Team || distance(pCharacter->m_Pos, pBuilding->m_Pos) > 40.0f)
				continue;
			if(RefillNodesAmmo(pCharacter, Type))
			{
				pBuilding->m_NodesAttackTick = Server()->Tick() + Server()->TickSpeed();
				break;
			}
		}
		return;
	}
	if(Type == NODES_TELEPORT)
	{
		if(pBuilding->m_NodesAttackTick > Server()->Tick())
			return;
		CBuilding *pOther = nullptr;
		for(int i = 0; i < m_Runtime.Buildings().Count(Team); ++i)
		{
			CBuilding *pCandidate = m_Runtime.Buildings().At(Team, i);
			if(pCandidate != pBuilding && pCandidate && pCandidate->NodesType() == NODES_TELEPORT &&
				pCandidate->NodesAlive() && pCandidate->NodesPower())
			{
				pOther = pCandidate;
				break;
			}
		}
		if(!pOther)
			return;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			CCharacter *pCharacter = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pCharacter || pPlayer->GetTeam() != Team || distance(pCharacter->m_Pos, pBuilding->m_Pos) > 40.0f)
				continue;
			pCharacter->Teleport(pOther->m_Pos - vec2(0.0f, 48.0f));
			pBuilding->m_NodesAttackTick = Server()->Tick() + Server()->TickSpeed() * 2;
			pOther->m_NodesAttackTick = Server()->Tick() + Server()->TickSpeed() * 2;
			GameServer()->CreatePlayerSpawn(pOther->m_Pos);
			GameServer()->CreateSound(pBuilding->m_Pos, SOUND_WEAPON_SPAWN);
			GameServer()->CreateSound(pOther->m_Pos, SOUND_WEAPON_SPAWN);
			break;
		}
		return;
	}
	if(Type == NODES_HEALTH || Type == NODES_ARMOR)
	{
		if(pBuilding->m_NodesAttackTick > Server()->Tick())
			return;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			CCharacter *pCharacter = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pCharacter || pPlayer->GetTeam() != Team || distance(pCharacter->m_Pos, pBuilding->m_Pos) > 36.0f)
				continue;
			const bool Recovered = Type == NODES_HEALTH ? pCharacter->IncreaseHealth(2) : pCharacter->IncreaseArmor(2);
			if(Recovered)
				pBuilding->m_NodesAttackTick = Server()->Tick() + Server()->TickSpeed() / 4;
		}
	}
}

void CGameControllerNodes::DamageBuilding(CBuilding *pBuilding, int Damage, int Owner)
{
	if(!pBuilding || pBuilding->NodesHealth() <= 0 || Damage == 0)
		return;
	if(Damage < 0)
	{
		if(Owner < 0 || Owner >= MAX_CLIENTS || !GameServer()->m_apPlayers[Owner] ||
			GameServer()->m_apPlayers[Owner]->GetTeam() != pBuilding->m_Team)
			return;
		pBuilding->SetNodesHealth(pBuilding->NodesHealth() - Damage);
		pBuilding->m_Life = pBuilding->NodesHealth();
		pBuilding->SetNodesDeconstruction(false);
		if(pBuilding->NodesHealth() >= pBuilding->NodesMaxHealth())
			pBuilding->SetNodesAlive(true);
		return;
	}
	if(Owner >= 0 && Owner < MAX_CLIENTS && GameServer()->m_apPlayers[Owner] &&
		GameServer()->m_apPlayers[Owner]->GetTeam() == pBuilding->m_Team)
		return;
	pBuilding->SetNodesHealth(pBuilding->NodesHealth() - max(1, Damage));
	pBuilding->SetNodesDeconstruction(false);
	if(pBuilding->NodesHealth() <= 0)
		DestroyBuilding(pBuilding);
}

void CGameControllerNodes::DestroyBuilding(CBuilding *pBuilding, bool Init)
{
	if(!pBuilding || pBuilding->m_NodesDestroyed)
		return;
	pBuilding->m_NodesDestroyed = true;
	const int Team = pBuilding->m_Team;
	if(!Init && !pBuilding->NodesFree() && ValidTeam(Team))
		m_Runtime.Economy().Refund(Team, g_aNodesBuildingInfo[pBuilding->NodesType()].m_Price);
	if(ValidTeam(Team))
		m_Runtime.Buildings().Remove(pBuilding, Team);
	GameServer()->m_World.DestroyEntity(pBuilding);
}

bool CGameControllerNodes::IsPowered(const CBuilding *pBuilding) const
{
	if(!pBuilding || !ValidTeam(pBuilding->m_Team))
		return false;
	if(pBuilding->NodesType() == NODES_REACTOR || pBuilding->NodesType() == NODES_SPAWN)
		return pBuilding->NodesAlive();
	if(!HasReactor(pBuilding->m_Team))
		return false;
	for(int i = 0; i < m_Runtime.Buildings().Count(pBuilding->m_Team); ++i)
	{
		const CBuilding *pSource = m_Runtime.Buildings().At(pBuilding->m_Team, i);
		if(!pSource || !pSource->NodesAlive())
			continue;
		const float Range = pSource->NodesType() == NODES_REACTOR ? NODE_POWER_REACTOR_RANGE :
			(pSource->NodesType() == NODES_REPEATER ? NODE_POWER_REPEATER_RANGE : 0.0f);
		if(Range > 0.0f && distance(pSource->m_Pos, pBuilding->m_Pos) <= Range)
			return true;
	}
	return false;
}

bool CGameControllerNodes::HasReactor(int Team) const
{
	if(!ValidTeam(Team))
		return false;
	for(int i = 0; i < m_Runtime.Buildings().Count(Team); ++i)
		if(m_Runtime.Buildings().At(Team, i) && m_Runtime.Buildings().At(Team, i)->NodesType() == NODES_REACTOR &&
			m_Runtime.Buildings().At(Team, i)->NodesAlive())
			return true;
	return false;
}

int CGameControllerNodes::CountAliveSpawns(int Team) const
{
	int Count = 0;
	if(!ValidTeam(Team))
		return Count;
	for(int i = 0; i < m_Runtime.Buildings().Count(Team); ++i)
		if(m_Runtime.Buildings().At(Team, i) && m_Runtime.Buildings().At(Team, i)->NodesType() == NODES_SPAWN &&
			m_Runtime.Buildings().At(Team, i)->NodesAlive() && IsPowered(m_Runtime.Buildings().At(Team, i)))
			++Count;
	return Count;
}

int CGameControllerNodes::CountLivingPlayers(int Team) const
{
	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == Team &&
			GameServer()->m_apPlayers[i]->GetCharacter())
			++Count;
	return Count;
}

bool CGameControllerNodes::FindSpawn(CPlayer *pPlayer, vec2 *pPos)
{
	if(!pPlayer || !pPos || !ValidTeam(pPlayer->GetTeam()))
		return false;
	int Best = -1;
	float BestDistance = 1000000.0f;
	for(int i = 0; i < m_Runtime.Buildings().Count(pPlayer->GetTeam()); ++i)
	{
		CBuilding *pSpawn = m_Runtime.Buildings().At(pPlayer->GetTeam(), i);
		if(!pSpawn || pSpawn->NodesType() != NODES_SPAWN || !pSpawn->NodesAlive() || !IsPowered(pSpawn))
			continue;
		const float Distance = distance(pPlayer->m_ViewPos, pSpawn->m_Pos);
		if(Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = i;
		}
	}
	if(Best < 0)
		return false;
	*pPos = m_Runtime.Buildings().At(pPlayer->GetTeam(), Best)->m_Pos - vec2(0.0f, 80.0f);
	return true;
}

bool CGameControllerNodes::IsSpawnQueued(const CPlayer *pPlayer) const
{
	return m_Runtime.SpawnQueue().Contains(pPlayer);
}

void CGameControllerNodes::EnterSpawnQueue(CPlayer *pPlayer)
{
	if(!pPlayer || !ValidTeam(pPlayer->GetTeam()) || IsSpawnQueued(pPlayer))
		return;
	const int Team = pPlayer->GetTeam();
	const int ReadyTick = NextSpawnTick(Team);
	if(m_Runtime.SpawnQueue().Enqueue(Team, pPlayer, ReadyTick))
		pPlayer->m_RespawnTick = ReadyTick;
}

void CGameControllerNodes::LeaveSpawnQueue(CPlayer *pPlayer)
{
	m_Runtime.SpawnQueue().Remove(pPlayer);
}

int CGameControllerNodes::SpawnIntervalTicks(int Team) const
{
	const int BaseDelay = max(1, g_Config.m_SvNodesSpawnDelay);
	return max(1, (HasReactor(Team) ? BaseDelay : BaseDelay * 2) * Server()->TickSpeed());
}

int CGameControllerNodes::NextSpawnTick(int Team) const
{
	const int Interval = SpawnIntervalTicks(Team);
	const int Now = Server()->Tick();
	int ReadyTick = (Now / Interval + 1) * Interval;
	const CNodesSpawnQueue &Queue = m_Runtime.SpawnQueue();
	if(const CNodesSpawnQueue::CEntry *pTail = Queue.At(Team, Queue.Count(Team) - 1))
		ReadyTick = max(ReadyTick, pTail->m_ReadyTick + Interval);
	return ReadyTick;
}

bool CGameControllerNodes::CanSpawn(int Team, vec2 *pPos, bool IsBot)
{
	if(IsBot || !ValidTeam(Team))
		return false;
	CPlayer *pPlayer = nullptr;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == Team &&
			!GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->IsSpawning())
			pPlayer = GameServer()->m_apPlayers[i];
	if(!pPlayer)
		return false;
	return CanSpawnPlayer(pPlayer, pPos, IsBot);
}

bool CGameControllerNodes::CanSpawnPlayer(CPlayer *pPlayer, vec2 *pPos, bool IsBot)
{
	if(IsBot || !pPlayer || !ValidTeam(pPlayer->GetTeam()) || !pPlayer->IsSpawning())
		return false;
	const int Team = pPlayer->GetTeam();
	if(!IsSpawnQueued(pPlayer))
	{
		EnterSpawnQueue(pPlayer);
		return false;
	}
	const CNodesSpawnQueue::CEntry *pFront = m_Runtime.SpawnQueue().At(Team, 0);
	if(!pFront || pFront->m_pPlayer != pPlayer || pFront->m_ReadyTick > Server()->Tick())
		return false;
	if(!FindSpawn(pPlayer, pPos))
		return false;
	LeaveSpawnQueue(pPlayer);
	return true;
}

bool CGameControllerNodes::CanCharacterSpawn(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && GameServer()->m_apPlayers[ClientID] &&
		ValidTeam(GameServer()->m_apPlayers[ClientID]->GetTeam());
}

void CGameControllerNodes::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	(void)RequestAI;
	IGameController::OnCharacterSpawn(pChr);
	pChr->SetArmor(0);
}

int CGameControllerNodes::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);
	if(pVictim && pKiller && pKiller != pVictim->GetPlayer() && ValidTeam(pKiller->GetTeam()) &&
		pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam())
	{
		const int Team = pKiller->GetTeam();
		m_aTeamscore[Team] = m_Runtime.Economy().TeamKills(Team) + 1;
		UpdateTechLevel(pKiller->GetTeam());
	}
	if(pVictim)
		EnterSpawnQueue(pVictim->GetPlayer());
	return 0;
}

void CGameControllerNodes::UpdateTechLevel(int Team)
{
	if(!ValidTeam(Team))
		return;
	if(m_Runtime.Economy().RegisterKill(Team, NODE_MAX_TECH_LEVEL))
	{
		GameServer()->SendChat(-1, CHATMODE_TEAM, "Your team reached a new tech level");
	}
}

void CGameControllerNodes::CheckWin()
{
	if(!m_Runtime.IsInitialized() || IsGameOver() || !CountPlayers())
		return;
	const int RedPlayers = CountPlayers(TEAM_RED);
	const int BluePlayers = CountPlayers(TEAM_BLUE);
	if(!NodesWinCheckReady(RedPlayers, BluePlayers))
		return;
	bool aInitialReactor[2] = {false, false};
	for(int i = 0; i < m_Runtime.InitialBuildings().Count(); ++i)
	{
		const CNodesInitialBuilding &Initial = m_Runtime.InitialBuildings().At(i);
		if(Initial.m_Type == NODES_REACTOR && ValidTeam(Initial.m_Team))
			aInitialReactor[Initial.m_Team] = true;
	}
	if(!aInitialReactor[TEAM_RED] || !aInitialReactor[TEAM_BLUE])
		return;
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
		if(CountAliveSpawns(Team) == 0 && CountLivingPlayers(Team) == 0)
		{
			m_aTeamscore[Team ^ 1] = 1;
			m_aTeamscore[Team] = 0;
			m_Runtime.EndRound();
			EndRound();
			return;
		}
}

void CGameControllerNodes::Tick()
{
	IGameController::Tick();
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
		for(int i = 0; i < m_Runtime.SpawnQueue().Count(Team); ++i)
			if(const CNodesSpawnQueue::CEntry *pEntry = m_Runtime.SpawnQueue().At(Team, i))
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Nodes respawn: %ds", max(0, (pEntry->m_ReadyTick - Server()->Tick()) / Server()->TickSpeed()));
				GameServer()->SendBroadcast(aBuf, pEntry->m_pPlayer->GetCID(), true);
			}
	CheckWin();
}

void CGameControllerNodes::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);
	CNetObj_GameData *pData = static_cast<CNetObj_GameData *>(
		Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData)));
	if(!pData)
		return;
	const int Team = SnappingClient >= 0 && SnappingClient < MAX_CLIENTS && GameServer()->m_apPlayers[SnappingClient]
		? GameServer()->m_apPlayers[SnappingClient]->GetTeam()
		: TEAM_RED;
	pData->m_TeamscoreRed = ValidTeam(Team) ? m_Runtime.Economy().BuildPoints(Team) : 0;
	pData->m_TeamscoreBlue = ValidTeam(Team) ? m_Runtime.Economy().TechLevel(Team) : 0;
	pData->m_FlagCarrierRed = FLAG_MISSING;
	pData->m_FlagCarrierBlue = FLAG_MISSING;
}

int CGameControllerNodes::BuildPoints(int Team) const
{
	return m_Runtime.Economy().BuildPoints(Team);
}

int CGameControllerNodes::TechLevel(int Team) const
{
	return m_Runtime.Economy().TechLevel(Team);
}
