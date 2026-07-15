#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_bulwark.h"
CBulwark::CBulwark(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_BULWARK, 850, false) {}
void CBulwark::AbilityTick()
{
	// Stay close to vulnerable support specialists before engaging players.
	CDroid *apDroids[32];
	const int Num = GameServer()->m_World.FindEntities(m_Pos, 700.0f, (CEntity **)apDroids, 32, CGameWorld::ENTTYPE_DROID);
	CDroid *pSupport = 0;
	float Best = 700.0f;
	for(int i = 0; i < Num; i++)
	{
		if(!apDroids[i] || apDroids[i]->m_Health <= 0 || (apDroids[i]->m_Type != DROIDTYPE_ASSEMBLER && apDroids[i]->m_Type != DROIDTYPE_RAILGUNNER)) continue;
		float Dist = distance(m_Pos, apDroids[i]->m_Pos);
		if(Dist < Best) { Best = Dist; pSupport = apDroids[i]; }
	}
	if(pSupport && Best > 180.0f) m_Vel.x += (pSupport->m_Pos.x < m_Pos.x ? -1.0f : 1.0f) * 1.2f;
	else if(AcquireTarget(500.0f)) FireProjectile(8, .08f);
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed();
}
