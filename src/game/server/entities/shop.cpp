#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>
#include <game/weapons/weapons.h>
#include "character.h"
#include "building.h"
#include "shop.h"

CShop::CShop(CGameWorld *pGameWorld, vec2 Pos) : CBuilding(pGameWorld, Pos, BUILDING_SHOP, TEAM_NEUTRAL)
{
	m_ProximityRadius = 64;
	m_Life = 9000;

	m_Collision = false;
	m_Autofill = !GameServer()->m_pController->IsSurvival();

	for(int i = 0; i < 5; i++)
		m_aItem[i] = {};

	FillSlots();
}

void CShop::Reset()
{
	m_Life = 9000;
}

void CShop::FillSlots()
{
	for(int i = 0; i < 5; i++)
	{
		if(m_aItem[i].IsValid())
			continue;

		CWeaponSpec Spec = GameServer()->m_pController->GetRandomWeapon();
		while(!Spec.IsValid() || (Spec.m_DefinitionId == CWeaponCatalog::Static(SW_GUN1).m_DefinitionId ||
								  Spec.m_DefinitionId == CWeaponCatalog::Static(SW_GUN2).m_DefinitionId))
			Spec = GameServer()->m_pController->GetRandomWeapon();
		CWeaponDefinition Definition;
		CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition);

		if(i == 4)
		{
			for(int Try = 0; Try < 64 && (Definition.m_MaxLevel <= 0 ||
										  Spec.m_DefinitionId == CWeaponCatalog::Static(SW_UPGRADE).m_DefinitionId);
				Try++)
			{
				Spec = GameServer()->m_pController->GetRandomWeapon();
				CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition);
			}
			if(Definition.m_MaxLevel > 0)
			{
				Spec.m_Level = min(WEAPON_SPEC_MAX_LEVEL, Definition.m_MaxLevel + WEAPON_HIGH_TIER_SUPERCHARGE_STEP);
				m_aItem[i] = Spec;
			}
			else
				m_aItem[i] = Spec;
			continue;
		}
		if(frandom() < 0.1f)
		{
			Spec = CWeaponCatalog::Static(SW_UPGRADE);
			CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition);
		}

		if(Definition.m_MaxLevel > 0 && Spec.m_DefinitionId != CWeaponCatalog::Static(SW_UPGRADE).m_DefinitionId)
		{
			if(frandom() < 0.1f && Definition.m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
				Spec.m_Level = Definition.m_MaxLevel + WEAPON_HIGH_TIER_SUPERCHARGE_BONUS;
			else if(frandom() < 0.1f && Definition.m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
				Spec.m_Level = Definition.m_MaxLevel + WEAPON_HIGH_TIER_SUPERCHARGE_STEP;
			else if(frandom() < 0.1f && Definition.m_MaxLevel < WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
				Spec.m_Level = Definition.m_MaxLevel + WEAPON_LOW_TIER_SUPERCHARGE_BONUS;
			else if(frandom() < 0.1f && Definition.m_MaxLevel < WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
				Spec.m_Level = Definition.m_MaxLevel + WEAPON_LOW_TIER_SUPERCHARGE_STEP;
			else if(frandom() < 0.4f)
				Spec.m_Level = frandom() * (Definition.m_MaxLevel + 1);
			m_aItem[i] = Spec;
		}
		else
			m_aItem[i] = Spec;
	}
}

CWeaponSpec CShop::GetItem(int Slot)
{
	if(Slot < 0 || Slot >= 5)
		return {};

	return m_aItem[Slot];
}

void CShop::ClearItem(int Slot)
{
	if(Slot < 0 || Slot >= 5)
		return;

	m_aItem[Slot] = {};

	if(m_Autofill)
		FillSlots();
}

void CShop::SurvivalReset()
{
	for(int i = 0; i < 5; i++)
		ClearItem(i);

	FillSlots();
}

void CShop::Tick()
{
	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	/*
	if (m_Item >= 0)
	{
		CCharacter *apEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(m_Pos+vec2(0, -24), 16.0f, (CEntity**)apEnts, MAX_CLIENTS,
	CGameWorld::ENTTYPE_CHARACTER);

		int Bots = 0;
		bool Taken = false;

		for(int i = 0; i < Num; i++)
		{
			if (apEnts[i]->m_IsBot)
				Bots++;
			else
				Taken = apEnts[i]->GiveBuff(m_Item);
		}

		//if (Num - Bots > 0)
		if (Taken)
		{
			m_Item = -1;
			m_ItemTakenTick = GameServer()->Server()->Tick();
		}
	}
	*/
}

void CShop::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	m_SnapTick = Server()->Tick();

	CNetObj_Shop *pP = static_cast<CNetObj_Shop *>(Server()->SnapNewItem(NETOBJTYPE_SHOP, m_ID, sizeof(CNetObj_Shop)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Team = m_Team;
	pP->m_Item1DefinitionId = static_cast<int>(m_aItem[0].m_DefinitionId);
	pP->m_Item1Level = m_aItem[0].m_Level;
	pP->m_Item2DefinitionId = static_cast<int>(m_aItem[1].m_DefinitionId);
	pP->m_Item2Level = m_aItem[1].m_Level;
	pP->m_Item3DefinitionId = static_cast<int>(m_aItem[2].m_DefinitionId);
	pP->m_Item3Level = m_aItem[2].m_Level;
	pP->m_Item4DefinitionId = static_cast<int>(m_aItem[3].m_DefinitionId);
	pP->m_Item4Level = m_aItem[3].m_Level;
	const bool ShowPremium = GameServer()->m_pPveDirector && SnappingClient >= 0 &&
							 GameServer()->m_pPveDirector->PerkStacks(SnappingClient, PVE_CARD_PREMIUM_STOCK);
	pP->m_Item5DefinitionId = ShowPremium ? static_cast<int>(m_aItem[4].m_DefinitionId) : 0;
	pP->m_Item5Level = ShowPremium ? m_aItem[4].m_Level : 0;
}
