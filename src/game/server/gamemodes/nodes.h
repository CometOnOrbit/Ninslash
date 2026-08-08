#ifndef GAME_SERVER_GAMEMODES_NODES_H
#define GAME_SERVER_GAMEMODES_NODES_H

#include <game/nodes.h>
#include <game/weapon_catalog.h>
#include <game/server/gamecontroller.h>
#include <game/server/entities/building.h>
#include <game/server/nodes/nodes_runtime.h>

class CGameControllerNodes : public IGameController
{
public:
	CGameControllerNodes(class CGameContext *pGameServer);

	bool IsNodes() const override { return true; }
	void Tick() override;
	void Snap(int SnappingClient) override;
	void PostReset() override;
	bool OnEntity(int Index, vec2 Pos) override;
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source) override;
	bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false) override;
	bool CanSpawnPlayer(class CPlayer *pPlayer, vec2 *pPos, bool IsBot = false) override;
	bool CanCharacterSpawn(int ClientID) override;
	bool BuildNodes(vec2 Pos, int Type, int Owner) override;

	void TickBuilding(CBuilding *pBuilding);
	bool PrepareNodesTurret(CBuilding *pBuilding);
	void DamageBuilding(CBuilding *pBuilding, int Damage, int Owner);
	void DestroyBuilding(CBuilding *pBuilding, bool Init = false);

	int BuildPoints(int Team) const;
	int TechLevel(int Team) const;
	bool IsSpawnQueued(const class CPlayer *pPlayer) const;
	bool IsPowered(const CBuilding *pBuilding) const;
	void EnterSpawnQueue(class CPlayer *pPlayer);
	void LeaveSpawnQueue(class CPlayer *pPlayer);

private:
	CNodesRuntime m_Runtime;

	void ResetNodes();
	bool FindSpawn(class CPlayer *pPlayer, vec2 *pPos);
	bool HasReactor(int Team) const;
	int CountAliveSpawns(int Team) const;
	int CountLivingPlayers(int Team) const;
	void UpdateTechLevel(int Team);
	void CheckWin();
	bool AlignBuildingToGround(vec2 Pos, const CNodesBuildingInfo &Info, vec2 *pAlignedPos) const;
	bool ValidBuildPosition(vec2 Pos, const CNodesBuildingInfo &Info) const;
	bool BuildNodesForTeam(vec2 Pos, int Type, int Team, int Owner);
	int SpawnIntervalTicks(int Team) const;
	int NextSpawnTick(int Team) const;
};

#endif
