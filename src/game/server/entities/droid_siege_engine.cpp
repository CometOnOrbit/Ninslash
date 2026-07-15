#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_siege_engine.h"
#include "character.h"
#include "droid_bulwark.h"
#include "laser.h"
CSiegeEngine::CSiegeEngine(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_SIEGE_ENGINE, 2600, true), m_SkillCycle(0) {}
void CSiegeEngine::AbilityTick()
{
	if(!AcquireTarget(1200.0f)) { m_AbilityTick = Server()->Tick() + Server()->TickSpeed(); return; }
	CCharacter *p = TargetCharacter(); if(!p) return;
	switch(m_SkillCycle++ % 3)
	{
	case 0: // Orbital strike: telegraph at the target, then a vertical rail.
		GameServer()->CreateEffect(FX_ELECTRIC, p->m_Pos); new CLaser(GameWorld(), p->m_Pos - vec2(0, 700), vec2(0, 1), 700.0f, NEUTRAL_BASE, GetDroidWeapon(m_Type), 24, 0); break;
	case 1: // Armoured charge.
		m_Vel.x += (p->m_Pos.x < m_Pos.x ? -1.0f : 1.0f) * 18.0f; if(distance(m_Pos, p->m_Pos) < 180.0f) p->TakeDamage(NEUTRAL_BASE, GetDroidWeapon(m_Type), 20, normalize(p->m_Pos - m_Pos) * 12.0f, p->m_Pos); break;
	default: // Mine fan, left behind along the approach lane.
		for(int i = -2; i <= 2; i++) GameServer()->CreateProjectile(NEUTRAL_BASE, GetDroidWeapon(m_Type), 0, m_Pos + vec2(i * 38.0f, -16), vec2(0, 1), m_Pos); break;
	}
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 4;
}
void CSiegeEngine::OnHealthThreshold(int)
{
	// Each strict 70/35 transition adds one guard, with a hard cap of two.
	if(CountDroids(DROIDTYPE_BULWARK) < 2) new CBulwark(GameWorld(), m_Pos + vec2(CountDroids(DROIDTYPE_BULWARK) ? 80.0f : -80.0f, -20));
}
