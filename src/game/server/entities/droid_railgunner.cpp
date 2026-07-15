#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_railgunner.h"
#include "character.h"
#include "droid.h"
#include "laser.h"
CRailgunner::CRailgunner(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_RAILGUNNER, 500, false), m_ChargeStart(0), m_AimDir(1, 0) {}
void CRailgunner::FireRail()
{
	vec2 From = m_Pos + m_Center, To = From + m_AimDir * 1400.0f, Hit;
	GameServer()->Collision()->IntersectLine(From, To, &Hit, &To);
	new CLaser(GameWorld(), From, m_AimDir, distance(From, To), NEUTRAL_BASE, GetDroidWeapon(m_Type), 0, 0);
	CCharacter *apChars[MAX_CLIENTS]; int NumChars = GameServer()->m_World.FindEntities(From, 0, (CEntity **)apChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < NumChars; i++) { vec2 Closest = closest_point_on_line(From, To, apChars[i]->m_Pos); if(distance(Closest, apChars[i]->m_Pos) <= 28.0f) apChars[i]->TakeDamage(NEUTRAL_BASE, GetDroidWeapon(m_Type), 28, m_AimDir * 4.0f, Closest); }
	CDroid *apDroids[256]; int NumDroids = GameServer()->m_World.FindEntities(From, 0, (CEntity **)apDroids, 256, CGameWorld::ENTTYPE_DROID);
	for(int i = 0; i < NumDroids; i++) if(apDroids[i] != this) { vec2 Closest = closest_point_on_line(From, To, apDroids[i]->m_Pos + apDroids[i]->m_Center); if(distance(Closest, apDroids[i]->m_Pos + apDroids[i]->m_Center) <= 32.0f) apDroids[i]->TakeDamage(m_AimDir * 3.0f, 28, NEUTRAL_BASE, Closest, GetDroidWeapon(m_Type)); }
}
void CRailgunner::AbilityTick()
{
	if(!AcquireTarget(1400.0f)) { m_ChargeStart = 0; m_AbilityTick = Server()->Tick() + 10; return; }
	CCharacter *p = TargetCharacter(); if(!p) return;
	if(!m_ChargeStart) { m_ChargeStart = Server()->Tick(); m_AimDir = normalize(p->m_Pos - vec2(0, 24) - (m_Pos + m_Center)); m_AttackTick = m_ChargeStart; }
	// Repeated zero-damage beams form a conspicuous one-second warning line.
	if((Server()->Tick() & 3) == 0) { vec2 End = m_Pos + m_Center + m_AimDir * 1400.0f, Hit; GameServer()->Collision()->IntersectLine(m_Pos + m_Center, End, &Hit, &End); new CLaser(GameWorld(), m_Pos + m_Center, m_AimDir, distance(m_Pos + m_Center, End), NEUTRAL_BASE, GetDroidWeapon(m_Type), 0, 0); }
	if(Server()->Tick() - m_ChargeStart >= Server()->TickSpeed()) { FireRail(); m_ChargeStart = 0; m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 3; }
	else m_AbilityTick = Server()->Tick() + 1;
}
