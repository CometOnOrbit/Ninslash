#include "script_entity.h"

#include <generated/protocol.h>
#include <game/collision.h>
#include <game/server/gamecontext.h>

#include "character.h"

namespace
{
constexpr float FIXED_VELOCITY_SCALE = 100.0f;
constexpr int AREA_DAMAGE_INTERVAL = 5;
} // namespace

CScriptEntity::CScriptEntity(CGameWorld *pGameWorld, const CAttackSource &Source, const CScriptEntitySpec &Spec)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_SCRIPTED), m_Source(Source), m_Velocity(Spec.m_Velocity),
	  m_From(Spec.m_From), m_Kind(Spec.m_Kind), m_LifeTicks(max(1, Spec.m_LifeTicks)), m_Damage(max(0, Spec.m_Damage)),
	  m_Bounces(max(0, Spec.m_Bounces)), m_Gravity(Spec.m_Gravity)
{
	m_Pos = Spec.m_Pos;
	m_ProximityRadius = max(0, Spec.m_Radius);
	mem_copy(m_aState, Spec.m_State, sizeof(m_aState));
	GameWorld()->InsertEntity(this);
}

void CScriptEntity::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CScriptEntity::DestroySelf()
{
	GameWorld()->DestroyEntity(this);
}

bool CScriptEntity::HitCharacters(vec2 From, vec2 To)
{
	vec2 Hit = To;
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Source.m_Owner);
	CCharacter *pTarget = GameWorld()->IntersectCharacter(From, To, m_ProximityRadius, Hit, pOwner);
	if(!pTarget)
		return false;
	const vec2 Force = length(m_Velocity) > 0.001f ? normalize(m_Velocity) * max(0, m_Damage) * 0.15f : vec2(0, 0);
	pTarget->TakeDamage(m_Source, m_Damage, Force, Hit);
	m_Pos = Hit;
	return true;
}

void CScriptEntity::Tick()
{
	if(--m_LifeTicks < 0)
	{
		DestroySelf();
		return;
	}

	if(m_Kind == SCRIPT_ENTITY_RAY)
	{
		vec2 Hit = m_Pos;
		GameServer()->Collision()->IntersectLine(m_From, m_Pos, 0, &Hit);
		HitCharacters(m_From, Hit);
		m_Pos = Hit;
		return;
	}

	if(m_Kind == SCRIPT_ENTITY_AREA)
	{
		if(m_Damage > 0 && Server()->Tick() % AREA_DAMAGE_INTERVAL == 0)
		{
			CEntity *apEntities[MAX_CLIENTS];
			const int Count = GameWorld()->FindEntities(
				m_Pos, m_ProximityRadius, apEntities, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Count; ++i)
			{
				CCharacter *pCharacter = static_cast<CCharacter *>(apEntities[i]);
				if(pCharacter != GameServer()->GetPlayerChar(m_Source.m_Owner))
					pCharacter->TakeDamage(m_Source, m_Damage, vec2(0, 0), m_Pos);
			}
		}
		return;
	}

	if(m_Kind == SCRIPT_ENTITY_SUMMON)
	{
		CCharacter *pTarget =
			GameWorld()->ClosestCharacter(m_Pos, 700.0f, GameServer()->GetPlayerChar(m_Source.m_Owner));
		if(pTarget)
		{
			vec2 Direction = pTarget->m_Pos - m_Pos;
			if(length(Direction) > 0.001f)
				m_Velocity += normalize(Direction) * 0.12f;
		}
	}

	m_Velocity.y += m_Gravity / FIXED_VELOCITY_SCALE;
	const vec2 Previous = m_Pos;
	vec2 Current = m_Pos + m_Velocity;
	int Collision = GameServer()->Collision()->IntersectLine(Previous, Current, &Current, 0);
	const bool Hit = HitCharacters(Previous, Current);
	if(Hit)
	{
		DestroySelf();
		return;
	}
	if(Collision)
	{
		if(m_Bounces-- > 0)
		{
			m_Velocity = GameServer()->Collision()->WallReflect(Current, m_Velocity, Collision);
			m_Pos = Current;
		}
		else
			DestroySelf();
		return;
	}
	m_Pos = Current;
	if(GameLayerClipped(m_Pos))
		DestroySelf();
}

void CScriptEntity::TickPaused()
{
	// Lifetimes are decremented only by Tick(), which the paused world skips.
}

void CScriptEntity::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CNetObj_ScriptEntity *pObj = static_cast<CNetObj_ScriptEntity *>(
		Server()->SnapNewItem(NETOBJTYPE_SCRIPTENTITY, m_ID, sizeof(CNetObj_ScriptEntity)));
	if(!pObj)
		return;
	pObj->m_Kind = m_Kind;
	pObj->m_X = round_to_int(m_Pos.x);
	pObj->m_Y = round_to_int(m_Pos.y);
	pObj->m_VelX = round_to_int(m_Velocity.x * FIXED_VELOCITY_SCALE);
	pObj->m_VelY = round_to_int(m_Velocity.y * FIXED_VELOCITY_SCALE);
	pObj->m_FromX = round_to_int(m_From.x);
	pObj->m_FromY = round_to_int(m_From.y);
	pObj->m_Radius = round_to_int(m_ProximityRadius);
	pObj->m_Life = m_LifeTicks;
	pObj->m_SourceKind = static_cast<int>(m_Source.m_Kind);
	pObj->m_SourceType = m_Source.m_Type;
	pObj->m_WeaponDefinitionId = static_cast<int>(m_Source.m_Weapon.m_DefinitionId);
	pObj->m_WeaponLevel = m_Source.m_Weapon.m_Level;
	pObj->m_State0 = m_aState[0];
	pObj->m_State1 = m_aState[1];
	pObj->m_State2 = m_aState[2];
	pObj->m_State3 = m_aState[3];
	pObj->m_State4 = m_aState[4];
	pObj->m_State5 = m_aState[5];
	pObj->m_State6 = m_aState[6];
	pObj->m_State7 = m_aState[7];
}
