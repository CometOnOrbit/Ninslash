#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_railgunner.h"
#include "character.h"
#include "laser.h"
CRailgunner::CRailgunner(CGameWorld *pWorld, vec2 Pos)
	: CSpecialistDroid(pWorld, Pos, DROIDTYPE_RAILGUNNER, 680, false), m_ChargeStart(0), m_AimDir(1, 0)
{
}
void CRailgunner::FireRail()
{
	vec2 From = m_Pos + m_Center, To = From + m_AimDir * 1400.0f, Hit;
	GameServer()->Collision()->IntersectLine(From, To, &Hit, &To);
	new CLaser(GameWorld(), From, m_AimDir, distance(From, To), CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0, 0);
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->m_IsBot)
			continue;
		const vec2 Closest = closest_point_on_line(From, To, pCharacter->m_Pos);
		if(distance(Closest, pCharacter->m_Pos) <= 30.0f)
			pCharacter->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), 56, m_AimDir * 6.0f, Closest);
	}
}
void CRailgunner::AbilityTick()
{
	if(!AcquireTarget(1400.0f))
	{
		m_ChargeStart = 0;
		m_AbilityTick = Server()->Tick() + 6;
		return;
	}
	CCharacter *p = TargetCharacter();
	if(!p)
		return;
	if(!m_ChargeStart)
	{
		m_ChargeStart = Server()->Tick();
		m_AimDir = normalize(p->m_Pos - vec2(0, 24) - (m_Pos + m_Center));
	}
	// Repeated zero-damage beams form a conspicuous warning line.
	if((Server()->Tick() & 3) == 0)
	{
		vec2 End = m_Pos + m_Center + m_AimDir * 1400.0f, Hit;
		GameServer()->Collision()->IntersectLine(m_Pos + m_Center, End, &Hit, &End);
		new CLaser(GameWorld(),
				   m_Pos + m_Center,
				   m_AimDir,
				   distance(m_Pos + m_Center, End),
				   CAttackSource::Droid(NEUTRAL_BASE, m_Type),
				   0,
				   0);
	}
	if(Server()->Tick() - m_ChargeStart >= Server()->TickSpeed() / 4)
	{
		FireRail();
		m_AttackTick = Server()->Tick();
		m_ChargeStart = 0;
		m_AbilityTick = Server()->Tick() + Server()->TickSpeed() / 2;
	}
	else
		m_AbilityTick = Server()->Tick() + 1;
}
