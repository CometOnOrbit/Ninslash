#include <generated/protocol.h>
#include <game/server/core/gamecontext.h>
#include "droid_overseer_core.h"
#include "droid_bulwark.h"
#include "droid_assembler.h"
#include <game/server/entities/actors/character.h>
#include <game/server/entities/structures/building.h>
#include "pve_drone.h"
#include <game/server/entities/combat/projectile.h>

namespace
{
class COverseerShieldNode : public CBuilding
{
	COverseerCore *m_pCore;
	int m_ExpireTick;
	int m_Index;
	static int s_Alive;
public:
	COverseerShieldNode(CGameWorld *pWorld, vec2 Pos, COverseerCore *pCore, int Index) :
		CBuilding(pWorld, Pos, BUILDING_PVE_SHIELD_NODE, TEAM_NEUTRAL),
		m_pCore(pCore),
		m_ExpireTick(Server()->Tick() + Server()->TickSpeed() * 18),
		m_Index(Index)
	{
		m_Life = m_MaxLife = 260;
		m_Collision = false;
		m_CanMove = false;
		m_Moving = false;
		s_Alive++;
	}
	~COverseerShieldNode() override { s_Alive--; }
	static bool CanSpawn() { return s_Alive < 4; }
	static int Alive() { return s_Alive; }
	static bool Protects(CGameWorld *pWorld, COverseerCore *pCore)
	{
		for(CBuilding *pBuilding = (CBuilding *)pWorld->FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding; pBuilding = (CBuilding *)pBuilding->TypeNext())
		{
			COverseerShieldNode *pNode = dynamic_cast<COverseerShieldNode *>(pBuilding);
			if(pNode && pNode->m_pCore == pCore && pNode->m_Life > 0)
				return true;
		}
		return false;
	}
	void Reset() override { GameWorld()->DestroyEntity(this); }
	void Tick() override
	{
		COverseerCore *pLivingCore = 0;
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_pCore && pDroid->m_Health > 0)
			{
				pLivingCore = m_pCore;
				break;
			}
		if(!pLivingCore || Server()->Tick() >= m_ExpireTick || m_Life <= 0)
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
		const float Angle = Server()->Tick() * 0.025f + m_Index * 2.0f * pi / 3.0f;
		m_Pos = pLivingCore->m_Pos + vec2(cosf(Angle) * 125.0f, sinf(Angle) * 85.0f - 25.0f);
		if((Server()->Tick() % Server()->TickSpeed()) == 0)
		{
			pLivingCore->m_Health = min(pLivingCore->m_MaxHealth, pLivingCore->m_Health + 10);
			GameServer()->CreateEffect(FX_SMALLELECTRIC, m_Pos);
		}
	}
};
int COverseerShieldNode::s_Alive = 0;
}

COverseerCore::COverseerCore(CGameWorld *pWorld, vec2 Pos) :
	CSpecialistDroid(pWorld, Pos, DROIDTYPE_OVERSEER_CORE, 4400, true),
	m_EmpTick(0),
	m_Burst(0),
	m_OrbitAngle(frandom() * 2.0f * pi)
{
	m_apAssemblers[0] = m_apAssemblers[1] = 0;
}

void COverseerCore::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	if(Dmg > 0 && COverseerShieldNode::Protects(GameWorld(), this))
		Dmg = max(1, Dmg * 40 / 100);
	CSpecialistDroid::TakeDamage(Force, Dmg, Source, Pos);
}

void COverseerCore::MovementTick(CCharacter *pTarget)
{
	// The Core is a true flying boss. It orbits above and around its target,
	// accelerates decisively when displaced, and relies on MoveBox to slide
	// along real map geometry rather than crossing it.
		m_OrbitAngle += 0.022f;
	vec2 Desired = m_StartPos + vec2(cosf(m_OrbitAngle) * 200.0f, -150.0f + sinf(m_OrbitAngle * 0.7f) * 55.0f);
	if(pTarget)
		Desired = pTarget->m_Pos + vec2(cosf(m_OrbitAngle) * 280.0f, -170.0f + sinf(m_OrbitAngle) * 95.0f);
	vec2 HitPos, BeforeHit;
	if(GameServer()->Collision()->IntersectLine(m_Pos, Desired, &HitPos, &BeforeHit))
	{
		const vec2 Forward = normalize(Desired - m_Pos);
		const vec2 Side(-Forward.y, Forward.x);
		vec2 Best = Desired;
		float BestRemaining = 1e9f;
		for(float Sign : {-1.0f, 1.0f})
		{
			const vec2 Candidate = BeforeHit - Forward * 70.0f + Side * Sign * 190.0f;
			vec2 CandidateHit, CandidateBefore;
			if(GameServer()->Collision()->TestBox(Candidate, CollisionSize()) ||
				GameServer()->Collision()->IntersectLine(m_Pos, Candidate, &CandidateHit, &CandidateBefore))
				continue;
			const float Remaining = distance(Candidate, Desired);
			if(Remaining < BestRemaining)
			{
				BestRemaining = Remaining;
				Best = Candidate;
			}
		}
		if(BestRemaining < 1e8f)
			Desired = Best;
	}
	vec2 ToDesired = Desired - m_Pos;
	if(length(ToDesired) > 1.0f)
		m_Vel += normalize(ToDesired) * min(0.95f, length(ToDesired) * 0.0075f);
	m_Vel *= 0.93f;
	const float MaxSpeed = pTarget ? 13.0f : 8.0f;
	if(length(m_Vel) > MaxSpeed)
		m_Vel = normalize(m_Vel) * MaxSpeed;
	// Flying droids must include one-way platforms in their downward collision
	// pass; otherwise they visually settle halfway through platform surfaces.
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, CollisionSize(), 0, false, true);
	m_Anim = 2;
}

void COverseerCore::AbilityTick()
{
	if(AcquireTarget(1400.0f))
	{
		// Sustained three-shot tracking bursts make remaining near the Core
		// dangerous while preserving visible projectile travel and dodge time.
		FireProjectile(30, 0.02f);
		FireProjectile(22, 0.06f);
		FireProjectile(22, 0.06f);
		m_Burst++;
		if((m_Burst % 2) == 0)
		{
			for(int i = 0; i < 8; i++)
			{
				const float Angle = i * 2.0f * pi / 8.0f + m_OrbitAngle;
				const vec2 Dir(cosf(Angle), sinf(Angle));
				const CAttackSource Source = CAttackSource::Droid(NEUTRAL_BASE, m_Type);
				CWeaponCombatProfile Combat;
				CWeaponCatalog::TryResolveAttack(Source, &Combat);
				new CProjectile(&GameServer()->m_World, Source,
					m_Pos + m_Center + Dir * 38.0f, Dir, vec2(0, 0), Server()->TickSpeed() * 2,
					18, Combat.m_ProjectileKnockback, -1);
			}
			m_AttackTick = Server()->Tick();
		}
	}
	if(Server()->Tick() >= m_EmpTick)
	{
		CCharacter *apChars[MAX_CLIENTS]; int Num = GameServer()->m_World.FindEntities(m_Pos, 420.0f, (CEntity **)apChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++) apChars[i]->Electrocute(5.0f);
		for(CEntity *pEnt = GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext()) if(CPveDrone *pDrone = dynamic_cast<CPveDrone *>(pEnt)) if(distance(m_Pos, pDrone->m_Pos) <= 600.0f) pDrone->ApplyEmp(Server()->TickSpeed() * 4);
		GameServer()->CreateEffect(FX_ELECTRIC, m_Pos); m_EmpTick = Server()->Tick() + Server()->TickSpeed() * 8;
	}
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 2 / 5;
}
void COverseerCore::OnHealthThreshold(int Threshold) { SpawnPhase(Threshold); }
void COverseerCore::SpawnPhase(int Threshold)
{
	// Four total active phase assets: shield nodes plus Assemblers.
	int Assemblers = CountDroids(DROIDTYPE_ASSEMBLER);
	int Assets = Assemblers + COverseerShieldNode::Alive();
	if(Threshold == 75)
	{
		if(Assets++ < 4 && COverseerShieldNode::CanSpawn()) new COverseerShieldNode(GameWorld(), m_Pos + vec2(-110, -70), this, 0);
		if(Assets++ < 4 && COverseerShieldNode::CanSpawn()) new COverseerShieldNode(GameWorld(), m_Pos + vec2(110, -70), this, 1);
	}
	else
	{
		if(Assets++ < 4 && Assemblers++ < 2) m_apAssemblers[0] = new CAssembler(GameWorld(), m_Pos + vec2(-80, -20));
		if(Assets++ < 4 && Assemblers++ < 2) m_apAssemblers[1] = new CAssembler(GameWorld(), m_Pos + vec2(80, -20));
	}
}

void COverseerCore::OnSpecialistDeath()
{
	for(int i = 0; i < 2; i++)
	{
		for(CDroid *pDroid = (CDroid *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_DROID); pDroid; pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid == m_apAssemblers[i])
			{
				GameWorld()->DestroyEntity(pDroid);
				break;
			}
		m_apAssemblers[i] = 0;
	}
}
