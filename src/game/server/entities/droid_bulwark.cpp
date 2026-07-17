#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_bulwark.h"
CBulwark::CBulwark(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_BULWARK, 1050, false) {}
void CBulwark::AbilityTick()
{
	// Stay close to vulnerable support specialists before engaging players.
	CDroid *apDroids[32];
	const int Num = GameServer()->m_World.FindEntities(m_Pos, 700.0f, (CEntity **)apDroids, 32, CGameWorld::ENTTYPE_DROID);
	CDroid *pSupport = 0;
	float BestDistanceSquared = 700.0f * 700.0f;
	for(int i = 0; i < Num; i++)
	{
		if(!apDroids[i] || apDroids[i]->m_Health <= 0 || (apDroids[i]->m_Type != DROIDTYPE_ASSEMBLER && apDroids[i]->m_Type != DROIDTYPE_RAILGUNNER)) continue;
		const vec2 Delta = m_Pos - apDroids[i]->m_Pos;
		const float DistanceSquared = dot(Delta, Delta);
		if(DistanceSquared < BestDistanceSquared) { BestDistanceSquared = DistanceSquared; pSupport = apDroids[i]; }
	}
	if(pSupport && BestDistanceSquared > 180.0f * 180.0f) SetMovementGoal(pSupport->m_Pos, Server()->TickSpeed() * 2);
	else if(AcquireTarget(680.0f)) FireProjectile(28, .035f);
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 3 / 10;
}
