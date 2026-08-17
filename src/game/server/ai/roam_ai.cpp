#include <engine/shared/config.h>

#include <game/server/ai.h>
#include <game/server/entities/building.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/roam.h>

#include "roam_ai.h"

CAIroam::CAIroam(CGameContext *pGameServer, CCharacter *pCharacter, int Level) : CAI(pGameServer, pCharacter)
{
	m_Level = clamp(Level, 1, 30);
	m_ShockTimer = 0;
	m_TargetCourseOrdinal = -1;
	m_LastProgressTick = 0;
	m_LastMistakeTick = 0;
	m_HesitateUntil = 0;
	m_RecoveryDirection = 1;
	m_LastProgressPos = vec2(0, 0);
	m_AccelerationHookActive = false;
	m_AccelerationHookStartTick = 0;
	m_AccelerationHookCooldownTick = 0;
	m_AccelerationHookTarget = vec2(0, 0);
}

void CAIroam::OnCharacterSpawn(CCharacter *pChr)
{
	CAI::OnCharacterSpawn(pChr);

	m_WaypointDir = vec2(0, 0);
	m_PowerLevel = clamp(2 + m_Level / 3, 2, 12);
	m_TargetPos = Player()->GetCharacter()->m_Pos;
	m_WaypointPos = m_TargetPos;
	m_TargetCourseOrdinal = -1;
	m_LastProgressTick = GameServer()->Server()->Tick();
	m_LastMistakeTick = m_LastProgressTick;
	m_HesitateUntil = 0;
	m_RecoveryDirection = (Player()->GetCID() & 1) ? -1 : 1;
	m_LastProgressPos = pChr->m_Pos;
	m_ShockTimer = 0;
	m_ReactionTime = clamp(6 - m_Level / 6, 1, 5);
	m_AccelerationHookActive = false;
	m_AccelerationHookStartTick = 0;
	m_AccelerationHookCooldownTick = 0;
	m_AccelerationHookTarget = pChr->m_Pos;
}

void CAIroam::ReceiveDamage(int CID, int Dmg)
{
	(void)CID;
	m_Attack = 0;
	if(Dmg >= 8)
		m_ShockTimer = min(2, Dmg / 8);
}

bool CAIroam::FindAccelerationHookTarget(CCharacter *pCharacter, vec2 Travel, vec2 *pTargetPos) const
{
	if(!pCharacter || !pTargetPos || length(Travel) < 160.0f)
		return false;

	const vec2 Desired = normalize(Travel);
	const vec2 Up(0, -1);
	const vec2 Perpendicular(-Desired.y, Desired.x);
	vec2 aDirections[5] = {
		normalize(Desired + Up * 0.45f),
		normalize(Desired + Up * 0.90f),
		Desired,
		normalize(Desired - Perpendicular * 0.55f + Up * 0.35f),
		normalize(Desired + Perpendicular * 0.55f + Up * 0.35f),
	};
	const int CandidateCount = m_Level >= 20 ? 5 : m_Level >= 10 ? 3 : 2;
	const float HookLength = GameServer()->m_World.m_Core.m_Tuning.m_HookLength;
	float BestScore = -1000000.0f;
	bool Found = false;
	for(int i = 0; i < CandidateCount; i++)
	{
		vec2 HitPos;
		if(!GameServer()->Collision()->IntersectLine(m_Pos, m_Pos + aDirections[i] * HookLength, &HitPos, 0))
			continue;

		const vec2 Delta = HitPos - m_Pos;
		const float Distance = length(Delta);
		if(Distance < 96.0f || Delta.y > 64.0f)
			continue;
		const vec2 PullDirection = Delta / Distance;
		const float Alignment = dot(PullDirection, Desired);
		if(Alignment < 0.34f || dot(Delta, Desired) < 64.0f)
			continue;

		vec2 CharacterHitPos;
		if(GameServer()->m_World.IntersectCharacter(m_Pos, HitPos, 2.0f, CharacterHitPos, pCharacter))
			continue;

		const float Lateral = abs(dot(Delta, Perpendicular));
		const float Score = dot(Delta, Desired) * 1.4f - Lateral * 0.35f - max(0.0f, Delta.y) * 2.0f;
		if(!Found || Score > BestScore)
		{
			Found = true;
			BestScore = Score;
			*pTargetPos = HitPos;
		}
	}
	return Found;
}

void CAIroam::ReleaseAccelerationHook(int Now, int CooldownTicks)
{
	const bool InputChanged = m_AccelerationHookActive || m_Hook != 0;
	m_AccelerationHookActive = false;
	m_Hook = 0;
	if(Now >= 0)
		m_AccelerationHookCooldownTick = max(m_AccelerationHookCooldownTick, Now + max(0, CooldownTicks));
	if(InputChanged)
		m_InputUpdateSkip = 0;
}

void CAIroam::UpdateAccelerationHook(CCharacter *pCharacter, vec2 Travel, bool Suppress)
{
	if(!pCharacter)
		return;

	const int Now = GameServer()->Server()->Tick();
	const int TickSpeed = GameServer()->Server()->TickSpeed();
	const int CooldownTicks = max(1, round_to_int(TickSpeed * (1.4f - (m_Level - 1) * (0.75f / 29.0f))));
	const CCharacterCore &Core = pCharacter->GetCore();
	if(Suppress)
	{
		ReleaseAccelerationHook(Now, TickSpeed / 4);
		return;
	}

	if(m_AccelerationHookActive)
	{
		if(Core.m_HookedPlayer >= 0 || Core.m_HookState == HOOK_RETRACTED ||
		   (Core.m_HookState >= HOOK_RETRACT_START && Core.m_HookState <= HOOK_RETRACT_END))
		{
			ReleaseAccelerationHook(Now, CooldownTicks);
			return;
		}

		const vec2 Desired = length(Travel) > 1.0f ? normalize(Travel) : vec2(0, 0);
		const vec2 Anchor = Core.m_HookState == HOOK_GRABBED ? Core.m_HookPos : m_AccelerationHookTarget;
		const vec2 AnchorDelta = Anchor - m_Pos;
		const float AnchorDistance = length(AnchorDelta);
		const float ForwardSpeed = dot(Core.m_Vel, Desired);
		const int MaximumHoldTicks = max(1, round_to_int(TickSpeed * (0.35f + m_Level * 0.015f)));
		const bool LaunchTimedOut = Core.m_HookState != HOOK_GRABBED && Now > m_AccelerationHookStartTick + max(1, TickSpeed * 3 / 10);
		const bool BadAnchor = AnchorDistance < 72.0f || length(Desired) < 0.5f ||
			dot(AnchorDelta, Desired) < 32.0f ||
			(AnchorDistance > 1.0f && dot(AnchorDelta / AnchorDistance, Desired) < 0.34f);
		if(LaunchTimedOut || BadAnchor || ForwardSpeed >= 12.5f || Now > m_AccelerationHookStartTick + MaximumHoldTicks)
		{
			ReleaseAccelerationHook(Now, CooldownTicks);
			return;
		}

		m_Hook = 1;
		m_Direction = AnchorDelta;
		return;
	}

	m_Hook = 0;
	if(Now < m_AccelerationHookCooldownTick || Core.m_HookState != HOOK_IDLE ||
	   length(Travel) < 160.0f || distance(m_Pos, m_TargetPos) < 192.0f)
		return;

	m_AccelerationHookCooldownTick = Now + CooldownTicks;
	const int AttemptChance = clamp(15 + m_Level * 3, 20, 95);
	if(rand() % 100 >= AttemptChance)
		return;

	vec2 Target;
	if(!FindAccelerationHookTarget(pCharacter, Travel, &Target))
		return;

	m_AccelerationHookActive = true;
	m_AccelerationHookStartTick = Now;
	m_AccelerationHookTarget = Target;
	m_Hook = 1;
	m_Direction = Target - m_Pos;
	m_InputUpdateSkip = 0;
}

void CAIroam::DoBehavior()
{
	m_Attack = 0;
	m_Down = 0;

	CCharacter *pCharacter = Player()->GetCharacter();
	if(!pCharacter)
		return;
	const int Now = GameServer()->Server()->Tick();
	const int TickSpeed = GameServer()->Server()->TickSpeed();
	if(m_ShockTimer > 0 && m_ShockTimer--)
	{
		m_Move = 0;
		m_Jump = 0;
		ReleaseAccelerationHook(Now, TickSpeed / 2);
		return;
	}

	CGameControllerRoam *pRoam = static_cast<CGameControllerRoam *>(GameServer()->m_pController);
	vec2 RaceTarget;
	int CourseOrdinal = -1;
	if(!pRoam->GetRaceTarget(Player()->GetCID(), &RaceTarget, &CourseOrdinal))
	{
		m_Move = 0;
		m_Jump = 0;
		ReleaseAccelerationHook(Now, 0);
		m_TargetPos = m_Pos;
		m_WaypointPos = m_Pos;
		return;
	}

	if(CourseOrdinal != m_TargetCourseOrdinal)
	{
		ReleaseAccelerationHook(Now, TickSpeed / 4);
		m_TargetCourseOrdinal = CourseOrdinal;
		m_LastProgressTick = Now;
		m_LastProgressPos = m_Pos;
		m_WaypointUpdateNeeded = true;
	}
	m_TargetPos = RaceTarget;

	// Low skill racers make short, deterministic-looking mistakes. There is
	// no rubber-banding: higher levels only react more consistently.
	if(Now >= m_LastMistakeTick + TickSpeed)
	{
		m_LastMistakeTick = Now;
		const int MistakeChance = max(0, 18 - m_Level);
		if(MistakeChance > 0 && rand() % 100 < MistakeChance)
		{
			const int DurationMs = 150 + rand() % 251;
			m_HesitateUntil = Now + max(1, TickSpeed * DurationMs / 1000);
		}
	}
	if(Now < m_HesitateUntil)
	{
		m_Move = 0;
		m_Jump = 0;
		ReleaseAccelerationHook(Now, TickSpeed / 4);
		return;
	}

	if(distance(m_Pos, m_LastProgressPos) >= 96.0f)
	{
		m_LastProgressPos = m_Pos;
		m_LastProgressTick = Now;
	}

	if(UpdateWaypoint())
		MoveTowardsWaypoint(false);
	else
	{
		m_WaypointPos = m_TargetPos;
		MoveTowardsWaypoint(!GameServer()->Collision()->FastIntersectLine(m_Pos, m_TargetPos));
	}
	HeadToMovingDirection();

	const vec2 Travel = m_WaypointPos - m_Pos;
	const int TravelDir = Travel.x < -8.0f ? -1 : Travel.x > 8.0f ? 1 : 0;
	if(pCharacter->IsGrounded() && TravelDir != 0)
	{
		const bool WallAhead = GameServer()->Collision()->IsTileSolid(m_Pos.x + TravelDir * 32.0f, m_Pos.y);
		const bool FloorAhead = GameServer()->Collision()->IsTileSolid(m_Pos.x + TravelDir * 64.0f, m_Pos.y + 32.0f);
		if(WallAhead || !FloorAhead)
			m_Jump = 1;
	}
	if(Travel.y < -64.0f)
		m_Jump = 1;
	else if(Travel.y > 96.0f)
		m_Down = 1;

	// Runtime Path hazards are entities rather than collision tiles. Jump a
	// saw on approach and wait for an active wall flame instead of reversing.
	CBuilding *apBuildings[16];
	const int NumBuildings = GameServer()->m_World.FindEntities(
		m_Pos, 240.0f, reinterpret_cast<CEntity **>(apBuildings), 16, CGameWorld::ENTTYPE_BUILDING);
	vec2 Forward(0, 0);
	if(length(Travel) > 1.0f)
		Forward = normalize(Travel);
	else if(length(m_TargetPos - m_Pos) > 1.0f)
		Forward = normalize(m_TargetPos - m_Pos);
	bool SuppressHook = false;
	for(int i = 0; i < NumBuildings; i++)
	{
		CBuilding *pBuilding = apBuildings[i];
		const vec2 Delta = pBuilding->m_Pos - m_Pos;
		if(pBuilding->m_Type == BUILDING_SAWBLADE && dot(Delta, Forward) > 0.0f && distance(pBuilding->m_Pos, m_Pos) < 190.0f)
		{
			m_Jump = 1;
			m_Down = 0;
			SuppressHook = true;
		}
		else if(pBuilding->m_Type == BUILDING_FLAMETRAP && pBuilding->m_aStatus[BSTATUS_FIRE])
		{
			const bool InFront = pBuilding->m_Mirror ?
				(m_Pos.x < pBuilding->m_Pos.x && m_Pos.x > pBuilding->m_Pos.x - 150.0f) :
				(m_Pos.x > pBuilding->m_Pos.x && m_Pos.x < pBuilding->m_Pos.x + 150.0f);
			if(InFront && abs(m_Pos.y - pBuilding->m_Pos.y) < 72.0f)
			{
				m_Move = 0;
				m_Jump = 0;
				SuppressHook = true;
			}
		}
	}

	const int RecoverAfter = TickSpeed * clamp(6 - m_Level / 6, 2, 6);
	const bool Recovering = Now > m_LastProgressTick + RecoverAfter;
	UpdateAccelerationHook(pCharacter, Travel,
		SuppressHook || Recovering || distance(m_Pos, m_TargetPos) < 192.0f);
	if(Recovering)
	{
		m_RecoveryDirection = ((Now / max(1, TickSpeed / 2)) & 1) ? 1 : -1;
		m_Move = TravelDir != 0 ? TravelDir : m_RecoveryDirection;
		m_Jump = 1;
		m_Hook = pCharacter->GetCore().m_JetpackPower > 30 && ((Now / max(1, TickSpeed / 3)) & 1);
		m_Direction = normalize(vec2((float)m_Move, -1.0f));
	}
	if(Now > m_LastProgressTick + RecoverAfter + TickSpeed * 5)
		pCharacter->m_DelayedKill = true;

	Player()->GetCharacter()->m_SkipPickups = 999;
	m_ReactionTime = clamp(6 - m_Level / 6, 1, 5);
}
