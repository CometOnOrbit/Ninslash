#ifndef GAME_SERVER_NODES_NODES_RUNTIME_H
#define GAME_SERVER_NODES_NODES_RUNTIME_H

#include <array>

#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <game/nodes.h>

class CBuilding;
class CPlayer;

struct CNodesInitialBuilding
{
	vec2 m_Pos;
	int m_Team;
	int m_Type;
};

class CNodesInitialBuildingRegistry
{
	std::array<CNodesInitialBuilding, NODES_MAX_BUILDINGS> m_aEntries{};
	int m_Count = 0;

public:
	void Reset();
	bool Add(vec2 Pos, int Team, int Type);
	void SortForBootstrap();

	int Count() const { return m_Count; }
	const CNodesInitialBuilding &At(int Index) const { return m_aEntries[Index]; }
};

class CNodesBuildingRegistry
{
	std::array<std::array<CBuilding *, NODES_MAX_BUILDINGS>, 2> m_apBuildings{};
	std::array<int, 2> m_aCount{};

public:
	void Reset();
	bool Add(CBuilding *pBuilding, int Team);
	bool Remove(CBuilding *pBuilding, int Team);

	int Count(int Team) const;
	CBuilding *At(int Team, int Index) const;
	CBuilding *const *Data(int Team) const;
	CBuilding **Data(int Team);
};

class CNodesEconomy
{
	std::array<int, 2> m_aBuildPoints{};
	std::array<int, 2> m_aTechLevel{};
	std::array<int, 2> m_aTeamKills{};

public:
	void Reset(int StartBuildPoints);

	int BuildPoints(int Team) const;
	int TechLevel(int Team) const;
	int TeamKills(int Team) const;

	bool Spend(int Team, int Amount);
	void Refund(int Team, int Amount);
	bool RegisterKill(int Team, int MaxTechLevel);
};

class CNodesSpawnQueue
{
public:
	struct CEntry
	{
		CPlayer *m_pPlayer = nullptr;
		int m_ReadyTick = 0;
	};

private:
	std::array<std::array<CEntry, NODES_MAX_SPAWN_QUEUE>, 2> m_aEntries{};
	std::array<int, 2> m_aCount{};

public:
	void Reset();
	bool Contains(const CPlayer *pPlayer) const;
	bool Enqueue(int Team, CPlayer *pPlayer, int ReadyTick);
	bool Remove(const CPlayer *pPlayer);

	int Count(int Team) const;
	const CEntry *At(int Team, int Index) const;
	CEntry *At(int Team, int Index);
};

class CNodesBuildCooldowns
{
	std::array<int, MAX_CLIENTS> m_aReadyTick{};

public:
	void Reset();
	bool Ready(int ClientID, int Now) const;
	void SetReadyTick(int ClientID, int ReadyTick);
};

class CNodesRuntime
{
public:
	enum class EState
	{
		BOOTSTRAP,
		ACTIVE,
		ROUND_OVER,
	};

private:
	CNodesInitialBuildingRegistry m_InitialBuildings;
	CNodesBuildingRegistry m_Buildings;
	CNodesEconomy m_Economy;
	CNodesSpawnQueue m_SpawnQueue;
	CNodesBuildCooldowns m_BuildCooldowns;
	EState m_State = EState::BOOTSTRAP;

public:
	void Reset(int StartBuildPoints);
	void PrepareBootstrap();
	void Activate();
	void EndRound();

	bool IsInitialized() const { return m_State != EState::BOOTSTRAP; }
	EState State() const { return m_State; }

	CNodesInitialBuildingRegistry &InitialBuildings() { return m_InitialBuildings; }
	const CNodesInitialBuildingRegistry &InitialBuildings() const { return m_InitialBuildings; }
	CNodesBuildingRegistry &Buildings() { return m_Buildings; }
	const CNodesBuildingRegistry &Buildings() const { return m_Buildings; }
	CNodesEconomy &Economy() { return m_Economy; }
	const CNodesEconomy &Economy() const { return m_Economy; }
	CNodesSpawnQueue &SpawnQueue() { return m_SpawnQueue; }
	const CNodesSpawnQueue &SpawnQueue() const { return m_SpawnQueue; }
	CNodesBuildCooldowns &BuildCooldowns() { return m_BuildCooldowns; }
	const CNodesBuildCooldowns &BuildCooldowns() const { return m_BuildCooldowns; }
};

#endif
