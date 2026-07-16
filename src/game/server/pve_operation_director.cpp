#include <engine/shared/config.h>
#include <game/pve_roguelite.h>
#include <game/server/entities/character.h>
#include <game/server/entities/droid.h>
#include <game/server/entities/pve_operation_target.h>
#include <game/server/entities/pve_operation_hazard.h>
#include <game/server/bosspool.h>
#include <game/server/gamecontroller.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>

#include "pve_operation_director.h"

CPveOperationDirector::CPveOperationDirector(CGameContext *pGameServer) :
	m_pGameServer(pGameServer), m_pTarget(0), m_NumCandidates(0), m_Operation(-1), m_Stage(0),
	m_Progress(0), m_Required(0), m_StageStartTick(0), m_Running(false), m_Complete(false),
	m_TargetType(PVE_OPERATION_TARGET_NONE), m_TargetPos(0, 0), m_EndTick(0), m_LastSyncTick(0)
	, m_LastAdvanceTick(-1), m_ModeFallback(false), m_pStageBoss(0), m_NumOwnedEntities(0),
	m_ModeEventSatisfied(false), m_StageSecondary(false), m_NextReinforcementTick(0)
{
	mem_zero(m_apOwnedEntities, sizeof(m_apOwnedEntities));
}

CPveOperationDirector::~CPveOperationDirector()
{
	Clear();
}

void CPveOperationDirector::AddCandidate(vec2 Pos)
{
	if(m_NumCandidates < (int)(sizeof(m_aCandidates) / sizeof(m_aCandidates[0])))
		m_aCandidates[m_NumCandidates++] = Pos;
}

CPveOperationDirector::EStageKind CPveOperationDirector::StageKind() const
{
	const CPveOperationDef *pDef = PveOperationDef(m_Operation);
	const int Type = pDef ? pDef->m_aTargetTypes[clamp(m_Stage, 0, 2)] : PVE_OPERATION_TARGET_NONE;
	// Siege Route's final compound step temporarily reuses the defense-area
	// target type, but it is still a completion-on-occupy area stage.
	if(m_Operation == PVE_OPERATION_SIEGE_ROUTE && m_Stage == 2 && m_StageSecondary && Type == PVE_OPERATION_TARGET_DEFENSE_AREA)
		return STAGE_AREA;
	if(Type == PVE_OPERATION_TARGET_BOSS)
		return STAGE_KILLS;
	if(Type == PVE_OPERATION_TARGET_EVACUATION)
		return STAGE_EVACUATE;
	if(Type == PVE_OPERATION_TARGET_DEFENSE_AREA || Type == PVE_OPERATION_TARGET_NONE)
		return STAGE_WAVES;
	return STAGE_AREA;
}

int CPveOperationDirector::StageRequirement() const
{
	const CPveOperationDef *pDef = PveOperationDef(m_Operation);
	const int Defined = pDef ? max(1, pDef->m_aStepTargets[clamp(m_Stage, 0, 2)]) : 1;
	switch(StageKind())
	{
	case STAGE_KILLS:
	case STAGE_WAVES:
	case STAGE_SWITCHES: return Defined;
	case STAGE_EVACUATE:
	{
		return 1;
	}
	case STAGE_TIMER: return m_pGameServer->Server()->TickSpeed() * (10 + m_Stage * 5);
	case STAGE_AREA: return m_pGameServer->Server()->TickSpeed() * 3;
	}
	return 1;
}

void CPveOperationDirector::TrackEntity(CEntity *pEntity)
{
	if(pEntity && m_NumOwnedEntities < (int)(sizeof(m_apOwnedEntities) / sizeof(m_apOwnedEntities[0])))
		m_apOwnedEntities[m_NumOwnedEntities++] = pEntity;
}

bool CPveOperationDirector::OwnedEntityAlive(CEntity *pEntity, int Type) const
{
	if(!pEntity)
		return false;
	const int FirstType = Type >= 0 ? Type : 0;
	const int LastType = Type >= 0 ? Type : CGameWorld::NUM_ENTTYPES - 1;
	for(int EntityType = FirstType; EntityType <= LastType; EntityType++)
		for(CEntity *pCurrent = m_pGameServer->m_World.FindFirst(EntityType); pCurrent; pCurrent = pCurrent->TypeNext())
			if(pCurrent == pEntity)
			{
				CDroid *pDroid = EntityType == CGameWorld::ENTTYPE_DROID ? static_cast<CDroid *>(pCurrent) : 0;
				return !pDroid || pDroid->m_Health > 0;
			}
	return false;
}

bool CPveOperationDirector::AnyOwnedDroidAlive(bool IncludeBoss) const
{
	for(int i = 0; i < m_NumOwnedEntities; i++)
	{
		if(!IncludeBoss && m_apOwnedEntities[i] == m_pStageBoss)
			continue;
		if(OwnedEntityAlive(m_apOwnedEntities[i], CGameWorld::ENTTYPE_DROID))
			return true;
	}
	return false;
}

void CPveOperationDirector::DestroyOwnedEntities()
{
	for(int EntityType = 0; EntityType < CGameWorld::NUM_ENTTYPES; EntityType++)
		for(CEntity *pEntity = m_pGameServer->m_World.FindFirst(EntityType); pEntity; pEntity = pEntity->TypeNext())
			for(int i = 0; i < m_NumOwnedEntities; i++)
				if(pEntity == m_apOwnedEntities[i])
				{
					m_pGameServer->m_World.DestroyEntity(pEntity);
					break;
				}
	mem_zero(m_apOwnedEntities, sizeof(m_apOwnedEntities));
	m_NumOwnedEntities = 0;
	m_pStageBoss = 0;
}

bool CPveOperationDirector::SnapToGround(vec2 *pPos) const
{
	if(!pPos)
		return false;
	// Drop from above the candidate onto the first floor/platform so cargo and
	// relays sit on walkable tiles instead of floating at enemy-spawn height.
	const vec2 Start = *pPos + vec2(0.0f, -96.0f);
	const vec2 End = *pPos + vec2(0.0f, 520.0f);
	vec2 Hit;
	if(!m_pGameServer->Collision()->IntersectLine(Start, End, 0x0, &Hit, false, true))
		return false;
	*pPos = Hit + vec2(0.0f, -28.0f);
	return true;
}

bool CPveOperationDirector::ValidTargetPosition(vec2 Pos) const
{
	if(m_pGameServer->Collision()->TestBox(Pos, vec2(40.0f, 52.0f)))
		return false;
	if(m_pGameServer->Collision()->GetCollisionAt(Pos.x, Pos.y + 40.0f) & CCollision::COLFLAG_DEATH)
		return false;
	// Require solid footing under the building so cores/nodes are not mid-air.
	const bool Floor =
		m_pGameServer->Collision()->CheckPoint(Pos.x, Pos.y + 34.0f) ||
		m_pGameServer->Collision()->CheckPoint(Pos.x - 14.0f, Pos.y + 34.0f) ||
		m_pGameServer->Collision()->CheckPoint(Pos.x + 14.0f, Pos.y + 34.0f);
	return Floor;
}

float CPveOperationDirector::NearestHumanDistance(vec2 Pos) const
{
	float NearestHuman = 1000000.0f;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		CCharacter *pCharacter = m_pGameServer->GetPlayerChar(ClientID);
		if(pCharacter && !pCharacter->m_IsBot)
			NearestHuman = min(NearestHuman, distance(Pos, pCharacter->m_Pos));
	}
	return NearestHuman;
}

bool CPveOperationDirector::FindTargetPosition(vec2 *pOut, const char **ppSource)
{
	const bool ClusterFollowUps = m_Progress > 0 && length(m_TargetPos) > 1.0f;
	const vec2 Anchor = m_TargetPos;
	int Best = -1;
	float BestScore = -1.0e9f;
	for(int Attempt = 0; Attempt < m_NumCandidates; Attempt++)
	{
		const int Index = (m_Stage * 37 + Attempt * 17 + m_Progress * 13) % m_NumCandidates;
		vec2 Pos = m_aCandidates[Index];
		if(!SnapToGround(&Pos) || !ValidTargetPosition(Pos))
			continue;
		if(ClusterFollowUps && distance(Pos, Anchor) < 140.0f)
			continue;

		const float NearestHuman = NearestHumanDistance(Pos);
		float Score = 0.0f;
		// Prefer a reachable mid-range fight, not the farthest spawn on the map.
		if(NearestHuman < 220.0f)
			Score -= 8000.0f;
		else if(NearestHuman < 450.0f)
			Score += NearestHuman;
		else if(NearestHuman < 950.0f)
			Score += 700.0f - (NearestHuman - 450.0f) * 0.35f;
		else
			Score += 400.0f - (NearestHuman - 950.0f) * 0.55f;

		if(ClusterFollowUps)
		{
			const float ToAnchor = distance(Pos, Anchor);
			if(ToAnchor < 720.0f)
				Score += 900.0f - ToAnchor * 0.45f;
			else
				Score -= (ToAnchor - 720.0f) * 1.1f;
		}

		if(Score > BestScore)
		{
			BestScore = Score;
			Best = Index;
			*pOut = Pos;
		}
	}
	if(Best >= 0)
	{
		*ppSource = ClusterFollowUps ? "cluster-follow-up" : "enemy-spawn";
		return true;
	}
	*pOut = vec2(4000.0f + m_Stage * 96.0f, 4000.0f);
	*ppSource = "safe-platform-anchor";
	return SnapToGround(pOut) && ValidTargetPosition(*pOut);
}

bool CPveOperationDirector::FindDeliveryPosition(vec2 Source, vec2 *pOut) const
{
	int Best = -1;
	float BestScore = -1.0e9f;
	vec2 BestPos(0, 0);
	for(int i = 0; i < m_NumCandidates; i++)
	{
		vec2 Pos = m_aCandidates[i];
		if(!SnapToGround(&Pos) || !ValidTargetPosition(Pos))
			continue;
		const float Dist = distance(Source, Pos);
		if(Dist < 280.0f)
			continue;
		// Delivery should feel like a short carry, not a second map traversal.
		float Score = Dist < 850.0f ? Dist : 850.0f - (Dist - 850.0f) * 0.9f;
		const float NearestHuman = NearestHumanDistance(Pos);
		if(NearestHuman < 180.0f)
			Score -= 1500.0f;
		if(Score > BestScore)
		{
			BestScore = Score;
			Best = i;
			BestPos = Pos;
		}
	}
	if(Best < 0)
		return false;
	*pOut = BestPos;
	return true;
}

void CPveOperationDirector::SpawnStageThreats(vec2 Pos)
{
	int Specialist = -1;
	if(m_Operation == PVE_OPERATION_SIEGE_LINE)
		Specialist = m_Stage == 0 ? DROIDTYPE_BULWARK : DROIDTYPE_RAILGUNNER;
	else if(m_Operation == PVE_OPERATION_ASSEMBLY_SURGE)
		Specialist = DROIDTYPE_ASSEMBLER;
	else if(m_Operation == PVE_OPERATION_GRID_STORM)
		Specialist = DROIDTYPE_SABOTEUR;
	else if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE)
		Specialist = DROIDTYPE_RAILGUNNER;
	else if(m_Operation == PVE_OPERATION_LOCKDOWN_BREAK)
		Specialist = DROIDTYPE_BULWARK;
	if(Specialist >= 0 && m_TargetType != PVE_OPERATION_TARGET_BOSS)
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos, Specialist));

	// Fire-Control's middle stage is a recognisable mixed elite force rather
	// than an ordinary wave with a numeric modifier.
	if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE && m_Stage == 1)
	{
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-100, 0), DROIDTYPE_RAILGUNNER));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(100, 0), DROIDTYPE_SABOTEUR));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(0, -40), DROIDTYPE_BULWARK));
	}
	if(m_Operation == PVE_OPERATION_SIEGE_LINE && m_Stage == 1)
	{
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-90, 0), DROIDTYPE_RAILGUNNER));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(90, 0), DROIDTYPE_SABOTEUR));
	}
	if(m_Operation == PVE_OPERATION_ASSEMBLY_SURGE && m_Stage == 2)
	{
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-110, 0), DROIDTYPE_ASSEMBLER));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(110, 0), DROIDTYPE_SABOTEUR));
	}
	if(m_Operation == PVE_OPERATION_SIEGE_ROUTE && m_Stage == 0)
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(100, 0), DROIDTYPE_RAILGUNNER));
	// Timed terminal/upload stages must create an actual attack to defend
	// against. These entities are director-owned and disappear with the stage.
	if(m_TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL)
	{
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-180, -20), DROIDTYPE_SABOTEUR));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(180, -20), DROIDTYPE_RAILGUNNER));
	}
	else if(m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT)
	{
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-170, -20), DROIDTYPE_ASSEMBLER));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(170, -20), DROIDTYPE_SABOTEUR));
	}
	else if(m_TargetType == PVE_OPERATION_TARGET_SHIELD_RELAY || m_TargetType == PVE_OPERATION_TARGET_ASSEMBLY_NODE ||
		m_TargetType == PVE_OPERATION_TARGET_TARGETING_BEACON || m_TargetType == PVE_OPERATION_TARGET_SHIELD_NODE ||
		m_TargetType == PVE_OPERATION_TARGET_COOLANT_CORE || m_TargetType == PVE_OPERATION_TARGET_DATA_CORE ||
		m_TargetType == PVE_OPERATION_TARGET_ENERGY_CORE)
	{
		// Destroy/carry steps previously spawned no escorts, so the floor felt
		// like empty sightseeing between objectives.
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-150, -16), DROIDTYPE_SABOTEUR));
		TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(150, -16), DROIDTYPE_RAILGUNNER));
		if(m_TargetType == PVE_OPERATION_TARGET_ASSEMBLY_NODE || m_TargetType == PVE_OPERATION_TARGET_COOLANT_CORE)
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(0, -48), DROIDTYPE_BULWARK));
	}
	if(m_TargetType == PVE_OPERATION_TARGET_BOSS)
	{
		const int Boss = (m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE || m_Operation == PVE_OPERATION_SIEGE_LINE || m_Operation == PVE_OPERATION_SIEGE_ROUTE) ? DROIDTYPE_SIEGE_ENGINE : DROIDTYPE_OVERSEER_CORE;
		m_pStageBoss = SpawnBoss(&m_pGameServer->m_World, Pos, 10, Boss);
		TrackEntity(m_pStageBoss);
	}
	if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE && m_Stage == 0)
		TrackEntity(new CPveOperationHazard(&m_pGameServer->m_World, Pos, CPveOperationHazard::BOMBARDMENT, m_pGameServer->Server()->TickSpeed() * 180));
	if(m_Operation == PVE_OPERATION_GRID_STORM && m_Stage == 2)
		TrackEntity(new CPveOperationHazard(&m_pGameServer->m_World, Pos, CPveOperationHazard::ROTATING_EMP, m_pGameServer->Server()->TickSpeed() * 180));
}

void CPveOperationDirector::Diagnose(const char *pReason, const char *pFallback) const
{
	dbg_msg("pve-operation", "operation=%d stage=%d target placement: %s; fallback=%s candidates=%d",
		m_Operation, m_Stage + 1, pReason, pFallback, m_NumCandidates);
}

void CPveOperationDirector::FallbackToMode(const char *pReason)
{
	const int FailedOperation = m_Operation;
	Diagnose(pReason, "mode-objective-flow");
	Clear();
	if(m_pGameServer->m_pPveDirector)
		m_pGameServer->m_pPveDirector->OnOperationChainFailed(FailedOperation);
}

void CPveOperationDirector::BeginStage(bool ResetProgress)
{
	const CPveOperationDef *pDef = PveOperationDef(m_Operation);
	if(ResetProgress)
		m_Progress = 0;
	m_Required = StageRequirement();
	m_StageStartTick = m_pGameServer->Server()->Tick();
	m_TargetType = pDef ? pDef->m_aTargetTypes[m_Stage] : PVE_OPERATION_TARGET_NONE;
	m_TargetPos = vec2(0, 0);
	m_EndTick = 0;
	m_ModeFallback = false;
	m_ModeEventSatisfied = false;
	if(ResetProgress)
		m_StageSecondary = false;
	m_NextReinforcementTick = m_StageStartTick + m_pGameServer->Server()->TickSpeed() * 5;
	ClearTarget();
	DestroyOwnedEntities();
	// Production Halt's final step is a native exit. Open the door here so the
	// route quest "Reach the exit" is actually completable.
	if(m_Operation == PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2)
	{
		vec2 ExitPos;
		if(!m_pGameServer->m_pController->FindEscape(&ExitPos))
		{
			FallbackToMode("escape door missing");
			return;
		}
		m_TargetPos = ExitPos;
		m_Required = 1;
		m_pGameServer->m_pController->TriggerEscape(&ExitPos);
		m_pGameServer->m_pController->SpawnOperationOrdinaryEnemies(m_Stage, 8);
	}
	else if(StageKind() == STAGE_AREA || m_TargetType == PVE_OPERATION_TARGET_DEFENSE_AREA)
	{
		vec2 Pos;
		const char *pSource = "none";
		if(FindTargetPosition(&Pos, &pSource))
		{
			if(str_comp(pSource, "distant-enemy-spawn") != 0)
				Diagnose("no valid map candidate", pSource);
			m_TargetPos = Pos;
			const bool DefenseMarker = m_TargetType == PVE_OPERATION_TARGET_DEFENSE_AREA;
			const bool Carry = m_TargetType == PVE_OPERATION_TARGET_COOLANT_CORE || m_TargetType == PVE_OPERATION_TARGET_DATA_CORE || m_TargetType == PVE_OPERATION_TARGET_ENERGY_CORE;
			const bool TimedHold = m_TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL || m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT;
			vec2 DeliveryPos = Pos;
			if(Carry && !FindDeliveryPosition(Pos, &DeliveryPos))
			{
				FallbackToMode("cargo delivery platform missing");
				return;
			}
			const int HoldTicks = TimedHold ? m_pGameServer->Server()->TickSpeed() * max(1, pDef->m_aStepTargets[m_Stage]) : m_pGameServer->Server()->TickSpeed() * 2;
			if(TimedHold)
				m_EndTick = m_StageStartTick + HoldTicks;
			m_Required = TimedHold ? HoldTicks : max(1, pDef->m_aStepTargets[m_Stage]);
			m_pTarget = new CPveOperationTarget(&m_pGameServer->m_World, this, Pos, DeliveryPos, 120.0f,
				DefenseMarker ? m_pGameServer->Server()->TickSpeed() * 60 * 60 : HoldTicks, m_TargetType);
			SpawnStageThreats(Pos);
		}
		else
		{
			FallbackToMode("all placement strategies rejected");
			return;
		}
	}
	else
	{
		vec2 ThreatPos;
		const char *pSource = "none";
		if(FindTargetPosition(&ThreatPos, &pSource))
		{
			m_TargetPos = ThreatPos;
			SpawnStageThreats(ThreatPos);
		}
		else
		{
			FallbackToMode("threat placement rejected");
			return;
		}
	}
	// Keep ordinary Invasion bots flowing so destroy/carry steps are fights,
	// not empty scavenger hunts between objective markers.
	if(StageKind() == STAGE_AREA || StageKind() == STAGE_EVACUATE || StageKind() == STAGE_WAVES)
		m_pGameServer->m_pController->SpawnOperationOrdinaryEnemies(m_Stage, 6 + m_Stage * 2);
	dbg_msg("pve-operation", "operation=%d stage=%d/3 kind=%d required=%d", m_Operation, m_Stage + 1, (int)StageKind(), m_Required);
	SendState();
}

void CPveOperationDirector::CompleteStage()
{
	if(m_LastAdvanceTick == m_pGameServer->Server()->Tick())
		return;
	m_LastAdvanceTick = m_pGameServer->Server()->Tick();
	ClearTarget();
	DestroyOwnedEntities();
	if(++m_Stage >= 3)
	{
		m_Running = false;
		m_Complete = true;
		m_pGameServer->m_pController->ClearOperationOrdinaryEnemies();
		dbg_msg("pve-operation", "operation=%d chain complete", m_Operation);
		SendState();
		if(m_pGameServer->m_pPveDirector)
			m_pGameServer->m_pPveDirector->OnOperationChainFinished(m_Operation);
		return;
	}
	BeginStage();
}

void CPveOperationDirector::ClearTarget()
{
	if(!m_pTarget)
		return;
	m_pTarget->DetachDirector();
	m_pGameServer->m_World.DestroyEntity(m_pTarget);
	m_pTarget = 0;
}

void CPveOperationDirector::Start(int Operation)
{
	if(Operation < 0 || Operation >= NUM_PVE_OPERATIONS)
		return;
	ClearTarget();
	m_Operation = Operation;
	m_Stage = 0;
	m_Running = true;
	m_Complete = false;
	BeginStage();
}

void CPveOperationDirector::Tick()
{
	if(!m_Running)
		return;
	m_pGameServer->m_pController->TickOperationOrdinaryEnemies();
	TickStageSemantics();
	if(!m_Running)
		return;
	if(!m_ModeFallback && (StageKind() == STAGE_TIMER || (StageKind() == STAGE_AREA && !m_pTarget)) &&
		m_pGameServer->Server()->Tick() - m_StageStartTick >= m_Required)
		CompleteStage();
	else if(m_pGameServer->Server()->Tick() >= m_LastSyncTick)
	{
		m_LastSyncTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed();
		SendState();
	}
}

void CPveOperationDirector::TickStageSemantics()
{
	// Boss stages are bound to the exact instance spawned by this director.
	// Generic mode boss events are only compatibility hints and cannot finish
	// the stage while this instance is alive.
	if(m_TargetType == PVE_OPERATION_TARGET_BOSS && m_pStageBoss && !OwnedEntityAlive(m_pStageBoss, CGameWorld::ENTTYPE_DROID))
	{
		m_pStageBoss = 0;
		CompleteStage();
		return;
	}

	// Fire-Control replaces the ordinary Invasion quest. Its own elite group is
	// authoritative and does not wait for a legacy wave-complete signal.
	if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE && m_Stage == 1 && !AnyOwnedDroidAlive(false))
	{
		CompleteStage();
		return;
	}

	// A timed defense remains contested for its full duration. Replenish a
	// bounded pair only after the previous attackers have been cleared.
	const bool DefendTerminal = m_TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL || m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT;
	if(DefendTerminal && m_pTarget)
	{
		m_Progress = m_pTarget->ProgressTicks();
		m_Required = max(1, m_pTarget->RequiredTicks());
		// This is an occupancy timer, not an unconditional wall-clock deadline.
		// Keep the HUD countdown tied to actual defended progress so leaving the
		// terminal cannot show zero while the stage is still incomplete.
		m_EndTick = m_pGameServer->Server()->Tick() + max(0, m_Required - m_Progress);
	}
	if(DefendTerminal && m_pTarget && m_pGameServer->Server()->Tick() >= m_NextReinforcementTick)
	{
		if(!AnyOwnedDroidAlive(false))
		{
			const vec2 Pos = m_pTarget->HudTargetPos();
			const int First = m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT ? DROIDTYPE_ASSEMBLER : DROIDTYPE_SABOTEUR;
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-190, -32), First));
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(190, -32), DROIDTYPE_RAILGUNNER));
		}
		m_NextReinforcementTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 6;
	}
	else if(!DefendTerminal && StageKind() == STAGE_AREA && m_pTarget &&
		m_pGameServer->Server()->Tick() >= m_NextReinforcementTick)
	{
		if(!AnyOwnedDroidAlive(false))
		{
			const vec2 Pos = m_pTarget->HudTargetPos();
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(-160, -24), DROIDTYPE_SABOTEUR));
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos + vec2(160, -24), DROIDTYPE_RAILGUNNER));
		}
		m_NextReinforcementTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 7;
	}

	// Extraction pressure is stage-specific and bounded. These reinforcements
	// belong to this director and are therefore safe to remove on completion.
	const bool CorePressure = m_Operation == PVE_OPERATION_CORE_RECOVERY && m_Stage == 2;
	const bool ShieldPressure = m_Operation == PVE_OPERATION_LOCKDOWN_BREAK && m_Stage == 2;
	if((CorePressure || ShieldPressure) && m_pGameServer->Server()->Tick() >= m_NextReinforcementTick)
	{
		vec2 Pos;
		const char *pSource = "none";
		if(FindTargetPosition(&Pos, &pSource))
		{
			const int Type = ShieldPressure ? DROIDTYPE_BULWARK : ((m_Progress & 1) ? DROIDTYPE_RAILGUNNER : DROIDTYPE_SABOTEUR);
			TrackEntity(SpawnSpecialist(&m_pGameServer->m_World, Pos, Type));
		}
		m_NextReinforcementTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 7;
	}
}

void CPveOperationDirector::OnEvent(EEvent Event, int Amount)
{
	if(!m_Running || Amount <= 0)
		return;
	if(m_ModeFallback)
	{
		const CPveOperationDef *pDef = PveOperationDef(m_Operation);
		const bool ModeAdvanced = pDef && ((pDef->m_Mode == PVE_MODE_INVASION && Event == EVENT_WAVE) ||
			(pDef->m_Mode == PVE_MODE_HORDE && Event == EVENT_WAVE) ||
			(pDef->m_Mode == PVE_MODE_EXTRACTION && (Event == EVENT_SWITCH || Event == EVENT_EVACUATE)));
		if(ModeAdvanced)
			CompleteStage();
		return;
	}
	const EStageKind Kind = StageKind();
	if(Kind == STAGE_KILLS && Event == EVENT_BOSS)
	{
		m_ModeEventSatisfied = true;
		if(!m_pStageBoss || !OwnedEntityAlive(m_pStageBoss, CGameWorld::ENTTYPE_DROID))
			CompleteStage();
		return;
	}
	if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE && m_Stage == 1 && Event == EVENT_WAVE)
	{
		m_ModeEventSatisfied = true;
		if(!AnyOwnedDroidAlive(false))
			CompleteStage();
		return;
	}
	if((Kind == STAGE_WAVES && Event == EVENT_WAVE) ||
		(Kind == STAGE_SWITCHES && Event == EVENT_SWITCH) || (Kind == STAGE_EVACUATE && Event == EVENT_EVACUATE))
	{
		m_Progress += Amount;
		SendState();
		if(m_Progress >= m_Required)
			CompleteStage();
	}
}

void CPveOperationDirector::OnTargetCompleted(CPveOperationTarget *pTarget)
{
	if(m_Running && pTarget == m_pTarget && StageKind() == STAGE_AREA)
	{
		const bool TimedHold = m_TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL || m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT;
		pTarget->DeactivateRadar();
		m_pTarget = 0;
		pTarget->DetachDirector();
		m_pGameServer->m_World.DestroyEntity(pTarget);
		// Siege Route's last step is deliberately compound: deliver the
		// engine core, then hold its delivery/extraction platform.
		if(m_Operation == PVE_OPERATION_SIEGE_ROUTE && m_Stage == 2 && !m_StageSecondary)
		{
			m_StageSecondary = true;
			vec2 HoldPos;
			if(!FindDeliveryPosition(m_TargetPos, &HoldPos))
			{
				FallbackToMode("energy-core hold platform missing");
				return;
			}
			m_TargetType = PVE_OPERATION_TARGET_DEFENSE_AREA;
			m_TargetPos = HoldPos;
			m_Required = 1;
			m_EndTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 20;
			m_pTarget = new CPveOperationTarget(&m_pGameServer->m_World, this, HoldPos, HoldPos, 140.0f,
				m_pGameServer->Server()->TickSpeed() * 20, PVE_OPERATION_TARGET_DEFENSE_AREA);
			SendState();
			return;
		}
		if(TimedHold)
		{
			m_Progress = m_Required;
			CompleteStage();
			return;
		}
		m_Progress++;
		if(m_Progress >= m_Required)
			CompleteStage();
		else
			BeginStage(false);
	}
}

void CPveOperationDirector::SendState(int ClientID)
{
	CNetMsg_Sv_PveOperationState Msg;
	Msg.m_Operation = m_Operation;
	Msg.m_State = m_Running ? PVE_OPERATION_STATE_ACTIVE : (m_Complete ? PVE_OPERATION_STATE_NONE : PVE_OPERATION_STATE_FAILED);
	Msg.m_Step = m_Running ? m_Stage : -1;
	Msg.m_Progress = m_Progress;
	Msg.m_Target = m_Required;
	Msg.m_EndTick = m_EndTick;
	Msg.m_TargetType = m_TargetType;
	const vec2 HudTarget = m_pTarget ? m_pTarget->HudTargetPos() : m_TargetPos;
	Msg.m_TargetX = (int)HudTarget.x;
	Msg.m_TargetY = (int)HudTarget.y;
	Msg.m_CargoCarrier = m_pTarget ? m_pTarget->CarrierCID() : -1;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CPveOperationDirector::Clear()
{
	ClearTarget();
	DestroyOwnedEntities();
	m_pGameServer->m_pController->ClearOperationOrdinaryEnemies();
	m_Running = false;
	m_Complete = false;
	m_Operation = -1;
	m_Stage = 0;
	m_TargetType = PVE_OPERATION_TARGET_NONE;
	m_TargetPos = vec2(0, 0);
	m_EndTick = 0;
	m_Progress = 0;
	m_Required = 0;
	m_ModeFallback = false;
	m_ModeEventSatisfied = false;
}
