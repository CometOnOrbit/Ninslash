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

bool CPveOperationDirector::ValidTargetPosition(vec2 Pos) const
{
	return !m_pGameServer->Collision()->TestBox(Pos, vec2(48.0f, 72.0f)) &&
		!(m_pGameServer->Collision()->GetCollisionAt(Pos.x, Pos.y + 48.0f) & CCollision::COLFLAG_DEATH);
}

bool CPveOperationDirector::FindTargetPosition(vec2 *pOut, const char **ppSource)
{
	int Best = -1;
	float BestDistance = -1.0f;
	for(int Attempt = 0; Attempt < m_NumCandidates; Attempt++)
	{
		const int Index = (m_Stage * 37 + Attempt) % m_NumCandidates;
		vec2 Pos = m_aCandidates[Index] + vec2(0.0f, -32.0f);
		if(!ValidTargetPosition(Pos))
			continue;
		float NearestHuman = 1000000.0f;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		{
			CCharacter *pCharacter = m_pGameServer->GetPlayerChar(ClientID);
			if(pCharacter && !pCharacter->m_IsBot)
				NearestHuman = min(NearestHuman, distance(Pos, pCharacter->m_Pos));
		}
		if(NearestHuman > BestDistance)
		{
			BestDistance = NearestHuman;
			Best = Index;
		}
	}
	if(Best >= 0)
	{
		*pOut = m_aCandidates[Best] + vec2(0.0f, -32.0f);
		*ppSource = BestDistance >= 600.0f ? "distant-enemy-spawn" : "enemy-spawn";
		return true;
	}
	*pOut = vec2(4000.0f + m_Stage * 96.0f, 4000.0f);
	*ppSource = "safe-platform-anchor";
	return ValidTargetPosition(*pOut);
}

bool CPveOperationDirector::FindDeliveryPosition(vec2 Source, vec2 *pOut) const
{
	int Best = -1;
	float BestDistance = 600.0f;
	for(int i = 0; i < m_NumCandidates; i++)
	{
		const vec2 Pos = m_aCandidates[i] + vec2(0, -32);
		const float Dist = distance(Source, Pos);
		if(Dist > BestDistance && ValidTargetPosition(Pos))
		{
			Best = i;
			BestDistance = Dist;
		}
	}
	if(Best < 0)
		return false;
	*pOut = m_aCandidates[Best] + vec2(0, -32);
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
	if(m_Operation == PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2)
		m_pGameServer->m_pController->BeginRisingAcid(60);
}

void CPveOperationDirector::Diagnose(const char *pReason, const char *pFallback) const
{
	dbg_msg("pve-operation", "operation=%d stage=%d target placement: %s; fallback=%s candidates=%d",
		m_Operation, m_Stage + 1, pReason, pFallback, m_NumCandidates);
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
	if(StageKind() == STAGE_AREA || m_TargetType == PVE_OPERATION_TARGET_DEFENSE_AREA)
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
			vec2 DeliveryPos = Pos;
			if(Carry && !FindDeliveryPosition(Pos, &DeliveryPos))
			{
				Diagnose("cargo delivery platform missing", "mode-objective-flow");
				m_ModeFallback = true;
				SendState();
				return;
			}
			const bool TimedHold = m_TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL || m_TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT;
			const int HoldTicks = TimedHold ? m_pGameServer->Server()->TickSpeed() * max(1, pDef->m_aStepTargets[m_Stage]) : m_pGameServer->Server()->TickSpeed() * 2;
			if(TimedHold)
				m_EndTick = m_StageStartTick + HoldTicks;
			m_Required = TimedHold ? 1 : max(1, pDef->m_aStepTargets[m_Stage]);
			m_pTarget = new CPveOperationTarget(&m_pGameServer->m_World, this, Pos, DeliveryPos, 120.0f,
				DefenseMarker ? m_pGameServer->Server()->TickSpeed() * 60 * 60 : HoldTicks, m_TargetType);
			SpawnStageThreats(Pos);
		}
		else
		{
			Diagnose("all placement strategies rejected", "mode-objective-flow");
			m_ModeFallback = true;
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
			Diagnose("threat placement rejected", "mode-objective-flow");
			m_ModeFallback = true;
		}
	}
	if(m_Operation == PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2)
		m_EndTick = m_StageStartTick + m_pGameServer->Server()->TickSpeed() * 60;
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
	if(m_Operation == PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2 && m_EndTick > 0 && m_pGameServer->Server()->Tick() >= m_EndTick)
	{
		Diagnose("acid escape deadline expired", "mode failure flow");
		Clear();
		return;
	}
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

	// The explicit Fire-Control elite group needs both the legacy wave signal
	// and elimination of this director's own specialists.
	if(m_Operation == PVE_OPERATION_FIRE_CONTROL_PURGE && m_Stage == 1 && m_ModeEventSatisfied && !AnyOwnedDroidAlive(false))
	{
		CompleteStage();
		return;
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
				Diagnose("energy-core hold platform missing", "mode-objective-flow");
				m_ModeFallback = true;
				SendState();
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
	Msg.m_TargetX = (int)m_TargetPos.x;
	Msg.m_TargetY = (int)m_TargetPos.y;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CPveOperationDirector::Clear()
{
	ClearTarget();
	DestroyOwnedEntities();
	if(m_Operation == PVE_OPERATION_FOUNDRY_SHUTDOWN)
		m_pGameServer->m_pController->ClearRisingAcid();
	m_Running = false;
	m_Complete = false;
	m_Operation = -1;
	m_Stage = 0;
	m_TargetType = PVE_OPERATION_TARGET_NONE;
	m_TargetPos = vec2(0, 0);
	m_EndTick = 0;
}
