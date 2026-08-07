#include <base/system.h>
#include <engine/external/json-parser/json.h>

#include "pve_roguelite.h"

#include <generated/pve_cards.inc>
#include <generated/pve_contracts.inc>

namespace
{
// Cards and contracts are defined in data/pve/*.json, embedded at build time
// and loaded by PveLoadDefinitions, so new definitions no longer require
// recompiling the game.

static CPveCardDef gs_aCards[NUM_PVE_CARDS];
static CPveContractDef gs_aContracts[NUM_PVE_CONTRACTS];
static char gs_aCardNames[NUM_PVE_CARDS][32];
static char gs_aCardDescriptions[NUM_PVE_CARDS][256];
static char gs_aCardShortDescriptions[NUM_PVE_CARDS][96];
static char gs_aContractNames[NUM_PVE_CONTRACTS][32];
static char gs_aContractRules[NUM_PVE_CONTRACTS][160];
static char gs_aContractRisks[NUM_PVE_CONTRACTS][64];
static bool gs_PveDefinitionsLoaded = false;
static bool gs_PveLoadFailed = false;

static bool PveLoadCard(const json_value &Entry, int Index)
{
	if(Entry.type != json_object)
		return false;
	CPveCardDef &Def = gs_aCards[Index];
	Def.m_ID = (json_int_t)Entry["id"];
	if(Def.m_ID != Index)
		return false;
	str_copy(gs_aCardNames[Index], (const char *)Entry["name"], sizeof(gs_aCardNames[Index]));
	str_copy(gs_aCardDescriptions[Index], (const char *)Entry["description"], sizeof(gs_aCardDescriptions[Index]));
	str_copy(gs_aCardShortDescriptions[Index], (const char *)Entry["short_description"], sizeof(gs_aCardShortDescriptions[Index]));
	Def.m_pName = gs_aCardNames[Index];
	Def.m_pDescription = gs_aCardDescriptions[Index];
	Def.m_pShortDescription = gs_aCardShortDescriptions[Index];
	Def.m_Rarity = (json_int_t)Entry["rarity"];
	Def.m_MaxStacks = (json_int_t)Entry["max_stacks"];
	Def.m_ResearchCost = (json_int_t)Entry["research_cost"];
	Def.m_Base = (bool)Entry["base"];
	Def.m_Legendary = (bool)Entry["legendary"];
	Def.m_Keywords = (json_int_t)Entry["keywords"];
	Def.m_NumPrerequisites = 0;
	const json_value &Prereqs = Entry["prerequisites"];
	if(Prereqs.type != json_array)
		return false;
	for(int i = 0; i < 3 && i < (int)Prereqs.u.array.length; i++)
	{
		if(Prereqs[i].type != json_integer)
			return false;
		Def.m_aPrerequisites[Def.m_NumPrerequisites++] = (json_int_t)Prereqs[i];
	}
	Def.m_Tab = (json_int_t)Entry["tab"];
	Def.m_Branch = (json_int_t)Entry["branch"];
	Def.m_Tier = (json_int_t)Entry["tier"];
	Def.m_Mode = (json_int_t)Entry["mode"];
	Def.m_Specialization = (json_int_t)Entry["specialization"];
	return true;
}

static bool PveLoadContract(const json_value &Entry, int Index)
{
	if(Entry.type != json_object)
		return false;
	CPveContractDef &Def = gs_aContracts[Index];
	Def.m_ID = (json_int_t)Entry["id"];
	if(Def.m_ID != Index)
		return false;
	str_copy(gs_aContractNames[Index], (const char *)Entry["name"], sizeof(gs_aContractNames[Index]));
	str_copy(gs_aContractRules[Index], (const char *)Entry["rule"], sizeof(gs_aContractRules[Index]));
	str_copy(gs_aContractRisks[Index], (const char *)Entry["risk"], sizeof(gs_aContractRisks[Index]));
	Def.m_pName = gs_aContractNames[Index];
	Def.m_pRule = gs_aContractRules[Index];
	Def.m_pRisk = gs_aContractRisks[Index];
	Def.m_Mode = (json_int_t)Entry["mode"];
	return true;
}

bool PveLoadDefinitions(char *pError, int ErrorSize)
{
	if(gs_PveDefinitionsLoaded)
		return true;
	if(gs_PveLoadFailed)
		return false;

	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aError[128];
	json_value *pCards = json_parse_ex(
		&Settings, reinterpret_cast<const char *>(gs_aPveCardsJson), gs_aPveCardsJsonSize, aError);
	if(!pCards || pCards->type != json_array)
	{
		str_copy(pError, aError[0] ? aError : "invalid pve_cards.json", ErrorSize);
		if(pCards)
			json_value_free(pCards);
		gs_PveLoadFailed = true;
		return false;
	}
	json_value *pContracts = json_parse_ex(
		&Settings, reinterpret_cast<const char *>(gs_aPveContractsJson), gs_aPveContractsJsonSize, aError);
	if(!pContracts || pContracts->type != json_array)
	{
		str_copy(pError, aError[0] ? aError : "invalid pve_contracts.json", ErrorSize);
		json_value_free(pCards);
		if(pContracts)
			json_value_free(pContracts);
		gs_PveLoadFailed = true;
		return false;
	}
	if((int)pCards->u.array.length != NUM_PVE_CARDS || (int)pContracts->u.array.length != NUM_PVE_CONTRACTS)
	{
		str_format(pError, ErrorSize, "pve definitions count mismatch: %d cards, %d contracts",
				   (int)pCards->u.array.length, (int)pContracts->u.array.length);
		json_value_free(pCards);
		json_value_free(pContracts);
		gs_PveLoadFailed = true;
		return false;
	}
	bool Ok = true;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
	{
		if(!PveLoadCard((*pCards)[i], i))
		{
			str_format(pError, ErrorSize, "invalid card entry %d", i);
			Ok = false;
			break;
		}
	}
	for(int i = 0; Ok && i < NUM_PVE_CONTRACTS; i++)
	{
		if(!PveLoadContract((*pContracts)[i], i))
		{
			str_format(pError, ErrorSize, "invalid contract entry %d", i);
			Ok = false;
			break;
		}
	}
	json_value_free(pCards);
	json_value_free(pContracts);
	if(!Ok)
	{
		gs_PveLoadFailed = true;
		return false;
	}
	gs_PveDefinitionsLoaded = true;
	return true;
}

// Safe fallback used when definitions cannot be loaded: UI text lookups must
// never receive a null text pointer (Localize would crash).
static const CPveCardDef &PveEmptyCard()
{
	static CPveCardDef s_Empty;
	static const char *const s_apEmptyText[] = {"", "", ""};
	if(!s_Empty.m_pName)
	{
		s_Empty.m_pName = s_apEmptyText[0];
		s_Empty.m_pDescription = s_apEmptyText[1];
		s_Empty.m_pShortDescription = s_apEmptyText[2];
		s_Empty.m_Rarity = PVE_RARITY_COMMON;
		s_Empty.m_MaxStacks = 1;
		s_Empty.m_Base = true;
	}
	return s_Empty;
}

static const CPveContractDef &PveEmptyContract()
{
	static CPveContractDef s_Empty;
	static const char *const s_apEmptyText[] = {"", "", ""};
	if(!s_Empty.m_pName)
	{
		s_Empty.m_pName = s_apEmptyText[0];
		s_Empty.m_pRule = s_apEmptyText[1];
		s_Empty.m_pRisk = s_apEmptyText[2];
	}
	return s_Empty;
}


bool ValidatePrerequisiteDfs(int ID, int *pState)
{
	if(pState[ID] == 1)
		return false;
	if(pState[ID] == 2)
		return true;
	pState[ID] = 1;
	const CPveCardDef &Def = gs_aCards[ID];
	for(int i = 0; i < Def.m_NumPrerequisites; i++)
	{
		const int Prerequisite = Def.m_aPrerequisites[i];
		if(Prerequisite < 0 || Prerequisite >= NUM_PVE_CARDS || !ValidatePrerequisiteDfs(Prerequisite, pState))
			return false;
	}
	pState[ID] = 2;
	return true;
}
} // namespace

CPveResearchMask::CPveResearchMask(unsigned long long Low, unsigned long long High)
{
	m_aWords[0] = Low;
	m_aWords[1] = High;
}

bool CPveResearchMask::Test(int CardID) const
{
	return CardID >= 0 && CardID < 128 && (m_aWords[CardID / 64] & (1ULL << (CardID % 64))) != 0;
}

void CPveResearchMask::Set(int CardID, bool Value)
{
	if(CardID < 0 || CardID >= 128)
		return;
	const unsigned long long Bit = 1ULL << (CardID % 64);
	if(Value)
		m_aWords[CardID / 64] |= Bit;
	else
		m_aWords[CardID / 64] &= ~Bit;
}

bool CPveResearchMask::PrerequisitesMet(int CardID) const
{
	const CPveCardDef *pDef = PveCardDef(CardID);
	if(!pDef)
		return false;
	for(int i = 0; i < pDef->m_NumPrerequisites; i++)
		if(!PveCardIsUnlocked(pDef->m_aPrerequisites[i], *this))
			return false;
	return true;
}

void CPveResearchMask::Sanitize()
{
	for(int ID = 0; ID < 128; ID++)
		if(ID >= NUM_PVE_CARDS || (ID < NUM_PVE_CARDS && PveCardIsBase(ID)))
			Set(ID, false);
	for(int Pass = 0; Pass < NUM_PVE_CARDS; Pass++)
	{
		bool Changed = false;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(Test(ID) && !PrerequisitesMet(ID))
			{
				Set(ID, false);
				Changed = true;
			}
		if(!Changed)
			break;
	}
}

bool CPveResearchMask::operator==(const CPveResearchMask &Other) const
{
	return m_aWords[0] == Other.m_aWords[0] && m_aWords[1] == Other.m_aWords[1];
}

const CPveCardDef *PveCardDef(int ID)
{
	// Load on demand: the client may render selection UI before the server
	// has triggered PveValidateDefinitions.
	if(!gs_PveDefinitionsLoaded && !gs_PveLoadFailed)
	{
		char aError[128];
		PveLoadDefinitions(aError, sizeof(aError));
	}
	if(ID < 0 || ID >= NUM_PVE_CARDS)
		return 0;
	return gs_PveDefinitionsLoaded ? &gs_aCards[ID] : &PveEmptyCard();
}

const CPveContractDef *PveContractDef(int ID)
{
	if(!gs_PveDefinitionsLoaded && !gs_PveLoadFailed)
	{
		char aError[128];
		PveLoadDefinitions(aError, sizeof(aError));
	}
	if(ID < 0 || ID >= NUM_PVE_CONTRACTS)
		return 0;
	return gs_PveDefinitionsLoaded ? &gs_aContracts[ID] : &PveEmptyContract();
}

const char *PveChoiceName(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	if(pDef)
		return pDef->m_pName;
	if(ID == PVE_SUPPLY_ARMOR)
		return "Emergency Armor";
	if(ID == PVE_SUPPLY_AMMO)
		return "Full Ammunition";
	if(ID == PVE_SUPPLY_KITS)
		return "Emergency Kits";
	return "Unknown perk";
}

const char *PveChoiceDescription(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	if(pDef)
		return pDef->m_pShortDescription;
	if(ID == PVE_SUPPLY_ARMOR)
		return "+25 armor now";
	if(ID == PVE_SUPPLY_AMMO)
		return "Refill all ammo now";
	if(ID == PVE_SUPPLY_KITS)
		return "+5 kits now";
	return "";
}

const char *PveRarityName(int Rarity)
{
	if(Rarity == PVE_RARITY_RARE)
		return "Rare";
	if(Rarity == PVE_RARITY_EPIC)
		return "Epic";
	if(Rarity == PVE_RARITY_LEGENDARY)
		return "Legendary";
	return "Common";
}

const char *PveRewardReasonName(int Reason)
{
	if(Reason == PVE_REWARD_HORDE_SECTION)
		return "Horde section cleared";
	if(Reason == PVE_REWARD_EXTRACTION)
		return "Extraction completed";
	if(Reason == PVE_REWARD_CONTRACT)
		return "Contract completed";
	return "Invasion depth cleared";
}

bool PveCardIsBase(int ID)
{
	const CPveCardDef *pDef = PveCardDef(ID);
	return pDef && pDef->m_Base;
}

bool PveCardIsUnlocked(int ID, const CPveResearchMask &ResearchMask)
{
	return PveCardIsBase(ID) || (ID >= 0 && ID < NUM_PVE_CARDS && ResearchMask.Test(ID));
}

CPveResearchMask PveSanitizeResearchMask(CPveResearchMask ResearchMask)
{
	ResearchMask.Sanitize();
	return ResearchMask;
}

bool PveResearchMaskIsValid(const CPveResearchMask &ResearchMask)
{
	return PveSanitizeResearchMask(ResearchMask) == ResearchMask;
}

bool PveContractAvailableInMode(int ContractID, int Mode)
{
	const CPveContractDef *pDef = PveContractDef(ContractID);
	return pDef && (pDef->m_Mode == PVE_MODE_ANY || pDef->m_Mode == Mode);
}

bool PveValidateDefinitions(char *pError, int ErrorSize)
{
	if(!PveLoadDefinitions(pError, ErrorSize))
		return false;

	bool aCardIDs[NUM_PVE_CARDS] = {false};
	int BaseCards = 0;
	int NewBaseCards = 0;
	int LegendaryCards = 0;
	int NewResearchCost = 0;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
	{
		const CPveCardDef &Def = gs_aCards[i];
		if(Def.m_ID < 0 || Def.m_ID >= NUM_PVE_CARDS || aCardIDs[Def.m_ID] || Def.m_ID != i)
		{
			str_format(pError, ErrorSize, "invalid or duplicate card id %d", Def.m_ID);
			return false;
		}
		aCardIDs[Def.m_ID] = true;
		if(!Def.m_pName || !Def.m_pName[0] || str_length(Def.m_pName) > 28 || !Def.m_pShortDescription ||
		   !Def.m_pShortDescription[0] || str_length(Def.m_pShortDescription) > 88 ||
		   str_find(Def.m_pShortDescription, "/stack") || str_find(Def.m_pShortDescription, " · ") ||
		   str_find(Def.m_pShortDescription, ":") || str_find(Def.m_pShortDescription, " +") ||
		   str_find(Def.m_pShortDescription, " -"))
		{
			str_format(pError, ErrorSize, "invalid short description for card %d", i);
			return false;
		}
		if(Def.m_MaxStacks < 1 || Def.m_MaxStacks > 3 || (Def.m_Rarity != PVE_RARITY_COMMON && Def.m_MaxStacks != 1) ||
		   Def.m_Legendary != (Def.m_Rarity == PVE_RARITY_LEGENDARY))
		{
			str_format(pError, ErrorSize, "invalid rarity or stack rule for card %d", i);
			return false;
		}
		if(Def.m_Base)
		{
			BaseCards++;
			NewBaseCards += i >= 40 && i <= 51;
			if(Def.m_ResearchCost != 0 || Def.m_NumPrerequisites != 0)
			{
				str_format(pError, ErrorSize, "base card %d has research metadata", i);
				return false;
			}
		}
		else if(i >= PVE_CARD_PREDATOR)
			NewResearchCost += Def.m_ResearchCost;
		LegendaryCards += Def.m_Legendary;
		if(Def.m_NumPrerequisites < 0 || Def.m_NumPrerequisites > 3)
		{
			str_format(pError, ErrorSize, "invalid prerequisite count at card %d", i);
			return false;
		}
	}
	int aState[NUM_PVE_CARDS] = {0};
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!ValidatePrerequisiteDfs(ID, aState))
		{
			str_format(pError, ErrorSize, "cyclic or invalid prerequisite at card %d", ID);
			return false;
		}
	if(BaseCards != 19 || NewBaseCards != 12 || LegendaryCards != 8 || NewResearchCost < 150 || NewResearchCost > 165)
	{
		str_format(pError,
				   ErrorSize,
				   "invalid content totals: base=%d newbase=%d legendary=%d cost=%d",
				   BaseCards,
				   NewBaseCards,
				   LegendaryCards,
				   NewResearchCost);
		return false;
	}

	bool aContractIDs[NUM_PVE_CONTRACTS] = {false};
	for(int i = 0; i < NUM_PVE_CONTRACTS; i++)
	{
		const int ID = gs_aContracts[i].m_ID;
		if(ID < 0 || ID >= NUM_PVE_CONTRACTS || aContractIDs[ID] || ID != i)
		{
			str_format(pError, ErrorSize, "invalid or duplicate contract id %d", ID);
			return false;
		}
		aContractIDs[ID] = true;
	}

	if(pError && ErrorSize > 0)
		pError[0] = 0;
	return true;
}

static_assert(PVE_CARD_NO_ONE_LEFT == 39, "legacy card IDs must remain stable");
static_assert(PVE_CARD_MARKING_ROUNDS == 40, "new base card IDs must start at 40");
static_assert(PVE_CARD_PREDATOR == 52, "new research card IDs must start at 52");
static_assert(NUM_PVE_CARDS == 100, "PvE Roguelite must contain exactly 100 cards");
static_assert(NUM_PVE_CONTRACTS == 20, "PvE Roguelite must contain exactly 20 contracts");
