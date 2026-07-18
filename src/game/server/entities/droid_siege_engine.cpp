#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_siege_engine.h"
#include "character.h"
#include "droid_bulwark.h"
#include "building.h"
#include "laser.h"

namespace
{
class CSiegeStrike : public CEntity
{
	vec2 m_Target;
	int m_FireTick;
	CSiegeEngine *m_pOwner;
public:
	CSiegeStrike(CGameWorld *pWorld, vec2 Target, CSiegeEngine *pOwner) :
		CEntity(pWorld, CGameWorld::ENTTYPE_LASER),
		m_Target(Target),
		m_FireTick(Server()->Tick() + Server()->TickSpeed()),
		m_pOwner(pOwner)
	{
		m_Pos = Target;
		GameWorld()->InsertEntity(this);
	}
	void Reset() override { GameWorld()->DestroyEntity(this); }
	void TickPaused() override { m_FireTick++; }
	void Tick() override
	{
		bool OwnerAlive = false;
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_pOwner && pDroid->m_Health > 0)
			{
				OwnerAlive = true;
				break;
			}
		if(!OwnerAlive)
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
		if(Server()->Tick() < m_FireTick)
		{
			if((Server()->Tick() & 3) == 0)
				GameServer()->CreateEffect(FX_ELECTRIC, m_Target);
			return;
		}
		new CLaser(GameWorld(), m_Target - vec2(0, 760), vec2(0, 1), 760.0f, CAttackSource::Droid(NEUTRAL_BASE, DROIDTYPE_SIEGE_ENGINE), 38, 0);
		GameServer()->CreateExplosion(m_Target, CAttackSource::Droid(NEUTRAL_BASE, DROIDTYPE_SIEGE_ENGINE));
		GameWorld()->DestroyEntity(this);
	}
	void Snap(int) override {}
};

class CSiegeMine : public CBuilding
{
	CSiegeEngine *m_pOwner;
	int m_ExpireTick;
public:
	CSiegeMine(CGameWorld *pWorld, vec2 Pos, CSiegeEngine *pOwner) :
		CBuilding(pWorld, Pos, BUILDING_MINE1, TEAM_NEUTRAL),
		m_pOwner(pOwner),
		m_ExpireTick(Server()->Tick() + Server()->TickSpeed() * 14)
	{
		m_Life = m_MaxLife = 45;
	}
	void Tick() override
	{
		bool OwnerAlive = false;
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_pOwner && pDroid->m_Health > 0)
			{
				OwnerAlive = true;
				break;
			}
		if(!OwnerAlive || Server()->Tick() >= m_ExpireTick)
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
		CBuilding::Tick();
	}
	void TickPaused() override
	{
		CBuilding::TickPaused();
		m_ExpireTick++;
	}
};
}

CSiegeEngine::CSiegeEngine(CGameWorld *pWorld, vec2 Pos) :
	CSpecialistDroid(pWorld, Pos, DROIDTYPE_SIEGE_ENGINE, 3200, true),
	m_SkillCycle(0),
	m_ChargeEndTick(0),
	m_ChargeHit(false)
{
	m_apGuards[0] = m_apGuards[1] = 0;
}

void CSiegeEngine::MovementTick(CCharacter *pTarget)
{
	if(Server()->Tick() >= m_ChargeEndTick)
	{
		CSpecialistDroid::MovementTick(pTarget);
		return;
	}
	const vec2 Size = CollisionSize();
	m_Vel.x = m_Dir * 15.0f;
	m_Vel.y = min(10.0f, m_Vel.y + 0.65f);
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, Size, 0, false);
	m_Anim = 1;
	if(pTarget && !m_ChargeHit && distance(m_Pos, pTarget->m_Pos) < 115.0f)
	{
		pTarget->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), 34, vec2(m_Dir * 18.0f, -5.0f), pTarget->m_Pos);
		m_ChargeHit = true;
	}
}

void CSiegeEngine::AbilityTick()
{
	if(!AcquireTarget(1200.0f))
	{
		m_AbilityTick = Server()->Tick() + Server()->TickSpeed();
		return;
	}
	CCharacter *p = TargetCharacter();
	if(!p)
		return;
	switch(m_SkillCycle++ % 3)
	{
	case 0: // One full second of telegraph before the vertical impact.
		new CSiegeStrike(GameWorld(), p->m_Pos, this);
		m_AttackTick = Server()->Tick();
		break;
	case 1: // A sustained, colliding armoured charge.
		m_Dir = p->m_Pos.x < m_Pos.x ? -1 : 1;
		m_ChargeEndTick = Server()->Tick() + Server()->TickSpeed() * 4 / 5;
		m_ChargeHit = false;
		m_AttackTick = Server()->Tick();
		break;
	default: // Visible mines deny the approach lane, then expire with the Boss.
		for(int i = -3; i <= 3; i++)
			new CSiegeMine(GameWorld(), m_Pos + vec2(i * 48.0f, 34.0f), this);
		m_AttackTick = Server()->Tick();
		break;
	}
		m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 2;
}
void CSiegeEngine::OnHealthThreshold(int)
{
	// Each strict 70/35 transition adds one guard, with a hard cap of two.
	for(int i = 0; i < 2; i++)
		if(!m_apGuards[i])
		{
			m_apGuards[i] = new CBulwark(GameWorld(), m_Pos + vec2(i ? 110.0f : -110.0f, -20));
			break;
		}
}

void CSiegeEngine::OnSpecialistDeath()
{
	for(int i = 0; i < 2; i++)
	{
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_apGuards[i])
			{
				GameWorld()->DestroyEntity(pDroid);
				break;
			}
		m_apGuards[i] = 0;
	}
}
