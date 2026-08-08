#include "nodes_runtime.h"

#include <algorithm>

namespace
{
bool ValidTeam(int Team)
{
	return Team == TEAM_RED || Team == TEAM_BLUE;
}
}

void CNodesInitialBuildingRegistry::Reset()
{
	m_Count = 0;
}

bool CNodesInitialBuildingRegistry::Add(vec2 Pos, int Team, int Type)
{
	if(!ValidTeam(Team) || Type < 0 || Type >= NODES_BUILDING_COUNT || m_Count >= NODES_MAX_BUILDINGS)
		return false;
	m_aEntries[m_Count++] = {Pos, Team, Type};
	return true;
}

void CNodesInitialBuildingRegistry::SortForBootstrap()
{
	std::stable_sort(m_aEntries.begin(), m_aEntries.begin() + m_Count, [](const CNodesInitialBuilding &Left, const CNodesInitialBuilding &Right) {
		const bool LeftReactor = Left.m_Type == NODES_REACTOR;
		const bool RightReactor = Right.m_Type == NODES_REACTOR;
		if(LeftReactor != RightReactor)
			return LeftReactor > RightReactor;
		return Left.m_Team < Right.m_Team;
	});
}

void CNodesBuildingRegistry::Reset()
{
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
	{
		m_aCount[Team] = 0;
		m_apBuildings[Team].fill(nullptr);
	}
}

bool CNodesBuildingRegistry::Add(CBuilding *pBuilding, int Team)
{
	if(!pBuilding || !ValidTeam(Team) || m_aCount[Team] >= NODES_MAX_BUILDINGS)
		return false;
	m_apBuildings[Team][m_aCount[Team]++] = pBuilding;
	return true;
}

bool CNodesBuildingRegistry::Remove(CBuilding *pBuilding, int Team)
{
	if(!pBuilding || !ValidTeam(Team))
		return false;
	for(int Index = 0; Index < m_aCount[Team]; ++Index)
	{
		if(m_apBuildings[Team][Index] != pBuilding)
			continue;
		m_apBuildings[Team][Index] = m_apBuildings[Team][m_aCount[Team] - 1];
		m_apBuildings[Team][--m_aCount[Team]] = nullptr;
		return true;
	}
	return false;
}

int CNodesBuildingRegistry::Count(int Team) const
{
	return ValidTeam(Team) ? m_aCount[Team] : 0;
}

CBuilding *CNodesBuildingRegistry::At(int Team, int Index) const
{
	return ValidTeam(Team) && Index >= 0 && Index < m_aCount[Team] ? m_apBuildings[Team][Index] : nullptr;
}

CBuilding *const *CNodesBuildingRegistry::Data(int Team) const
{
	return ValidTeam(Team) ? m_apBuildings[Team].data() : nullptr;
}

CBuilding **CNodesBuildingRegistry::Data(int Team)
{
	return ValidTeam(Team) ? m_apBuildings[Team].data() : nullptr;
}

void CNodesEconomy::Reset(int StartBuildPoints)
{
	m_aBuildPoints.fill(max(0, StartBuildPoints));
	m_aTechLevel.fill(1);
	m_aTeamKills.fill(0);
}

int CNodesEconomy::BuildPoints(int Team) const
{
	return ValidTeam(Team) ? m_aBuildPoints[Team] : 0;
}

int CNodesEconomy::TechLevel(int Team) const
{
	return ValidTeam(Team) ? m_aTechLevel[Team] : 0;
}

int CNodesEconomy::TeamKills(int Team) const
{
	return ValidTeam(Team) ? m_aTeamKills[Team] : 0;
}

bool CNodesEconomy::Spend(int Team, int Amount)
{
	if(!ValidTeam(Team) || Amount < 0 || m_aBuildPoints[Team] < Amount)
		return false;
	m_aBuildPoints[Team] -= Amount;
	return true;
}

void CNodesEconomy::Refund(int Team, int Amount)
{
	if(ValidTeam(Team) && Amount > 0)
		m_aBuildPoints[Team] += Amount;
}

bool CNodesEconomy::RegisterKill(int Team, int MaxTechLevel)
{
	if(!ValidTeam(Team))
		return false;
	++m_aTeamKills[Team];
	const int Required = m_aTechLevel[Team] * m_aTechLevel[Team] * 3;
	if(m_aTeamKills[Team] < Required || m_aTechLevel[Team] >= MaxTechLevel)
		return false;
	++m_aTechLevel[Team];
	return true;
}

void CNodesSpawnQueue::Reset()
{
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
	{
		m_aCount[Team] = 0;
		for(CEntry &Entry : m_aEntries[Team])
			Entry = {};
	}
}

bool CNodesSpawnQueue::Contains(const CPlayer *pPlayer) const
{
	if(!pPlayer)
		return false;
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
		for(int Index = 0; Index < m_aCount[Team]; ++Index)
			if(m_aEntries[Team][Index].m_pPlayer == pPlayer)
				return true;
	return false;
}

bool CNodesSpawnQueue::Enqueue(int Team, CPlayer *pPlayer, int ReadyTick)
{
	if(!ValidTeam(Team) || !pPlayer || Contains(pPlayer) || m_aCount[Team] >= NODES_MAX_SPAWN_QUEUE)
		return false;
	m_aEntries[Team][m_aCount[Team]++] = {pPlayer, ReadyTick};
	return true;
}

bool CNodesSpawnQueue::Remove(const CPlayer *pPlayer)
{
	if(!pPlayer)
		return false;
	for(int Team = TEAM_RED; Team <= TEAM_BLUE; ++Team)
	{
		for(int Index = 0; Index < m_aCount[Team]; ++Index)
		{
			if(m_aEntries[Team][Index].m_pPlayer != pPlayer)
				continue;
			for(int Move = Index; Move + 1 < m_aCount[Team]; ++Move)
				m_aEntries[Team][Move] = m_aEntries[Team][Move + 1];
			m_aEntries[Team][--m_aCount[Team]] = {};
			return true;
		}
	}
	return false;
}

int CNodesSpawnQueue::Count(int Team) const
{
	return ValidTeam(Team) ? m_aCount[Team] : 0;
}

const CNodesSpawnQueue::CEntry *CNodesSpawnQueue::At(int Team, int Index) const
{
	return ValidTeam(Team) && Index >= 0 && Index < m_aCount[Team] ? &m_aEntries[Team][Index] : nullptr;
}

CNodesSpawnQueue::CEntry *CNodesSpawnQueue::At(int Team, int Index)
{
	return ValidTeam(Team) && Index >= 0 && Index < m_aCount[Team] ? &m_aEntries[Team][Index] : nullptr;
}

void CNodesBuildCooldowns::Reset()
{
	m_aReadyTick.fill(0);
}

bool CNodesBuildCooldowns::Ready(int ClientID, int Now) const
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aReadyTick[ClientID] <= Now;
}

void CNodesBuildCooldowns::SetReadyTick(int ClientID, int ReadyTick)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
		m_aReadyTick[ClientID] = ReadyTick;
}

void CNodesRuntime::Reset(int StartBuildPoints)
{
	m_Buildings.Reset();
	m_Economy.Reset(StartBuildPoints);
	m_SpawnQueue.Reset();
	m_BuildCooldowns.Reset();
	m_State = EState::BOOTSTRAP;
}

void CNodesRuntime::PrepareBootstrap()
{
	m_InitialBuildings.SortForBootstrap();
}

void CNodesRuntime::Activate()
{
	m_State = EState::ACTIVE;
}

void CNodesRuntime::EndRound()
{
	m_State = EState::ROUND_OVER;
}
