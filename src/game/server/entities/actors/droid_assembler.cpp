#include <generated/protocol.h>
#include <game/server/core/gamecontext.h>
#include "droid_assembler.h"
#include <game/server/entities/actors/droid.h>

namespace
{
class CRepairOrb : public CEntity
{
	CDroid *m_pTarget;
	int m_ExpireTick;
	static int s_Alive;
public:
	CRepairOrb(CGameWorld *pWorld, vec2 Pos, CDroid *pTarget) : CEntity(pWorld, CGameWorld::ENTTYPE_PROJECTILE), m_pTarget(pTarget), m_ExpireTick(Server()->Tick() + Server()->TickSpeed() * 5)
	{
		m_Pos = Pos; m_ProximityRadius = 8; s_Alive++; GameWorld()->InsertEntity(this);
	}
	~CRepairOrb() override { s_Alive--; }
	static bool CanSpawn() { return s_Alive < 6; }
	bool TargetAlive()
	{
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_pTarget)
				return pDroid->m_Health > 0;
		return false;
	}
	void Reset() override { GameWorld()->DestroyEntity(this); }
	void Tick() override
	{
		if(Server()->Tick() >= m_ExpireTick || !TargetAlive()) { GameWorld()->DestroyEntity(this); return; }
		vec2 To = m_pTarget->m_Pos + m_pTarget->m_Center - m_Pos;
		if(length(To) < 24.0f) { m_pTarget->m_Health = min(m_pTarget->m_MaxHealth, m_pTarget->m_Health + 60); GameServer()->CreateRepairInd(m_Pos); GameWorld()->DestroyEntity(this); return; }
		m_Pos += normalize(To) * 10.0f;
		if((Server()->Tick() & 7) == 0) GameServer()->CreateEffect(FX_SMALLELECTRIC, m_Pos);
	}
	void Snap(int) override {}
};
int CRepairOrb::s_Alive = 0;
}
CAssembler::CAssembler(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_ASSEMBLER, 720, false) {}
void CAssembler::AbilityTick()
{
	// Electric suppression or recent incoming fire interrupts fabrication.
	if(m_Status == DROIDSTATUS_ELECTRIC || Server()->Tick() < m_DamageTakenTick + Server()->TickSpeed() * 2) { m_AbilityTick = Server()->Tick() + Server()->TickSpeed(); return; }
	CDroid *apDroids[48];
	int Num = GameServer()->m_World.FindEntities(m_Pos, 600.0f, (CEntity **)apDroids, 48, CGameWorld::ENTTYPE_DROID);
	CDroid *pMostDamaged = 0;
	float Lowest = 1.0f;
	for(int i = 0; i < Num; i++) if(apDroids[i] && apDroids[i] != this && apDroids[i]->m_Health > 0 && apDroids[i]->m_Health < apDroids[i]->m_MaxHealth) { float Ratio = (float)apDroids[i]->m_Health / apDroids[i]->m_MaxHealth; if(Ratio < Lowest) { Lowest = Ratio; pMostDamaged = apDroids[i]; } }
	if(pMostDamaged)
	{
		SetMovementGoal(pMostDamaged->m_Pos, Server()->TickSpeed() * 3);
		if(CRepairOrb::CanSpawn())
			new CRepairOrb(GameWorld(), m_Pos + m_Center, pMostDamaged);
	}
	else if(AcquireTarget(760.0f))
		FireProjectile(22, 0.055f);
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed() / 2;
}
