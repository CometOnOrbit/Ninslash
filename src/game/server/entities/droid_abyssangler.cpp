#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/abyss_angler.h>
#include <game/pve/pve_environment.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include "character.h"
#include "droid_abyssangler.h"

// Hull only, and no wider than the box TryBossLanding clears; fins and tail are hit circles.
const vec2 s_AnglerBox(96.0f, 88.0f);

CAbyssAngler::CAbyssAngler(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_BOSSANGLER)
{
	m_StartPos = Pos;
	for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
		m_aBubbleID[i] = -1;
	Reset();
	GameWorld()->InsertEntity(this);
}

CAbyssAngler::~CAbyssAngler()
{
	ClearBubbles();
}

void CAbyssAngler::Reset()
{
	m_Center = vec2(8, -15);
	m_Health = ANGLER_HEALTH;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	for(int i = 0; i < NUM_ANGLER_PARTS; i++)
	{
		m_aPartMax[i] = (int)(m_MaxHealth * s_aAnglerPartShare[i]);
		m_aPartHealth[i] = m_aPartMax[i];
	}
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(0);
	m_TargetIndex = -1;
	m_ReloadTimer = Server()->TickSpeed();
	m_AttackTick = Server()->Tick();
	m_TargetTimer = 0;
	m_Target = vec2(0, 0);
	m_NewTarget = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_FlyTargetTick = 0;
	m_Mode = 0;
	m_ProximityRadius = 90.0f;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Act = ANGLER_ACT_IDLE;
	m_ActTick = Server()->Tick();
	m_Events = 0;
	m_Phase = 0;
	m_Stagger = 0;
	m_ActsDone = 0;
	m_LureCooldown = 0;
	m_SpitCooldown = 0;
	m_DiveCooldown = 0;
	m_DiveAim = vec2(0, 1);
	ClearBubbles();
	m_Anim = DROIDANIM_IDLE;
}

void CAbyssAngler::StartAct(int Act)
{
	m_Act = Act;
	m_ActTick = Server()->Tick();
	m_AttackTick = m_ActTick;
	m_Events = 0;
	m_Anim = AnglerAct(Act).m_Anim;
	if(Act == ANGLER_ACT_DEATH)
		m_Status = DROIDSTATUS_TERMINATED;
}

bool CAbyssAngler::Dark()
{
	return GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->EnvironmentPhase() == PVE_ENV_PHASE_DARK;
}

// Gills flare while it recovers from a lure chomp or a stagger, and through the tide's recovery.
bool CAbyssAngler::GillsOpen()
{
	const int Elapsed = Server()->Tick() - m_ActTick;
	if(m_Act == ANGLER_ACT_STAGGER || (m_Act == ANGLER_ACT_LURE && Elapsed >= AnglerLureChompTick()))
		return true;
	return GameServer()->m_pPveDirector &&
		   GameServer()->m_pPveDirector->EnvironmentPhase() == PVE_ENV_PHASE_RECOVERY;
}

bool CAbyssAngler::Suction()
{
	const int Elapsed = Server()->Tick() - m_ActTick;
	return m_Act == ANGLER_ACT_LURE && Elapsed >= AnglerLureStartTick() && Elapsed < AnglerLureChompTick();
}

vec2 CAbyssAngler::TargetPos()
{
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
	if(pTarget && pTarget->IsAlive())
		return pTarget->m_Pos;
	return m_Pos + m_Target;
}

bool CAbyssAngler::AcquireTarget(bool NeedSight)
{
	m_TargetIndex = -1;
	CCharacter *pClosest = 0;
	int Closest = 0;
	const float RangeX = NeedSight ? 1100.0f : 1400.0f;
	const float RangeY = NeedSight ? 700.0f : 900.0f;
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible() || pCharacter->m_IsBot)
			continue;
		if(abs(m_Pos.x - pCharacter->m_Pos.x) >= RangeX || abs(m_Pos.y - pCharacter->m_Pos.y) >= RangeY)
			continue;
		if(NeedSight && GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos))
			continue;
		const int Distance = distance(pCharacter->m_Pos, m_Pos);
		if(!pClosest || Distance < Closest)
		{
			pClosest = pCharacter;
			Closest = Distance;
			m_TargetIndex = i;
		}
	}
	if(!pClosest)
		return false;
	m_Target = pClosest->m_Pos - m_Pos;
	// Hysteresis: hovering right above the player must not flip it every tick.
	if(abs(m_Target.x) > 40.0f)
		m_Dir = m_Target.x >= 0.0f ? 1 : -1;
	return true;
}

void CAbyssAngler::HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, bool Front, float Slow)
{
	CCharacter *apEnts[MAX_CHARACTERS];
	int Num = GameServer()->m_World.FindEntities(
		Pos, Radius, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apEnts[i];
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		if(Front && (pChr->m_Pos.x - m_Pos.x) * m_Dir < 0.0f)
			continue;
		vec2 Diff = pChr->m_Pos - Pos;
		const float Len = length(Diff);
		vec2 Force = vec2((float)m_Dir * Knock, -Knock * 0.25f);
		if(Len > 0.01f)
			Force = Diff / Len * Knock;
		pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), Dmg, Force, pChr->m_Pos);
		if(Slow > 0.0f)
			pChr->Slow(Slow);
	}
	GameServer()->CreateBuildingHit(Pos);
}

void CAbyssAngler::FireBubble(vec2 Dir)
{
	int Slot = -1;
	for(int i = 0; i < ANGLER_MAX_BUBBLES && Slot < 0; i++)
		if(m_aBubbleID[i] < 0)
			Slot = i;
	if(Slot < 0)
		return;
	m_aBubbleID[Slot] = Server()->SnapNewID();
	m_aBubblePos[Slot] = Mouth();
	m_aBubbleVel[Slot] = Dir * 14.0f;
	m_aBubbleLife[Slot] = 0;
	m_aBubbleTarget[Slot] = m_TargetIndex;
}

void CAbyssAngler::PopBubble(int i, bool Shot)
{
	const vec2 Pos = m_aBubblePos[i];
	Server()->SnapFreeID(m_aBubbleID[i]);
	m_aBubbleID[i] = -1;
	GameServer()->CreateEffect(FX_SHIELDHIT, Pos);
	GameServer()->CreateSound(Pos, SOUND_SHIELD_HIT);
	if(!Shot)
		HurtPlayers(Pos, 72.0f, ANGLER_BUBBLE_DAMAGE, 5.0f, false, 1.5f);
}

void CAbyssAngler::ClearBubbles()
{
	for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
	{
		if(m_aBubbleID[i] >= 0)
			Server()->SnapFreeID(m_aBubbleID[i]);
		m_aBubbleID[i] = -1;
	}
}

// Homing shots: a short coast out of the mouth, then they turn onto the player.
void CAbyssAngler::TickBubbles()
{
	CCollision *pCollision = GameServer()->Collision();
	const float Speed = 11.0f + m_Phase * 2.0f;
	for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
	{
		if(m_aBubbleID[i] < 0)
			continue;
		vec2 &Vel = m_aBubbleVel[i];
		CCharacter *pTarget = GameServer()->GetPlayerChar(m_aBubbleTarget[i]);
		if(++m_aBubbleLife[i] < 8)
			Vel *= 0.98f;
		else if(pTarget && pTarget->IsAlive())
		{
			const vec2 Want = pTarget->m_Pos + vec2(0, -20) - m_aBubblePos[i];
			if(length(Want) > 1.0f)
				Vel += (normalize(Want) * Speed - Vel) * 0.12f;
		}
		else
			Vel.y -= 0.08f;

		const vec2 Next = m_aBubblePos[i] + Vel;
		if(pCollision->FastIntersectLine(m_aBubblePos[i], Next) || m_aBubbleLife[i] > WARDEN_TICK_SPEED * 7)
		{
			PopBubble(i, false);
			continue;
		}
		m_aBubblePos[i] = Next;
		CCharacter *pNear = GameServer()->m_World.ClosestCharacter(Next, 34.0f, 0);
		if(pNear && !pNear->m_IsBot)
			PopBubble(i, false);
	}
}

void CAbyssAngler::BreakPart(int Part)
{
	m_aPartHealth[Part] = 0;
	m_Stagger = 1;
	vec2 At = m_Pos;
	for(int i = 0; i < NUM_ANGLER_HIT; i++)
		if(s_aAnglerHit[i].m_Part == Part)
			At = m_Pos + AnglerHitPos(i, m_Dir);
	GameServer()->CreateExplosion(At, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	if(Part == ANGLER_PART_LURE && m_Act == ANGLER_ACT_LURE)
		StartAct(ANGLER_ACT_STAGGER);
}

void CAbyssAngler::TickAct()
{
	const int Elapsed = Server()->Tick() - m_ActTick;
	const float Tide = Dark() ? 1.3f : 1.0f;
	if(m_Act == ANGLER_ACT_BITE)
	{
		if(!(m_Events & 1) && Elapsed >= AnglerBiteLungeTick())
		{
			m_Events |= 1;
			vec2 Aim = TargetPos() - Mouth();
			if(length(Aim) < 1.0f)
				Aim = vec2((float)m_Dir, 0.0f);
			m_Dir = Aim.x >= 0.0f ? 1 : -1;
			m_Vel = normalize(Aim) * 30.0f * (PartAlive(ANGLER_PART_FINS) ? 1.0f : 0.6f) * (Dark() ? 1.15f : 1.0f);
			GameServer()->CreateSound(m_Pos, SOUND_DASH);
		}
		if((m_Events & 1) && !(m_Events & 2))
		{
			CCharacter *pNear = GameServer()->m_World.ClosestCharacter(Mouth(), 64.0f, 0);
			if(Elapsed >= AnglerBiteChompTick() || (pNear && !pNear->m_IsBot))
			{
				m_Events |= 2;
				HurtPlayers(Mouth(), 100.0f, PartAlive(ANGLER_PART_JAW) ? ANGLER_BITE_DAMAGE : ANGLER_BITE_DAMAGE / 2, 10.0f, false);
				GameServer()->CreateSound(Mouth(), SOUND_HAMMER_HIT);
				m_Vel *= 0.3f;
			}
		}
	}
	else if(m_Act == ANGLER_ACT_LURE)
	{
		if(Suction())
		{
			// Recoil decays by 0.6 per tick, so a steady pull F settles near 1.75F of velocity a tick.
			const vec2 MouthPos = Mouth();
			for(int i = 0; i < MAX_CHARACTERS; i++)
			{
				CCharacter *pChr = GameServer()->GetPlayerChar(i);
				if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
					continue;
				const vec2 Diff = MouthPos - pChr->m_Pos;
				const float Dist = length(Diff);
				if(Dist < 1.0f || Dist > ANGLER_LURE_RANGE || GameServer()->Collision()->FastIntersectLine(MouthPos, pChr->m_Pos))
					continue;
				pChr->AddRecoil(Diff / Dist * (0.35f + 0.55f * (1.0f - Dist / ANGLER_LURE_RANGE)) * Tide);
			}
		}
		if(!(m_Events & 1) && Elapsed >= AnglerLureChompTick())
		{
			m_Events |= 1;
			HurtPlayers(Mouth(), 120.0f, PartAlive(ANGLER_PART_JAW) ? ANGLER_CHOMP_DAMAGE : ANGLER_CHOMP_DAMAGE / 2, 14.0f, false);
			GameServer()->CreateSound(Mouth(), SOUND_HAMMER_HIT);
		}
	}
	else if(m_Act == ANGLER_ACT_SPIT)
	{
		for(int Shot = 0; Shot < 3; Shot++)
		{
			if((m_Events & (1 << Shot)) || Elapsed < AnglerSpitTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			vec2 Aim = TargetPos() - Mouth();
			Aim = length(Aim) > 1.0f ? normalize(Aim) : vec2((float)m_Dir, 0.0f);
			const int Count = 1 + m_Phase;
			for(int n = 0; n < Count; n++)
			{
				const float a = (n - (Count - 1) * 0.5f) * 0.4f;
				FireBubble(vec2(Aim.x * cosf(a) - Aim.y * sinf(a), Aim.x * sinf(a) + Aim.y * cosf(a)));
			}
			GameServer()->CreateSound(Mouth(), SOUND_BOUNCER_FIRE);
		}
	}
	else if(m_Act == ANGLER_ACT_DIVE)
	{
		if(Elapsed < AnglerDiveStartTick())
		{
			m_Vel = vec2(m_Vel.x * 0.9f, -5.0f * (1.0f - (float)Elapsed / AnglerDiveStartTick()));
			return;
		}
		if(!(m_Events & 1))
		{
			m_Events |= 1;
			vec2 Aim = TargetPos() - m_Pos;
			Aim = length(Aim) > 1.0f ? normalize(Aim) : vec2(0.0f, 1.0f);
			if(Aim.y < 0.4f)
				Aim = normalize(vec2(Aim.x, 0.4f));
			m_DiveAim = Aim;
			m_Dir = Aim.x >= 0.0f ? 1 : -1;
			m_Vel = Aim * 32.0f * Tide;
			GameServer()->CreateSound(m_Pos, SOUND_WALKER_TAKEOFF);
			return;
		}
		if(!(m_Events & 2) && Elapsed >= AnglerDiveEndTick())
			DiveImpact();
	}
	else if(m_Act == ANGLER_ACT_ROAR && !(m_Events & 1) && Elapsed >= WardenTicks(0.5f))
	{
		m_Events |= 1;
		GameServer()->CreateSound(m_Pos, SOUND_WALKER_TAKEOFF);
		for(int i = 0; i < MAX_CHARACTERS; i++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(i);
			if(!pChr || !pChr->IsAlive() || pChr->m_IsBot || distance(pChr->m_Pos, m_Pos) > 360.0f)
				continue;
			const vec2 Diff = pChr->m_Pos - m_Pos;
			pChr->AddRecoil(length(Diff) > 1.0f ? normalize(Diff) * 6.0f : vec2(0.0f, -6.0f));
		}
	}
}

void CAbyssAngler::DiveImpact()
{
	m_Events |= 2;
	HurtPlayers(m_Pos + vec2(0.0f, 30.0f), 220.0f, ANGLER_DIVE_DAMAGE, 12.0f, false, 1.0f);
	GameServer()->CreateExplosion(m_Pos + vec2(0.0f, 40.0f), CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0.0f);
	m_Vel *= 0.2f;
}

void CAbyssAngler::FinishAct()
{
	static const int s_aReload[3] = {8, 5, 3};
	static const int s_aLure[3] = {140, 100, 70};
	static const int s_aSpit[3] = {55, 40, 28};
	static const int s_aDive[3] = {110, 80, 55};
	const int Act = m_Act;
	if(Act == ANGLER_ACT_BITE || Act == ANGLER_ACT_LURE || Act == ANGLER_ACT_SPIT || Act == ANGLER_ACT_DIVE)
		m_ActsDone++;
	if(Act == ANGLER_ACT_LURE)
		m_LureCooldown = s_aLure[m_Phase];
	if(Act == ANGLER_ACT_SPIT)
		m_SpitCooldown = s_aSpit[m_Phase];
	if(Act == ANGLER_ACT_DIVE)
		m_DiveCooldown = s_aDive[m_Phase];
	m_ReloadTimer = s_aReload[m_Phase];
	StartAct(ANGLER_ACT_SWIM);

	// Late phases chain a second bite when the first leaves the player close.
	if(Act == ANGLER_ACT_BITE && m_Phase >= 1 && AcquireTarget(true) && length(m_Target) < 340.0f && frandom() < 0.7f)
		StartAct(ANGLER_ACT_BITE);
}

void CAbyssAngler::Think()
{
	const int Phase = WardenPhase(m_Health, m_MaxHealth);
	if(Phase > m_Phase)
	{
		m_Phase = Phase;
		m_Stagger = 0;
		StartAct(ANGLER_ACT_ROAR);
		return;
	}
	if(m_Stagger)
	{
		m_Stagger = 0;
		StartAct(ANGLER_ACT_STAGGER);
		return;
	}

	if(m_ReloadTimer > 0)
		m_ReloadTimer -= Dark() ? 2 : 1;
	const bool Seen = AcquireTarget(true);
	if(!Seen && !AcquireTarget(false))
	{
		if(m_Act != ANGLER_ACT_IDLE)
			StartAct(ANGLER_ACT_IDLE);
		return;
	}
	const float Dist = length(m_Target);
	// Kiting is answered with a bubble volley, even mid-reload.
	if(!m_SpitCooldown && Dist > 200.0f)
	{
		StartAct(ANGLER_ACT_SPIT);
		return;
	}
	if(!Seen || m_ReloadTimer > 0)
	{
		if(m_Act != ANGLER_ACT_SWIM)
			StartAct(ANGLER_ACT_SWIM);
		return;
	}

	if(Dist < 300.0f)
		StartAct(ANGLER_ACT_BITE);
	else if(PartAlive(ANGLER_PART_LURE) && !m_LureCooldown && Dist < ANGLER_LURE_RANGE && frandom() < 0.45f)
		StartAct(ANGLER_ACT_LURE);
	else if(PartAlive(ANGLER_PART_FINS) && !m_DiveCooldown && m_Target.y > 40.0f && abs(m_Target.x) < 520.0f)
		StartAct(ANGLER_ACT_DIVE);
	else if(m_Act != ANGLER_ACT_SWIM)
		StartAct(ANGLER_ACT_SWIM);
}

void CAbyssAngler::MoveBody()
{
	CCollision *pCollision = GameServer()->Collision();
	for(int n = 0; n < 6 && pCollision->TestBox(m_Pos, s_AnglerBox); n++)
		m_Pos.y -= 8.0f;

	const bool Lunging = m_Act == ANGLER_ACT_BITE && (m_Events & 1) && !(m_Events & 2);
	const bool Diving = m_Act == ANGLER_ACT_DIVE && (m_Events & 1) && !(m_Events & 2);
	if(m_Act == ANGLER_ACT_DEATH)
	{
		m_Vel.x *= 0.95f;
		m_Vel.y = min(m_Vel.y + 0.2f, 4.0f);
	}
	else if(Diving)
		m_Vel += (m_DiveAim * length(m_Vel) - m_Vel) * 0.2f;
	else if(m_Act == ANGLER_ACT_IDLE || m_Act == ANGLER_ACT_SWIM || m_Act == ANGLER_ACT_SPIT)
	{
		// Close on the player. Only hold a short lead once it is already on top of them.
		float Speed = 13.0f * (Dark() ? 1.25f : 1.0f) * (PartAlive(ANGLER_PART_FINS) ? 1.0f : 0.55f) * (m_Phase >= 2 ? 1.2f : 1.0f);
		if(m_Act == ANGLER_ACT_SPIT)
			Speed *= 0.75f;
		vec2 Want = m_Pos + m_Target;
		if(m_TargetIndex >= 0 && GameServer()->GetPlayerChar(m_TargetIndex))
		{
			const vec2 At = TargetPos();
			const float Gap = distance(m_Pos, At);
			if(Gap < 180.0f)
			{
				Want = At + vec2(-m_Dir * 90.0f, -50.0f);
				if(pCollision->TestBox(Want, s_AnglerBox))
					Want = At + vec2(-m_Dir * 90.0f, 20.0f);
			}
			else
			{
				Want = At + vec2(-m_Dir * 40.0f, -30.0f);
				if(Gap > 420.0f)
					Speed *= 1.35f;
			}
		}
		Want.y += sinf(Server()->Tick() * 0.08f) * 10.0f;
		vec2 Desired = (Want - m_Pos) * 0.14f;
		if(length(Desired) > Speed)
			Desired = normalize(Desired) * Speed;
		m_Vel += (Desired - m_Vel) * 0.18f;
	}
	else if(!Lunging)
		m_Vel *= 0.86f;

	const float Want = length(m_Vel);
	pCollision->MoveBox(&m_Pos, &m_Vel, s_AnglerBox, 0, false);
	if(Diving && length(m_Vel) < Want * 0.5f)
		DiveImpact();
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 70);
}

void CAbyssAngler::TickDeath()
{
	m_Status = DROIDSTATUS_TERMINATED;
	m_Act = ANGLER_ACT_DEATH;
	MoveBody();
	if(Server()->Tick() < m_DeathTick + WardenTicks(AnglerAct(ANGLER_ACT_DEATH).m_Duration))
		return;

	GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true));
	for(int i = 0; i < 4; i++)
	{
		const int Kind = frandom() < 0.34f ? POWERUP_AMMO : (frandom() < 0.5f ? POWERUP_ARMOR : POWERUP_KIT);
		GameServer()->m_pController->DropPickup(
			m_Pos, Kind, vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f), 0);
	}
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_RESPAWNER)));
	GameServer()->m_World.DestroyEntity(this);
}

void CAbyssAngler::Tick()
{
	if(DespawnIfUnsnapped())
		return;

	TickBubbles();
	if(m_Health <= 0)
	{
		if(!m_DeathTick)
			m_DeathTick = Server()->Tick();
		TickDeath();
		return;
	}

	// The dark tide halves every cooldown.
	const int Cool = Dark() ? 2 : 1;
	m_LureCooldown = max(0, m_LureCooldown - Cool);
	m_SpitCooldown = max(0, m_SpitCooldown - Cool);
	m_DiveCooldown = max(0, m_DiveCooldown - Cool);

	if(m_Act != ANGLER_ACT_IDLE && m_Act != ANGLER_ACT_SWIM)
	{
		TickAct();
		if(Server()->Tick() - m_ActTick >= WardenTicks(AnglerAct(m_Act).m_Duration))
			FinishAct();
	}
	else
		Think();

	MoveBody();

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status == DROIDSTATUS_HURT)
		m_Status = DROIDSTATUS_IDLE;
}

void CAbyssAngler::TickPaused()
{
}

void CAbyssAngler::Snap(int SnappingClient)
{
	CDroid::Snap(SnappingClient);

	for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
	{
		if(m_aBubbleID[i] < 0 || NetworkClipped(SnappingClient, m_aBubblePos[i]))
			continue;
		CNetObj_BossShot *pShot = static_cast<CNetObj_BossShot *>(
			Server()->SnapNewItem(NETOBJTYPE_BOSSSHOT, m_aBubbleID[i], sizeof(CNetObj_BossShot)));
		if(!pShot)
			continue;
		pShot->m_X = (int)m_aBubblePos[i].x;
		pShot->m_Y = (int)m_aBubblePos[i].y;
		pShot->m_VelX = (int)(m_aBubbleVel[i].x * 100.0f);
		pShot->m_VelY = (int)(m_aBubbleVel[i].y * 100.0f);
		pShot->m_Kind = 1;
	}

	CPlayer *pPlayer = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	if(pPlayer && distance(pPlayer->m_ViewPos, m_Pos) > 2200.0f)
		return;
	CNetObj_BossStatus *pStatus = static_cast<CNetObj_BossStatus *>(
		Server()->SnapNewItem(NETOBJTYPE_BOSSSTATUS, m_ID, sizeof(CNetObj_BossStatus)));
	if(!pStatus)
		return;
	pStatus->m_Health = max(0, m_Health);
	pStatus->m_MaxHealth = m_MaxHealth;
	pStatus->m_Phase = m_Phase;
	pStatus->m_Part0 = m_MaxHealth > 0 ? clamp(m_Health * 100 / m_MaxHealth, 0, 100) : 0;
	pStatus->m_Part1 = m_aPartMax[ANGLER_PART_LURE] > 0 ? m_aPartHealth[ANGLER_PART_LURE] * 100 / m_aPartMax[ANGLER_PART_LURE] : 0;
	pStatus->m_Part2 = m_aPartMax[ANGLER_PART_FINS] > 0 ? m_aPartHealth[ANGLER_PART_FINS] * 100 / m_aPartMax[ANGLER_PART_FINS] : 0;
	pStatus->m_Part3 = m_aPartMax[ANGLER_PART_JAW] > 0 ? m_aPartHealth[ANGLER_PART_JAW] * 100 / m_aPartMax[ANGLER_PART_JAW] : 0;
	pStatus->m_ArmOut = Suction();
	pStatus->m_ArmX = (int)Mouth().x;
	pStatus->m_ArmY = (int)Mouth().y;
}

static void HitCircle(vec2 Pos0, vec2 Pos1, vec2 Center, float Range, float *pBest, vec2 *pAt)
{
	const vec2 P = closest_point_on_line(Pos0, Pos1, Center);
	if(distance(P, Center) >= Range)
		return;
	const float Along = distance(Pos0, P);
	if(Along < *pBest)
	{
		*pBest = Along;
		*pAt = P;
	}
}

bool CAbyssAngler::HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt)
{
	float Best = 1e9f;
	for(int i = 0; i < NUM_ANGLER_HIT; i++)
		HitCircle(Pos0, Pos1, m_Pos + AnglerHitPos(i, m_Dir), s_aAnglerHit[i].m_R + Radius, &Best, pAt);
	for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
		if(m_aBubbleID[i] >= 0)
			HitCircle(Pos0, Pos1, m_aBubblePos[i], 18.0f + Radius, &Best, pAt);
	return Best < 1e9f;
}

void CAbyssAngler::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || !Dmg || Source.m_Kind == EAttackSourceKind::Droid || IgnoresMapObject(Source))
		return;

	const bool HasPos = Pos.x != 0.0f || Pos.y != 0.0f;
	int Part = ANGLER_PART_CORE;
	if(HasPos)
	{
		for(int i = 0; i < ANGLER_MAX_BUBBLES; i++)
			if(m_aBubbleID[i] >= 0 && distance(Pos, m_aBubblePos[i]) < 30.0f)
			{
				PopBubble(i, true);
				return;
			}
		Part = AnglerPartAt(Pos.x - m_Pos.x, Pos.y - m_Pos.y, m_Dir);
	}
	if(m_Act == ANGLER_ACT_ROAR)
		return;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, true, this);
	Dmg = AnglerIncomingDamage(Dmg, Part, GillsOpen());

	if(Part != ANGLER_PART_CORE && m_aPartHealth[Part] > 0)
	{
		m_aPartHealth[Part] -= Dmg;
		if(m_aPartHealth[Part] <= 0)
			BreakPart(Part);
	}

	vec2 DmgPos = PresentDamage(Combat, Force, Dmg, Pos);
	if(m_Act != ANGLER_ACT_BITE && m_Act != ANGLER_ACT_DIVE)
		m_Vel += Force * 0.25f;
	if(length(m_Vel) > 24.0f)
		m_Vel = normalize(m_Vel) * 24.0f;
	CommitDamage(DmgPos, Dmg, Source);
	m_DamageTakenTick = Server()->Tick();

	if(m_Health <= 0)
	{
		m_DeathTick = Server()->Tick();
		ClearBubbles();
		StartAct(ANGLER_ACT_DEATH);
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		return;
	}

	if((m_Act == ANGLER_ACT_IDLE || m_Act == ANGLER_ACT_SWIM) && frandom() < 0.05f)
		m_Stagger = 1;
}
