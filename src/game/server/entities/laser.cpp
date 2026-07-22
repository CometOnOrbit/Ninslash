#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "laser.h"
#include "building.h"
#include "droid.h"

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, const CAttackSource &Source, int Damage, int Charge, int Penetration)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Damage = Damage;
	m_Pos = Pos;
	m_Source = Source;
	m_Owner = Source.m_Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	//m_OwnerBuilding = OwnerBuilding;
	m_Charge = Charge;
	m_InfinitePenetration = Penetration == WEAPON_INFINITE_PENETRATION;
	m_RemainingPenetrations = max(0, Penetration);
	
	if (m_Charge == -1)
		m_Bounces = 99;
	else
		m_Bounces = 0;
	m_EvalTick = 0;
	m_IgnoreScythe = -1;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	
	CWeaponDefinition Definition;
	if (m_Source.m_Kind == EAttackSourceKind::PlayerWeapon && CWeaponCatalog::TryGetDefinition(m_Source.m_Weapon.m_DefinitionId, &Definition) && Definition.m_Kind == EWeaponDefinitionKind::Static && Definition.m_StaticType == SW_GRENADE2)
		pOwnerChar = NULL;
	
	CCharacter *pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar);
	if(!pHit)
		return false;
	
	if (pHit->GetPlayer()->GetCID() == m_IgnoreScythe)
		return false;
	
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	
	pHit->TakeDamage(m_Source, m_Damage, normalize(To-From)*0.1f, At);
	
	return true;
}


bool CLaser::HitScythe(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit = GameServer()->m_World.IntersectReflect(m_Pos, To, 0.0f, At, pOwnerChar);
	if(!pHit)
		return false;

	if (pHit->GetPlayer()->GetCID() == m_IgnoreScythe)
		return false;
	
	m_From = From;
	m_Pos = At;
	
	//vec2 d = (pHit->m_Pos+vec2(0, -24))-From;
	//d += vec2(frandom()-frandom(), frandom()-frandom()) * length(d) * 0.4f;
	//m_Dir = -normalize(d);
	//m_Dir = normalize(vec2(frandom()-0.5f, frandom()-0.5f));
	
	vec2 d = (pHit->m_Pos+vec2(0, -24)) - From;
	m_Dir = GameServer()->Collision()->Reflect(m_Dir, normalize(d));
	
	GameServer()->CreateBuildingHit(m_Pos);
	m_IgnoreScythe = pHit->GetPlayer()->GetCID();
	
	return true;
}


bool CLaser::HitMonster(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwnerChar)
		return false;
	
	CDroid *pHit = GameServer()->m_World.IntersectWalker(m_Pos, To, 8.0f, At);
	if(!pHit)
		return false;
	
	m_From = From;
	m_Pos = At;
	m_Energy = -1;

	pHit->TakeDamage(normalize(To-From)*0.1f, m_Damage, m_Source, At);
	return true;
}

bool CLaser::HitBuilding(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if (!pOwnerChar)
		return false;
	
	CBuilding *pHit = GameServer()->m_World.IntersectBuilding(m_Pos, To, 8.0f, At, pOwnerChar->GetPlayer()->GetTeam(), m_OwnerBuilding);
	if(!pHit)
		return false;
	
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	DamageBuilding(pHit, At);
	return true;
}

void CLaser::DamageBuilding(CBuilding *pHit, vec2 At)
{
	if (pHit->m_Type == BUILDING_GENERATOR)
	{
		pHit->m_DamagePos = At;
		
		if (distance(pHit->m_Pos, At) > pHit->m_ProximityRadius)
		{
			GameServer()->CreateEffect(FX_SHIELDHIT, At);
			pHit->TakeDamage(m_Damage/3, m_Source);
		}
		else
		{
			GameServer()->CreateBuildingHit(At);
			pHit->TakeDamage(m_Damage, m_Source);
		}
	}
	else
	{
		GameServer()->CreateBuildingHit(At);
		pHit->TakeDamage(m_Damage, m_Source);
	}
}

bool CLaser::HitPenetratingTargets(vec2 From, vec2 To)
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pIgnoredCharacter = NULL;
	CDroid *pIgnoredDroid = NULL;
	vec2 SearchFrom = From;

	while(true)
	{
		vec2 CharacterAt = To;
		CCharacter *pCharacter = GameServer()->m_World.IntersectCharacter(
			SearchFrom, To, 0.0f, CharacterAt, pOwnerChar, false, NULL, 0.0f, pIgnoredCharacter);

		vec2 DroidAt = To;
		CDroid *pDroid = pOwnerChar ? GameServer()->m_World.IntersectWalker(SearchFrom, To, 8.0f, DroidAt, pIgnoredDroid) : NULL;

		vec2 BuildingAt = To;
		CBuilding *pBuilding = pOwnerChar ? GameServer()->m_World.IntersectBuilding(
			SearchFrom, To, 8.0f, BuildingAt, pOwnerChar->GetPlayer()->GetTeam(), m_OwnerBuilding) : NULL;

		enum
		{
			HIT_NONE,
			HIT_CHARACTER,
			HIT_DROID,
			HIT_BUILDING,
		};
		int HitType = HIT_NONE;
		vec2 At = To;
		float BestDistanceSquared = dot(To - SearchFrom, To - SearchFrom) * 10000.0f;

		if(pCharacter)
		{
			BestDistanceSquared = dot(CharacterAt - SearchFrom, CharacterAt - SearchFrom);
			At = CharacterAt;
			HitType = HIT_CHARACTER;
		}
		if(pDroid)
		{
			const float DistanceSquared = dot(DroidAt - SearchFrom, DroidAt - SearchFrom);
			if(DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				At = DroidAt;
				HitType = HIT_DROID;
			}
		}
		if(pBuilding)
		{
			const float DistanceSquared = dot(BuildingAt - SearchFrom, BuildingAt - SearchFrom);
			if(DistanceSquared <= BestDistanceSquared)
			{
				At = BuildingAt;
				HitType = HIT_BUILDING;
			}
		}

		if(HitType == HIT_NONE)
			return false;

		if(HitType == HIT_BUILDING)
		{
			m_From = From;
			m_Pos = At;
			m_Energy = -1;
			DamageBuilding(pBuilding, At);
			return true;
		}

		if(HitType == HIT_CHARACTER)
		{
			pCharacter->TakeDamage(m_Source, m_Damage, normalize(To - From) * 0.1f, At);
			pIgnoredCharacter = pCharacter;
		}
		else
		{
			pDroid->TakeDamage(normalize(To - From) * 0.1f, m_Damage, m_Source, At);
			pIgnoredDroid = pDroid;
		}

		if(!m_InfinitePenetration && m_RemainingPenetrations <= 0)
		{
			m_From = From;
			m_Pos = At;
			m_Energy = -1;
			return true;
		}

		if(!m_InfinitePenetration)
			--m_RemainingPenetrations;
		SearchFrom = At + m_Dir;
		if(dot(To - SearchFrom, m_Dir) <= 0.0f)
			return false;
	}
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	
	if (GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		m_Energy = -1;
	
	if(m_Energy < 0)
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}

	vec2 To = m_Pos + m_Dir * m_Energy;
	vec2 ColPos;

	int Collision = GameServer()->Collision()->IntersectLine(m_Pos, To, &ColPos, &To);
	
	const vec2 From = m_Pos;
	if(HitScythe(From, To))
		return;

	const bool Penetrating = m_InfinitePenetration || m_RemainingPenetrations > 0;
	if(Penetrating)
	{
		if(HitPenetratingTargets(From, To))
			return;
	}
	else if(HitCharacter(From, To) || HitBuilding(From, To) || HitMonster(From, To))
		return;

	m_From = From;
	m_Pos = To;
	if(!Collision)
	{
		m_Energy = -1;
		return;
	}

	m_Dir = GameServer()->Collision()->WallReflect(ColPos, m_Dir, Collision);
	m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
	++m_Bounces;
	if(m_Bounces > 4)
		m_Energy = -1;
	m_IgnoreScythe = -1;

	if (GameServer()->Collision()->CheckBlocks(m_Pos))
		GameServer()->DamageBlocks(m_Pos, m_Damage, 1);
	else if (GameServer()->Collision()->CheckBlocks(m_Pos+vec2(-4, -4)))
		GameServer()->DamageBlocks(m_Pos+vec2(-4, -4), m_Damage, 1);
	else if (GameServer()->Collision()->CheckBlocks(m_Pos+vec2(4, -4)))
		GameServer()->DamageBlocks(m_Pos+vec2(4, -4), m_Damage, 1);
	else if (GameServer()->Collision()->CheckBlocks(m_Pos+vec2(-4, 4)))
		GameServer()->DamageBlocks(m_Pos+vec2(-4, 4), m_Damage, 1);
	else if (GameServer()->Collision()->CheckBlocks(m_Pos+vec2(4, 4)))
		GameServer()->DamageBlocks(m_Pos+vec2(4, 4), m_Damage, 1);

	GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE);
}

void CLaser::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CLaser::Tick()
{
	if(Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*GameServer()->Tuning()->m_LaserBounceDelay)/1000.0f)
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_Charge = m_Charge;
	pObj->m_StartTick = m_EvalTick;
}
