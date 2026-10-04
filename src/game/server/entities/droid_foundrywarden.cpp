#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/foundry_warden.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include "character.h"
#include "droid_foundrywarden.h"

// Same box TryBossLanding clears. A taller box starts inside the floor, and MoveBox never unsticks an overlap.
const vec2 s_WardenBox(96.0f, 128.0f);
const vec2 s_GrabBox(28.0f, 28.0f);

CFoundryWarden::CFoundryWarden(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_BOSSWARDEN)
{
	m_ProximityRadius = FoundryWardenPhysSize;
	m_StartPos = Pos;
	for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
		m_aRocketID[i] = -1;
	Reset();
	GameWorld()->InsertEntity(this);
}

CFoundryWarden::~CFoundryWarden()
{
	ReleaseGrab();
	ClearRockets();
}

void CFoundryWarden::Reset()
{
	m_Center = vec2(0, -40);
	m_Health = WARDEN_HEALTH;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	for(int i = 0; i < NUM_WARDEN_PARTS; i++)
	{
		m_aPartMax[i] = (int)(m_MaxHealth * s_aWardenPartShare[i]);
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
	m_ProximityRadius = FoundryWardenPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Act = WARDEN_ACT_IDLE;
	m_ActTick = Server()->Tick();
	m_Events = 0;
	m_Phase = 0;
	m_Stagger = 0;
	m_ActsDone = 0;
	m_GrabCooldown = 0;
	m_RocketCooldown = 0;
	m_ChargeCooldown = 0;
	m_ArmState = ARM_IN;
	m_ArmTip = m_Pos;
	m_ArmVel = vec2(0, 0);
	m_GrabCID = -1;
	m_GrabBreak = 0;
	ClearRockets();
	m_Anim = DROIDANIM_IDLE;
}

void CFoundryWarden::StartAct(int Act)
{
	m_Act = Act;
	m_ActTick = Server()->Tick();
	m_AttackTick = m_ActTick;
	m_Events = 0;
	m_Anim = WardenAct(Act).m_Anim;
	if(Act == WARDEN_ACT_DEATH)
		m_Status = DROIDSTATUS_TERMINATED;
}

void CFoundryWarden::SetLocomotion(int Act)
{
	if(m_Act != Act)
		StartAct(Act);
}

bool CFoundryWarden::AcquireTarget(bool NeedSight)
{
	m_TargetIndex = -1;
	CCharacter *pClosest = 0;
	int Closest = 0;
	const float RangeX = NeedSight ? 1100.0f : 1400.0f;
	const float RangeY = NeedSight ? 520.0f : 700.0f;
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible() || pCharacter->m_IsBot)
			continue;
		if(abs(m_Pos.x - pCharacter->m_Pos.x) >= RangeX || abs(m_Pos.y - pCharacter->m_Pos.y) >= RangeY)
			continue;
		if(NeedSight && GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos + vec2(0, -60)))
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
	m_Dir = m_Target.x >= 0.0f ? 1 : -1;
	return true;
}

void CFoundryWarden::HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, bool Front, float Slow)
{
	CCharacter *apEnts[MAX_CHARACTERS];
	int Num = GameServer()->m_World.FindEntities(
		Pos, Radius, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apEnts[i];
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		if(Front && m_Dir > 0 && pChr->m_Pos.x < m_Pos.x)
			continue;
		if(Front && m_Dir < 0 && pChr->m_Pos.x > m_Pos.x)
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

void CFoundryWarden::FireRocket(float Side)
{
	int Slot = -1;
	for(int i = 0; i < WARDEN_MAX_ROCKETS && Slot < 0; i++)
		if(m_aRocketID[i] < 0)
			Slot = i;
	if(Slot < 0)
		return;
	m_aRocketID[Slot] = Server()->SnapNewID();
	m_aRocketPos[Slot] = m_Pos + vec2(Side * 60.0f, -185.0f);
	m_aRocketVel[Slot] = vec2(Side * 3.0f + frandom() * 2.0f - 1.0f, -12.0f);
	m_aRocketLife[Slot] = 0;
	m_aRocketTarget[Slot] = m_TargetIndex;
	GameServer()->CreateSound(m_aRocketPos[Slot], SOUND_GRENADE_FIRE);
}

void CFoundryWarden::ExplodeRocket(int i, bool Shot)
{
	const vec2 Pos = m_aRocketPos[i];
	Server()->SnapFreeID(m_aRocketID[i]);
	m_aRocketID[i] = -1;
	GameServer()->CreateExplosion(Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0.0f);
	HurtPlayers(Pos, Shot ? 70.0f : 100.0f, Shot ? WARDEN_ROCKET_DAMAGE / 2 : WARDEN_ROCKET_DAMAGE, 9.0f, false);
}

void CFoundryWarden::ClearRockets()
{
	for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
	{
		if(m_aRocketID[i] >= 0)
			Server()->SnapFreeID(m_aRocketID[i]);
		m_aRocketID[i] = -1;
	}
}

void CFoundryWarden::TickRockets()
{
	CCollision *pCollision = GameServer()->Collision();
	const float Speed = 10.0f + m_Phase * 2.0f;
	for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
	{
		if(m_aRocketID[i] < 0)
			continue;
		vec2 &Vel = m_aRocketVel[i];
		CCharacter *pTarget = GameServer()->GetPlayerChar(m_aRocketTarget[i]);
		if(++m_aRocketLife[i] < 20)
			Vel *= 0.95f;
		else if(pTarget && pTarget->IsAlive())
		{
			vec2 Want = pTarget->m_Pos - m_aRocketPos[i];
			if(length(Want) > 1.0f)
				Vel += (normalize(Want) * Speed - Vel) * 0.08f;
		}
		else
			Vel.y += 0.3f;

		const vec2 Next = m_aRocketPos[i] + Vel;
		vec2 At;
		if(pCollision->IntersectLine(m_aRocketPos[i], Next, &At, 0))
		{
			m_aRocketPos[i] = At;
			ExplodeRocket(i, false);
			continue;
		}
		m_aRocketPos[i] = Next;
		CCharacter *pNear = GameServer()->m_World.ClosestCharacter(Next, 30.0f, 0);
		if((pNear && !pNear->m_IsBot) || m_aRocketLife[i] > WARDEN_TICK_SPEED * 7)
			ExplodeRocket(i, false);
	}
}

void CFoundryWarden::ReleaseGrab()
{
	CCharacter *pChr = m_GrabCID >= 0 ? GameServer()->GetPlayerChar(m_GrabCID) : 0;
	if(pChr)
		pChr->Slow(0.8f);
	m_GrabCID = -1;
	if(m_ArmState != ARM_IN)
		m_ArmState = ARM_RETRACT;
}

void CFoundryWarden::TickGrab()
{
	CCollision *pCollision = GameServer()->Collision();
	const vec2 Shoulder = m_Pos + vec2(m_Dir * 80.0f, -72.0f);
	const int Elapsed = Server()->Tick() - m_ActTick;

	if(m_ArmState == ARM_IN)
	{
		if(Elapsed < WardenTicks(0.2f) || (m_Events & 1))
			return;
		m_Events |= 1;
		CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
		const vec2 Aim = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
		m_ArmTip = Shoulder;
		m_ArmVel = normalize(Aim - Shoulder) * 42.0f;
		m_ArmState = ARM_EXTEND;
		GameServer()->CreateSound(Shoulder, SOUND_HAMMER_FIRE);
		return;
	}

	if(m_ArmState == ARM_EXTEND)
	{
		const vec2 Next = m_ArmTip + m_ArmVel;
		vec2 At;
		if(pCollision->IntersectLine(m_ArmTip, Next, &At, 0))
		{
			m_ArmTip = At;
			m_ArmState = ARM_RETRACT;
			GameServer()->CreateSound(At, SOUND_HOOK_ATTACH_GROUND);
			return;
		}
		CCharacter *apEnts[MAX_CHARACTERS];
		const int Num = GameServer()->m_World.FindEntities(
			Next, 60.0f, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			CCharacter *pChr = apEnts[i];
			if(!pChr->IsAlive() || pChr->m_IsBot)
				continue;
			if(distance(closest_point_on_line(m_ArmTip, Next, pChr->m_Pos), pChr->m_Pos) > 36.0f)
				continue;
			m_GrabCID = pChr->GetCID();
			m_GrabBreak = 0;
			m_ArmTip = pChr->m_Pos;
			m_ArmState = ARM_PULL;
			pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), WARDEN_GRAB_DAMAGE, vec2(0, 0), pChr->m_Pos);
			GameServer()->CreateSound(pChr->m_Pos, SOUND_HOOK_ATTACH_PLAYER);
			return;
		}
		m_ArmTip = Next;
		if(distance(m_ArmTip, Shoulder) > WARDEN_GRAB_RANGE)
			m_ArmState = ARM_RETRACT;
		return;
	}

	if(m_ArmState == ARM_PULL)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_GrabCID);
		if(!pChr || !pChr->IsAlive())
		{
			ReleaseGrab();
			return;
		}
		const vec2 Hold = m_Pos + vec2(m_Dir * 150.0f, -20.0f);
		const vec2 Diff = Hold - m_ArmTip;
		const float Len = length(Diff);
		const vec2 Next = Len <= 24.0f ? Hold : m_ArmTip + Diff / Len * 24.0f;
		if(pCollision->FastIntersectLine(m_ArmTip, Next) || pCollision->TestBox(Next, s_GrabBox))
		{
			ReleaseGrab();
			return;
		}
		m_ArmTip = Next;
		pChr->Teleport(Next);
		if(Len > 24.0f)
			return;
		ReleaseGrab();
		m_ArmState = ARM_IN;
		StartAct(PartAlive(WARDEN_PART_LEGS) && frandom() < 0.4f ? WARDEN_ACT_SLAM : WARDEN_ACT_CLAMP);
		return;
	}

	const vec2 Diff = Shoulder - m_ArmTip;
	const float Len = length(Diff);
	if(Len < 40.0f)
	{
		m_ArmState = ARM_IN;
		FinishAct();
		return;
	}
	m_ArmTip += Diff / Len * 44.0f;
}

void CFoundryWarden::EndCharge(float Radius, int Dmg, float Knock)
{
	m_Events |= 2;
	const vec2 Body = m_Pos + vec2(0.0f, -40.0f);
	if(Dmg > 0)
		HurtPlayers(Body, Radius, Dmg, Knock, false, 0.6f);
	GameServer()->CreateExplosion(Body + normalize(m_Vel) * 60.0f, CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0.0f);
	m_Vel *= 0.25f;
}

void CFoundryWarden::TickCharge(int Elapsed)
{
	static const float s_aSpeed[3] = {24.0f, 27.0f, 30.0f};
	const float Speed = s_aSpeed[m_Phase];
	const vec2 Body = m_Pos + vec2(0.0f, -40.0f);
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
	if(pTarget && !pTarget->IsAlive())
		pTarget = 0;

	// Tuck and hover while the legs swing round into thrusters, tracking the target the whole time.
	if(Elapsed < WardenChargeLaunchTick())
	{
		if(pTarget)
		{
			m_Target = pTarget->m_Pos - m_Pos;
			m_Dir = m_Target.x >= 0.0f ? 1 : -1;
		}
		m_Vel = vec2(m_Vel.x * 0.8f, -2.6f * (1.0f - (float)Elapsed / WardenChargeLaunchTick()));
		return;
	}
	if(!(m_Events & 1))
	{
		m_Events |= 1;
		vec2 Aim = (pTarget ? pTarget->m_Pos : m_Pos + m_Target) - Body;
		if(length(Aim) < 1.0f)
			Aim = vec2((float)m_Dir, 0.0f);
		m_Vel = normalize(Aim) * Speed;
		GameServer()->CreateSound(Body, SOUND_GRENADE_FIRE);
		return;
	}
	if(m_Events & 2)
		return;
	if(Elapsed >= WardenChargeEndTick())
	{
		m_Events |= 2;
		m_Vel *= 0.4f;
		return;
	}
	if(pTarget)
	{
		const vec2 Want = pTarget->m_Pos - Body;
		if(length(Want) > 1.0f)
			m_Vel += (normalize(Want) * Speed - m_Vel) * 0.05f;
	}
	m_Dir = m_Vel.x >= 0.0f ? 1 : -1;

	CCharacter *pHit = GameServer()->m_World.ClosestCharacter(Body, 110.0f, 0);
	if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
		EndCharge(150.0f, WARDEN_CHARGE_DAMAGE, 18.0f);
}

void CFoundryWarden::TickAct()
{
	const int Elapsed = Server()->Tick() - m_ActTick;
	if(m_Act == WARDEN_ACT_GRAB)
		TickGrab();
	else if(m_Act == WARDEN_ACT_CHARGE)
		TickCharge(Elapsed);
	else if(m_Act == WARDEN_ACT_CLAMP && !(m_Events & 1) && Elapsed >= WardenClampImpactTick())
	{
		m_Events |= 1;
		HurtPlayers(m_Pos + vec2(m_Dir * 110.0f, -16.0f), 150.0f, WARDEN_CLAMP_DAMAGE, 9.0f, true);
	}
	else if(m_Act == WARDEN_ACT_SLAM)
	{
		// Leap at the target so the furnace lands on top of it instead of slamming air.
		if(!(m_Events & 2) && Elapsed >= WardenTicks(0.36f))
		{
			m_Events |= 2;
			if(PartAlive(WARDEN_PART_LEGS) && abs(m_Target.x) > 90.0f && abs(m_Target.x) < 520.0f)
				m_Vel = vec2(clamp(m_Target.x / 34.0f, -12.0f, 12.0f), -14.0f);
		}
		const bool Grounded = GameServer()->Collision()->TestBox(m_Pos + vec2(0.0f, 6.0f), s_WardenBox);
		if(!(m_Events & 1) && Elapsed >= WardenSlamImpactTick() && Grounded)
		{
			m_Events |= 1;
			HurtPlayers(m_Pos + vec2(0.0f, 30.0f), 200.0f, WARDEN_SLAM_DAMAGE, 12.0f, false, 1.2f);
		}
	}
	else if(m_Act == WARDEN_ACT_MORTAR && PartAlive(WARDEN_PART_MORTAR))
	{
		for(int Shot = 0; Shot < 3; Shot++)
		{
			if(m_Events & (1 << Shot))
				continue;
			if(Elapsed < WardenMortarShotTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			FireRocket(Shot == 1 ? -1.0f : 1.0f);
			if(m_Phase > 0)
				FireRocket(Shot == 1 ? 1.0f : -1.0f);
		}
	}
}

void CFoundryWarden::FinishAct()
{
	static const int s_aReload[3] = {12, 8, 4};
	static const int s_aGrabCooldown[3] = {110, 85, 60};
	static const int s_aRocketCooldown[3] = {150, 115, 80};
	static const int s_aChargeCooldown[3] = {140, 100, 70};
	const int Act = m_Act;
	if(Act == WARDEN_ACT_CLAMP || Act == WARDEN_ACT_SLAM || Act == WARDEN_ACT_MORTAR || Act == WARDEN_ACT_VENT ||
	   Act == WARDEN_ACT_GRAB || Act == WARDEN_ACT_CHARGE)
		m_ActsDone++;
	if(Act == WARDEN_ACT_GRAB)
		m_GrabCooldown = s_aGrabCooldown[m_Phase];
	if(Act == WARDEN_ACT_MORTAR)
		m_RocketCooldown = s_aRocketCooldown[m_Phase];
	if(Act == WARDEN_ACT_CHARGE)
		m_ChargeCooldown = s_aChargeCooldown[m_Phase];
	m_ReloadTimer = s_aReload[m_Phase];
	StartAct(WARDEN_ACT_IDLE);

	// From phase 2 a clamp that leaves the player in reach rolls straight into a slam.
	if(Act == WARDEN_ACT_CLAMP && m_Phase > 0 && PartAlive(WARDEN_PART_LEGS) && AcquireTarget(true) &&
	   abs(m_Target.x) < 260.0f && abs(m_Target.y) < 160.0f)
		StartAct(WARDEN_ACT_SLAM);
}

void CFoundryWarden::BreakPart(int Part)
{
	m_aPartHealth[Part] = 0;
	m_Stagger = 1;
	float x = 0.0f, y = 0.0f;
	for(int i = 0; i < NUM_WARDEN_HIT; i++)
		if(s_aWardenHit[i].m_Part == Part)
		{
			x = s_aWardenHit[i].m_X * m_Dir;
			y = s_aWardenHit[i].m_Y;
		}
	GameServer()->CreateExplosion(m_Pos + vec2(x, y), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	if(Part == WARDEN_PART_ARMS)
		ReleaseGrab();
}

void CFoundryWarden::Think()
{
	const int Phase = WardenPhase(m_Health, m_MaxHealth);
	if(Phase > m_Phase)
	{
		m_Phase = Phase;
		m_Stagger = 0;
		StartAct(WARDEN_ACT_ENRAGE);
		return;
	}
	if(m_Stagger)
	{
		m_Stagger = 0;
		StartAct(WARDEN_ACT_STAGGER);
		return;
	}

	if(m_ReloadTimer > 0)
		m_ReloadTimer--;
	const bool Seen = AcquireTarget(true);
	if(!Seen && !AcquireTarget(false))
	{
		SetLocomotion(WARDEN_ACT_IDLE);
		return;
	}
	const float Dx = abs(m_Target.x);
	const float Dy = abs(m_Target.y);
	const float Dist = length(m_Target);
	const bool Arms = PartAlive(WARDEN_PART_ARMS);
	const bool Legs = PartAlive(WARDEN_PART_LEGS);
	const bool Mortar = PartAlive(WARDEN_PART_MORTAR) && !m_RocketCooldown;

	// Running away is never safe: past charge range it thrusts in even mid-reload.
	if(Seen && Legs && !m_ChargeCooldown && Dx > WARDEN_CHARGE_RANGE && Dy < 420.0f)
	{
		StartAct(WARDEN_ACT_CHARGE);
		return;
	}
	if(m_ReloadTimer > 0)
	{
		SetLocomotion(Dx < 100.0f ? WARDEN_ACT_IDLE : WARDEN_ACT_WALK);
		return;
	}
	if(!Seen)
	{
		if(Mortar)
			StartAct(WARDEN_ACT_MORTAR);
		else
			SetLocomotion(WARDEN_ACT_WALK);
		return;
	}
	if(m_ActsDone > 0 && (m_ActsDone % 7) == 0)
		StartAct(WARDEN_ACT_VENT);
	else if(Arms && !m_GrabCooldown && Dist > 200.0f && Dist < WARDEN_GRAB_RANGE)
		StartAct(WARDEN_ACT_GRAB);
	else if(Dx < 220.0f && Dy < 150.0f)
		StartAct(Arms && frandom() < 0.6f ? WARDEN_ACT_CLAMP : WARDEN_ACT_SLAM);
	else if(Legs && Dx < 480.0f && Dy < 260.0f && frandom() < 0.5f)
		StartAct(WARDEN_ACT_SLAM);
	else if(Mortar)
		StartAct(WARDEN_ACT_MORTAR);
	else
		SetLocomotion(WARDEN_ACT_WALK);
}

bool CFoundryWarden::GroundAt(float x)
{
	return GameServer()->Collision()->FastIntersectLine(vec2(x, m_Pos.y + 40.0f), vec2(x, m_Pos.y + 120.0f));
}

void CFoundryWarden::MoveBody()
{
	CCollision *pCollision = GameServer()->Collision();
	for(int n = 0; n < 6 && pCollision->TestBox(m_Pos, s_WardenBox); n++)
		m_Pos.y -= 8.0f;

	// Thrusters carry it: no gravity, no friction. Losing half the speed means it rammed a wall.
	if(Thrusting())
	{
		const float Want = length(m_Vel);
		pCollision->MoveBox(&m_Pos, &m_Vel, s_WardenBox, 0, false);
		if(length(m_Vel) < Want * 0.5f)
			EndCharge(170.0f, WARDEN_CHARGE_DAMAGE / 2, 12.0f);
		GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 70);
		return;
	}

	const bool Grounded = pCollision->TestBox(m_Pos + vec2(0.0f, 6.0f), s_WardenBox);
	const bool Rooted = m_Act == WARDEN_ACT_CLAMP || m_Act == WARDEN_ACT_SLAM || m_Act == WARDEN_ACT_STAGGER ||
						m_Act == WARDEN_ACT_ENRAGE || m_Act == WARDEN_ACT_DEATH || m_Act == WARDEN_ACT_GRAB ||
						m_Act == WARDEN_ACT_CHARGE;
	const bool WalkDown = m_Target.y > 96.0f;
	m_Vel.y += 0.75f;
	if(Rooted || m_Act == WARDEN_ACT_IDLE)
	{
		if(Grounded)
			m_Vel.x *= 0.86f;
	}
	else if(Grounded && !WalkDown && !GroundAt(m_Pos.x + m_Dir * 118.0f))
		m_Vel.x *= 0.5f;
	else
	{
		float Speed = m_Phase >= 2 ? 12.0f : 9.0f;
		float Accel = m_Phase >= 2 ? 1.8f : 1.3f;
		if(m_Act == WARDEN_ACT_MORTAR || m_Act == WARDEN_ACT_VENT)
		{
			Speed *= 0.45f;
			Accel *= 0.45f;
		}
		if(!PartAlive(WARDEN_PART_LEGS))
		{
			Speed *= 0.55f;
			Accel *= 0.55f;
		}
		m_Vel.x += (float)m_Dir * Accel;
		if(m_Vel.x > Speed)
			m_Vel.x = Speed;
		if(m_Vel.x < -Speed)
			m_Vel.x = -Speed;
		if(abs(m_Target.x) > 64.0f && abs(m_Vel.x) < 1.0f && m_Vel.y >= 0.0f && Grounded)
			m_Vel.y = -12.0f;
	}

	// A foot pair over thin air: shuffle back onto the side that has floor.
	if(Grounded && !(WalkDown && m_Act == WARDEN_ACT_WALK))
	{
		const bool Left = GroundAt(m_Pos.x - 100.0f);
		const bool Right = GroundAt(m_Pos.x + 100.0f);
		if(Left != Right)
			m_Vel.x += Right ? 0.9f : -0.9f;
	}
	m_Vel.y *= 0.99f;
	pCollision->MoveBox(&m_Pos, &m_Vel, s_WardenBox, 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 70);
}

void CFoundryWarden::TickDeath()
{
	m_Status = DROIDSTATUS_TERMINATED;
	m_Act = WARDEN_ACT_DEATH;
	MoveBody();
	if(Server()->Tick() < m_DeathTick + WardenTicks(WardenAct(WARDEN_ACT_DEATH).m_Duration))
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

void CFoundryWarden::Tick()
{
	if(DespawnIfUnsnapped())
		return;

	if(m_Health > 0 && GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, -0.5f), 12, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	TickRockets();
	if(m_Health <= 0)
	{
		if(!m_DeathTick)
			m_DeathTick = Server()->Tick();
		ReleaseGrab();
		m_ArmState = ARM_IN;
		TickDeath();
		return;
	}

	if(m_GrabCooldown > 0)
		m_GrabCooldown--;
	if(m_RocketCooldown > 0)
		m_RocketCooldown--;
	if(m_ChargeCooldown > 0)
		m_ChargeCooldown--;

	if(m_Act != WARDEN_ACT_IDLE && m_Act != WARDEN_ACT_WALK)
	{
		TickAct();
		// The grapple ends itself once the arm is back in.
		if(m_Act != WARDEN_ACT_GRAB && m_Act != WARDEN_ACT_IDLE &&
		   Server()->Tick() - m_ActTick >= WardenTicks(WardenAct(m_Act).m_Duration))
			FinishAct();
	}
	else
		Think();

	MoveBody();

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status == DROIDSTATUS_HURT)
		m_Status = DROIDSTATUS_IDLE;
}

void CFoundryWarden::TickPaused()
{
}

void CFoundryWarden::Snap(int SnappingClient)
{
	CDroid::Snap(SnappingClient);

	for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
	{
		if(m_aRocketID[i] < 0 || NetworkClipped(SnappingClient, m_aRocketPos[i]))
			continue;
		CNetObj_BossShot *pRocket = static_cast<CNetObj_BossShot *>(
			Server()->SnapNewItem(NETOBJTYPE_BOSSSHOT, m_aRocketID[i], sizeof(CNetObj_BossShot)));
		if(!pRocket)
			continue;
		pRocket->m_X = (int)m_aRocketPos[i].x;
		pRocket->m_Y = (int)m_aRocketPos[i].y;
		pRocket->m_VelX = (int)(m_aRocketVel[i].x * 100.0f);
		pRocket->m_VelY = (int)(m_aRocketVel[i].y * 100.0f);
		pRocket->m_Kind = 0;
	}

	// Wider than the normal clip so the boss bar shows up as the arena comes into view.
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
	pStatus->m_Part1 = m_aPartMax[WARDEN_PART_ARMS] > 0 ? m_aPartHealth[WARDEN_PART_ARMS] * 100 / m_aPartMax[WARDEN_PART_ARMS] : 0;
	pStatus->m_Part2 = m_aPartMax[WARDEN_PART_LEGS] > 0 ? m_aPartHealth[WARDEN_PART_LEGS] * 100 / m_aPartMax[WARDEN_PART_LEGS] : 0;
	pStatus->m_Part3 = m_aPartMax[WARDEN_PART_MORTAR] > 0 ? m_aPartHealth[WARDEN_PART_MORTAR] * 100 / m_aPartMax[WARDEN_PART_MORTAR] : 0;
	pStatus->m_ArmOut = m_ArmState != ARM_IN;
	pStatus->m_ArmX = (int)m_ArmTip.x;
	pStatus->m_ArmY = (int)m_ArmTip.y;
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

bool CFoundryWarden::HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt)
{
	float Best = 1e9f;
	for(int i = 0; i < NUM_WARDEN_HIT; i++)
	{
		const CWardenHitCircle &c = s_aWardenHit[i];
		HitCircle(Pos0, Pos1, m_Pos + vec2(c.m_X, c.m_Y), c.m_R + Radius, &Best, pAt);
		if(c.m_X != 0.0f)
			HitCircle(Pos0, Pos1, m_Pos + vec2(-c.m_X, c.m_Y), c.m_R + Radius, &Best, pAt);
	}
	if(m_ArmState != ARM_IN)
		HitCircle(Pos0, Pos1, m_ArmTip, 26.0f + Radius, &Best, pAt);
	for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
		if(m_aRocketID[i] >= 0)
			HitCircle(Pos0, Pos1, m_aRocketPos[i], 14.0f + Radius, &Best, pAt);
	return Best < 1e9f;
}

void CFoundryWarden::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || !Dmg || Source.m_Kind == EAttackSourceKind::Droid || IgnoresMapObject(Source))
		return;

	// (0,0) is what splash and melee pass; those never pick a rocket or limb.
	const bool HasPos = Pos.x != 0.0f || Pos.y != 0.0f;
	int Part = WARDEN_PART_CORE;
	if(HasPos)
	{
		for(int i = 0; i < WARDEN_MAX_ROCKETS; i++)
			if(m_aRocketID[i] >= 0 && distance(Pos, m_aRocketPos[i]) < 40.0f)
			{
				ExplodeRocket(i, true);
				return;
			}
		if(m_ArmState != ARM_IN && distance(Pos, m_ArmTip) < 50.0f)
			Part = WARDEN_PART_ARMS;
		else
			Part = WardenPartAt(Pos.x - m_Pos.x, Pos.y - m_Pos.y);
	}
	if(m_Act == WARDEN_ACT_ENRAGE)
		return;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, true, this);
	Dmg = WardenIncomingDamage(Dmg, Part, WardenCoreExposed(m_Act, Server()->Tick() - m_ActTick));

	if(Part != WARDEN_PART_CORE && m_aPartHealth[Part] > 0)
	{
		m_aPartHealth[Part] -= Dmg;
		if(m_aPartHealth[Part] <= 0)
			BreakPart(Part);
	}
	if(Part == WARDEN_PART_ARMS && m_ArmState == ARM_PULL)
	{
		m_GrabBreak += Dmg;
		if(m_GrabBreak >= 120)
			ReleaseGrab();
	}

	vec2 DmgPos = PresentDamage(Combat, Force, Dmg, Pos);
	if(m_Act != WARDEN_ACT_SLAM && m_Act != WARDEN_ACT_CHARGE)
		m_Vel += Force * 0.35f;
	if(length(m_Vel) > 16.0f)
		m_Vel = normalize(m_Vel) * 16.0f;
	CommitDamage(DmgPos, Dmg, Source);
	m_DamageTakenTick = Server()->Tick();

	if(m_Health <= 0)
	{
		m_DeathTick = Server()->Tick();
		m_Act = WARDEN_ACT_DEATH;
		m_Status = DROIDSTATUS_TERMINATED;
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		return;
	}

	const bool Fluid = Source.m_Kind == EAttackSourceKind::World && Source.m_Type == DAMAGETYPE_FLUID;
	if(!Fluid && (m_Act == WARDEN_ACT_IDLE || m_Act == WARDEN_ACT_WALK) && frandom() < 0.05f)
		m_Stagger = 1;
}
