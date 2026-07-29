#include <cassert>
#include <cstring>

#include <game/pve_roguelite.h>

int main()
{
	char aError[256] = {0};
	assert(PveValidateDefinitions(aError, sizeof(aError)));

	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		const CPveCardDef *pDef = PveCardDef(ID);
		assert(pDef);
		assert(pDef->m_pName && pDef->m_pName[0]);
		assert(pDef->m_pDescription && pDef->m_pDescription[0]);
		assert(pDef->m_pShortDescription && pDef->m_pShortDescription[0]);
		assert(std::strlen(pDef->m_pName) <= 28);
		assert(std::strlen(pDef->m_pShortDescription) <= 88);
		assert(std::strstr(pDef->m_pShortDescription, "/stack") == nullptr);
		assert(std::strstr(pDef->m_pShortDescription, " · ") == nullptr);
		assert(std::strstr(pDef->m_pShortDescription, ":") == nullptr);
		assert(std::strstr(pDef->m_pShortDescription, " +") == nullptr);
		assert(std::strstr(pDef->m_pShortDescription, " -") == nullptr);
		assert(std::strcmp(PveChoiceDescription(ID), pDef->m_pShortDescription) == 0);
	}

	const CPveCardDef *pCombatTraining = PveCardDef(PVE_CARD_COMBAT_TRAINING);
	const CPveCardDef *pFocus = PveCardDef(PVE_CARD_FOCUS_DRILL);
	const CPveCardDef *pBlast = PveCardDef(PVE_CARD_BLAST_BATTERY);
	const CPveCardDef *pVoltage = PveCardDef(PVE_CARD_VOLTAGE_BANK);
	const CPveCardDef *pFury = PveCardDef(PVE_CARD_FURY_METER);
	assert(std::strcmp(pCombatTraining->m_pName, "All Damage") == 0);
	assert(std::strcmp(pCombatTraining->m_pShortDescription, "All damage is 8% higher for each stack, up to three stacks.") == 0);
	assert(std::strcmp(pFocus->m_pName, "Bonus Shot Damage") == 0);
	assert(std::strcmp(pFocus->m_pShortDescription, "Every 10 firearm hits, the next shot deals 15% more damage per stack.") == 0);
	assert(std::strcmp(pBlast->m_pName, "Bonus Blast Damage") == 0);
	assert(std::strcmp(pBlast->m_pShortDescription, "Every 5 enemy hits, the next blast deals 8% more damage per stack.") == 0);
	assert(std::strcmp(pVoltage->m_pName, "Extra Chain Target") == 0);
	assert(std::strcmp(pVoltage->m_pShortDescription, "Every 10 electric hits, the next hit chains to one extra target per stack.") == 0);
	assert(std::strcmp(pFury->m_pName, "Bonus Melee Damage") == 0);
	assert(std::strcmp(pFury->m_pShortDescription, "Every 10 melee hits, the next hit deals 20% more damage per stack.") == 0);

	assert(PveChoiceDescription(PVE_SUPPLY_ARMOR)[0]);
	assert(PveChoiceDescription(PVE_SUPPLY_AMMO)[0]);
	assert(PveChoiceDescription(PVE_SUPPLY_KITS)[0]);
	return 0;
}
