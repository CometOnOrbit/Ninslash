#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "character.h"
#include "droid_stormseraph.h"

static const CBossBodyShape s_SeraphShape = {SERAPH_BODY_HALF_W, SERAPH_BODY_TOP, SERAPH_BODY_BOTTOM, 0.0f};

CStormSeraph::CStormSeraph(CGameWorld *pGameWorld, vec2 Pos) :
	CBossV5(pGameWorld, Pos, DROIDTYPE_BOSSARC, SERAPH_HEALTH, s_aSeraphPartShare, s_aSeraphHit, NUM_SERAPH_HIT, SERAPH_ACT_DEATH)
{
	m_ProximityRadius = 64.0f;
	for(int i = 0; i < SERAPH_NODES; i++)
	{
		m_aNodeShot[i] = -1;
		m_aNodeTarget[i] = Pos;
	}
	m_NodeState = SERAPH_NODE_ORBIT_STATE;
	m_Side = -1;
	m_SideTimer = 200;
	m_DiveCooldown = 60;
	m_LatticeCooldown = 70;
	m_OrbCooldown = 0;
	m_PlungeCooldown = 180;
	m_ZapCooldown = 0;
	m_DiveIndex = 0;
	m_DiveStart = 0;
	m_Dashing = false;
	m_DashDir = vec2(1, 0);
	m_PlungeStage = 0;
	m_StageTick = 0;
	m_FlyTo = Pos;
	m_FlyFast = false;
	m_Grounded = false;
	m_BlindTicks = 0;
	SpawnNodes();
	m_Body.Init(s_SeraphShape, 0.0f);
}

void CStormSeraph::SpawnNodes()
{
	const int Hp = max(1, m_aPartMax[SERAPH_PART_NODES] / SERAPH_NODES);
	for(int i = 0; i < SERAPH_NODES; i++)
		m_aNodeShot[i] = AddShot(SERAPH_SHOT_NODE, NodeHome(i), vec2(0, 0), i, Hp);
}

int CStormSeraph::AliveNodes()
{
	int n = 0;
	for(int i = 0; i < SERAPH_NODES; i++)
		if(m_aNodeShot[i] >= 0)
			n++;
	return n;
}

bool CStormSeraph::Shielded()
{
	return m_NodeState == SERAPH_NODE_ORBIT_STATE && AliveNodes() > 0;
}

bool CStormSeraph::Exposed()
{
	return m_Act == SERAPH_ACT_RECHARGE || m_Act == SERAPH_ACT_STAGGER;
}

int CStormSeraph::StatusFlags()
{
	int Flags = CBossV5::StatusFlags();
	if(m_Dashing || (m_Act == SERAPH_ACT_PLUNGE && m_PlungeStage == 2))
		Flags |= BOSSV5_FLAG_THRUST;
	if(m_Grounded)
		Flags |= BOSSV5_FLAG_AIR; // "grounded" for the seraph
	return Flags;
}

void CStormSeraph::TickTimers()
{
	int *apTimer[] = {&m_DiveCooldown, &m_LatticeCooldown, &m_OrbCooldown, &m_PlungeCooldown, &m_ZapCooldown};
	for(int *p : apTimer)
		if(*p > 0)
			(*p)--;
	// Part health of the nodes is the sum of the surviving nodes.
	int Sum = 0;
	for(int i = 0; i < SERAPH_NODES; i++)
		if(m_aNodeShot[i] >= 0)
			Sum += max(0, m_aShots[m_aNodeShot[i]].m_Hp);
	m_aPartHealth[SERAPH_PART_NODES] = Sum;
}

void CStormSeraph::SetNodeState(int State)
{
	m_NodeState = State;
}

void CStormSeraph::OnActStart(int Act)
{
	m_Dashing = false;
	m_FlyFast = false;
	if(Act != SERAPH_ACT_RECHARGE && Act != m_DeathAct)
		m_Grounded = false;
	if(Act != SERAPH_ACT_LATTICE && Act != SERAPH_ACT_RECHARGE)
		SetNodeState(SERAPH_NODE_ORBIT_STATE);
	if(Act == SERAPH_ACT_DIVE)
	{
		m_DiveIndex = 0;
		m_DiveStart = Server()->Tick();
	}
	if(Act == SERAPH_ACT_PLUNGE)
	{
		m_PlungeStage = 0;
		m_StageTick = Server()->Tick();
	}
}

void CStormSeraph::OnActFinished(int Act)
{
	static const int s_aReload[3] = {5, 3, 1};
	static const int s_aDive[3] = {85, 65, 45};
	static const int s_aLattice[3] = {190, 150, 115};
	static const int s_aOrb[3] = {90, 70, 55};
	static const int s_aPlunge[3] = {280, 220, 170};
	static const int s_aZap[3] = {55, 42, 32};
	m_ReloadTimer = s_aReload[m_Phase];
	if(Act == SERAPH_ACT_DIVE)
		m_DiveCooldown = s_aDive[m_Phase];
	if(Act == SERAPH_ACT_LATTICE)
		m_LatticeCooldown = s_aLattice[m_Phase];
	if(Act == SERAPH_ACT_ORBS)
		m_OrbCooldown = s_aOrb[m_Phase];
	if(Act == SERAPH_ACT_PLUNGE || Act == SERAPH_ACT_RECHARGE)
		m_PlungeCooldown = s_aPlunge[m_Phase];
	if(Act == SERAPH_ACT_ZAP)
		m_ZapCooldown = s_aZap[m_Phase];
	m_Grounded = false;
	SetNodeState(SERAPH_NODE_ORBIT_STATE);
	// A plunge that ran out its clock without landing still owes the punish window.
	if(Act == SERAPH_ACT_PLUNGE && m_PlungeStage == 2)
		Impact();
}

void CStormSeraph::OnDeathStart()
{
	for(int i = 0; i < SERAPH_NODES; i++)
		if(m_aNodeShot[i] >= 0)
		{
			GameServer()->CreateExplosion(m_aShots[m_aNodeShot[i]].m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
			RemoveShot(m_aNodeShot[i]);
			m_aNodeShot[i] = -1;
		}
	m_Grounded = false;
}

int CStormSeraph::IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg)
{
	return SeraphIncomingDamage(Dmg, Part, Exposed(), Shielded());
}

// ---------------------------------------------------------------- nodes / shots

vec2 CStormSeraph::NodeHome(int Node)
{
	const float a = SeraphNodeAngle(Node, Server()->Tick());
	return m_Pos + vec2(cosf(a) * SERAPH_NODE_ORBIT, sinf(a) * SERAPH_NODE_ORBIT * 0.55f - 10.0f);
}

int CStormSeraph::NearestNode(vec2 To)
{
	int Best = -1;
	float BestD = 1e9f;
	for(int i = 0; i < SERAPH_NODES; i++)
	{
		if(m_aNodeShot[i] < 0)
			continue;
		const float d = distance(m_aShots[m_aNodeShot[i]].m_Pos, To);
		if(d < BestD)
		{
			BestD = d;
			Best = i;
		}
	}
	return Best;
}

bool CStormSeraph::ShotHittable(const CShot &s)
{
	if(s.m_Kind == SERAPH_SHOT_NODE)
		return m_NodeState != SERAPH_NODE_DOCKED && m_Health > 0;
	return s.m_Hp > 0;
}

void CStormSeraph::OnShotDestroyed(int i)
{
	CShot &s = m_aShots[i];
	GameServer()->CreateExplosion(s.m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	if(s.m_Kind == SERAPH_SHOT_NODE)
	{
		GameServer()->CreateSound(s.m_Pos, SOUND_ELECTRODEATH);
		if(s.m_Data >= 0 && s.m_Data < SERAPH_NODES)
			m_aNodeShot[s.m_Data] = -1;
		if(!AliveNodes())
			m_Stagger = 1;
	}
	RemoveShot(i);
}

void CStormSeraph::SnapShot(const CShot &s, CNetObj_BossShot *pShot)
{
	CBossV5::SnapShot(s, pShot);
	if(s.m_Kind == SERAPH_SHOT_NODE)
	{
		pShot->m_VelX = m_NodeState;
		pShot->m_VelY = s.m_Data;
	}
	else if(s.m_Kind == SERAPH_SHOT_SPARK)
		pShot->m_VelY = SERAPH_SPARK_LIFE - s.m_Life;
}

void CStormSeraph::FireOrb(int Shot)
{
	const vec2 From = LocalToWorld(s_SeraphHalo.m_X, s_SeraphHalo.m_Y);
	const float a = -1.5707963f + (Shot - 2) * 0.55f;
	AddShot(SERAPH_SHOT_ORB, From, vec2(cosf(a), sinf(a)) * 4.0f, 0, 1);
	GameServer()->CreateSound(From, SOUND_ELECTROMINE);
}

void CStormSeraph::TickShot(int i)
{
	CCollision *pCollision = GameServer()->Collision();
	CShot &s = m_aShots[i];
	if(s.m_Kind == SERAPH_SHOT_NODE)
	{
		const int Node = s.m_Data;
		vec2 Want = NodeHome(Node);
		if(m_NodeState == SERAPH_NODE_ARMED || m_NodeState == SERAPH_NODE_LIVE)
			Want = m_aNodeTarget[Node];
		else if(m_NodeState == SERAPH_NODE_DOCKED)
			Want = m_Pos;
		const vec2 d = Want - s.m_Pos;
		const float Len = length(d);
		const float Step = min(Len, max(6.0f, Len * 0.25f));
		if(Len > 0.01f)
			s.m_Pos += d / Len * min(Step, 34.0f);
		s.m_Vel = vec2(0, 0);
		return;
	}
	if(s.m_Kind == SERAPH_SHOT_ORB)
	{
		// Slow homing: turns at a capped rate, so a dash or a sharp jump shakes it.
		CCharacter *pTarget = GameServer()->m_World.ClosestCharacter(s.m_Pos, 1200.0f, 0);
		const float Speed = min(10.0f, 5.5f + s.m_Life * 0.07f);
		float a = atan2f(s.m_Vel.y, s.m_Vel.x);
		if(pTarget && !pTarget->m_IsBot)
		{
			const vec2 To = pTarget->m_Pos - s.m_Pos;
			float Want = atan2f(To.y, To.x);
			float Diff = Want - a;
			while(Diff > pi)
				Diff -= 2 * pi;
			while(Diff < -pi)
				Diff += 2 * pi;
			a += clamp(Diff, -0.075f, 0.075f);
		}
		s.m_Vel = vec2(cosf(a), sinf(a)) * Speed;
		const vec2 Next = s.m_Pos + s.m_Vel;
		CCharacter *pHit = GameServer()->m_World.ClosestCharacter(Next, 28.0f, 0);
		if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
		{
			pHit->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(SERAPH_ORB_DAMAGE), s.m_Vel * 0.6f, pHit->m_Pos);
			GameServer()->CreateSound(Next, SOUND_ELECTROMINE);
			GameServer()->CreateExplosion(Next, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
			RemoveShot(i);
			return;
		}
		if(pCollision->CheckPoint(Next) || s.m_Life > SERAPH_ORB_LIFE)
		{
			GameServer()->CreateExplosion(s.m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
			RemoveShot(i);
			return;
		}
		s.m_Pos = Next;
		return;
	}
	if(s.m_Kind == SERAPH_SHOT_SPARK)
	{
		const vec2 Next = s.m_Pos + vec2(s.m_Vel.x, 0.0f);
		vec2 Floor;
		if(s.m_Life > SERAPH_SPARK_LIFE || !pCollision->IntersectLine(Next + vec2(0, -60.0f), Next + vec2(0, 90.0f), &Floor, 0, false, true) ||
			pCollision->CheckPoint(Floor + vec2(0, -6.0f)))
		{
			RemoveShot(i);
			return;
		}
		s.m_Pos = Floor;
		for(int c = 0; c < MAX_CHARACTERS; c++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(c);
			if(!pChr || !pChr->IsAlive() || pChr->m_IsBot || m_aHitCooldown[c] > Server()->Tick())
				continue;
			const vec2 d = pChr->m_Pos - s.m_Pos;
			if(fabsf(d.x) > 30.0f || d.y < -70.0f || d.y > 12.0f)
				continue;
			m_aHitCooldown[c] = Server()->Tick() + 25;
			pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(SERAPH_SPARK_DAMAGE), vec2(s.m_Vel.x > 0 ? 5.0f : -5.0f, -9.0f), pChr->m_Pos);
		}
	}
}

// ---------------------------------------------------------------- acts

void CStormSeraph::Impact()
{
	m_Vel = vec2(0, 0);
	m_Grounded = true;
	const vec2 Floor = vec2(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + SERAPH_BODY_BOTTOM);
	HurtPlayers(Floor + vec2(0, -30.0f), SERAPH_PLUNGE_RADIUS, SERAPH_PLUNGE_DAMAGE, 15.0f, 0.7f);
	GameServer()->CreateExplosion(Floor + vec2(0, -20.0f), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	GameServer()->CreateBuildingHit(Floor);
	GameServer()->CreateSound(m_Pos, SOUND_TESLACOIL_FIRE);
	for(int Side = -1; Side <= 1; Side += 2)
	{
		vec2 At;
		const vec2 From = Floor + vec2(Side * 60.0f, 0.0f);
		if(GameServer()->Collision()->IntersectLine(From + vec2(0, -60.0f), From + vec2(0, 90.0f), &At, 0, false, true))
			AddShot(SERAPH_SHOT_SPARK, At, vec2(Side * 12.0f, 0.0f), Side);
	}
	StartAct(SERAPH_ACT_RECHARGE);
	SetNodeState(SERAPH_NODE_DOCKED);
	m_Grounded = true;
}

void CStormSeraph::TickAct(int Elapsed)
{
	switch(m_Act)
	{
	case SERAPH_ACT_DIVE:
	{
		const int Windup = m_DiveIndex == 0 ? SeraphDiveWindup(m_Phase) : BossV5Ticks(0.2f);
		const int Since = Server()->Tick() - m_DiveStart;
		if(!m_Dashing)
		{
			if(AcquireTarget(false))
			{
				CCharacter *pTarget = TargetChr();
				m_Aim = pTarget ? pTarget->m_Pos + pTarget->GetVel() * 8.0f : m_Pos + m_Target;
			}
			// Rear back away from the target while winding up.
			const vec2 Away = normalize(m_Pos - m_Aim);
			m_Vel += Away * 0.6f;
			if(Since >= Windup)
			{
				m_Dashing = true;
				m_DashDir = normalize(m_Aim + vec2(0, 20.0f) - m_Pos);
				m_DiveStart = Server()->Tick();
				for(int c = 0; c < MAX_CHARACTERS; c++)
					m_aHitCooldown[c] = 0;
				GameServer()->CreateSound(m_Pos, SOUND_DASH);
			}
			break;
		}
		m_Vel = m_DashDir * SeraphDiveSpeed(PartAlive(SERAPH_PART_WINGS));
		m_Dir = m_DashDir.x >= 0.0f ? 1 : -1;
		for(int c = 0; c < MAX_CHARACTERS; c++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(c);
			if(!pChr || !pChr->IsAlive() || pChr->m_IsBot || m_aHitCooldown[c] > Server()->Tick() || distance(pChr->m_Pos, m_Pos) > 82.0f)
				continue;
			m_aHitCooldown[c] = Server()->Tick() + 40;
			pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(SERAPH_DIVE_DAMAGE), m_DashDir * 14.0f + vec2(0, -6.0f), pChr->m_Pos);
			GameServer()->CreateSound(pChr->m_Pos, SOUND_HAMMER_HIT);
		}
		const bool Blocked = !m_Body.Fits(GameServer()->Collision(), m_Pos + m_DashDir * 24.0f);
		if(Since >= SeraphDiveTicks() || Blocked)
		{
			m_Dashing = false;
			m_Vel *= 0.25f;
			if(Blocked)
				GameServer()->CreateBuildingHit(m_Pos + m_DashDir * 40.0f);
			if(++m_DiveIndex >= SeraphDiveCount(m_Phase))
				FinishAct();
			else
				m_DiveStart = Server()->Tick();
		}
		break;
	}
	case SERAPH_ACT_LATTICE:
	{
		if(Elapsed == 1 || !(m_Events & 1))
		{
			m_Events |= 1;
			AcquireTarget(false);
			CCharacter *pTarget = TargetChr();
			const vec2 Center = pTarget ? pTarget->m_Pos + vec2(0, -20.0f) : m_Pos + m_Target;
			static const float s_aCorner[SERAPH_NODES][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
			for(int i = 0; i < SERAPH_NODES; i++)
			{
				vec2 Want = Center + vec2(s_aCorner[i][0], s_aCorner[i][1]) * SERAPH_LATTICE_HALF;
				vec2 At;
				if(GameServer()->Collision()->IntersectLine(Center, Want, &At, 0))
					Want = Center + (At - Center) * 0.88f; // tuck inside walls
				m_aNodeTarget[i] = Want;
			}
			m_Aim = Center;
			SetNodeState(SERAPH_NODE_ARMED);
			GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
		}
		if(m_NodeState == SERAPH_NODE_ARMED && Elapsed >= SeraphLatticeLive(m_Phase))
		{
			SetNodeState(SERAPH_NODE_LIVE);
			GameServer()->CreateSound(m_Aim, SOUND_TESLACOIL_FIRE);
		}
		if(m_NodeState == SERAPH_NODE_LIVE)
		{
			static const int s_aPair[6][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 2}, {1, 3}};
			const int Pairs = m_Phase >= 2 ? 6 : 4;
			for(int p = 0; p < Pairs; p++)
			{
				const int a = m_aNodeShot[s_aPair[p][0]], b = m_aNodeShot[s_aPair[p][1]];
				if(a < 0 || b < 0)
					continue;
				HurtSegment(m_aShots[a].m_Pos, m_aShots[b].m_Pos, 14.0f, SERAPH_LATTICE_DAMAGE, 8.0f, m_aHitCooldown, 18);
			}
			if(Elapsed >= SeraphLatticeEnd(m_Phase))
			{
				SetNodeState(SERAPH_NODE_ORBIT_STATE);
				FinishAct();
			}
		}
		break;
	}
	case SERAPH_ACT_ORBS:
	{
		AcquireTarget(false);
		const int Count = SeraphOrbCount(m_Phase, PartAlive(SERAPH_PART_HALO));
		for(int Shot = 0; Shot < Count; Shot++)
		{
			if((m_Events & (1 << Shot)) || Elapsed < SeraphOrbTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			FireOrb(Shot);
		}
		break;
	}
	case SERAPH_ACT_PLUNGE:
	{
		const int Since = Server()->Tick() - m_StageTick;
		CCharacter *pTarget = TargetChr();
		if(!pTarget && AcquireTarget(false))
			pTarget = TargetChr();
		if(m_PlungeStage == 0)
		{
			// Climb above the target.
			if(pTarget)
				m_FlyTo = pTarget->m_Pos + vec2(0, -330.0f);
			vec2 Ceil;
			if(pTarget && GameServer()->Collision()->IntersectLine(pTarget->m_Pos, m_FlyTo, &Ceil, 0))
				m_FlyTo = pTarget->m_Pos + (Ceil - pTarget->m_Pos) * 0.8f;
			m_FlyFast = true;
			if(Since >= SeraphPlungeRise())
			{
				m_PlungeStage = 1;
				m_StageTick = Server()->Tick();
				GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
			}
		}
		else if(m_PlungeStage == 1)
		{
			// Hang there crackling, drifting with the target: the telegraph.
			if(pTarget)
				m_FlyTo = vec2(pTarget->m_Pos.x, m_FlyTo.y);
			m_FlyFast = false;
			m_Aim = vec2(m_Pos.x, m_Pos.y + 400.0f);
			if(Since >= SeraphPlungeHold(m_Phase))
			{
				m_PlungeStage = 2;
				m_StageTick = Server()->Tick();
				GameServer()->CreateSound(m_Pos, SOUND_DASH);
				for(int c = 0; c < MAX_CHARACTERS; c++)
					m_aHitCooldown[c] = 0;
			}
		}
		else
		{
			m_Vel = vec2(0, 30.0f);
			HurtPlayers(m_Pos + vec2(0, 40.0f), 60.0f, SERAPH_DIVE_DAMAGE / 2, 8.0f, 0.0f);
			const bool Landed = m_Body.m_Grounded || !m_Body.Fits(GameServer()->Collision(), m_Pos + vec2(0, 32.0f)) ||
					    (m_Ground < 1e8f && m_Ground - m_Pos.y < SERAPH_BODY_BOTTOM + 10.0f);
			if(Landed || Since > 60)
				Impact();
		}
		break;
	}
	case SERAPH_ACT_ZAP:
	{
		if(Elapsed < SeraphZapTick())
		{
			if(AcquireTarget(false))
			{
				CCharacter *pTarget = TargetChr();
				m_Aim = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
			}
			if(Elapsed == 1)
				GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
			break;
		}
		if(!(m_Events & 1))
		{
			m_Events |= 1;
			const int Node = NearestNode(m_Aim);
			const vec2 From = Node >= 0 ? m_aShots[m_aNodeShot[Node]].m_Pos : m_Pos;
			const vec2 Dir = normalize(m_Aim - From);
			vec2 To = m_Aim + Dir * 80.0f;
			vec2 Wall;
			if(GameServer()->Collision()->IntersectLine(From, To, &Wall, 0))
				To = Wall;
			HurtSegment(From, To, 22.0f, SERAPH_ZAP_DAMAGE, 10.0f);
			GameServer()->CreateSound(m_Aim, SOUND_TESLACOIL_FIRE);
		}
		break;
	}
	case SERAPH_ACT_ROAR:
		if(!(m_Events & 1) && Elapsed >= SeraphRoarTick())
		{
			m_Events |= 1;
			HurtPlayers(m_Pos, SERAPH_ROAR_RADIUS, SERAPH_ROAR_DAMAGE, 15.0f, 0.4f);
			GameServer()->CreateSound(m_Pos, SOUND_TESLACOIL_FIRE);
		}
		break;
	default:
		break;
	}
}

vec2 CStormSeraph::HoverPoint()
{
	CCharacter *pTarget = TargetChr();
	const vec2 Base = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
	const float Bob = sinf(Server()->Tick() * 0.06f) * 26.0f;
	vec2 Want = Base + vec2(m_Side * SERAPH_HOVER_SIDE, -SERAPH_HOVER_UP + Bob);
	// Keep the hover spot in open air with a clear line to the target.
	vec2 At;
	if(GameServer()->Collision()->IntersectLine(Base + vec2(0, -30.0f), Want, &At, 0))
	{
		Want = Base + vec2(0, -30.0f) + (At - Base - vec2(0, -30.0f)) * 0.75f;
		if(distance(Want, Base) < 120.0f)
			Want = Base + vec2(0, -SERAPH_HOVER_UP);
	}
	return Want;
}

void CStormSeraph::Think()
{
	const bool Seen = AcquireTarget(true);
	if(!Seen && !AcquireTarget(false))
	{
		SetLocomotion(SERAPH_ACT_IDLE);
		return;
	}
	m_BlindTicks = Seen ? 0 : m_BlindTicks + 1;
	if(--m_SideTimer <= 0)
	{
		// Strafe over the target to the other side now and then (more often when hurt).
		m_SideTimer = (m_Phase >= 1 ? 90 : 130) + rand() % 60;
		m_Side = -m_Side;
	}
	if(m_Target.x * -m_Side > 600.0f)
		m_Side = m_Target.x > 0 ? -1 : 1;

	const float Dist = length(m_Target);
	const bool Nodes = AliveNodes() > 0;

	// Anti-hug: anyone under or next to it gets zapped.
	if(!m_ZapCooldown && Dist < SERAPH_ZAP_RANGE && Seen)
	{
		StartAct(SERAPH_ACT_ZAP);
		return;
	}
	if(m_ReloadTimer > 0)
	{
		SetLocomotion(SERAPH_ACT_FLY);
		return;
	}
	if(!Seen)
	{
		if(!m_OrbCooldown)
			StartAct(SERAPH_ACT_ORBS); // homing orbs find people behind cover
		else
			SetLocomotion(SERAPH_ACT_FLY);
		return;
	}
	// Anti-kite: far targets get dived.
	if(!m_DiveCooldown && (Dist > 460.0f || frandom() < 0.5f || Pressured()))
		StartAct(SERAPH_ACT_DIVE);
	else if(!m_PlungeCooldown && m_Phase >= 1 && fabsf(m_Target.x) < 420.0f)
		StartAct(SERAPH_ACT_PLUNGE);
	else if(Nodes && !m_LatticeCooldown && Dist < 700.0f)
		StartAct(SERAPH_ACT_LATTICE);
	else if(!m_OrbCooldown)
		StartAct(SERAPH_ACT_ORBS);
	else if(!m_PlungeCooldown && fabsf(m_Target.x) < 360.0f)
		StartAct(SERAPH_ACT_PLUNGE);
	else if(!m_DiveCooldown)
		StartAct(SERAPH_ACT_DIVE);
	else
		SetLocomotion(SERAPH_ACT_FLY);
}

// ---------------------------------------------------------------- body

void CStormSeraph::MoveBody()
{

	if(m_Health <= 0 || m_Grounded)
	{
		// Dead or recharging: drop to the floor and sit there.
		m_Vel.x *= 0.85f;
		if(m_Vel.y > 18.0f)
			m_Vel.y = 18.0f;
		WalkBody(0.0f, 0.0f, SERAPH_GRAVITY, false, 0.0f);
		return;
	}
	const bool Dashing = m_Dashing || (m_Act == SERAPH_ACT_PLUNGE && m_PlungeStage == 2);
	if(!Dashing)
	{
		vec2 Want;
		float MaxSpeed = SeraphSpeed(m_Phase, PartAlive(SERAPH_PART_WINGS));
		if(m_Act == SERAPH_ACT_PLUNGE)
		{
			Want = m_FlyTo;
			if(m_FlyFast)
				MaxSpeed *= 1.5f;
		}
		else if(m_Act == SERAPH_ACT_DIVE)
			Want = m_Pos + m_Vel; // windup: keep the rear-back drift
		else
		{
			Want = HoverPoint();
			if(m_BlindTicks > 50)
				Want = m_Pos + m_Target + vec2(0, -SERAPH_HOVER_UP); // lost sight: come around over the top
			if(m_Act == SERAPH_ACT_LATTICE || m_Act == SERAPH_ACT_ORBS || m_Act == SERAPH_ACT_ZAP || m_Act == SERAPH_ACT_ROAR ||
				m_Act == SERAPH_ACT_STAGGER)
				MaxSpeed *= 0.6f; // keeps strafing while it casts
		}
		const vec2 d = Want - m_Pos;
		vec2 Acc = d * 0.06f;
		if(length(Acc) > 1.7f)
			Acc = normalize(Acc) * 1.7f;
		m_Vel += Acc;
		m_Vel *= 0.92f;
		if(length(m_Vel) > MaxSpeed)
			m_Vel = normalize(m_Vel) * MaxSpeed;
		if(m_Act != SERAPH_ACT_DIVE && fabsf(m_Target.x) > 20.0f)
			m_Dir = m_Target.x > 0 ? 1 : -1;
	}
	// Flying: full-box collision, no gravity, slides along walls/ceilings/floors.
	WalkBody(0.0f, 0.0f, 0.0f, false, 0.0f);
}

void CStormSeraph::ExtraDebug(char *pBuffer, int Size)
{
	str_format(pBuffer, Size, "nodes=%d nodestate=%d shielded=%d grounded=%d dash=%d plunge=%d", AliveNodes(), m_NodeState, Shielded(),
		m_Grounded, m_Dashing, m_PlungeStage);
}
