#include <base/math.h>
#include <base/system.h>

#include <game/server/core/playerdata.h>

CPlayerData::CPlayerData(const char *pName, int ColorID)
{
	m_pChild1 = 0;
	m_pChild2 = 0;
	str_copy(m_aName, pName, sizeof(m_aName));
	m_ColorID = ColorID;
	Reset();
}

void CPlayerData::Die()
{
}

void CPlayerData::Add(CPlayerData *pPlayerData)
{
	if(pPlayerData->m_ColorID < m_ColorID)
	{
		if(m_pChild1)
			m_pChild1->Add(pPlayerData);
		else
			m_pChild1 = pPlayerData;
	}
	else
	{
		if(m_pChild2)
			m_pChild2->Add(pPlayerData);
		else
			m_pChild2 = pPlayerData;
	}
}

CPlayerData *CPlayerData::Get(const char *pName, int ColorID)
{
	if(ColorID == m_ColorID && str_comp(pName, m_aName) == 0)
		return this;
	if(ColorID < m_ColorID)
		return m_pChild1 ? m_pChild1->Get(pName, ColorID) : 0;
	return m_pChild2 ? m_pChild2->Get(pName, ColorID) : 0;
}

int CPlayerData::GetHighScore(int Score)
{
	int Result = max(m_Score, Score);
	if(m_pChild1)
		Result = max(Result, m_pChild1->GetHighScore(Result));
	if(m_pChild2)
		Result = max(Result, m_pChild2->GetHighScore(Result));
	return Result;
}

int CPlayerData::GetPlayerCount(int Score)
{
	if(m_pChild1)
		Score += m_pChild1->GetPlayerCount(0);
	if(m_pChild2)
		Score += m_pChild2->GetPlayerCount(0);
	return Score + 1;
}

void CPlayerData::Reset()
{
	ResetWeapons();
	m_Armor = 0;
	m_Kits = 0;
	m_Score = 0;
	m_Gold = 0;
	ClearPveRun();
}

void CPlayerData::ResetWeapons()
{
	m_WeaponDataVersion = WEAPON_DATA_VERSION;
	for(int i = 0; i < 99; i++)
	{
		m_aWeaponDefinitionId[i] = 0;
		m_aWeaponLevel[i] = 0;
		m_aWeaponAmmo[i] = 0;
	}
}

void CPlayerData::ClearPveRun()
{
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		m_aPvePerks[i] = 0;
	m_PveChoices = 0;
	m_PveRunMode = PVE_MODE_ANY;
	m_PveUsedContracts = 0;
	m_PveInvasionFloors = 0;
	m_PveStageSuppliesApplied = false;
	m_PveLastStandUsed = false;
	m_PveEmergencyPlatingUsed = false;
	m_PveContractParticipant = false;
	m_PveContractNonce = 0;
	m_PvePendingArmor = 0;
	m_PvePendingKits = 0;
	m_PvePendingAmmo = false;
	m_PveLegendaryCard = -1;
	for(int i = 0; i < 4; i++)
		m_aPveWeaponResources[i] = 0;
	m_PveBarrier = 0;
	m_PveDroneModule = PVE_DRONE_NONE;
	m_PveDroneSwitchReadyTick = 0;
	m_PveDeathlessFloors = 0;
}
