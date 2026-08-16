#include "invasion_ai.h"

#include <cmath>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/entities/weapon.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/weapons.h>

namespace
{
float DistanceScore(EInvasionTargetStrategy Strategy, const CCharacter *pCharacter, const vec2 &Pos, int Nearby)
{
	const float Distance = distance(pCharacter->m_Pos, Pos);
	switch(Strategy)
	{
	case INVASION_TARGET_LOW_HEALTH:
		return pCharacter->GetHealth() * 1000.0f + Distance;
	case INVASION_TARGET_CLUSTER:
		return -Nearby * 10000.0f + Distance;
	case INVASION_TARGET_ISOLATED:
		return Nearby * 10000.0f + Distance;
	case INVASION_TARGET_OBJECTIVE:
	case INVASION_TARGET_NEAREST:
	default:
		return Distance;
	}
}
}

CInvasionAI::CInvasionAI(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CAI(pGameServer, pCharacter),
	m_ProfileId(IsValidInvasionSkinProfile(ProfileId) ? ProfileId : INVASION_SKIN_ALIEN1),
	m_pProfile(&InvasionSkinProfile(m_ProfileId)),
	m_Level(max(0, Level)),
	m_StartPos(0, 0),
	m_ShockTimer(0),
	m_NextRepositionTick(0),
	m_StrafeSide(frandom() < 0.5f ? -1 : 1)
{
}

void CInvasionAI::OnCharacterSpawn(CCharacter *pChr)
{
	CAI::OnCharacterSpawn(pChr);

	m_StartPos = pChr->m_Pos;
	m_LastPos = pChr->m_Pos;
	m_TargetPos = pChr->m_Pos;
	m_WaypointDir = vec2(0, 0);
	m_Triggered = Profile().m_StartTriggered;
	m_ShockTimer = Profile().m_ShockTicks;
	m_PowerLevel = Profile().m_PowerLevel;
	m_AttackOnDamage = Profile().m_AttackOnDamage;
	m_TriggerLevel = Profile().m_TriggerLevel + m_Level / 3;
	m_ReactionTime = Profile().m_ReactionTime;
	m_NextRepositionTick = GameServer()->Server()->Tick() + Profile().m_RepositionTicks;

	const int Health = Profile().m_Health + min(m_Level * Profile().m_HealthPerLevel, Profile().m_HealthCap);
	const int Armor = Profile().m_Armor + min(m_Level * Profile().m_ArmorPerLevel, Profile().m_ArmorCap);
	pChr->SetHealth(Health);
	pChr->SetArmor(Armor);

	if(frandom() < 0.4f && pChr->GetPlayer())
		pChr->GetPlayer()->IncreaseGold(4 + (Profile().m_Family == INVASION_FAMILY_PYRO ? 2 : 0));

	// A profile may describe alternatives. Only one primary is equipped so
	// every AI always attacks with the weapon its strategy was tuned for.
	const int FirstChoice = Profile().m_PrimaryCount > 0 ? rand() % Profile().m_PrimaryCount : 0;
	for(int Attempt = 0; Attempt < Profile().m_PrimaryCount; ++Attempt)
	{
		const int Choice = (FirstChoice + Attempt) % Profile().m_PrimaryCount;
		CWeaponSpec Spec = Profile().m_aPrimaryChoices[Choice];
		if(!Spec.IsValid())
			continue;
		if(pChr->GiveWeapon(GameServer()->NewWeapon(Spec)))
			break;
	}

	for(int i = 0; i < Profile().m_UtilityCount; ++i)
	{
		if(Profile().m_aUtilityWeapons[i].IsValid())
			pChr->GiveWeapon(GameServer()->NewWeapon(Profile().m_aUtilityWeapons[i]));
	}

	if(!pChr->GetWeapon())
	{
		// Keep malformed custom content from producing an unarmed invasion bot.
		pChr->GiveWeapon(GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GUN1)));
		dbg_msg("invasion-ai", "profile %d had no usable primary weapon, using gun1", static_cast<int>(m_ProfileId));
	}

	pChr->m_SkipPickups = 999;
}

void CInvasionAI::ReceiveDamage(int CID, int Dmg)
{
	if(CID >= 0 && frandom() < min(1.0f, Dmg * 0.02f))
		m_Triggered = true;

	if(Dmg > 0 && frandom() < min(1.0f, Dmg * 0.03f))
		m_ShockTimer = max(m_ShockTimer, 2 + Dmg / 2);

	if(m_AttackOnDamage)
	{
		m_Attack = 1;
		m_InputChanged = true;
		m_AttackOnDamageTick = GameServer()->Server()->Tick() + GameServer()->Server()->TickSpeed();
	}
}

bool CInvasionAI::SelectProfileTarget()
{
	CCharacter *pBestCharacter = 0;
	float BestScore = 0.0f;
	const float SightRangeSq = 900.0f * 900.0f;
	m_EnemiesInSight = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || IsSelf(pPlayer))
			continue;
		if(pPlayer->GetTeam() == Player()->GetTeam() && GameServer()->m_pController->IsTeamplay())
			continue;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
			continue;
		if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
			continue;

		const vec2 Delta = pCharacter->m_Pos - m_Pos;
		if(dot(Delta, Delta) >= SightRangeSq || !HasLineOfSight(pCharacter))
			continue;

		m_EnemiesInSight++;
		int Nearby = 0;
		if(Profile().m_Targeting == INVASION_TARGET_CLUSTER ||
		   Profile().m_Targeting == INVASION_TARGET_ISOLATED)
		{
			for(int j = 0; j < MAX_CLIENTS; ++j)
			{
				CPlayer *pOther = GameServer()->m_apPlayers[j];
				if(!pOther || IsSelf(pOther) || pOther->m_IsBot)
					continue;
				CCharacter *pOtherCharacter = pOther->GetCharacter();
				if(pOtherCharacter && pOtherCharacter->IsAlive() &&
					distance(pOtherCharacter->m_Pos, pCharacter->m_Pos) < 360)
					Nearby++;
			}
		}

		const float Score = DistanceScore(Profile().m_Targeting, pCharacter, m_Pos, Nearby);
		if(!pBestCharacter || Score < BestScore)
		{
			pBestCharacter = pCharacter;
			BestScore = Score;
		}
	}

	if(!pBestCharacter)
	{
		m_PlayerSpotCount = 0;
		m_pTargetPlayer = 0;
		return false;
	}

	m_pTargetPlayer = pBestCharacter->GetPlayer();
	m_PlayerSpotCount++;
	m_PlayerPos = pBestCharacter->m_Pos;
	m_PlayerDirection = m_PlayerPos - m_Pos;
	m_PlayerDistance = static_cast<int>(distance(m_PlayerPos, m_Pos));
	m_EnemyInLine = abs(pBestCharacter->m_Pos.x - m_Pos.x) < 96 &&
		abs(pBestCharacter->m_Pos.y - m_Pos.y) < 22;
	m_Triggered = true;
	return true;
}

bool CInvasionAI::ShootAtProfileTarget()
{
	const int Range = WeaponShootRange();
	if(!m_pTargetPlayer || !m_pTargetPlayer->GetCharacter() || Range <= 0 || m_PlayerDistance >= Range * 1.2f)
		return false;

	const int Tick = GameServer()->Server()->Tick();
	if(Profile().m_BurstTicks > 0)
	{
		const int Cycle = Profile().m_BurstTicks + 45;
		if(Cycle > 0 && Tick % Cycle >= Profile().m_BurstTicks)
		{
			m_AttackTimer = 0;
			return true;
		}
	}

	const float DispersionScale = max(0.4f, 3.765f - m_PowerLevel * 0.188f);
	vec2 Direction = m_PlayerDirection;
	if(m_PlayerDistance > 0)
	{
		const float t = m_DispersionTick * 0.1f;
		Direction += vec2(11 * cos(t) - 6 * cos(11.0f / 6 * t),
			11 * sin(t) - 6 * sin(11.0f / 6 * t)) * DispersionScale * m_PlayerDistance * 0.0025f;
	}

	m_Direction = Direction;
	m_Hook = 0;
	if(m_PlayerDistance < Range && m_AttackTimer++ > max(0, 20 - m_PowerLevel))
	{
		m_Attack = 1;
		return true;
	}
	return true;
}

void CInvasionAI::SetProfileTargetPosition(bool HasTarget, bool Shooting)
{
	if(!HasTarget)
	{
		if(!m_Triggered)
			m_TargetPos = m_StartPos;
		else if(Profile().m_Movement == INVASION_MOVE_AMBUSH)
			m_TargetPos = m_StartPos;
		else if(SeekClosestEnemy())
			m_TargetPos = m_PlayerPos;
		return;
	}

	vec2 ToTarget = m_PlayerPos - m_Pos;
	const float TargetDistance = length(ToTarget);
	if(TargetDistance <= 0.01f)
		return;
	const vec2 Direction = ToTarget / TargetDistance;
	const vec2 Side(-Direction.y, Direction.x);
	const int Range = max(80, Profile().m_PreferredRange);
	const int Retreat = max(60, Profile().m_RetreatRange);

	switch(Profile().m_Movement)
	{
	case INVASION_MOVE_RUSH:
		m_TargetPos = m_PlayerPos;
		break;
	case INVASION_MOVE_KITE:
		if(TargetDistance < Retreat)
			m_TargetPos = m_Pos - Direction * (Retreat + 180.0f);
		else
			m_TargetPos = m_PlayerPos - Direction * Range;
		break;
	case INVASION_MOVE_STRAFE:
		if(GameServer()->Server()->Tick() >= m_NextRepositionTick)
		{
			m_StrafeSide = -m_StrafeSide;
			m_NextRepositionTick = GameServer()->Server()->Tick() + Profile().m_RepositionTicks;
		}
		m_TargetPos = m_PlayerPos - Direction * Range + Side * (180.0f * m_StrafeSide);
		break;
	case INVASION_MOVE_FLANK:
		if(GameServer()->Server()->Tick() >= m_NextRepositionTick)
		{
			m_StrafeSide = -m_StrafeSide;
			m_NextRepositionTick = GameServer()->Server()->Tick() + Profile().m_RepositionTicks;
		}
		m_TargetPos = m_PlayerPos + Side * (260.0f * m_StrafeSide);
		break;
	case INVASION_MOVE_SIEGE:
	case INVASION_MOVE_HOLD_RANGE:
		if(TargetDistance < Retreat)
			m_TargetPos = m_Pos - Direction * (Retreat + 120.0f);
		else
			m_TargetPos = m_PlayerPos - Direction * Range;
		break;
	case INVASION_MOVE_AMBUSH:
		if(Shooting)
			m_TargetPos = m_PlayerPos - Direction * Range;
		else
			m_TargetPos = m_StartPos;
		break;
	}

	GameServer()->Collision()->IntersectLine(m_Pos, m_TargetPos, 0x0, &m_TargetPos);
}

void CInvasionAI::MoveWithProfileTarget()
{
	if(abs(m_Pos.x - m_TargetPos.x) < 40 && abs(m_Pos.y - m_TargetPos.y) < 40)
	{
		m_Move = 0;
		m_Jump = 0;
		m_Hook = 0;
		return;
	}

	if(GameServer()->Server()->Tick() < m_DontMoveTick)
	{
		m_Move = 0;
		m_Jump = 0;
		m_Hook = 0;
		return;
	}

	if(UpdateWaypoint())
		MoveTowardsWaypoint();
	else
	{
		m_WaypointPos = m_TargetPos;
		MoveTowardsWaypoint(true);
	}
}

void CInvasionAI::ApplyFamilyTactics(bool HasTarget, bool Shooting)
{
	if(!HasTarget)
		return;

	// Small family hooks keep the shared strategies readable while preserving
	// recognizable movement signatures for each group.
	if(Profile().m_Family == INVASION_FAMILY_BUNNY && !Shooting && Player()->GetCharacter()->IsGrounded())
		m_Jump = 1;
	else if(Profile().m_Family == INVASION_FAMILY_CYBORG && Shooting &&
		(Profile().m_Movement == INVASION_MOVE_SIEGE || Profile().m_Movement == INVASION_MOVE_HOLD_RANGE))
		m_Move = 0;
}

void CInvasionAI::RunProfileBehavior()
{
	m_Attack = 0;
	m_Jump = 0;
	m_Hook = 0;

	if(m_ShockTimer > 0)
	{
		--m_ShockTimer;
		m_Move = 0;
		m_ReactionTime = 1 + rand() % 3;
		return;
	}

	HeadToMovingDirection();
	const bool HasTarget = SelectProfileTarget();
	if(!HasTarget && Profile().m_Targeting == INVASION_TARGET_OBJECTIVE)
		ShootAtClosestBuilding(true);
	if(!HasTarget && Profile().m_Movement == INVASION_MOVE_SIEGE)
		ShootAtClosestBuilding();
	if(!HasTarget)
		ShootAtBlocks();

	const bool Shooting = HasTarget && ShootAtProfileTarget();
	if(HasTarget)
		ReactToPlayer();
	SetProfileTargetPosition(HasTarget, Shooting);
	ApplyFamilyTactics(HasTarget, Shooting);
	MoveWithProfileTarget();

	if(Player()->GetCharacter())
		Player()->GetCharacter()->m_SkipPickups = 999;
	RandomlyStopShooting();

	if(m_AttackOnDamageTick > GameServer()->Server()->Tick())
		m_Attack = 1;

	m_ReactionTime = Profile().m_ReactionTime + rand() % 2;
}
