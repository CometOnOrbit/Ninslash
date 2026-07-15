#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_overseer_core.h"
#include "droid_bulwark.h"
#include "droid_assembler.h"
#include "character.h"
#include "pve_drone.h"

namespace
{
class COverseerShieldNode : public CEntity
{
	COverseerCore *m_pCore;
	int m_ExpireTick;
	static int s_Alive;
public:
	COverseerShieldNode(CGameWorld *pWorld, vec2 Pos, COverseerCore *pCore) : CEntity(pWorld, CGameWorld::ENTTYPE_LASER), m_pCore(pCore), m_ExpireTick(Server()->Tick() + Server()->TickSpeed() * 18) { m_Pos = Pos; s_Alive++; GameWorld()->InsertEntity(this); }
	~COverseerShieldNode() override { s_Alive--; }
	static bool CanSpawn() { return s_Alive < 4; }
	static int Alive() { return s_Alive; }
	void Reset() override { GameWorld()->DestroyEntity(this); }
	void Tick() override { if(!m_pCore || m_pCore->m_Health <= 0 || Server()->Tick() >= m_ExpireTick) { GameWorld()->DestroyEntity(this); return; } if((Server()->Tick() % Server()->TickSpeed()) == 0) { m_pCore->m_Health = min(m_pCore->m_MaxHealth, m_pCore->m_Health + 20); GameServer()->CreateEffect(FX_SMALLELECTRIC, m_Pos); } }
	void Snap(int) override {}
};
int COverseerShieldNode::s_Alive = 0;
}

COverseerCore::COverseerCore(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_OVERSEER_CORE, 3600, true), m_EmpTick(0) {}
void COverseerCore::AbilityTick()
{
	if(AcquireTarget(1200.0f)) { FireProjectile(16, .03f); FireProjectile(16, .18f); }
	if(Server()->Tick() >= m_EmpTick)
	{
		CCharacter *apChars[MAX_CLIENTS]; int Num = GameServer()->m_World.FindEntities(m_Pos, 420.0f, (CEntity **)apChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++) apChars[i]->Electrocute(3.0f);
		for(CEntity *pEnt = GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext()) if(CPveDrone *pDrone = dynamic_cast<CPveDrone *>(pEnt)) if(distance(m_Pos, pDrone->m_Pos) <= 600.0f) pDrone->ApplyEmp(Server()->TickSpeed() * 4);
		GameServer()->CreateEffect(FX_ELECTRIC, m_Pos); m_EmpTick = Server()->Tick() + Server()->TickSpeed() * 8;
	}
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed();
}
void COverseerCore::OnHealthThreshold(int Threshold) { SpawnPhase(Threshold); }
void COverseerCore::SpawnPhase(int Threshold)
{
	// Four total active phase assets: shield nodes plus Assemblers.
	int Assemblers = CountDroids(DROIDTYPE_ASSEMBLER);
	int Assets = Assemblers + COverseerShieldNode::Alive();
	if(Threshold == 75)
	{
		if(Assets++ < 4 && COverseerShieldNode::CanSpawn()) new COverseerShieldNode(GameWorld(), m_Pos + vec2(-110, -70), this);
		if(Assets++ < 4 && COverseerShieldNode::CanSpawn()) new COverseerShieldNode(GameWorld(), m_Pos + vec2(110, -70), this);
	}
	else
	{
		if(Assets++ < 4 && Assemblers++ < 2) new CAssembler(GameWorld(), m_Pos + vec2(-80, -20));
		if(Assets++ < 4 && Assemblers++ < 2) new CAssembler(GameWorld(), m_Pos + vec2(80, -20));
	}
}
