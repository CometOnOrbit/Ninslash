#ifndef GAME_SERVER_PVE_OPERATION_DIRECTOR_H
#define GAME_SERVER_PVE_OPERATION_DIRECTOR_H

#include <base/vmath.h>

class CGameContext;
class CEntity;
class CDroid;
class CPveOperationTarget;

// Runtime behavior for the nine operation routes. Shared operation IDs remain
// in pve_roguelite; this class only owns server-side stage behavior.
class CPveOperationDirector
{
public:
	enum EEvent
	{
		EVENT_KILL,
		EVENT_WAVE,
		EVENT_SWITCH,
		EVENT_EVACUATE,
		EVENT_BOSS,
	};
	enum EStageKind
	{
		STAGE_AREA,
		STAGE_KILLS,
		STAGE_WAVES,
		STAGE_SWITCHES,
		STAGE_EVACUATE,
		STAGE_TIMER,
	};

private:
	CGameContext *m_pGameServer;
	CPveOperationTarget *m_pTarget;
	vec2 m_aCandidates[512];
	int m_NumCandidates;
	int m_Operation;
	int m_Stage;
	int m_Progress;
	int m_Required;
	int m_StageStartTick;
	bool m_Running;
	bool m_Complete;
	int m_TargetType;
	vec2 m_TargetPos;
	int m_EndTick;
	int m_LastSyncTick;
	int m_LastAdvanceTick;
	bool m_ModeFallback;
	CDroid *m_pStageBoss;
	CEntity *m_apOwnedEntities[48];
	int m_NumOwnedEntities;
	bool m_ModeEventSatisfied;
	bool m_StageSecondary;
	int m_NextReinforcementTick;

	EStageKind StageKind() const;
	int StageRequirement() const;
	bool FindTargetPosition(vec2 *pOut, const char **ppSource);
	bool FindDeliveryPosition(vec2 Source, vec2 *pOut) const;
	bool SnapToGround(vec2 *pPos) const;
	bool ValidTargetPosition(vec2 Pos) const;
	float NearestHumanDistance(vec2 Pos) const;
	void BeginStage(bool ResetProgress = true);
	void CompleteStage();
	void ClearTarget();
	void Diagnose(const char *pReason, const char *pFallback) const;
	void FallbackToMode(const char *pReason);
	void SendState(int ClientID = -1);
	void SpawnStageThreats(vec2 Pos);
	void TrackEntity(CEntity *pEntity);
	bool OwnedEntityAlive(CEntity *pEntity, int Type = -1) const;
	bool AnyOwnedDroidAlive(bool IncludeBoss = true) const;
	void DestroyOwnedEntities();
	void TickStageSemantics();

public:
	CPveOperationDirector(CGameContext *pGameServer);
	~CPveOperationDirector();
	void AddCandidate(vec2 Pos);
	void Start(int Operation);
	void Tick();
	void OnEvent(EEvent Event, int Amount = 1);
	void OnTargetCompleted(CPveOperationTarget *pTarget);
	void OnCargoStateChanged() { SendState(); }
	void Clear();
	bool Running() const { return m_Running; }
	bool Complete() const { return m_Complete; }
	int Operation() const { return m_Operation; }
	int Stage() const { return m_Stage; }
	int Progress() const { return m_Progress; }
	int Required() const { return m_Required; }
	int TargetType() const { return m_TargetType; }
	void OnClientEnter(int ClientID) { SendState(ClientID); }
};

#endif
