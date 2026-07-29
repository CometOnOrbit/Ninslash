#include <math.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/components/camera.h>
#include <game/client/components/binds.h>
#include <game/client/components/build_placement.h>
#include <game/client/components/effects.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/pve_progress_storage.h>
#include <game/client/skelebank.h>
#include <game/questinfo.h>
#include <game/tutorial.h>

#include "pve_roguelite.h"

namespace
{
float UiEaseOutCubic(float Amount)
{
	const float Remaining = 1.0f - clamp(Amount, 0.0f, 1.0f);
	return 1.0f - Remaining * Remaining * Remaining;
}

float UiStagger(float Amount, int Index)
{
	const float Delay = 0.055f * Index;
	return UiEaseOutCubic(clamp((Amount - Delay) / (1.0f - Delay), 0.0f, 1.0f));
}

float UiConfirmPulse(float Remaining)
{
	if(Remaining <= 0.0f)
		return 0.0f;
	return sinf(clamp(1.0f - Remaining, 0.0f, 1.0f) * pi);
}

struct CPveUiIcon
{
	int m_Image;
	int m_Sprite;
	float m_Scale;

	CPveUiIcon(int Image = IMAGE_WEAPONS, int Sprite = SPRITE_PICKUP_ARMOR, float Scale = 1.0f) :
		m_Image(Image),
		m_Sprite(Sprite),
		m_Scale(Scale)
	{
	}
};

CPveUiIcon PveSpecializationIcon(int Specialization)
{
	if(Specialization == PVE_SPECIALIZATION_FIREARM)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_WEAPON_STATIC2, 1.35f);
	if(Specialization == PVE_SPECIALIZATION_EXPLOSIVE)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_WEAPON_STATIC7, 1.35f);
	if(Specialization == PVE_SPECIALIZATION_ELECTRIC)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_WEAPON_STATIC20, 1.05f);
	if(Specialization == PVE_SPECIALIZATION_MELEE)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_WEAPON_STATIC9, 1.35f);
	return CPveUiIcon();
}

CPveUiIcon PveBranchIcon(int Tab, int Branch)
{
	if(Tab == PVE_TAB_WEAPON)
		return PveSpecializationIcon(PVE_SPECIALIZATION_FIREARM + clamp(Branch, 0, 3));
	if(Tab == PVE_TAB_MODE)
	{
		const int aSprites[3] = {SPRITE_PICKUP_ARMOR, SPRITE_PICKUP_KIT, SPRITE_PICKUP_COIN};
		return CPveUiIcon(IMAGE_WEAPONS, aSprites[clamp(Branch, 0, 2)]);
	}
	if(Branch == 0)
		return PveSpecializationIcon(PVE_SPECIALIZATION_FIREARM);
	return CPveUiIcon(IMAGE_WEAPONS, Branch == 1 ? SPRITE_PICKUP_ARMOR : SPRITE_PICKUP_KIT);
}

CPveUiIcon PveCardIcon(const CPveCardDef *pDef)
{
	if(!pDef)
		return CPveUiIcon();
	if(pDef->m_Specialization != PVE_SPECIALIZATION_NONE)
		return PveSpecializationIcon(pDef->m_Specialization);

	switch(pDef->m_ID)
	{
	case PVE_CARD_REINFORCED_PLATES:
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_ARMOR);
	case PVE_CARD_FIELD_SUPPLIES:
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_KIT);
	case PVE_CARD_SCAVENGER:
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_COIN);
	case PVE_CARD_AMMO_RESERVE:
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_AMMO);
	case PVE_CARD_FIRST_AID:
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_HEALTH);
	default:
		return PveBranchIcon(pDef->m_Tab, pDef->m_Branch);
	}
}

CPveUiIcon PveChoiceIcon(int ID, const CPveCardDef *pCard)
{
	if(pCard)
		return PveCardIcon(pCard);
	if(ID == PVE_SUPPLY_AMMO)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_AMMO);
	if(ID == PVE_SUPPLY_KITS)
		return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_KIT);
	return CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_ARMOR);
}

int PveResearchRoute(const CPveCardDef *pDef)
{
	if(!pDef)
		return 0;
	if(pDef->m_Tab == PVE_TAB_WEAPON)
		return pDef->m_Tier <= 3 ? 0 : (pDef->m_Tier <= 6 ? 1 : 2);
	if(pDef->m_Tab == PVE_TAB_MODE)
		return pDef->m_Tier <= 3 ? 0 : (pDef->m_Tier <= 5 ? 1 : 2);
	if(pDef->m_Tab == PVE_TAB_CORE)
	{
		if(pDef->m_Branch == 3)
			return pDef->m_Tier <= 4 ? 0 : 1;
		return pDef->m_Tier <= 4 ? 0 : 1;
	}
	return 0;
}

int PveResearchRouteCount(int Tab, int Branch)
{
	(void)Branch;
	if(Tab == PVE_TAB_WEAPON || Tab == PVE_TAB_MODE)
		return 3;
	return 2;
}

const char *PveResearchRouteName(int Tab, int Branch, int Route)
{
	static const char *s_apCore[4][2] = {
		{"Combat Foundation", "Vulnerable Mastery"},
		{"Defense Foundation", "Barrier Mastery"},
		{"Logistics Foundation", "War Economy"},
		{"Drone Modules", "Drone Firmware"}};
	static const char *s_apWeapon[4][3] = {
		{"Firearm Foundation", "Focus Mastery", "Vulnerable Tactics"},
		{"Explosive Foundation", "Blast Mastery", "Siege Tactics"},
		{"Electric Foundation", "Voltage Mastery", "Grid Tactics"},
		{"Melee Foundation", "Fury Mastery", "Wound Tactics"}};
	static const char *s_apMode[3][3] = {
		{"Invasion Foundation", "Deep Run", "Exploration"},
		{"Horde Foundation", "Endless Defense", "Fortification"},
		{"Extraction Foundation", "Final Evacuation", "Signal Control"}};
	if(Tab == PVE_TAB_CORE)
		return s_apCore[clamp(Branch, 0, 3)][clamp(Route, 0, 1)];
	if(Tab == PVE_TAB_WEAPON)
		return s_apWeapon[clamp(Branch, 0, 3)][clamp(Route, 0, 2)];
	return s_apMode[clamp(Branch, 0, 2)][clamp(Route, 0, 2)];
}

// ponytail: local copies of picker DrawCircle + pie fan; no need to open picker.cpp
void DrawDroneWheelCircle(IGraphics *pGraphics, float x, float y, float r, int Segments)
{
	IGraphics::CFreeformItem Array[32];
	int NumItems = 0;
	const float FSegments = (float)Segments;
	for(int i = 0; i < Segments; i += 2)
	{
		const float a1 = i / FSegments * 2 * pi;
		const float a2 = (i + 1) / FSegments * 2 * pi;
		const float a3 = (i + 2) / FSegments * 2 * pi;
		Array[NumItems++] = IGraphics::CFreeformItem(
			x, y,
			x + cosf(a1) * r, y + sinf(a1) * r,
			x + cosf(a3) * r, y + sinf(a3) * r,
			x + cosf(a2) * r, y + sinf(a2) * r);
		if(NumItems == 32)
		{
			pGraphics->QuadsDrawFreeform(Array, 32);
			NumItems = 0;
		}
	}
	if(NumItems)
		pGraphics->QuadsDrawFreeform(Array, NumItems);
}

void DrawDroneWheelSlice(IGraphics *pGraphics, float x, float y, float r, float a0, float a1, int Segments)
{
	float Span = a1 - a0;
	if(Span <= 0.0f)
		Span += 2.0f * pi;
	IGraphics::CFreeformItem Array[32];
	int NumItems = 0;
	const float FSegments = (float)max(2, Segments);
	for(int i = 0; i < Segments; i += 2)
	{
		const float t1 = i / FSegments;
		const float t2 = min(1.0f, (i + 1) / FSegments);
		const float t3 = min(1.0f, (i + 2) / FSegments);
		const float A1 = a0 + Span * t1;
		const float A2 = a0 + Span * t2;
		const float A3 = a0 + Span * t3;
		Array[NumItems++] = IGraphics::CFreeformItem(
			x, y,
			x + cosf(A1) * r, y + sinf(A1) * r,
			x + cosf(A3) * r, y + sinf(A3) * r,
			x + cosf(A2) * r, y + sinf(A2) * r);
		if(NumItems == 32)
		{
			pGraphics->QuadsDrawFreeform(Array, 32);
			NumItems = 0;
		}
		if(t3 >= 1.0f)
			break;
	}
	if(NumItems)
		pGraphics->QuadsDrawFreeform(Array, NumItems);
}
}

CPveRoguelite::CPveRoguelite()
{
	m_ProgressStorageWritable = true;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		m_aNodeButtonIDs[i] = i;
	for(int i = 0; i < 3; i++)
		m_aTabButtonIDs[i] = i;
	for(int i = 0; i < 4; i++)
		m_aBranchButtonIDs[i] = i;
	for(int i = 0; i < 3; i++)
		m_aRouteButtonIDs[i] = i;
	m_BuyButtonID = 0;
	m_CheckpointButtonID = 0;
	m_DebugChoiceScreenshotFrames = 0;
	m_DebugResearchScreenshotFrames = 0;
	m_DebugBuildScreenshotFrames = 0;
	m_DebugGameScreenshotFrames = 0;
	m_DebugGameScreenshotEarliestTime = 0;
	m_DebugScreenshotPage = -1;
	m_DebugBuildPreview = false;
	m_DroneTutorialSeen = g_Config.m_ClPveDroneTutorialSeen != 0;
	m_RenderWorld.m_pRoguelite = this;
	OnReset();
}

void CPveRoguelite::OnInit()
{
	LoadProgress();
	m_DroneTutorialSeen = g_Config.m_ClPveDroneTutorialSeen != 0;
}

void CPveRoguelite::OnRelease()
{
	SaveProgress();
}

void CPveRoguelite::LoadProgress()
{
	CPveProgressData Data;
	EPveProgressLoadResult Result = CPveProgressStorage::Load(Storage(), &Data);
	bool UsedBackup = false;
	if(Result == PVE_PROGRESS_LOAD_CORRUPT || Result == PVE_PROGRESS_LOAD_MISSING)
	{
		if(Result == PVE_PROGRESS_LOAD_CORRUPT)
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pve", "pve_progress.json is corrupt; trying backup");
		const EPveProgressLoadResult BackupResult = CPveProgressStorage::Load(Storage(), &Data, "pve_progress.json.bak");
		if(BackupResult != PVE_PROGRESS_LOAD_MISSING)
		{
			Result = BackupResult;
			UsedBackup = BackupResult == PVE_PROGRESS_LOAD_OK;
		}
	}
	if(Result == PVE_PROGRESS_LOAD_FUTURE_VERSION)
	{
		m_ProgressStorageWritable = false;
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pve", "pve_progress.json was created by a newer game version; it will not be overwritten");
		return;
	}
	if(Result == PVE_PROGRESS_LOAD_OK)
	{
		g_Config.m_ClPveProgressVersion = Data.m_ProgressVersion;
		g_Config.m_ClPveResearchPoints = Data.m_ResearchPoints;
		str_copy(g_Config.m_ClPveResearchMask, Data.m_aResearchMask, sizeof(g_Config.m_ClPveResearchMask));
		g_Config.m_ClPveHighestInvasion = Data.m_HighestInvasion;
		g_Config.m_ClPvePreferredCheckpoint = Data.m_PreferredCheckpoint;
		g_Config.m_ClPveDroneTutorialSeen = Data.m_DroneTutorialSeen ? 1 : 0;
		if(UsedBackup)
			SaveProgress();
		return;
	}

	// Values loaded from an older settings.cfg are the migration source. If
	// none exist, the same path creates a valid empty progress file.
	SaveProgress();
}

void CPveRoguelite::SaveProgress()
{
	if(g_Config.m_ClTutorialActive || !m_ProgressStorageWritable || !Storage())
		return;
	CPveProgressData Data;
	Data.m_ProgressVersion = g_Config.m_ClPveProgressVersion;
	Data.m_ResearchPoints = g_Config.m_ClPveResearchPoints;
	str_copy(Data.m_aResearchMask, g_Config.m_ClPveResearchMask, sizeof(Data.m_aResearchMask));
	Data.m_HighestInvasion = g_Config.m_ClPveHighestInvasion;
	Data.m_PreferredCheckpoint = g_Config.m_ClPvePreferredCheckpoint;
	Data.m_DroneTutorialSeen = g_Config.m_ClPveDroneTutorialSeen != 0;
	if(!CPveProgressStorage::Save(Storage(), Data))
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pve", "failed to save pve_progress.json");
}

void CPveRoguelite::OnConsoleInit()
{
	Console()->Register("pve_debug_choice", "?i", CFGFLAG_CLIENT, ConDebugChoice, this, "Preview three perk cards from a starting card ID");
	Console()->Register("pve_debug_contract", "?i?i", CFGFLAG_CLIENT, ConDebugContract, this, "Preview a contract ID as vote/active/success/failure (state 0-3)");
	Console()->Register("pve_debug_invasion_retry", "?i", CFGFLAG_CLIENT, ConDebugInvasionRetry, this, "Preview the Invasion retry vote or result (state 0-3)");
	Console()->Register("pve_debug_research", "?i?i", CFGFLAG_CLIENT, ConDebugResearch, this, "Preview and capture research states (0-2) with an optional tab (0-2)");
	Console()->Register("pve_debug_build", "?i", CFGFLAG_CLIENT, ConDebugBuild, this, "Preview and capture the PvE build HUD with an optional drone module (1-3)");
	Console()->Register("pve_debug_screenshot", "?i", CFGFLAG_CLIENT, ConDebugScreenshot, this, "Capture the current client UI after initialization, optionally forcing a UI page");
	Console()->Register("pve_debug_game_screenshot", "?i?i", CFGFLAG_CLIENT, ConDebugGameScreenshot, this, "Capture gameplay after a local character appears: optional frame and millisecond delays");
	Console()->Register("pve_drone_module", "i", CFGFLAG_CLIENT, ConDroneModule, this, "Switch support drone module: 1 assault, 2 guardian, 3 repair");
	Console()->Register("+dronewheel", "", CFGFLAG_CLIENT, ConKeyDroneWheel, this, "Hold to open the drone command wheel");
}

void CPveRoguelite::ConDroneModule(IConsole::IResult *pResult, void *pUserData)
{
	((CPveRoguelite *)pUserData)->SendDroneModule(pResult->GetInteger(0));
}

void CPveRoguelite::ConKeyDroneWheel(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	if(pSelf->m_aRunPerks[PVE_CARD_DRONE_CHASSIS] <= 0)
		return;

	if(pResult->GetInteger(0))
	{
		if(pSelf->ChoiceActive() || pSelf->m_ResearchVisible || pSelf->m_pClient->GameplayInputCaptured() || pSelf->m_pClient->m_pBuildPlacement->Active())
			return;
		pSelf->m_DroneWheelActive = true;
		pSelf->m_DroneWheelMouse = vec2(0.0f, 0.0f);
		pSelf->m_DroneWheelSelected = -1;
		pSelf->m_DroneTutorialSeen = true;
		g_Config.m_ClPveDroneTutorialSeen = 1;
		pSelf->SaveProgress();
		return;
	}

	if(!pSelf->m_DroneWheelActive)
		return;

	const int aModules[3] = {PVE_DRONE_ASSAULT, PVE_DRONE_GUARDIAN, PVE_DRONE_REPAIR};
	const int aCards[3] = {PVE_CARD_ASSAULT_MODULE, PVE_CARD_GUARDIAN_MODULE, PVE_CARD_REPAIR_MODULE};
	int aUnlocked[3];
	int Count = 0;
	for(int i = 0; i < 3; i++)
		if(pSelf->m_aRunPerks[aCards[i]] > 0)
			aUnlocked[Count++] = i;
	if(pSelf->m_DroneWheelSelected >= 0 && pSelf->m_DroneWheelSelected < Count &&
		pSelf->Client()->GameTick() >= pSelf->m_DroneSwitchReadyTick)
		pSelf->SendDroneModule(aModules[aUnlocked[pSelf->m_DroneWheelSelected]]);
	pSelf->m_DroneWheelActive = false;
	pSelf->m_DroneWheelSelected = -1;
}

void CPveRoguelite::ConDebugChoice(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	const int Start = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, NUM_PVE_CARDS - 1) : 0;
	pSelf->m_ContractVoteActive = false;
	pSelf->m_InvasionRetryVoteActive = false;
	pSelf->m_InvasionRetryResultActive = false;
	pSelf->m_ChoiceActive = true;
	pSelf->m_ChoiceNonce = 1;
	pSelf->m_ChoiceSequence = 1;
	pSelf->m_ChoiceEndTick = pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 10;
	for(int i = 0; i < 3; i++)
	{
		pSelf->m_aChoiceCards[i] = (Start + i) % NUM_PVE_CARDS;
		pSelf->m_aChoiceStacks[i] = i;
		pSelf->m_aCardFocus[i] = 0.0f;
	}
	pSelf->m_FocusedChoice = 1;
	pSelf->m_AppearAmount = 0.0f;
	pSelf->m_DebugChoiceScreenshotFrames = 12;
}

void CPveRoguelite::ConDebugContract(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	const int Start = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, NUM_PVE_CONTRACTS - 1) : 0;
	pSelf->m_ChoiceActive = false;
	pSelf->m_ContractVoteActive = true;
	pSelf->m_InvasionRetryVoteActive = false;
	pSelf->m_InvasionRetryResultActive = false;
	pSelf->m_ContractNonce = 1;
	const int State = pResult->NumArguments() > 1 ? clamp(pResult->GetInteger(1), 0, 3) : 0;
	if(State > 0)
	{
		pSelf->m_ContractVoteActive = false;
		pSelf->m_ActiveContract = Start;
		pSelf->m_ContractState = State == 1 ? PVE_CONTRACT_STATE_ACTIVE : (State == 2 ? PVE_CONTRACT_STATE_SUCCESS : PVE_CONTRACT_STATE_FAILED);
		pSelf->m_ContractProgress = State == 1 ? 1 : 3;
		pSelf->m_ContractTarget = 3;
		pSelf->m_ContractStatusEndTick = State == 1 ? pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 30 : 0;
		return;
	}
	pSelf->m_ContractEndTick = pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 8;
	pSelf->m_aContractOptions[0] = Start;
	pSelf->m_aContractOptions[1] = (Start + 1) % NUM_PVE_CONTRACTS;
	pSelf->m_aContractVotes[0] = 2;
	pSelf->m_aContractVotes[1] = 1;
	pSelf->m_SelectedContract = -1;
	pSelf->m_FocusedChoice = 0;
	for(int i = 0; i < 3; i++)
		pSelf->m_aCardFocus[i] = 0.0f;
	pSelf->m_AppearAmount = 0.0f;
}

void CPveRoguelite::ConDebugInvasionRetry(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	const int State = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, 3) : 0;
	pSelf->m_ChoiceActive = false;
	pSelf->m_ContractVoteActive = false;
	pSelf->m_InvasionRetryVoteActive = State == 0;
	pSelf->m_InvasionRetryResultActive = State != 0;
	pSelf->m_InvasionRetryNonce = 1;
	pSelf->m_InvasionRetryEndTick = pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 15;
	pSelf->m_InvasionRetryFloor = 12;
	pSelf->m_aInvasionRetryVotes[0] = 2;
	pSelf->m_aInvasionRetryVotes[1] = 1;
	pSelf->m_SelectedInvasionRetry = -1;
	pSelf->m_InvasionRetryResult = State > 0 ? State - 1 : PVE_INVASION_RETRY_RESULT_RETRY;
	pSelf->m_InvasionRetryResultEndTick = pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 5;
	str_copy(pSelf->m_aInvasionRetryPlayerName, "Player", sizeof(pSelf->m_aInvasionRetryPlayerName));
	pSelf->m_FocusedChoice = 0;
	for(int i = 0; i < 3; i++)
		pSelf->m_aCardFocus[i] = 0.0f;
	pSelf->m_AppearAmount = 0.0f;
	pSelf->m_DebugChoiceScreenshotFrames = 12;
}

void CPveRoguelite::ConDebugResearch(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	const int State = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, 2) : 1;
	g_Config.m_ClPveResearchPoints = State == 0 ? 0 : 999;
	CPveResearchMask FullMask;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID))
			FullMask.Set(ID);
	pSelf->StoreResearchMask(State == 2 ? FullMask : CPveResearchMask());
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		pSelf->m_aRunPerks[ID] = State == 2 ? PveCardDef(ID)->m_MaxStacks : 0;
	if(pResult->NumArguments() > 1)
	{
		pSelf->m_ResearchTab = clamp(pResult->GetInteger(1), 0, 2);
		pSelf->m_ResearchBranch = 0;
		pSelf->m_ResearchRoute = 0;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == pSelf->m_ResearchTab)
			{
				pSelf->m_SelectedResearch = ID;
				break;
			}
	}
	pSelf->m_DebugResearchScreenshotFrames = 12;
}

void CPveRoguelite::ConDebugBuild(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	pSelf->m_DebugBuildPreview = true;
	pSelf->m_DebugBuildScreenshotFrames = 12;
	pSelf->m_DebugScreenshotPage = -1;
	pSelf->m_Barrier = 24;
	pSelf->m_aWeaponResources[0] = 7;
	pSelf->m_aWeaponResources[1] = 5;
	pSelf->m_aWeaponResources[2] = 8;
	pSelf->m_aWeaponResources[3] = 10;
	pSelf->m_VulnerableTargets = 3;
	pSelf->m_BleedingTargets = 2;
	pSelf->m_LegendaryCard = PVE_CARD_PERFECT_SEQUENCE;
	pSelf->m_DroneModule = pResult->NumArguments() ? clamp(pResult->GetInteger(0), (int)PVE_DRONE_ASSAULT, (int)PVE_DRONE_REPAIR) : PVE_DRONE_GUARDIAN;
	pSelf->m_DroneSwitchReadyTick = pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 3 / 2;
}

void CPveRoguelite::ConDebugScreenshot(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	pSelf->m_DebugBuildPreview = false;
	pSelf->m_DebugBuildScreenshotFrames = 12;
	pSelf->m_DebugScreenshotPage = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 1, 15) : -1;
}

void CPveRoguelite::ConDebugGameScreenshot(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	pSelf->m_DebugGameScreenshotFrames = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 12, 600) : 30;
	const int DelayMs = pResult->NumArguments() > 1 ? clamp(pResult->GetInteger(1), 0, 30000) : 0;
	pSelf->m_DebugGameScreenshotEarliestTime = time_get() + time_freq() * DelayMs / 1000;
}

void CPveRoguelite::OnReset()
{
	m_TutorialMoveMask = 0;
	m_TutorialFireCount = 0;
	m_TutorialKillCount = 0;
	m_TutorialObjectiveSignature = -1;
	m_TutorialPerkChosen = false;
	m_TutorialNonce = 0;
	m_TutorialProgress = 0;
	m_TutorialTarget = 1;
	m_TutorialFlags = 0;
	if(m_DebugChoiceScreenshotFrames <= 0)
	{
		m_ChoiceActive = false;
		m_ChoiceNonce = 0;
		m_ChoiceSequence = 0;
		m_ChoiceEndTick = 0;
		m_ChoiceDismissAt = 0;
		for(int i = 0; i < 3; i++)
		{
			m_aChoiceCards[i] = -1;
			m_aChoiceStacks[i] = 0;
			m_aCardFocus[i] = 0.0f;
		}
		m_FocusedChoice = 0;
	}
	m_ContractVoteActive = false;
	m_ResearchVisible = false;
	m_ProgressSent = false;
	m_MouseTrigger = false;
	m_ContractNonce = 0;
	m_ContractEndTick = 0;
	if(m_DebugChoiceScreenshotFrames <= 0)
	{
		m_InvasionRetryVoteActive = false;
		m_InvasionRetryResultActive = false;
		m_InvasionRetryNonce = 0;
		m_InvasionRetryEndTick = 0;
		m_InvasionRetryFloor = 1;
		m_aInvasionRetryVotes[0] = 0;
		m_aInvasionRetryVotes[1] = 0;
		m_SelectedInvasionRetry = -1;
		m_InvasionRetryResult = PVE_INVASION_RETRY_RESULT_RESET;
		m_InvasionRetryResultEndTick = 0;
		m_aInvasionRetryPlayerName[0] = 0;
	}
	m_aContractOptions[0] = -1;
	m_aContractOptions[1] = -1;
	m_aContractVotes[0] = 0;
	m_aContractVotes[1] = 0;
	m_SelectedContract = -1;
	m_ActiveContract = -1;
	m_ContractState = PVE_CONTRACT_STATE_NONE;
	m_ContractProgress = 0;
	m_ContractTarget = 0;
	m_ContractStatusEndTick = 0;
	if(m_DebugBuildScreenshotFrames <= 0)
	{
		for(int i = 0; i < 4; i++)
			m_aWeaponResources[i] = 0;
		m_Barrier = 0;
		m_VulnerableTargets = 0;
		m_BleedingTargets = 0;
		m_LegendaryCard = -1;
		m_DroneModule = PVE_DRONE_NONE;
		m_DroneSwitchReadyTick = 0;
	}
	m_DroneNonce = 0;
	m_DroneHealth = 0;
	m_DroneState = PVE_DRONE_STATE_DEPLOYING;
	m_DroneActionTick = 0;
	m_DroneWheelActive = false;
	m_DroneWheelSelected = -1;
	m_DroneWheelMouse = vec2(0.0f, 0.0f);
	m_ValidationCode = 0;
	m_ValidationUntil = 0;
	if(m_DebugResearchScreenshotFrames <= 0)
	{
		m_SelectedResearch = PVE_CARD_FINISHER;
		m_ResearchTab = PVE_TAB_CORE;
		m_ResearchBranch = 0;
		m_ResearchRoute = 0;
	}
	m_ResearchNonce = 1;
	if(m_DebugResearchScreenshotFrames <= 0)
		for(int i = 0; i < NUM_PVE_CARDS; i++)
			m_aRunPerks[i] = 0;
	m_SelectorMouse = vec2(150.0f, 150.0f);
	m_AppearAmount = 0.0f;
	m_ResearchAppearAmount = 0.0f;
	for(int i = 0; i < 4; i++)
		m_aBranchExpand[i] = i == 0 ? 1.0f : 0.0f;
	for(int i = 0; i < 3; i++)
		m_aRouteExpand[i] = i == 0 ? 1.0f : 0.0f;
	m_ResearchProgressDisplay = 0.0f;
	m_SelectionPulse = 0.0f;
	m_ResearchAnimTab = -1;
}

void CPveRoguelite::AdvanceTutorial()
{
	if(g_Config.m_ClTutorialState != 1)
		return;
	if(m_TutorialNonce > 0)
	{
		CNetMsg_Cl_TutorialAction Msg;
		Msg.m_Action = TUTORIAL_ACTION_UI_CONTINUE;
		Msg.m_Nonce = m_TutorialNonce;
		Msg.m_Value = 0;
		Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
		return;
	}
	g_Config.m_ClTutorialCheckpoint = min(6, g_Config.m_ClTutorialCheckpoint + 1);
	dbg_msg("tutorial", "checkpoint %d reached", g_Config.m_ClTutorialCheckpoint);
	if(g_Config.m_ClTutorialCheckpoint >= 6)
	{
		g_Config.m_ClTutorialState = 2;
		dbg_msg("tutorial", "completed locally; no gameplay or account data was uploaded");
	}
	else if(g_Config.m_ClTutorialCheckpoint == 5 && m_TutorialPerkChosen)
		AdvanceTutorial();
}

void CPveRoguelite::SendTutorialAction(int Action, int Value)
{
	if(!g_Config.m_ClTutorialActive || m_TutorialNonce <= 0)
		return;
	CNetMsg_Cl_TutorialAction Msg;
	Msg.m_Action = Action;
	Msg.m_Nonce = m_TutorialNonce;
	Msg.m_Value = Value;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CPveRoguelite::TickTutorial()
{
	if(!g_Config.m_ClTutorialActive || g_Config.m_ClTutorialState != 1 || Client()->State() != IClient::STATE_ONLINE)
		return;
	if(g_Config.m_ClTutorialCheckpoint != 3 || !m_pClient->m_Snap.m_pGameDataObj)
		return;
	const int Quest = m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed;
	const int Level = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
	const int Pack = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue;
	const int Signature = Quest * 31 + Level * 131 + (Pack & 0xFF);
	if(m_TutorialObjectiveSignature < 0)
	{
		m_TutorialObjectiveSignature = Signature;
		return;
	}
	// The server changes quest/level/phase only after the objective has been
	// completed. Progress counters are deliberately excluded from the signature.
	if(Signature != m_TutorialObjectiveSignature)
		AdvanceTutorial();
}

void CPveRoguelite::OnGameOver()
{
	if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 2)
	{
		m_pClient->m_pMenus->FinishTutorial();
		return;
	}
	if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 1)
		dbg_msg("tutorial", "mission ended at checkpoint %d", g_Config.m_ClTutorialCheckpoint);
}

void CPveRoguelite::DrawTutorialHud()
{
	if(!g_Config.m_ClTutorialActive || g_Config.m_ClTutorialState != 1 || Client()->State() != IClient::STATE_ONLINE)
		return;
	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);
	char aMove[96], aLeft[48], aRight[48], aJump[64], aFire[64], aForge[64], aDrone[64];
	m_pClient->m_pBinds->GetKeys("+left", aLeft, sizeof(aLeft));
	m_pClient->m_pBinds->GetKeys("+right", aRight, sizeof(aRight));
	str_format(aMove, sizeof(aMove), "%s%s%s", aLeft, aLeft[0] && aRight[0] ? ", " : "", aRight);
	m_pClient->m_pBinds->GetKeys("+jump", aJump, sizeof(aJump));
	m_pClient->m_pBinds->GetKeys("+fire", aFire, sizeof(aFire));
	m_pClient->m_pBinds->GetKeys("+inventory", aForge, sizeof(aForge));
	m_pClient->m_pBinds->GetKeys("+dronewheel", aDrone, sizeof(aDrone));
	const char *pMove = aMove[0] ? aMove : "?";
	const char *pJump = aJump[0] ? aJump : "?";
	const char *pFire = aFire[0] ? aFire : "?";
	const char *pForge = aForge[0] ? aForge : "?";
	const char *pDrone = aDrone[0] ? aDrone : "?";
	static const char *s_apChapterNames[6] = {"First Deployment", "Combat and Recovery", "PvE Mission", "Forge and Build", "Build and Growth", "Multiplayer Ready"};
	char aText[256];
	const int Chapter = clamp(g_Config.m_ClTutorialChapter, 1, 6);
	const int Step = clamp(g_Config.m_ClTutorialStep, 0, 9);
	switch(Chapter)
	{
	case 1: str_format(aText, sizeof(aText), Step == 0 ? Localize("Move with %s, then jump with %s") : Step == 1 ? Localize("Aim and fire with %s") : Localize("Switch weapons and hit the training target."), Step == 0 ? pMove : pFire, pJump); break;
	case 2: str_copy(aText, Localize(Step == 0 ? "Defeat the marked enemies and watch your ammunition." : Step == 1 ? "Take controlled damage, then collect health." : "Respawn near the current objective and finish the encounter."), sizeof(aText)); break;
	case 3: str_copy(aText, Localize(Step == 0 ? "Follow the radar marker. Stay near the switch for 2 seconds to activate it." : Step == 1 ? "Stay near the next marked switch for 2 seconds to secure the defense area." : Step == 2 ? "Activate the next marked switch and watch the HUD progress." : "Activate the final marked switch to open the extraction route."), sizeof(aText)); break;
	case 4: str_format(aText, sizeof(aText), Step == 0 ? Localize("Collect the marked sandbox materials.") : Step == 1 ? Localize("Open Forge with %s and craft the recommended weapon.") : Localize("Build a defense and survive the controlled wave."), pForge); break;
	case 5:
		if(Step == 1)
			str_format(aText, sizeof(aText), Localize("Hold %s, choose a drone module, then release to switch."), pDrone);
		else
			str_copy(aText, Localize(Step == 0 ? "Choose one of the three run perks." : "Purchase the highlighted sandbox research node. Tutorial points do not affect your save."), sizeof(aText));
		break;
	default: str_copy(aText, Localize(Step == 0 ? "Complete the short bot PvP objective." : Step == 1 ? "Configure and create the simulated room." : "Filter the simulated room list and join a room."), sizeof(aText)); break;
	}
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	CUIRect Hud = {ScreenWidth * 0.5f - 100.0f, 12.0f, 200.0f, 34.0f};
	DrawPanel(Hud, vec4(Panel.r, Panel.g, Panel.b, 0.94f), 6.0f);
	CUIRect Edge = {Hud.x, Hud.y, 2.0f, Hud.h};
	DrawPanel(Edge, Accent, 1.0f);
	char aTitle[128];
	str_format(aTitle, sizeof(aTitle), Localize("CHAPTER %d/6 · %s · %d/%d"), Chapter, Localize(s_apChapterNames[Chapter - 1]), m_TutorialProgress, max(1, m_TutorialTarget));
	DrawText(Hud.x + 8.0f, Hud.y + 5.0f, 6.0f, aTitle, Accent);
	DrawWrappedText(Hud.x + 8.0f, Hud.y + 15.0f, 5.3f, aText, Text, Hud.w - 16.0f, 2);
}

CPveResearchMask CPveRoguelite::ParseResearchMask() const
{
	const char *p = g_Config.m_ClPveResearchMask;
	const int Length = str_length(p);
	if(Length != 16 && Length != 32)
		return CPveResearchMask();
	unsigned long long High = 0;
	unsigned long long Low = 0;
	for(int i = 0; i < Length; i++)
	{
		int Value = -1;
		if(p[i] >= '0' && p[i] <= '9')
			Value = p[i] - '0';
		else if(p[i] >= 'a' && p[i] <= 'f')
			Value = p[i] - 'a' + 10;
		else if(p[i] >= 'A' && p[i] <= 'F')
			Value = p[i] - 'A' + 10;
		if(Value < 0)
			return CPveResearchMask();
		if(Length == 32 && i < 16)
			High = (High << 4) | (unsigned)Value;
		else
			Low = (Low << 4) | (unsigned)Value;
	}
	return PveSanitizeResearchMask(CPveResearchMask(Low, High));
}

void CPveRoguelite::StoreResearchMask(CPveResearchMask Mask)
{
	Mask.Sanitize();
	str_format(g_Config.m_ClPveResearchMask, sizeof(g_Config.m_ClPveResearchMask), "%016llX%016llX", Mask.m_aWords[1], Mask.m_aWords[0]);
}

void CPveRoguelite::SyncProgress()
{
	// The server requests persistent progress while handling EnterGame. At
	// that point the network connection is ready but the client can still be
	// in STATE_LOADING until its first snapshots arrive.
	if(Client()->State() < IClient::STATE_LOADING)
		return;
	const CPveResearchMask Mask = ParseResearchMask();
	CNetMsg_Cl_PveProgress Msg;
	Msg.m_Version = 2;
	Msg.m_ResearchPoints = clamp(g_Config.m_ClPveResearchPoints, 0, 999);
	Msg.m_ResearchMask0 = (int)(Mask.m_aWords[0] & 0xffffffffULL);
	Msg.m_ResearchMask1 = (int)((Mask.m_aWords[0] >> 32) & 0xffffffffULL);
	Msg.m_ResearchMask2 = (int)(Mask.m_aWords[1] & 0xffffffffULL);
	Msg.m_ResearchMask3 = (int)((Mask.m_aWords[1] >> 32) & 0xffffffffULL);
	Msg.m_HighestInvasion = clamp(g_Config.m_ClPveHighestInvasion, 0, 9999);
	Msg.m_PreferredCheckpoint = max(1, g_Config.m_ClPvePreferredCheckpoint);
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_ProgressSent = true;
	if(g_Config.m_Debug)
	{
		char aBuf[192];
		str_format(aBuf, sizeof(aBuf), "progress points=%d mask=%016llX%016llX checkpoint=%d state=%d",
			Msg.m_ResearchPoints, Mask.m_aWords[1], Mask.m_aWords[0], Msg.m_PreferredCheckpoint, Client()->State());
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "pve", aBuf);
	}
}

void CPveRoguelite::SendChoice(int Slot)
{
	if(!m_ChoiceActive || m_ChoiceNonce <= 0 || Slot < 0 || Slot >= 3)
		return;
	CNetMsg_Cl_PveChoice Msg;
	Msg.m_Nonce = m_ChoiceNonce;
	Msg.m_Card = m_aChoiceCards[Slot];
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_FocusedChoice = Slot;
	m_ChoiceNonce = 0;
	m_SelectionPulse = 1.0f;
}

void CPveRoguelite::SendContractVote(int Slot)
{
	if(!m_ContractVoteActive || Slot < 0 || Slot >= 2 || m_SelectedContract >= 0)
		return;
	CNetMsg_Cl_PveContractVote Msg;
	Msg.m_Nonce = m_ContractNonce;
	Msg.m_Contract = m_aContractOptions[Slot];
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_SelectedContract = Slot;
	m_FocusedChoice = Slot;
	m_SelectionPulse = 1.0f;
}

void CPveRoguelite::SendInvasionRetryVote(int Choice)
{
	if(!m_InvasionRetryVoteActive || m_InvasionRetryNonce <= 0 || m_SelectedInvasionRetry >= 0 ||
		Choice < PVE_INVASION_RETRY || Choice > PVE_INVASION_RESET)
		return;
	CNetMsg_Cl_PveInvasionRetryVote Msg;
	Msg.m_Nonce = m_InvasionRetryNonce;
	Msg.m_Choice = Choice;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_SelectedInvasionRetry = Choice;
	m_FocusedChoice = Choice;
	m_SelectionPulse = 1.0f;
}

void CPveRoguelite::SendDroneModule(int Module)
{
	if(Client()->State() != IClient::STATE_ONLINE || Module < PVE_DRONE_ASSAULT || Module > PVE_DRONE_REPAIR || Client()->GameTick() < m_DroneSwitchReadyTick)
		return;
	CNetMsg_Cl_PveDroneModule Msg;
	Msg.m_Nonce = ++m_DroneNonce;
	Msg.m_Module = Module;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CPveRoguelite::DrawPanel(const CUIRect &Rect, vec4 Color, float Rounding)
{
	RenderTools()->DrawUIRect(&Rect, Color, CUI::CORNER_ALL, Rounding);
}

void CPveRoguelite::DrawIcon(int Image, int Sprite, float X, float Y, float Size, vec4 Color)
{
	Graphics()->TextureSet(g_pData->m_aImages[Image].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	RenderTools()->SelectSprite(Sprite);
	RenderTools()->DrawSprite(X, Y, Size);
	Graphics()->QuadsEnd();
}

void CPveRoguelite::DrawText(float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int Align)
{
	TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
	TextRender()->TextOutlineColor(CMenus::ThemeBgDeep().r, CMenus::ThemeBgDeep().g, CMenus::ThemeBgDeep().b, 0.45f * Color.a);
	if(MaxWidth <= 0.0f)
	{
		float DrawX = X;
		const float Width = TextRender()->TextWidth(0, Size, pText, -1);
		if(Align == 0)
			DrawX -= Width * 0.5f;
		else if(Align > 0)
			DrawX -= Width;
		TextRender()->Text(0, DrawX, Y, Size, pText, -1);
		return;
	}
	float ActualSize = Size;
	while(ActualSize > Size * 0.72f && TextRender()->TextWidth(0, ActualSize, pText, -1) > MaxWidth * 2.0f)
		ActualSize -= 0.3f;
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, X, Y, ActualSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = MaxWidth;
	TextRender()->TextEx(&Cursor, pText, -1);
}

void CPveRoguelite::DrawWrappedText(float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines)
{
	TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
	TextRender()->TextOutlineColor(CMenus::ThemeBgDeep().r, CMenus::ThemeBgDeep().g, CMenus::ThemeBgDeep().b, 0.45f * Color.a);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, X, Y, Size, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = MaxWidth;
	Cursor.m_MaxLines = MaxLines;
	TextRender()->TextEx(&Cursor, pText, -1);
}

void CPveRoguelite::DrawSelectionOverlay(bool ContractVote)
{
	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_AppearAmount += (1.0f - m_AppearAmount) * (1.0f - expf(-10.0f * Dt));
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 4.0f);
	float DismissAlpha = 1.0f;
	if(!ContractVote && m_ChoiceDismissAt > 0)
	{
		const float Remaining = (float)(m_ChoiceDismissAt - time_get()) / (float)time_freq();
		DismissAlpha = clamp(Remaining / 0.08f, 0.0f, 1.0f);
	}
	const float Alpha = clamp(m_AppearAmount, 0.0f, 1.0f) * DismissAlpha;
	const float Entry = UiEaseOutCubic(Alpha);
	const float EntryOffset = (1.0f - Entry) * 10.0f;
	const float ConfirmPulse = UiConfirmPulse(m_SelectionPulse);
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();

	CUIRect Screen = {0, 0, ScreenWidth, 300.0f};
	DrawPanel(Screen, vec4(Deep.r, Deep.g, Deep.b, 0.94f * Alpha), 0.0f);
	CUIRect Stage = {8.0f, 42.0f + EntryOffset * 0.35f, ScreenWidth - 16.0f, 218.0f};
	DrawPanel(Stage, vec4(Inset.r, Inset.g, Inset.b, 0.96f * Alpha), 13.0f);
	CUIRect Line = {22.0f, 55.0f + EntryOffset * 0.35f, (ScreenWidth - 44.0f) * Entry, 1.2f};
	DrawPanel(Line, vec4(Accent.r, Accent.g, Accent.b, 0.65f * Alpha), 0.6f);
	DrawText(ScreenWidth * 0.5f, 10.0f, 12.0f, Localize(ContractVote ? "Team Contract" : "Choose a Perk"), vec4(Text.r, Text.g, Text.b, Alpha), -1.0f, 0);

	const int EndTick = ContractVote ? m_ContractEndTick : m_ChoiceEndTick;
	const int Seconds = max(0, (EndTick - Client()->GameTick() + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
	char aTimer[64];
	str_format(aTimer, sizeof(aTimer), Localize("%d seconds remaining"), Seconds);
	const float WarningPulse = Seconds <= 3 ? 0.82f + 0.18f * sinf((float)time_get() / (float)time_freq() * 7.0f) : 1.0f;
	const vec4 TimerColor = Seconds <= 3 ? Danger : Accent;
	CUIRect Timer = {ScreenWidth * 0.5f - 48.0f, 27.0f, 96.0f, 12.0f};
	DrawPanel(Timer, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 6.0f);
	DrawText(ScreenWidth * 0.5f, 29.3f, 6.4f, aTimer, vec4(TimerColor.r, TimerColor.g, TimerColor.b, Alpha * WarningPulse), -1.0f, 0);

	const int Count = ContractVote ? 2 : 3;
	const float Gap = ContractVote ? 16.0f : 8.0f;
	const float MaxCardWidth = ContractVote ? 160.0f : 105.0f;
	const float CardWidth = min(MaxCardWidth, (Stage.w - 24.0f - Gap * (Count - 1)) / Count);
	const float TotalWidth = CardWidth * Count + Gap * (Count - 1);
	const float StartX = ScreenWidth * 0.5f - TotalWidth * 0.5f;
	int Hovered = -1;
	for(int i = 0; i < Count; i++)
	{
		const float CardEntry = UiStagger(Alpha, i);
		CUIRect Hit = {StartX + i * (CardWidth + Gap), 66.0f + (1.0f - CardEntry) * 9.0f, CardWidth, 178.0f};
		if(m_SelectorMouse.x >= Hit.x && m_SelectorMouse.x <= Hit.x + Hit.w && m_SelectorMouse.y >= Hit.y && m_SelectorMouse.y <= Hit.y + Hit.h)
			Hovered = i;
	}
	if(m_MouseTrigger)
	{
		if(Hovered >= 0)
		{
			m_FocusedChoice = Hovered;
			if(ContractVote)
				SendContractVote(Hovered);
			else
				SendChoice(Hovered);
		}
		m_MouseTrigger = false;
	}
	else if(Hovered >= 0)
		m_FocusedChoice = Hovered;

	for(int i = 0; i < Count; i++)
	{
		const bool Focused = i == m_FocusedChoice;
		m_aCardFocus[i] += ((Focused ? 1.0f : 0.0f) - m_aCardFocus[i]) * (1.0f - expf(-14.0f * Dt));
		const float FocusAmount = clamp(m_aCardFocus[i], 0.0f, 1.0f);
		const float CardEntry = UiStagger(Alpha, i);
		const float CardAlpha = Alpha * CardEntry;
		const bool Selected = ContractVote ? i == m_SelectedContract : (m_ChoiceNonce == 0 && i == m_FocusedChoice);
		const int ID = ContractVote ? m_aContractOptions[i] : m_aChoiceCards[i];
		const CPveContractDef *pContract = ContractVote ? PveContractDef(ID) : 0;
		const CPveCardDef *pCard = ContractVote ? 0 : PveCardDef(ID);
		// Rarity is text, not a competing color code. One accent now carries all
		// focus and selection state across perks and contracts.
		const vec4 CategoryColor = Accent;
		const float Scale = 1.0f + FocusAmount * 0.018f + (Selected ? ConfirmPulse * 0.012f : 0.0f);
		CUIRect Card = {StartX + i * (CardWidth + Gap) - CardWidth * (Scale - 1.0f) * 0.5f,
			66.0f + (1.0f - CardEntry) * 9.0f - FocusAmount * 2.0f - (Selected ? ConfirmPulse * 0.8f : 0.0f), CardWidth * Scale, 178.0f * Scale};
		CUIRect Border = Card;
		Border.Margin(-1.4f, &Border);
		const vec4 BorderColor = Selected || Focused ? CategoryColor : AccentDim;
		const float BorderAlpha = Selected ? 0.92f : (Focused ? 0.78f : 0.24f);
		DrawPanel(Border, vec4(BorderColor.r, BorderColor.g, BorderColor.b, min(1.0f, BorderAlpha + ConfirmPulse * 0.08f) * CardAlpha), 11.0f);
		DrawPanel(Card, vec4(Panel.r, Panel.g, Panel.b, 0.98f * CardAlpha), 9.0f);
		const char *pName = Localize(ContractVote ? (pContract ? pContract->m_pName : "Unknown contract") : PveChoiceName(ID));
		const char *pDescription = Localize(ContractVote ? (pContract ? pContract->m_pRule : "") : PveChoiceDescription(ID));

		CUIRect Badge = {Card.x + 8.0f, Card.y + 8.0f, Card.w - 16.0f, 14.0f};
		DrawPanel(Badge, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 7.0f);
		char aBadge[64];
		if(ContractVote)
			str_format(aBadge, sizeof(aBadge), Localize("RISK • %d VOTES"), m_aContractVotes[i]);
		else if(pCard)
			str_format(aBadge, sizeof(aBadge), Localize("%d · %s · Lv. %d/%d"), i + 1, Localize(PveRarityName(pCard->m_Rarity)), m_aChoiceStacks[i], pCard->m_MaxStacks);
		else
			str_format(aBadge, sizeof(aBadge), Localize("%d · Supply"), i + 1);
		DrawText(Card.x + Card.w * 0.5f, Card.y + 11.2f, 5.6f, aBadge, vec4(CategoryColor.r, CategoryColor.g, CategoryColor.b, CardAlpha), -1.0f, 0);
		float NameSize = 8.5f + FocusAmount * 0.7f;
		while(NameSize > 6.0f && TextRender()->TextWidth(0, NameSize, pName, -1) > Card.w - 16.0f)
			NameSize -= 0.3f;
		DrawText(Card.x + Card.w * 0.5f, Card.y + 32.0f, NameSize, pName, vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		const CPveUiIcon Icon = ContractVote ? CPveUiIcon() : PveChoiceIcon(ID, pCard);
		DrawIcon(Icon.m_Image, Icon.m_Sprite, Card.x + Card.w * 0.5f, Card.y + 61.0f, (21.0f + FocusAmount * 3.0f) * Icon.m_Scale,
			vec4(Text.r, Text.g, Text.b, 0.82f * CardAlpha));
		const vec4 DescriptionText = vec4(0.72f, 0.74f, 0.76f, ContractVote ? 0.72f * CardAlpha : 0.64f * CardAlpha);
		DrawWrappedText(Card.x + 10.0f, Card.y + 82.0f, 6.2f, pDescription,
			DescriptionText, Card.w - 20.0f, ContractVote ? 4 : 3);
		if(ContractVote && pContract)
		{
			DrawText(Card.x + 10.0f, Card.y + 121.0f, 5.8f, Localize(pContract->m_pRisk), vec4(Danger.r, Danger.g, Danger.b, CardAlpha), Card.w - 20.0f, -1);
			DrawText(Card.x + 10.0f, Card.y + 140.0f, 5.8f, Localize("Reward: 1 Research Point"), vec4(Accent.r, Accent.g, Accent.b, CardAlpha), Card.w - 20.0f, -1);
		}
		if(ContractVote)
		{
			CUIRect Button = {Card.x + 10.0f, Card.y + Card.h - 27.0f, Card.w - 20.0f, 18.0f};
			DrawPanel(Button, vec4((Focused ? Accent : Inset).r, (Focused ? Accent : Inset).g, (Focused ? Accent : Inset).b, 0.95f * CardAlpha), 7.0f);
			DrawText(Button.x + Button.w * 0.5f, Button.y + 5.1f, 6.5f, Localize(Selected ? "Voted" : "Vote"), vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		}
		else if(Selected)
			DrawText(Card.x + Card.w * 0.5f, Card.y + Card.h - 23.0f, 6.4f + ConfirmPulse * 0.35f, Localize("Selected"), vec4(Accent.r, Accent.g, Accent.b, CardAlpha), -1.0f, 0);
	}

	if(ContractVote)
		DrawText(ScreenWidth * 0.5f, 274.0f, 6.5f, Localize("Mouse • Arrow Keys • 1–3 • Gamepad"), vec4(Text.r, Text.g, Text.b, 0.65f * Alpha), -1.0f, 0);
	if(m_ValidationCode && time_get() < m_ValidationUntil)
		DrawText(ScreenWidth * 0.5f, 287.0f, 6.0f, Localize("The server rejected that selection."), vec4(Danger.r, Danger.g, Danger.b, Alpha), -1.0f, 0);

	Graphics()->TextureSet(-1);
	CUIRect Cursor = {m_SelectorMouse.x, m_SelectorMouse.y, 5.0f, 5.0f};
	DrawPanel(Cursor, vec4(Accent.r, Accent.g, Accent.b, Alpha), 2.5f);
	TextRender()->TextColor(1, 1, 1, 1);
}


void CPveRoguelite::DrawInvasionRetryVote()
{
	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_AppearAmount += (1.0f - m_AppearAmount) * (1.0f - expf(-9.0f * Dt));
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 4.0f);
	const float Alpha = clamp(m_AppearAmount, 0.0f, 1.0f);
	const float Entry = UiEaseOutCubic(Alpha);
	const float EntryOffset = (1.0f - Entry) * 10.0f;
	const float ConfirmPulse = UiConfirmPulse(m_SelectionPulse);
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();

	CUIRect Screen = {0, 0, ScreenWidth, 300.0f};
	DrawPanel(Screen, vec4(Deep.r, Deep.g, Deep.b, 0.96f * Alpha), 0.0f);
	CUIRect Stage = {10.0f, 51.0f + EntryOffset * 0.35f, ScreenWidth - 20.0f, 207.0f};
	DrawPanel(Stage, vec4(Inset.r, Inset.g, Inset.b, 0.97f * Alpha), 13.0f);
	CUIRect TopLine = {Stage.x + 13.0f, Stage.y + 9.0f, (Stage.w - 26.0f) * Entry, 1.2f};
	DrawPanel(TopLine, vec4(Danger.r, Danger.g, Danger.b, 0.68f * Alpha), 0.6f);
	DrawText(ScreenWidth * 0.5f, 8.0f, 12.5f, Localize("The expedition has reached its limit"), vec4(Text.r, Text.g, Text.b, Alpha), -1.0f, 0);
	DrawText(ScreenWidth * 0.5f, 26.0f, 6.4f, Localize("Five failures. Decide the fate of this run."), vec4(Text.r, Text.g, Text.b, 0.72f * Alpha), -1.0f, 0);

	char aFloor[48];
	str_format(aFloor, sizeof(aFloor), Localize("Floor %d"), m_InvasionRetryFloor);
	CUIRect Floor = {Stage.x + 14.0f, Stage.y + 16.0f, 58.0f, 15.0f};
	DrawPanel(Floor, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 7.0f);
	DrawText(Floor.x + Floor.w * 0.5f, Floor.y + 4.0f, 6.3f, aFloor, vec4(Accent.r, Accent.g, Accent.b, Alpha), -1.0f, 0);
	const int Seconds = max(0, (m_InvasionRetryEndTick - Client()->GameTick() + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
	char aTimer[64];
	str_format(aTimer, sizeof(aTimer), Localize("%d seconds remaining"), Seconds);
	CUIRect Timer = {Stage.x + Stage.w - 92.0f, Stage.y + 16.0f, 78.0f, 15.0f};
	DrawPanel(Timer, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 7.0f);
	const float WarningPulse = Seconds <= 3 ? 0.82f + 0.18f * sinf((float)time_get() / (float)time_freq() * 7.0f) : 1.0f;
	const vec4 TimerColor = Seconds <= 3 ? Danger : Accent;
	DrawText(Timer.x + Timer.w * 0.5f, Timer.y + 4.0f, 6.0f, aTimer, vec4(TimerColor.r, TimerColor.g, TimerColor.b, Alpha * WarningPulse), -1.0f, 0);

	const float Gap = 16.0f;
	const float CardWidth = min(195.0f, (Stage.w - 30.0f - Gap) * 0.5f);
	const float StartX = ScreenWidth * 0.5f - CardWidth - Gap * 0.5f;
	int Hovered = -1;
	for(int i = 0; i < 2; i++)
	{
		const float CardEntry = UiStagger(Alpha, i);
		CUIRect Hit = {StartX + i * (CardWidth + Gap), 88.0f + (1.0f - CardEntry) * 9.0f, CardWidth, 145.0f};
		if(m_SelectorMouse.x >= Hit.x && m_SelectorMouse.x <= Hit.x + Hit.w && m_SelectorMouse.y >= Hit.y && m_SelectorMouse.y <= Hit.y + Hit.h)
			Hovered = i;
	}
	if(m_MouseTrigger)
	{
		if(Hovered >= 0)
		{
			m_FocusedChoice = Hovered;
			SendInvasionRetryVote(Hovered);
		}
		m_MouseTrigger = false;
	}
	else if(Hovered >= 0)
		m_FocusedChoice = Hovered;

	const char *apNames[2] = {"Retry Current Floor", "Return to Floor 1"};
	const char *apDescriptions[2] = {"Keep equipment and build. Retry this floor.", "Clear this run and start again from Floor 1."};
	const char *apConsequences[2] = {"One final attempt", "Equipment and build will be cleared"};
	for(int i = 0; i < 2; i++)
	{
		const bool Focused = i == m_FocusedChoice;
		const bool Selected = i == m_SelectedInvasionRetry;
		m_aCardFocus[i] += ((Focused ? 1.0f : 0.0f) - m_aCardFocus[i]) * (1.0f - expf(-14.0f * Dt));
		const float FocusAmount = clamp(m_aCardFocus[i], 0.0f, 1.0f);
		const float CardEntry = UiStagger(Alpha, i);
		const float CardAlpha = Alpha * CardEntry;
		const float Scale = 1.0f + FocusAmount * 0.018f + (Selected ? ConfirmPulse * 0.012f : 0.0f);
		const vec4 ChoiceColor = i == PVE_INVASION_RETRY ? Accent : Danger;
		CUIRect Card = {StartX + i * (CardWidth + Gap) - CardWidth * (Scale - 1.0f) * 0.5f,
			88.0f + (1.0f - CardEntry) * 9.0f - FocusAmount * 2.0f - (Selected ? ConfirmPulse * 0.8f : 0.0f), CardWidth * Scale, 145.0f * Scale};
		CUIRect Border = Card;
		Border.Margin(-1.5f, &Border);
		const float BorderAlpha = Selected ? 0.92f : (Focused ? 0.78f : 0.24f);
		DrawPanel(Border, vec4(ChoiceColor.r, ChoiceColor.g, ChoiceColor.b, min(1.0f, BorderAlpha + ConfirmPulse * 0.08f) * CardAlpha), 11.0f);
		DrawPanel(Card, vec4(Panel.r, Panel.g, Panel.b, 0.98f * CardAlpha), 9.0f);
		char aVotes[64];
		str_format(aVotes, sizeof(aVotes), Localize("%d votes"), m_aInvasionRetryVotes[i]);
		CUIRect VoteBadge = {Card.x + 9.0f, Card.y + 8.0f, 58.0f, 14.0f};
		DrawPanel(VoteBadge, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 7.0f);
		DrawText(VoteBadge.x + VoteBadge.w * 0.5f, VoteBadge.y + 3.8f, 5.8f, aVotes, vec4(ChoiceColor.r, ChoiceColor.g, ChoiceColor.b, CardAlpha), -1.0f, 0);
		char aKey[8];
		str_format(aKey, sizeof(aKey), "%d", i + 1);
		CUIRect Key = {Card.x + Card.w - 25.0f, Card.y + 8.0f, 16.0f, 14.0f};
		DrawPanel(Key, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 6.0f);
		DrawText(Key.x + Key.w * 0.5f, Key.y + 3.8f, 5.8f, aKey, vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		float NameSize = 9.2f + FocusAmount * 0.8f;
		const char *pName = Localize(apNames[i]);
		while(NameSize > 6.7f && TextRender()->TextWidth(0, NameSize, pName, -1) > Card.w - 18.0f)
			NameSize -= 0.3f;
		DrawText(Card.x + Card.w * 0.5f, Card.y + 31.0f, NameSize, pName, vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		DrawWrappedText(Card.x + 12.0f, Card.y + 52.0f, 6.5f, Localize(apDescriptions[i]), vec4(Text.r, Text.g, Text.b, 0.78f * CardAlpha), Card.w - 24.0f, 3);
		DrawText(Card.x + 12.0f, Card.y + 87.0f, 5.8f, Localize(apConsequences[i]), vec4(ChoiceColor.r, ChoiceColor.g, ChoiceColor.b, 0.92f * CardAlpha), Card.w - 24.0f, -1);
		CUIRect Button = {Card.x + 10.0f, Card.y + Card.h - 27.0f, Card.w - 20.0f, 18.0f};
		const vec4 ButtonColor = Focused || Selected ? ChoiceColor : AccentDim;
		DrawPanel(Button, vec4(ButtonColor.r, ButtonColor.g, ButtonColor.b, 0.94f * CardAlpha), 7.0f);
		DrawText(Button.x + Button.w * 0.5f, Button.y + 5.0f, 6.5f + (Selected ? ConfirmPulse * 0.3f : 0.0f), Localize(Selected ? "Voted" : "Vote"), vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
	}

	DrawText(ScreenWidth * 0.5f, 270.0f, 6.4f, Localize("Mouse • Arrow Keys • 1–2 • Gamepad"), vec4(Text.r, Text.g, Text.b, 0.65f * Alpha), -1.0f, 0);
	DrawText(ScreenWidth * 0.5f, 284.0f, 5.8f, Localize("A tie or no votes grants one final retry."), vec4(Text.r, Text.g, Text.b, 0.72f * Alpha), -1.0f, 0);

	Graphics()->TextureSet(-1);
	CUIRect Cursor = {m_SelectorMouse.x, m_SelectorMouse.y, 5.0f, 5.0f};
	DrawPanel(Cursor, vec4(Accent.r, Accent.g, Accent.b, Alpha), 2.5f);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CPveRoguelite::DrawInvasionRetryResult()
{
	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_AppearAmount += (1.0f - m_AppearAmount) * (1.0f - expf(-7.0f * Dt));
	const float Alpha = clamp(m_AppearAmount, 0.0f, 1.0f);
	const float Entry = UiEaseOutCubic(Alpha);
	const float Pulse = 0.94f + 0.06f * sinf((float)time_get() / (float)time_freq() * 4.0f);
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	const bool Retry = m_InvasionRetryResult == PVE_INVASION_RETRY_RESULT_RETRY;
	const vec4 ResultColor = Retry ? Accent : Danger;

	CUIRect Screen = {0, 0, ScreenWidth, 300.0f};
	DrawPanel(Screen, vec4(Deep.r, Deep.g, Deep.b, 0.97f * Alpha), 0.0f);
	CUIRect Glow = {ScreenWidth * 0.5f - min(245.0f, ScreenWidth - 34.0f) * 0.5f, 103.0f + (1.0f - Entry) * 8.0f, min(245.0f, ScreenWidth - 34.0f), 94.0f};
	DrawPanel(Glow, vec4(ResultColor.r, ResultColor.g, ResultColor.b, 0.14f * Pulse * Alpha), 18.0f);
	CUIRect Core = Glow;
	Core.Margin(4.0f, &Core);
	DrawPanel(Core, vec4(Panel.r, Panel.g, Panel.b, 0.95f * Alpha), 15.0f);
	const float LineWidth = (Glow.w - 36.0f) * Entry;
	CUIRect LineTop = {ScreenWidth * 0.5f - LineWidth * 0.5f, Glow.y - 12.0f, LineWidth, 1.5f};
	CUIRect LineBottom = {ScreenWidth * 0.5f - LineWidth * 0.5f, Glow.y + Glow.h + 10.0f, LineWidth, 1.5f};
	DrawPanel(LineTop, vec4(ResultColor.r, ResultColor.g, ResultColor.b, Pulse * Alpha), 0.7f);
	DrawPanel(LineBottom, vec4(ResultColor.r, ResultColor.g, ResultColor.b, Pulse * Alpha), 0.7f);

	char aHeadline[128];
	if(Retry)
		str_format(aHeadline, sizeof(aHeadline), Localize("%s did not succumb."), m_aInvasionRetryPlayerName[0] ? m_aInvasionRetryPlayerName : Localize("The team"));
	else if(m_InvasionRetryResult == PVE_INVASION_RETRY_RESULT_FINAL_FAILURE)
		str_copy(aHeadline, Localize("What a pity..."), sizeof(aHeadline));
	else
		str_copy(aHeadline, Localize("See you next time."), sizeof(aHeadline));
	float HeadlineSize = Retry ? 25.0f : 29.0f;
	while(HeadlineSize > 14.0f && TextRender()->TextWidth(0, HeadlineSize, aHeadline, -1) > Glow.w - 20.0f)
		HeadlineSize -= 0.5f;
	DrawText(ScreenWidth * 0.5f, Glow.y + 25.0f, HeadlineSize, aHeadline, vec4(ResultColor.r, ResultColor.g, ResultColor.b, Alpha), -1.0f, 0);
	const char *pSubtitle = Retry ? "The expedition continues." : "Returning all players to Floor 1.";
	DrawText(ScreenWidth * 0.5f, Glow.y + 66.0f, 6.8f, Localize(pSubtitle), vec4(Text.r, Text.g, Text.b, 0.72f * Alpha), -1.0f, 0);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CPveRoguelite::DrawContractHud()
{
	if(m_ActiveContract < 0 || m_ContractState == PVE_CONTRACT_STATE_NONE || Client()->State() != IClient::STATE_ONLINE)
		return;
	const CPveContractDef *pDef = PveContractDef(m_ActiveContract);
	if(!pDef)
		return;
	const float Aspect = Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, 300.0f * Aspect, 300.0f);
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	// Health, ammo and the four weapon slots occupy the upper-left through
	// roughly y=100. Keep contract status below that combat HUD so neither the
	// panel nor its text can cover the equipped weapons at any aspect ratio.
	CUIRect Hud = {10.0f, 112.0f, 118.0f, 35.0f};
	DrawPanel(Hud, vec4(Panel.r, Panel.g, Panel.b, 0.90f), 8.0f);
	CUIRect Edge = {Hud.x, Hud.y, 2.0f, Hud.h};
	const vec4 StateColor = m_ContractState == PVE_CONTRACT_STATE_FAILED ? Danger : Accent;
	DrawPanel(Edge, StateColor, 1.0f);
	DrawText(Hud.x + 8.0f, Hud.y + 5.0f, 6.5f, Localize(pDef->m_pName), Text);
	char aStatus[96];
	if(m_ContractState == PVE_CONTRACT_STATE_SUCCESS)
		str_copy(aStatus, Localize("Contract completed"), sizeof(aStatus));
	else if(m_ContractState == PVE_CONTRACT_STATE_FAILED)
		str_copy(aStatus, Localize("Contract failed"), sizeof(aStatus));
	else if(m_ContractStatusEndTick > Client()->GameTick())
		str_format(aStatus, sizeof(aStatus), Localize("%d seconds • %d/%d"), (m_ContractStatusEndTick - Client()->GameTick()) / Client()->GameTickSpeed(), m_ContractProgress, m_ContractTarget);
	else
		str_format(aStatus, sizeof(aStatus), Localize("Active • %d/%d"), m_ContractProgress, m_ContractTarget);
	DrawText(Hud.x + 8.0f, Hud.y + 20.0f, 5.7f, aStatus, vec4(StateColor.r, StateColor.g, StateColor.b, 0.95f));
	(void)Inset;
}

void CPveRoguelite::DrawBuildHud()
{
	if(Client()->State() != IClient::STATE_ONLINE && !m_DebugBuildPreview)
		return;
	bool HasState = m_Barrier > 0 || m_VulnerableTargets > 0 || m_BleedingTargets > 0 || m_LegendaryCard >= 0 || m_DroneModule != PVE_DRONE_NONE;
	for(int i = 0; i < 4; i++)
		HasState |= m_aWeaponResources[i] > 0;
	if(!HasState)
		return;
	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	char aaLines[14][96];
	int Lines = 0;
	if(m_Barrier > 0)
		str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %d/30", Localize("Barrier"), m_Barrier);
	const char *apResources[4] = {"Focus", "Blast Charge", "Voltage", "Fury"};
	for(int i = 0; i < 4; i++)
		if(m_aWeaponResources[i] > 0)
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %d/10", Localize(apResources[i]), m_aWeaponResources[i]);
	if(m_VulnerableTargets > 0 || m_BleedingTargets > 0)
		str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %d  •  %s %d", Localize("Vulnerable"), m_VulnerableTargets, Localize("Bleed"), m_BleedingTargets);
	if(m_LegendaryCard >= 0 && PveCardDef(m_LegendaryCard))
		str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s: %s", Localize("Legendary"), Localize(PveCardDef(m_LegendaryCard)->m_pName));
	if(m_DroneModule != PVE_DRONE_NONE)
	{
		const char *apModules[4] = {"None", "Assault Module", "Guardian Module", "Repair Module"};
		const int Cooldown = max(0, m_DroneSwitchReadyTick - Client()->GameTick());
		if(Cooldown > 0)
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s: %s  %.1fs", Localize("Drone"), Localize(apModules[m_DroneModule]), Cooldown / (float)Client()->GameTickSpeed());
		else
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s: %s", Localize("Drone"), Localize(apModules[m_DroneModule]));
		if(m_DroneHealth > 0)
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %d/40", Localize("Drone integrity"), m_DroneHealth);
		if((m_DroneState == PVE_DRONE_STATE_DISABLED || m_DroneState == PVE_DRONE_STATE_REBUILDING) && m_DroneActionTick > Client()->GameTick())
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %.1fs", Localize(m_DroneState == PVE_DRONE_STATE_DISABLED ? "EMP disabled" : "Rebuilding"), (m_DroneActionTick - Client()->GameTick()) / (float)Client()->GameTickSpeed());
		else if(!m_DroneTutorialSeen)
		{
			char aKeyHelp[96];
			const char *pWheelKey = m_pClient->m_pBinds->GetKey("+dronewheel");
			str_format(aKeyHelp, sizeof(aKeyHelp), Localize("Hold %s: drone command wheel"), pWheelKey[0] ? pWheelKey : "?");
			str_copy(aaLines[Lines++], aKeyHelp, sizeof(aaLines[0]));
		}
	}
	if(m_ValidationCode == PVE_VALIDATION_MODULE_LOCKED && time_get() < m_ValidationUntil)
		str_copy(aaLines[Lines++], Localize("Drone module not owned"), sizeof(aaLines[0]));
	else if(m_ValidationCode == PVE_VALIDATION_MODULE_COOLDOWN && time_get() < m_ValidationUntil)
		str_copy(aaLines[Lines++], Localize("Drone switch cooling down"), sizeof(aaLines[0]));
	const float Width = 130.0f;
	CUIRect Hud = {ScreenWidth - Width - 10.0f, 10.0f, Width, 12.0f + Lines * 12.0f};
	DrawPanel(Hud, vec4(Panel.r, Panel.g, Panel.b, 0.90f), 8.0f);
	CUIRect Edge = {Hud.x + Hud.w - 2.0f, Hud.y, 2.0f, Hud.h};
	DrawPanel(Edge, Accent, 1.0f);
	for(int i = 0; i < Lines; i++)
		DrawText(Hud.x + 8.0f, Hud.y + 6.0f + i * 12.0f, 5.8f, aaLines[i], Text, Hud.w - 16.0f, -1);
}

void CPveRoguelite::DrawDrones()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	// Client-only weapon-bone smoothing (same idea as lost-protocol droids).
	static float s_aSmoothedAim[MAX_CLIENTS];
	static vec2 s_aLastPos[MAX_CLIENTS];
	static char s_aAimInit[MAX_CLIENTS] = {0};

	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
		if(Item.m_Type != NETOBJTYPE_PVEDRONE)
			continue;
		const CNetObj_PveDrone *pDrone = (const CNetObj_PveDrone *)pData;
		const CNetObj_PveDrone *pPrev = pDrone;
		if(const void *pPrevData = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID))
			pPrev = (const CNetObj_PveDrone *)pPrevData;

		const bool Owned = pDrone->m_Owner == m_pClient->m_Snap.m_LocalClientID;
		if(Owned)
		{
			m_DroneHealth = pDrone->m_Health;
			m_DroneState = pDrone->m_State;
			m_DroneActionTick = pDrone->m_ActionTick;
			m_DroneSwitchReadyTick = pDrone->m_SwitchReadyTick;
		}
		const bool Disabled = pDrone->m_State == PVE_DRONE_STATE_DISABLED || pDrone->m_State == PVE_DRONE_STATE_REBUILDING;
		const vec4 Color = Disabled ? vec4(0.35f, 0.45f, 0.55f, 0.65f) : (pDrone->m_Module == PVE_DRONE_ASSAULT ? vec4(1.0f, 0.35f, 0.25f, 1.0f) : (pDrone->m_Module == PVE_DRONE_GUARDIAN ? vec4(0.25f, 0.65f, 1.0f, 1.0f) : vec4(0.3f, 1.0f, 0.55f, 1.0f)));
		const float Intra = Client()->IntraGameTick();
		const vec2 DronePos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pDrone->m_X, pDrone->m_Y), Intra);
		const vec2 TargetPos = mix(vec2(pPrev->m_TargetX, pPrev->m_TargetY), vec2(pDrone->m_TargetX, pDrone->m_TargetY), Intra);
		const vec2 Vel = mix(vec2(pPrev->m_VelX, pPrev->m_VelY), vec2(pDrone->m_VelX, pDrone->m_VelY), Intra) * 0.01f;
		if(Owned && !Disabled && pDrone->m_State == PVE_DRONE_STATE_ACTING && distance(DronePos, TargetPos) > 4.0f)
		{
			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.55f);
			IGraphics::CLineItem Line(DronePos.x, DronePos.y, TargetPos.x, TargetPos.y);
			Graphics()->LinesDraw(&Line, 1);
			Graphics()->LinesEnd();
		}
		if(!Disabled && pDrone->m_Module == PVE_DRONE_GUARDIAN)
			for(int Marker = 0; Marker < 16; Marker++)
			{
				const float Angle = Marker * 2.0f * pi / 16.0f;
				DrawIcon(IMAGE_WEAPONS, SPRITE_PICKUP_ARMOR, DronePos.x + cosf(Angle) * 280.0f, DronePos.y + sinf(Angle) * 280.0f, 9.0f, vec4(0.25f, 0.65f, 1.0f, 0.22f));
			}

		const char *pAnim = "follow";
		if(pDrone->m_State == PVE_DRONE_STATE_DEPLOYING)
			pAnim = "deploy";
		else if(pDrone->m_State == PVE_DRONE_STATE_DISABLED)
			pAnim = "emp";
		else if(pDrone->m_State == PVE_DRONE_STATE_DESTROYED)
			pAnim = "destroyed";
		else if(pDrone->m_State == PVE_DRONE_STATE_REBUILDING)
			pAnim = "rebuild";
		else if(pDrone->m_State == PVE_DRONE_STATE_SWITCHING)
			pAnim = "deploy";
		else if(pDrone->m_State == PVE_DRONE_STATE_ACTING)
			pAnim = pDrone->m_Module == PVE_DRONE_GUARDIAN ? "shield" : (pDrone->m_Module == PVE_DRONE_REPAIR ? "repair" : "attack");

		const float Time = (Client()->PrevGameTick() + Client()->IntraGameTick()) / (float)Client()->GameTickSpeed() + Item.m_ID * 0.11f;
		if(pDrone->m_State == PVE_DRONE_STATE_DISABLED)
			Graphics()->ShaderBegin(SHADER_ELECTRIC, 0.85f);
		const int DroneAtlas = pDrone->m_Module == PVE_DRONE_GUARDIAN ? ATLAS_LOST_PROTOCOL_PVE_DRONE_GUARDIAN :
			(pDrone->m_Module == PVE_DRONE_REPAIR ? ATLAS_LOST_PROTOCOL_PVE_DRONE_REPAIR : ATLAS_LOST_PROTOCOL_PVE_DRONE_ASSAULT);

		// Weapon bone: aim at action target while ACTING, otherwise face move dir.
		// abs(x) flip matches CDroid::Snap; never snap AimAngle back to idle 0.
		const int Owner = clamp(pDrone->m_Owner, 0, MAX_CLIENTS - 1);
		vec2 AimDelta(0.0f, 0.0f);
		if(!Disabled && pDrone->m_State == PVE_DRONE_STATE_ACTING && distance(DronePos, TargetPos) > 4.0f)
			AimDelta = TargetPos - DronePos;
		else if(length(Vel) > 0.12f)
			AimDelta = Vel;
		else
		{
			const vec2 TickDelta(pDrone->m_X - pPrev->m_X, pDrone->m_Y - pPrev->m_Y);
			if(length(TickDelta) > 0.5f)
				AimDelta = TickDelta;
		}

		float TargetAim = s_aAimInit[Owner] ? s_aSmoothedAim[Owner] : 0.0f;
		int Dir = Vel.x < -0.05f ? 1 : -1;
		if(length(AimDelta) > 0.5f)
		{
			Dir = AimDelta.x < 0.0f ? 1 : -1;
			TargetAim = GetAngle(vec2(fabs(AimDelta.x), AimDelta.y)) * (180.0f / pi);
		}

		if(!s_aAimInit[Owner] || distance(s_aLastPos[Owner], DronePos) > 256.0f)
		{
			s_aSmoothedAim[Owner] = TargetAim;
			s_aAimInit[Owner] = 1;
		}
		else
		{
			float SmoothDelta = TargetAim - s_aSmoothedAim[Owner];
			while(SmoothDelta > 180.0f) SmoothDelta -= 360.0f;
			while(SmoothDelta < -180.0f) SmoothDelta += 360.0f;
			s_aSmoothedAim[Owner] += SmoothDelta * 0.28f;
		}
		s_aLastPos[Owner] = DronePos;

		RenderTools()->RenderSkeleton(DronePos, DroneAtlas, pAnim, Time, vec2(1.0f, 1.0f), Dir, s_aSmoothedAim[Owner]);
		Graphics()->ShaderEnd();

		// Register lights during the world pass so ambient lighting can darken the
		// chassis while the module still blooms through the light buffer.
		if(Disabled)
			m_pClient->m_pEffects->SimpleLight(DronePos, vec4(0.35f, 0.55f, 0.7f, 0.22f), 70.0f);
		else
		{
			m_pClient->m_pEffects->SimpleLight(DronePos, vec4(Color.r, Color.g, Color.b, 0.55f), 150.0f);
			m_pClient->m_pEffects->SimpleLight(DronePos, vec4(Color.r, Color.g, Color.b, 0.85f), 64.0f);
		}
	}
}

void CPveRoguelite::DrawDroneWheel()
{
	if(!m_DroneWheelActive)
		return;

	const char *apAllNames[3] = {"Assault", "Guardian", "Repair"};
	const vec4 aColors[3] = {
		vec4(1.0f, 0.35f, 0.25f, 1.0f),
		vec4(0.25f, 0.65f, 1.0f, 1.0f),
		vec4(0.3f, 1.0f, 0.55f, 1.0f)};
	const int aCards[3] = {PVE_CARD_ASSAULT_MODULE, PVE_CARD_GUARDIAN_MODULE, PVE_CARD_REPAIR_MODULE};
	int aUnlocked[3];
	int Count = 0;
	for(int i = 0; i < 3; i++)
		if(m_aRunPerks[aCards[i]] > 0)
			aUnlocked[Count++] = i;
	if(Count <= 0)
		return;

	if(length(m_DroneWheelMouse) > 170.0f)
		m_DroneWheelMouse = normalize(m_DroneWheelMouse) * 170.0f;

	m_DroneWheelSelected = -1;
	if(length(m_DroneWheelMouse) > 100.0f)
	{
		float SelectedAngle = GetAngle(m_DroneWheelMouse) + pi / (float)Count + pi / 2.0f;
		if(SelectedAngle < 0.0f)
			SelectedAngle += 2.0f * pi;
		if(SelectedAngle >= 2.0f * pi)
			SelectedAngle -= 2.0f * pi;
		m_DroneWheelSelected = (int)(SelectedAngle / (2.0f * pi) * Count);
		if(m_DroneWheelSelected < 0 || m_DroneWheelSelected >= Count)
			m_DroneWheelSelected = -1;
	}

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	const float Cx = Screen.w / 2.0f;
	const float Cy = Screen.h / 2.0f;
	const float Radius = 190.0f;
	const float LabelRadius = 120.0f;

	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	DrawDroneWheelCircle(Graphics(), Cx, Cy, Radius, 64);

	const float Offset = pi / (float)Count;
	for(int i = 0; i < Count; i++)
	{
		const int ModuleIdx = aUnlocked[i];
		const vec4 &Base = aColors[ModuleIdx];
		const bool Selected = m_DroneWheelSelected == i;
		const float Alpha = Selected ? 0.55f : 0.28f;
		const float Bright = Selected ? 1.15f : 0.75f;
		Graphics()->SetColor(
			min(1.0f, Base.r * Bright),
			min(1.0f, Base.g * Bright),
			min(1.0f, Base.b * Bright),
			Alpha);
		const float a0 = 2.0f * pi * i / (float)Count - Offset - pi / 2.0f;
		const float a1 = 2.0f * pi * (i + 1) / (float)Count - Offset - pi / 2.0f;
		DrawDroneWheelSlice(Graphics(), Cx, Cy, Radius - 4.0f, a0, a1, max(8, 64 / Count));
	}
	Graphics()->QuadsEnd();

	for(int i = 0; i < Count; i++)
	{
		const float MidSelected = 2.0f * pi * (i + 0.5f) / (float)Count;
		const float Angle = MidSelected - Offset - pi / 2.0f;
		const float X = Cx + cosf(Angle) * LabelRadius;
		const float Y = Cy + sinf(Angle) * LabelRadius - 8.0f;
		const bool Selected = m_DroneWheelSelected == i;
		DrawText(X, Y, Selected ? 18.0f : 14.0f, Localize(apAllNames[aUnlocked[i]]),
			Selected ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(0.85f, 0.9f, 0.95f, 0.9f), -1.0f, 0);
	}

	DrawText(Cx, Cy - 10.0f, 14.0f,
		Localize(Client()->GameTick() < m_DroneSwitchReadyTick ? "Drone switch cooling down" :
			(m_DroneWheelSelected >= 0 ? "Release to switch" : "Aim to select")),
		vec4(1.0f, 1.0f, 1.0f, 0.85f), -1.0f, 0);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(m_DroneWheelMouse.x + Cx, m_DroneWheelMouse.y + Cy, 24.0f, 24.0f);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CPveRoguelite::RenderBuildDebug()
{
	if(m_DebugBuildPreview)
		DrawBuildHud();
}

bool CPveRoguelite::CanBuyResearch(int CardID, const CPveResearchMask &Mask) const
{
	const CPveCardDef *pDef = PveCardDef(CardID);
	const int ResearchPoints = TutorialResearchActive() ? 99 : g_Config.m_ClPveResearchPoints;
	return pDef && !pDef->m_Base && !PveCardIsUnlocked(CardID, Mask) && ResearchPoints >= pDef->m_ResearchCost &&
		Mask.PrerequisitesMet(CardID);
}

bool CPveRoguelite::TutorialResearchActive() const
{
	return g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 1 &&
		g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_BUILD && g_Config.m_ClTutorialStep == 2;
}

void CPveRoguelite::BuySelectedResearch()
{
	CPveResearchMask Mask = TutorialResearchActive() ? CPveResearchMask() : ParseResearchMask();
	const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
	if(!pSelected || !CanBuyResearch(m_SelectedResearch, Mask))
		return;
	if(Client()->State() == IClient::STATE_ONLINE && m_ProgressSent)
	{
		CNetMsg_Cl_PveResearchBuy Msg;
		Msg.m_Nonce = ++m_ResearchNonce;
		Msg.m_Card = m_SelectedResearch;
		Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	}
	else
	{
		g_Config.m_ClPveResearchPoints -= pSelected->m_ResearchCost;
		Mask.Set(m_SelectedResearch);
		StoreResearchMask(Mask);
		SaveProgress();
	}
	m_SelectionPulse = 1.0f;
}

int CPveRoguelite::ShopCost(int BaseCost) const
{
	float Multiplier = 1.0f - min(0.40f, m_aRunPerks[PVE_CARD_QUARTERMASTER] * 0.10f);
	if(m_ContractState == PVE_CONTRACT_STATE_ACTIVE && m_ActiveContract == PVE_CONTRACT_RESOURCE_DROUGHT)
		Multiplier *= 1.5f;
	return max(1, (int)(BaseCost * Multiplier + 0.5f));
}

int CPveRoguelite::BuildingCost(int BaseCost) const
{
	return m_aRunPerks[PVE_CARD_ENGINEER] ? max(1, (BaseCost * 80 + 99) / 100) : BaseCost;
}

void CPveRoguelite::CycleCheckpoint()
{
	const int MaxCheckpoint = g_Config.m_ClPveHighestInvasion >= 10 ? (g_Config.m_ClPveHighestInvasion / 10) * 10 + 1 : 1;
	g_Config.m_ClPvePreferredCheckpoint += 10;
	if(g_Config.m_ClPvePreferredCheckpoint > MaxCheckpoint)
		g_Config.m_ClPvePreferredCheckpoint = 1;
	SaveProgress();
	if(Client()->State() == IClient::STATE_ONLINE && m_ProgressSent)
		SyncProgress();
}

void CPveRoguelite::RenderResearch(CUIRect MainView)
{
	m_ResearchVisible = true;
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 4.0f);
	m_ResearchAppearAmount += (1.0f - m_ResearchAppearAmount) * (1.0f - expf(-11.0f * Dt));
	if(m_ResearchAnimTab != m_ResearchTab)
	{
		m_ResearchAnimTab = m_ResearchTab;
		for(int i = 0; i < 4; i++)
			m_aBranchExpand[i] = 0.0f;
		for(int i = 0; i < 3; i++)
			m_aRouteExpand[i] = 0.0f;
	}
	for(int i = 0; i < 4; i++)
		m_aBranchExpand[i] += (((i == m_ResearchBranch) ? 1.0f : 0.0f) - m_aBranchExpand[i]) * (1.0f - expf(-11.0f * Dt));
	for(int i = 0; i < 3; i++)
		m_aRouteExpand[i] += (((i == m_ResearchRoute) ? 1.0f : 0.0f) - m_aRouteExpand[i]) * (1.0f - expf(-12.0f * Dt));
	const float Alpha = clamp(m_ResearchAppearAmount, 0.0f, 1.0f);
	const float Time = (float)time_get() / (float)time_freq();
	const float Wave = 0.5f + 0.5f * sinf(Time * 2.4f);
	const float WaveFast = 0.5f + 0.5f * sinf(Time * 4.2f);
	const float Scale = clamp(min(MainView.w / 780.0f, MainView.h / 520.0f), 0.82f, 1.08f);
	// CUIRect split/margin helpers already apply ui_scale. Most of this page is
	// positioned explicitly, so compensate helper arguments to avoid applying
	// the global scale twice at 125-150%.
	const float LayoutScale = Scale / max(0.01f, UI()->Scale());
	const bool Compact = UI()->Scale() > 1.15f || MainView.h < 470.0f;
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	auto Fade = [&](const vec4 &Color, float Opacity) {
		return vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha);
	};
	auto Mix = [&](const vec4 &A, const vec4 &B, float Amount) {
		return vec4(A.r + (B.r - A.r) * Amount, A.g + (B.g - A.g) * Amount,
			A.b + (B.b - A.b) * Amount, A.a + (B.a - A.a) * Amount);
	};
	auto DrawSprite = [&](const CPveUiIcon &Icon, float X, float Y, float Size, const vec4 &Color, float Opacity) {
		DrawIcon(Icon.m_Image, Icon.m_Sprite, X, Y, Size * Icon.m_Scale,
			vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha));
	};
	auto SoftToward = [&](float &Value, float Target, float Speed) {
		Value += (Target - Value) * (1.0f - expf(-Speed * Dt));
	};
	// Research copy was tuned small for dense trees; bump readable size without
	// rewriting every layout rect.
	const float Font = 1.18f;
	auto ResearchText = [&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth = -1.0f, int Align = -1) {
		DrawText(X, Y, Size * Font, pText, Color, MaxWidth, Align);
	};
	auto ResearchWrapped = [&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines) {
		DrawWrappedText(X, Y, Size * Font, pText, Color, MaxWidth, MaxLines);
	};

	MainView.y += (1.0f - Alpha) * 8.0f * Scale;
	DrawPanel(MainView, Fade(AccentDim, 0.24f + 0.10f * Wave), 12.0f * Scale);
	MainView.Margin(1.4f * LayoutScale, &MainView);
	DrawPanel(MainView, Fade(Deep, 0.99f), 10.5f * Scale);

	CUIRect Header, Body;
	MainView.Margin(8.0f * LayoutScale, &MainView);
	MainView.HSplitTop((Compact ? 102.0f : 116.0f) * LayoutScale, &Header, &Body);
	CUIRect HeaderShadow = Header;
	HeaderShadow.y += 2.0f * Scale;
	DrawPanel(HeaderShadow, Fade(Deep, 0.62f), 10.0f * Scale);
	DrawPanel(Header, Fade(Panel, 0.98f), 9.0f * Scale);
	CUIRect HeaderEdge = {Header.x + 12.0f * Scale, Header.y + Header.h - 1.5f * Scale, Header.w - 24.0f * Scale, 1.5f * Scale};
	DrawPanel(HeaderEdge, Fade(Accent, 0.52f + 0.28f * Wave), 0.75f * Scale);
	CUIRect TitleMark = {Header.x + 12.0f * Scale, Header.y + 8.0f * Scale, 3.0f * Scale, 23.0f * Scale};
	TitleMark.y += Wave * 0.6f * Scale;
	DrawPanel(TitleMark, Fade(Accent, 0.78f + 0.22f * WaveFast), 1.5f * Scale);
	ResearchText(Header.x + 23.0f * Scale, Header.y + 6.0f * Scale, 13.0f * Scale, Localize("Research"), Fade(Text, 1.0f));

	char aPoints[64];
	str_format(aPoints, sizeof(aPoints), Localize("%d Research Points"), TutorialResearchActive() ? 99 : g_Config.m_ClPveResearchPoints);
	const float PointsWidth = clamp(Header.w * 0.23f, 156.0f * Scale, 196.0f * Scale);
	CUIRect Points = {Header.x + Header.w - PointsWidth - 10.0f * Scale, Header.y + 7.0f * Scale, PointsWidth, (Compact ? 28.0f : 32.0f) * Scale};
	DrawPanel(Points, Fade(AccentDim, 0.34f + 0.16f * Wave), 15.0f * Scale);
	CUIRect PointsInner = Points;
	PointsInner.Margin(1.2f * LayoutScale, &PointsInner);
	DrawPanel(PointsInner, Fade(Inset, 0.98f), 14.0f * Scale);
	const float CoinPulse = 1.0f + 0.06f * WaveFast;
	DrawSprite(CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_BIGCOIN), Points.x + 17.0f * Scale, Points.y + Points.h * 0.5f, 15.0f * Scale * CoinPulse, Accent, 0.88f + 0.12f * Wave);
	ResearchText(Points.x + Points.w * 0.57f, Points.y + (Compact ? 5.3f : 7.0f) * Scale, 9.2f * Scale, aPoints, Fade(Accent, 1.0f), -1.0f, 0);

	ResearchText(Header.x + 13.0f * Scale, Header.y + (Compact ? 31.0f : 35.0f) * Scale, 9.1f * Scale,
		Localize("Research unlocks perk cards; select them during a run to activate their effects."), Fade(Text, 0.92f), Header.w - 26.0f * Scale, -1);
	ResearchText(Header.x + 13.0f * Scale, Header.y + (Compact ? 46.0f : 53.5f) * Scale, 8.3f * Scale,
		Localize("Base cards are always available • Rare and Epic perks are unique"), Fade(Accent, 0.88f), Header.w - 26.0f * Scale, -1);

	const char *apTabs[3] = {Localize("Core"), Localize("Weapons"), Localize("Modes")};
	const CPveUiIcon aTabIcons[3] = {
		CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_ARMOR),
		PveSpecializationIcon(PVE_SPECIALIZATION_FIREARM),
		CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_KIT)};
	const float TabsX = Header.x + 12.0f * Scale;
	const float TabsRight = Header.x + Header.w - 10.0f * Scale;
	const float TabGap = 6.0f * Scale;
	const float TabWidth = (TabsRight - TabsX - TabGap * 2.0f) / 3.0f;
	for(int Tab = 0; Tab < 3; Tab++)
	{
		CUIRect TabRect = {TabsX + Tab * (TabWidth + TabGap), Header.y + (Compact ? 68.0f : 78.0f) * Scale, TabWidth, (Compact ? 26.0f : 29.0f) * Scale};
		const bool Selected = Tab == m_ResearchTab;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aTabButtonIDs[Tab]);
		const float Lift = (Selected ? 1.2f : 0.0f) + Hover * 1.4f;
		TabRect.y -= Lift * Scale;
		CUIRect TabBorder = TabRect;
		TabBorder.Margin((-1.0f - Hover * 0.4f) * LayoutScale, &TabBorder);
		DrawPanel(TabBorder, Fade(Selected || Hover > 0.2f ? Accent : AccentDim, Selected ? 0.78f : (0.18f + Hover * 0.42f)), 7.0f * Scale);
		DrawPanel(TabRect, Fade(Selected ? AccentDim : Inset, Selected ? 0.62f : 0.96f), 6.0f * Scale);
		DrawSprite(aTabIcons[Tab], TabRect.x + 18.0f * Scale, TabRect.y + TabRect.h * 0.5f, (14.0f + Hover) * Scale, Selected ? Accent : Text, Selected ? 1.0f : (0.55f + Hover * 0.25f));
		ResearchText(TabRect.x + TabRect.w * 0.54f, TabRect.y + 5.8f * Scale, 9.2f * Scale, apTabs[Tab], Fade(Selected ? Text : AccentDim, 1.0f), -1.0f, 0);
		if(Selected)
		{
			const float LineWidth = (TabRect.w - 20.0f * Scale) * (0.72f + 0.28f * Wave);
			CUIRect SelectedLine = {TabRect.x + (TabRect.w - LineWidth) * 0.5f, TabRect.y + TabRect.h - 1.5f * Scale, LineWidth, 1.5f * Scale};
			DrawPanel(SelectedLine, Fade(Accent, 0.85f + 0.15f * WaveFast), 0.75f * Scale);
		}
		if(UI()->DoButtonLogic(&m_aTabButtonIDs[Tab], &TabRect))
		{
			m_ResearchTab = Tab;
			m_ResearchBranch = 0;
			m_ResearchRoute = 0;
			m_SelectionPulse = 0.7f;
			for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == Tab)
				{
					m_SelectedResearch = ID;
					break;
				}
		}
	}
	if(m_ResearchTab == PVE_TAB_MODE)
	{
		const float CheckpointWidth = clamp(Header.w * 0.17f, 106.0f * Scale, 142.0f * Scale);
		CUIRect Checkpoint = {Points.x - CheckpointWidth - 7.0f * Scale, Header.y + 7.0f * Scale, CheckpointWidth, (Compact ? 26.0f : 30.0f) * Scale};
		const bool Hovered = UI()->HotItem() == &m_CheckpointButtonID;
		DrawPanel(Checkpoint, Fade(Hovered ? AccentDim : Inset, 0.98f), 8.0f * Scale);
		char aCheckpoint[64];
		str_format(aCheckpoint, sizeof(aCheckpoint), Localize("Checkpoint %d"), g_Config.m_ClPvePreferredCheckpoint);
		ResearchText(Checkpoint.x + Checkpoint.w * 0.5f, Checkpoint.y + (Compact ? 5.1f : 6.9f) * Scale, 8.6f * Scale, aCheckpoint, Fade(Accent, 1.0f), -1.0f, 0);
		if(UI()->DoButtonLogic(&m_CheckpointButtonID, &Checkpoint))
			CycleCheckpoint();
	}

	Body.HSplitTop(8.0f * LayoutScale, 0, &Body);
	CUIRect Tree, Details;
	const float DetailWidth = clamp(Body.w * 0.32f, 250.0f * Scale, 294.0f * Scale);
	Body.VSplitRight(DetailWidth / max(0.01f, UI()->Scale()), &Tree, &Details);
	Tree.VSplitRight(8.0f * LayoutScale, &Tree, 0);
	DrawPanel(Tree, Fade(Panel, 0.96f), 9.0f * Scale);
	DrawPanel(Details, Fade(Panel, 0.96f), 9.0f * Scale);
	const char *apTabDescriptions[3] = {
		Localize("Core research improves universal attack, survival, and logistics perks."),
		Localize("Weapon research unlocks specialization perks matched to your current weapon."),
		Localize("Mode research unlocks perks that appear only in the matching PvE mode.")};
	const float TreeIntroHeight = (Compact ? 36.0f : 42.0f) * Scale;
	CUIRect TreeIntro = {Tree.x + 8.0f * Scale, Tree.y + 7.0f * Scale, Tree.w - 16.0f * Scale, TreeIntroHeight};
	DrawPanel(TreeIntro, Fade(Inset, 0.84f), 7.0f * Scale);
	CUIRect TreeIntroMark = {TreeIntro.x, TreeIntro.y + 7.0f * Scale, 2.0f * Scale, TreeIntro.h - 14.0f * Scale};
	DrawPanel(TreeIntroMark, Fade(Accent, 0.72f + 0.20f * Wave), 1.0f * Scale);
	ResearchText(TreeIntro.x + 10.0f * Scale, TreeIntro.y + 4.0f * Scale, 9.0f * Scale, apTabs[m_ResearchTab], Fade(Text, 1.0f));
	ResearchText(TreeIntro.x + 10.0f * Scale, TreeIntro.y + (Compact ? 16.5f : 19.5f) * Scale, 8.2f * Scale, apTabDescriptions[m_ResearchTab], Fade(Text, 0.80f), TreeIntro.w - 20.0f * Scale, -1);
	const float BranchOffset = (Compact ? 46.0f : 52.0f) * Scale;
	CUIRect BranchArea = {Tree.x, Tree.y + BranchOffset, Tree.w, Tree.h - BranchOffset};
	int BranchCount = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
			BranchCount = max(BranchCount, PveCardDef(ID)->m_Branch + 1);
	BranchCount = max(1, BranchCount);
	m_ResearchBranch = clamp(m_ResearchBranch, 0, BranchCount - 1);
	const CPveResearchMask Mask = TutorialResearchActive() ? CPveResearchMask() : ParseResearchMask();
	const float BranchGap = (Compact ? 3.5f : 5.0f) * Scale;
	const float BranchHeaderHeight = (Compact ? 29.0f : 33.0f) * Scale;
	const float ExpandedHeight = max(BranchHeaderHeight,
		BranchArea.h - 8.0f * Scale - (BranchCount - 1) * (BranchHeaderHeight + BranchGap));
	float BranchExpandSum = 0.0f;
	for(int Branch = 0; Branch < BranchCount; Branch++)
		BranchExpandSum += m_aBranchExpand[clamp(Branch, 0, 3)];
	float BranchY = BranchArea.y + 4.0f * Scale;
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		int aNodes[NUM_PVE_CARDS];
		int Count = 0;
		int BoughtCount = 0;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			const CPveCardDef *pDef = PveCardDef(ID);
			if(!pDef->m_Base && pDef->m_Tab == m_ResearchTab && pDef->m_Branch == Branch)
			{
				aNodes[Count++] = ID;
				BoughtCount += PveCardIsUnlocked(ID, Mask);
			}
		}
		if(Count <= 0)
			continue;
		const char *pBranchName = "";
		if(m_ResearchTab == PVE_TAB_CORE)
		{
			const char *apNames[4] = {"Attack", "Survival", "Logistics", "Drone"};
			pBranchName = apNames[clamp(Branch, 0, 3)];
		}
		else if(m_ResearchTab == PVE_TAB_WEAPON)
		{
			const char *apNames[4] = {"Firearms", "Explosives", "Electric", "Melee"};
			pBranchName = apNames[Branch];
		}
		else
		{
			const char *apNames[3] = {"Invasion", "Horde", "Extraction"};
			pBranchName = apNames[Branch];
		}
		const bool Expanded = Branch == m_ResearchBranch;
		const float BranchOpen = BranchExpandSum > 0.001f ? m_aBranchExpand[clamp(Branch, 0, 3)] / BranchExpandSum : 0.0f;
		const float BranchHover = m_pClient->m_pMenus->AnimHover(&m_aBranchButtonIDs[Branch]);
		const float BranchHeight = BranchHeaderHeight + (ExpandedHeight - BranchHeaderHeight) * BranchOpen;
		CUIRect BranchPanel = {BranchArea.x + 8.0f * Scale, BranchY, BranchArea.w - 16.0f * Scale, BranchHeight};
		DrawPanel(BranchPanel, Fade(BranchOpen > 0.08f ? Inset : Deep, 0.68f + 0.16f * BranchOpen), 8.0f * Scale);
		CUIRect BranchHeader = {BranchPanel.x, BranchPanel.y, BranchPanel.w, BranchHeaderHeight};
		if(BranchOpen > 0.08f)
		{
			const float HeaderLineW = (BranchHeader.w - 16.0f * Scale) * (0.78f + 0.22f * Wave) * clamp(BranchOpen * 1.4f, 0.0f, 1.0f);
			CUIRect HeaderLine = {BranchHeader.x + (BranchHeader.w - HeaderLineW) * 0.5f, BranchHeader.y + BranchHeader.h - 1.5f * Scale, HeaderLineW, 1.5f * Scale};
			DrawPanel(HeaderLine, Fade(Accent, (0.78f + 0.18f * WaveFast) * BranchOpen), 0.75f * Scale);
		}
		DrawSprite(PveBranchIcon(m_ResearchTab, Branch), BranchHeader.x + 17.0f * Scale,
			BranchHeader.y + BranchHeader.h * 0.5f, (15.0f + BranchHover) * Scale, Expanded || BranchOpen > 0.5f ? Accent : AccentDim, 0.72f + 0.28f * BranchOpen);
		ResearchText(BranchHeader.x + 34.0f * Scale, BranchHeader.y + 7.0f * Scale, 9.0f * Scale,
			Localize(pBranchName), Fade(Text, 0.78f + 0.22f * BranchOpen));
		char aBranchProgress[32];
		str_format(aBranchProgress, sizeof(aBranchProgress), "%d / %d", BoughtCount, Count);
		ResearchText(BranchHeader.x + BranchHeader.w - 34.0f * Scale, BranchHeader.y + 7.0f * Scale,
			8.0f * Scale, aBranchProgress, Fade(Accent, 0.90f), -1.0f, 1);
		ResearchText(BranchHeader.x + BranchHeader.w - 13.0f * Scale, BranchHeader.y + 6.2f * Scale,
			9.0f * Scale, Expanded ? "−" : "+", Fade(Expanded ? Accent : Text, 0.90f), -1.0f, 0);
		if(UI()->DoButtonLogic(&m_aBranchButtonIDs[Branch], &BranchHeader))
		{
			m_ResearchBranch = Branch;
			m_ResearchRoute = 0;
			for(int i = 0; i < 3; i++)
				m_aRouteExpand[i] = 0.0f;
			m_SelectionPulse = 0.55f;
			int Best = aNodes[0];
			for(int n = 1; n < Count; n++)
				if(PveResearchRoute(PveCardDef(aNodes[n])) == 0 && PveCardDef(aNodes[n])->m_Tier < PveCardDef(Best)->m_Tier)
					Best = aNodes[n];
			m_SelectedResearch = Best;
		}

		if(BranchHeight <= BranchHeaderHeight + 6.0f * Scale)
		{
			BranchY += BranchHeight + BranchGap;
			continue;
		}

		CUIRect RouteArea = {BranchPanel.x + 7.0f * Scale, BranchHeader.y + BranchHeader.h + 6.0f * Scale,
			BranchPanel.w - 14.0f * Scale, BranchPanel.h - BranchHeader.h - 13.0f * Scale};
		const int RouteCount = PveResearchRouteCount(m_ResearchTab, Branch);
		m_ResearchRoute = clamp(m_ResearchRoute, 0, RouteCount - 1);
		const float RouteGap = (Compact ? 2.5f : 4.0f) * Scale;
		const float RouteHeaderHeight = (Compact ? 21.0f : 26.0f) * Scale;
		const float ExpandedRouteHeight = max(RouteHeaderHeight,
			RouteArea.h - (RouteCount - 1) * (RouteHeaderHeight + RouteGap));
		float RouteExpandSum = 0.0f;
		for(int Route = 0; Route < RouteCount; Route++)
			RouteExpandSum += m_aRouteExpand[clamp(Route, 0, 2)];
		float RouteY = RouteArea.y;
		for(int Route = 0; Route < RouteCount; Route++)
		{
			int aRouteNodes[NUM_PVE_CARDS];
			int RouteNodeCount = 0;
			int RouteBoughtCount = 0;
			for(int n = 0; n < Count; n++)
				if(PveResearchRoute(PveCardDef(aNodes[n])) == Route)
				{
					aRouteNodes[RouteNodeCount++] = aNodes[n];
					RouteBoughtCount += PveCardIsUnlocked(aNodes[n], Mask);
				}
			if(RouteNodeCount <= 0)
				continue;
			const bool RouteExpanded = Route == m_ResearchRoute;
			const float RouteOpen = RouteExpandSum > 0.001f ? m_aRouteExpand[clamp(Route, 0, 2)] / RouteExpandSum : 0.0f;
			const float RouteHover = m_pClient->m_pMenus->AnimHover(&m_aRouteButtonIDs[Route]);
			const float RouteHeight = RouteHeaderHeight + (ExpandedRouteHeight - RouteHeaderHeight) * RouteOpen;
			CUIRect RoutePanel = {RouteArea.x, RouteY, RouteArea.w, RouteHeight};
			DrawPanel(RoutePanel, Fade(RouteOpen > 0.08f ? Panel : Deep, 0.58f + 0.20f * RouteOpen), 7.0f * Scale);
			CUIRect RouteHeader = {RoutePanel.x, RoutePanel.y, RoutePanel.w, RouteHeaderHeight};
			if(RouteOpen > 0.08f)
			{
				const float RouteLineW = (RouteHeader.w - 18.0f * Scale) * (0.76f + 0.24f * Wave) * clamp(RouteOpen * 1.4f, 0.0f, 1.0f);
				CUIRect RouteLine = {RouteHeader.x + (RouteHeader.w - RouteLineW) * 0.5f, RouteHeader.y + RouteHeader.h - Scale,
					RouteLineW, Scale};
				DrawPanel(RouteLine, Fade(Accent, (0.62f + 0.22f * WaveFast) * RouteOpen), 0.5f * Scale);
			}
			ResearchText(RouteHeader.x + 10.0f * Scale, RouteHeader.y + (Compact ? 3.0f : 5.4f) * Scale, 8.2f * Scale,
				Localize(PveResearchRouteName(m_ResearchTab, Branch, Route)), Fade(RouteExpanded || RouteOpen > 0.5f ? Text : AccentDim, 0.90f + RouteHover * 0.08f));
			char aRouteProgress[32];
			str_format(aRouteProgress, sizeof(aRouteProgress), "%d / %d", RouteBoughtCount, RouteNodeCount);
			ResearchText(RouteHeader.x + RouteHeader.w - 30.0f * Scale, RouteHeader.y + (Compact ? 3.2f : 5.6f) * Scale,
				7.4f * Scale, aRouteProgress, Fade(Accent, 0.88f), -1.0f, 1);
			ResearchText(RouteHeader.x + RouteHeader.w - 12.0f * Scale, RouteHeader.y + (Compact ? 2.4f : 4.8f) * Scale,
				8.2f * Scale, RouteExpanded ? "−" : "+", Fade(RouteExpanded ? Accent : Text, 0.88f), -1.0f, 0);
			if(UI()->DoButtonLogic(&m_aRouteButtonIDs[Route], &RouteHeader))
			{
				m_ResearchRoute = Route;
				m_SelectionPulse = 0.5f;
				int Best = aRouteNodes[0];
				for(int n = 1; n < RouteNodeCount; n++)
					if(PveCardDef(aRouteNodes[n])->m_Tier < PveCardDef(Best)->m_Tier)
						Best = aRouteNodes[n];
				m_SelectedResearch = Best;
			}
			if(RouteHeight <= RouteHeaderHeight + 5.0f * Scale)
			{
				RouteY += RouteHeight + RouteGap;
				continue;
			}

			CUIRect NodeArea = {RoutePanel.x + 5.0f * Scale, RouteHeader.y + RouteHeader.h + 4.0f * Scale,
				RoutePanel.w - 10.0f * Scale, RoutePanel.h - RouteHeader.h - 9.0f * Scale};
			DrawPanel(NodeArea, Fade(Deep, 0.62f * clamp(RouteOpen * 1.5f, 0.0f, 1.0f)), 6.0f * Scale);
			NodeArea.x += 6.0f * Scale;
			NodeArea.w -= 12.0f * Scale;
			NodeArea.y += 5.0f * Scale;
			NodeArea.h -= 10.0f * Scale;
			const bool DroneModules = m_ResearchTab == PVE_TAB_CORE && Branch == 3 && Route == 0;
			const int Rows = DroneModules || RouteNodeCount > 4 ? 2 : 1;
			const int Columns = (RouteNodeCount + Rows - 1) / Rows;
			const float NodeGap = 6.0f * Scale;
			const float NodeWidth = min(110.0f * Scale, (NodeArea.w - NodeGap * (Columns - 1)) / Columns);
			const float NodeHeight = min(70.0f * Scale, (NodeArea.h - NodeGap * (Rows - 1)) / Rows);
			auto NodeRect = [&](int Index) {
				const int GridRow = Index / Columns;
				const int GridColumn = Index % Columns;
				const int ItemsInRow = min(Columns, RouteNodeCount - GridRow * Columns);
				const float RowWidth = ItemsInRow * NodeWidth + (ItemsInRow - 1) * NodeGap;
				const float StartX = NodeArea.x + (NodeArea.w - RowWidth) * 0.5f;
				const float UsedHeight = Rows * NodeHeight + (Rows - 1) * NodeGap;
				const float StartY = NodeArea.y + (NodeArea.h - UsedHeight) * 0.5f;
				CUIRect Result = {StartX + GridColumn * (NodeWidth + NodeGap), StartY + GridRow * (NodeHeight + NodeGap), NodeWidth, NodeHeight};
				return Result;
			};
			const float ContentFade = clamp(RouteOpen * 1.6f, 0.0f, 1.0f) * clamp(BranchOpen * 1.4f, 0.0f, 1.0f);
			for(int n = 0; n < RouteNodeCount; n++)
			{
				const CPveCardDef *pNodeDef = PveCardDef(aRouteNodes[n]);
				for(int Prerequisite = 0; Prerequisite < pNodeDef->m_NumPrerequisites; Prerequisite++)
				{
					int PreviousIndex = -1;
					for(int Candidate = 0; Candidate < RouteNodeCount; Candidate++)
						if(aRouteNodes[Candidate] == pNodeDef->m_aPrerequisites[Prerequisite])
							PreviousIndex = Candidate;
					if(PreviousIndex < 0)
						continue;
					const CUIRect Previous = NodeRect(PreviousIndex);
					const CUIRect Current = NodeRect(n);
					const bool PrerequisiteBought = PveCardIsUnlocked(aRouteNodes[PreviousIndex], Mask);
					const vec4 LinkColor = PrerequisiteBought ? Accent : AccentDim;
					const float LinkAlpha = (PrerequisiteBought ? (0.72f + 0.24f * WaveFast) : 0.34f) * ContentFade;
					if(PreviousIndex / Columns == n / Columns && Previous.x < Current.x)
					{
						CUIRect Link = {Previous.x + Previous.w, Previous.y + Previous.h * 0.5f - Scale,
							max(0.0f, Current.x - Previous.x - Previous.w), 2.0f * Scale};
						DrawPanel(Link, Fade(LinkColor, LinkAlpha), Scale);
					}
					else
					{
						const float PreviousX = Previous.x + Previous.w * 0.5f;
						const float CurrentX = Current.x + Current.w * 0.5f;
						const float MidY = (Previous.y + Previous.h + Current.y) * 0.5f;
						CUIRect LinkA = {PreviousX - Scale, Previous.y + Previous.h, 2.0f * Scale, max(0.0f, MidY - Previous.y - Previous.h)};
						CUIRect LinkB = {min(PreviousX, CurrentX), MidY - Scale, max(2.0f * Scale, abs(CurrentX - PreviousX)), 2.0f * Scale};
						CUIRect LinkC = {CurrentX - Scale, MidY, 2.0f * Scale, max(0.0f, Current.y - MidY)};
						DrawPanel(LinkA, Fade(LinkColor, LinkAlpha), Scale);
						DrawPanel(LinkB, Fade(LinkColor, LinkAlpha), Scale);
						DrawPanel(LinkC, Fade(LinkColor, LinkAlpha), Scale);
					}
				}
			}
			UI()->ClipEnable(&NodeArea);
			for(int n = 0; n < RouteNodeCount; n++)
			{
				const int ID = aRouteNodes[n];
				const CPveCardDef *pDef = PveCardDef(ID);
				CUIRect Node = NodeRect(n);
				const bool Bought = PveCardIsUnlocked(ID, Mask);
				const bool Available = CanBuyResearch(ID, Mask);
				const bool Selected = ID == m_SelectedResearch;
				const float Hover = m_pClient->m_pMenus->AnimHover(&m_aNodeButtonIDs[ID]);
				const float NodeIn = ContentFade;
				const float SelectBreath = Selected ? (0.70f + 0.30f * WaveFast) : 1.0f;
				const float AvailableGlow = Available && !Bought ? (0.78f + 0.22f * Wave) : 1.0f;
				const float NodeScale = 1.0f + Hover * 0.028f + (Selected ? m_SelectionPulse * 0.02f : 0.0f);
				Node.x -= Node.w * (NodeScale - 1.0f) * 0.5f;
				Node.y -= Node.h * (NodeScale - 1.0f) * 0.5f + Hover * 1.8f * Scale;
				Node.w *= NodeScale;
				Node.h *= NodeScale;
				CUIRect Shadow = Node;
				Shadow.y += (2.0f + Hover) * Scale;
				DrawPanel(Shadow, Fade(Deep, 0.72f * NodeIn), 8.0f * Scale);
				CUIRect Border = Node;
				Border.Margin((-1.2f - Hover * 0.6f - (Selected ? m_SelectionPulse * 1.2f : 0.0f)) * LayoutScale, &Border);
				const vec4 StateColor = Bought || Available ? Accent : Danger;
				DrawPanel(Border, Fade(Selected || Hover > 0.2f ? Accent : StateColor,
					(Selected ? 0.98f * SelectBreath : (Hover > 0.2f ? 0.76f : 0.42f * AvailableGlow)) * NodeIn), 8.0f * Scale);
				DrawPanel(Node, Fade(Bought ? Mix(Inset, AccentDim, 0.42f) : (Available ? Panel : Deep), (Bought ? 0.98f : 0.94f) * NodeIn), 7.0f * Scale);
				if(Selected || Hover > 0.15f)
				{
					const float LineW = (Node.w - 14.0f * Scale) * (Selected ? (0.82f + 0.18f * Wave) : 1.0f);
					CUIRect NodeLine = {Node.x + (Node.w - LineW) * 0.5f, Node.y + 2.5f * Scale, LineW, 1.4f * Scale};
					DrawPanel(NodeLine, Fade(Accent, (Selected ? SelectBreath : 0.65f) * NodeIn), 0.7f * Scale);
				}
				char aState[32];
				str_copy(aState, Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")), sizeof(aState));
				ResearchText(Node.x + 7.0f * Scale, Node.y + 5.8f * Scale, 8.5f * Scale,
					Bought ? "✓" : (Available ? "+" : "×"), Fade(StateColor, NodeIn));
				char aCost[16];
				str_format(aCost, sizeof(aCost), "%d", pDef->m_ResearchCost);
				CUIRect Cost = {Node.x + Node.w - 26.0f * Scale, Node.y + 6.0f * Scale, 20.0f * Scale, 15.0f * Scale};
				DrawPanel(Cost, Fade(Inset, 0.96f * NodeIn), 6.5f * Scale);
				ResearchText(Cost.x + Cost.w * 0.5f, Cost.y + 2.4f * Scale, 7.7f * Scale, aCost, Fade(Accent, 0.95f * NodeIn), -1.0f, 0);
				const char *pName = Localize(pDef->m_pName);
				const float NameWidth = Node.w - 13.0f * Scale;
				float NodeNameSize = 8.8f * Scale;
				while(NodeNameSize > 5.0f * Scale && TextRender()->TextWidth(0, NodeNameSize * Font, pName, -1) > NameWidth)
					NodeNameSize -= 0.2f * Scale;
				ResearchText(Node.x + Node.w * 0.5f, Node.y + 23.8f * Scale, NodeNameSize, pName, Fade(Text, NodeIn), -1.0f, 0);
				CUIRect StateBadge = {Node.x + 6.0f * Scale, Node.y + Node.h - 19.0f * Scale, Node.w - 12.0f * Scale, 14.0f * Scale};
				DrawPanel(StateBadge, Fade(Inset, (Bought || Available ? 0.82f : 0.55f) * NodeIn), 6.0f * Scale);
				ResearchText(StateBadge.x + StateBadge.w * 0.5f, StateBadge.y + 1.7f * Scale, 8.0f * Scale, aState, Fade(StateColor, 0.95f * NodeIn), -1.0f, 0);
				if(UI()->DoButtonLogic(&m_aNodeButtonIDs[ID], &Node))
				{
					m_SelectedResearch = ID;
					m_SelectionPulse = 0.7f;
				}
			}
			UI()->ClipDisable();
			RouteY += RouteHeight + RouteGap;
		}
		BranchY += BranchHeight + BranchGap;
	}

	const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
	if(pSelected)
	{
		Details.Margin((Compact ? 7.0f : 9.0f) * LayoutScale, &Details);
		const bool Bought = PveCardIsUnlocked(m_SelectedResearch, Mask);
		const bool Available = CanBuyResearch(m_SelectedResearch, Mask);
		const vec4 StateColor = Bought || Available ? Accent : Danger;
		char aState[48];
		str_format(aState, sizeof(aState), "%s  %s", Bought ? "✓" : (Available ? "+" : "×"), Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")));
		CUIRect DetailHeader = {Details.x, Details.y, Details.w, (Compact ? 20.0f : 23.0f) * Scale};
		ResearchText(DetailHeader.x + 2.0f * Scale, DetailHeader.y + 2.8f * Scale, 9.1f * Scale, Localize("Research Details"), Fade(Text, 0.96f));
		CUIRect DetailState = {DetailHeader.x + DetailHeader.w - 112.0f * Scale, DetailHeader.y, 112.0f * Scale, (Compact ? 20.0f : 23.0f) * Scale};
		DrawPanel(DetailState, Fade(Inset, 0.92f), 8.0f * Scale);
		CUIRect DetailStateEdge = {DetailState.x, DetailState.y + 5.0f * Scale, 2.0f * Scale, DetailState.h - 10.0f * Scale};
		DrawPanel(DetailStateEdge, Fade(StateColor, (0.82f + 0.18f * WaveFast)), 1.0f * Scale);
		ResearchText(DetailState.x + DetailState.w * 0.5f, DetailState.y + 4.2f * Scale, 7.8f * Scale, aState, Fade(StateColor, 1.0f), -1.0f, 0);

		CUIRect Hero = {Details.x, DetailHeader.y + DetailHeader.h + (Compact ? 4.0f : 6.0f) * Scale, Details.w, (Compact ? 60.0f : 74.0f) * Scale};
		DrawPanel(Hero, Fade(Inset, 0.94f), 8.0f * Scale);
		CUIRect HeroEdge = {Hero.x, Hero.y + 7.0f * Scale, 2.0f * Scale, Hero.h - 14.0f * Scale};
		DrawPanel(HeroEdge, Fade(StateColor, (0.78f + 0.18f * Wave)), 1.0f * Scale);
		CUIRect IconTile = {Hero.x + 10.0f * Scale, Hero.y + (Compact ? 10.0f : 12.0f) * Scale, (Compact ? 40.0f : 50.0f) * Scale, (Compact ? 40.0f : 50.0f) * Scale};
		const float IconPulse = 1.0f + (Available && !Bought ? 0.04f * WaveFast : 0.0f);
		DrawPanel(IconTile, Fade(Deep, 0.86f), 8.0f * Scale);
		DrawSprite(PveCardIcon(pSelected), IconTile.x + IconTile.w * 0.5f, IconTile.y + IconTile.h * 0.5f,
			(Compact ? 30.0f : 36.0f) * Scale * IconPulse, Text, 0.92f);
		const float HeroTextX = IconTile.x + IconTile.w + 10.0f * Scale;
		ResearchText(HeroTextX, Hero.y + 9.0f * Scale, 10.7f * Scale, Localize(pSelected->m_pName), Fade(Text, 1.0f),
			Hero.x + Hero.w - HeroTextX - 9.0f * Scale, -1);
		char aMeta[96];
		str_format(aMeta, sizeof(aMeta), Localize("%s • TIER %d • COST %d"), Localize(PveRarityName(pSelected->m_Rarity)), pSelected->m_Tier, pSelected->m_ResearchCost);
		ResearchText(HeroTextX, Hero.y + (Compact ? 37.0f : 46.8f) * Scale, 8.2f * Scale, aMeta, Fade(Accent, 0.96f), Hero.x + Hero.w - HeroTextX - 9.0f * Scale, -1);

		float SectionY = Hero.y + Hero.h + (Compact ? 4.0f : 7.0f) * Scale;
		ResearchText(Details.x + 2.0f * Scale, SectionY, 8.9f * Scale, Localize("Effect"), Fade(Accent, 1.0f));
		CUIRect Effect = {Details.x, SectionY + (Compact ? 13.0f : 15.0f) * Scale, Details.w, (Compact ? 50.0f : 68.0f) * Scale};
		DrawPanel(Effect, Fade(Inset, 0.74f), 7.0f * Scale);
		ResearchWrapped(Effect.x + 10.0f * Scale, Effect.y + 7.5f * Scale, 8.7f * Scale, Localize(pSelected->m_pDescription), vec4(0.72f, 0.74f, 0.76f, 0.65f), Effect.w - 20.0f * Scale, 5);
		SectionY = Effect.y + Effect.h + (Compact ? 4.0f : 7.0f) * Scale;
		CUIRect Rules = {Details.x, SectionY, Details.w, (Compact ? 42.0f : 50.0f) * Scale};
		DrawPanel(Rules, Fade(Deep, 0.58f), 7.0f * Scale);
		char aStackRule[64];
		if(pSelected->m_MaxStacks == 1)
			str_copy(aStackRule, Localize("Unique perk"), sizeof(aStackRule));
		else
			str_format(aStackRule, sizeof(aStackRule), Localize("Stack limit: %d"), pSelected->m_MaxStacks);
		ResearchText(Rules.x + 10.0f * Scale, Rules.y + 5.8f * Scale, 8.4f * Scale, aStackRule, Fade(Accent, 0.96f), Rules.w - 20.0f * Scale, -1);
		char aPrerequisite[128];
		if(pSelected->m_NumPrerequisites > 0)
		{
			str_copy(aPrerequisite, Localize("Requires:"), sizeof(aPrerequisite));
			str_append(aPrerequisite, " ", sizeof(aPrerequisite));
			for(int i = 0; i < pSelected->m_NumPrerequisites; i++)
			{
				if(i > 0)
					str_append(aPrerequisite, ", ", sizeof(aPrerequisite));
				str_append(aPrerequisite, Localize(PveCardDef(pSelected->m_aPrerequisites[i])->m_pName), sizeof(aPrerequisite));
			}
		}
		else
			str_copy(aPrerequisite, Localize("No prerequisite"), sizeof(aPrerequisite));
		ResearchText(Rules.x + 10.0f * Scale, Rules.y + (Compact ? 21.5f : 25.8f) * Scale, 8.2f * Scale, aPrerequisite, Fade(Available || Bought ? Accent : Danger, 0.94f), Rules.w - 20.0f * Scale, -1);
		int UnlockedResearch = 0;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(!PveCardIsBase(ID) && PveCardIsUnlocked(ID, Mask))
				UnlockedResearch++;
		SoftToward(m_ResearchProgressDisplay, UnlockedResearch / (float)NUM_PVE_RESEARCH_CARDS, 5.5f);
		const float BuyHeight = (Compact ? 30.0f : 34.0f) * Scale;
		CUIRect Buy = {Details.x, Details.y + Details.h - BuyHeight - Scale, Details.w, BuyHeight};
		const float ProgressY = Rules.y + Rules.h + (Compact ? 4.0f : 7.0f) * Scale;
		const float ProgressSpace = max(0.0f, Buy.y - ProgressY - (Compact ? 4.0f : 7.0f) * Scale);
		const float ProgressHeight = min((Compact ? 29.0f : 38.0f) * Scale, ProgressSpace);
		CUIRect Progress = {Details.x, ProgressY, Details.w, ProgressHeight};
		const bool ShowProgress = Progress.h >= 18.0f * Scale;
		if(ShowProgress)
			DrawPanel(Progress, Fade(Inset, 0.68f), 7.0f * Scale);
		if(ShowProgress)
			ResearchText(Progress.x + 10.0f * Scale, Progress.y + 4.0f * Scale, 8.3f * Scale, Localize("Research Progress"), Fade(Accent, 0.96f));
		char aProgress[32];
		str_format(aProgress, sizeof(aProgress), "%d / %d", UnlockedResearch, NUM_PVE_RESEARCH_CARDS);
		if(ShowProgress)
			ResearchText(Progress.x + Progress.w - 10.0f * Scale, Progress.y + 4.0f * Scale, 8.2f * Scale, aProgress, Fade(Text, 0.86f), -1.0f, 1);
		CUIRect ProgressBar = {Progress.x + 10.0f * Scale, Progress.y + Progress.h - 8.0f * Scale, Progress.w - 20.0f * Scale, 4.0f * Scale};
		if(ShowProgress)
			DrawPanel(ProgressBar, Fade(Deep, 0.78f), 2.0f * Scale);
		if(ShowProgress && m_ResearchProgressDisplay > 0.001f)
		{
			CUIRect ProgressFill = ProgressBar;
			ProgressFill.w *= clamp(m_ResearchProgressDisplay, 0.0f, 1.0f);
			DrawPanel(ProgressFill, Fade(Accent, (0.82f + 0.14f * Wave)), 2.0f * Scale);
			if(ProgressFill.w > 4.0f * Scale)
			{
				CUIRect ProgressTip = {ProgressFill.x + ProgressFill.w - 3.0f * Scale, ProgressFill.y - Scale, 3.0f * Scale, ProgressFill.h + 2.0f * Scale};
				DrawPanel(ProgressTip, Fade(Text, (0.35f + 0.35f * WaveFast)), Scale);
			}
		}

		const float BuyHover = Available ? m_pClient->m_pMenus->AnimHover(&m_BuyButtonID) : 0.0f;
		const float BuyPulse = Available ? (0.82f + 0.18f * WaveFast) : 1.0f;
		CUIRect BuyBorder = Buy;
		BuyBorder.Margin((-1.2f - BuyHover * 0.8f) * LayoutScale, &BuyBorder);
		DrawPanel(BuyBorder, Fade(Available ? Accent : (Bought ? AccentDim : Danger), (Available ? 0.90f * BuyPulse : 0.34f)), 9.0f * Scale);
		DrawPanel(Buy, Fade(Available ? (BuyHover > 0.35f ? AccentDim : Accent) : Inset, (Available ? 0.96f : 0.76f)), 8.0f * Scale);
		const bool PurchaseRejected = m_ValidationCode && time_get() < m_ValidationUntil;
		ResearchText(Buy.x + Buy.w * 0.5f, Buy.y + (Compact ? 4.3f : 5.5f) * Scale, (Compact ? 10.5f : 11.4f) * Scale,
			Localize(PurchaseRejected ? "Purchase rejected" : (Bought ? "Unlocked for future choices" : (Available ? "Purchase" : "Locked"))), Fade(PurchaseRejected ? Danger : Text, 1.0f), -1.0f, 0);
		if(Available && UI()->DoButtonLogic(&m_BuyButtonID, &Buy))
			BuySelectedResearch();

		if(Client()->State() == IClient::STATE_ONLINE)
		{
			int PerkLines = 0;
			int TotalPerks = 0;
			for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				if(m_aRunPerks[ID] > 0)
					TotalPerks++;
			const float RunY = Progress.y + Progress.h + 8.0f * Scale;
			CUIRect Run = {Details.x, RunY, Details.w, max(0.0f, Buy.y - RunY - 8.0f * Scale)};
			if(Run.h >= 38.0f * Scale)
			{
				DrawPanel(Run, Fade(Inset, 0.68f), 7.0f * Scale);
				ResearchText(Run.x + 10.0f * Scale, Run.y + 4.8f * Scale, 8.4f * Scale, Localize("Current Run"), Fade(Accent, 1.0f));
				char aCount[24];
				str_format(aCount, sizeof(aCount), "%d", TotalPerks);
				CUIRect CountBadge = {Run.x + Run.w - 30.0f * Scale, Run.y + 5.0f * Scale, 20.0f * Scale, 16.0f * Scale};
				DrawPanel(CountBadge, Fade(Deep, 0.80f), 7.0f * Scale);
				ResearchText(CountBadge.x + CountBadge.w * 0.5f, CountBadge.y + 2.6f * Scale, 7.6f * Scale, aCount, Fade(Text, 0.92f), -1.0f, 0);
				const int MaxLines = max(1, min(10, (int)((Run.h - 29.0f * Scale) / (13.0f * Scale))));
				float Y = Run.y + 26.0f * Scale;
				if(TotalPerks == 0)
					ResearchText(Run.x + 10.0f * Scale, Y, 7.8f * Scale, Localize("No perks selected"), Fade(Text, 0.62f), Run.w - 20.0f * Scale, -1);
				for(int ID = 0; ID < NUM_PVE_CARDS && PerkLines < MaxLines; ID++)
				if(m_aRunPerks[ID] > 0)
				{
					if(PerkLines == MaxLines - 1 && TotalPerks > MaxLines)
					{
						char aMore[64];
						str_format(aMore, sizeof(aMore), Localize("+%d more perks"), TotalPerks - PerkLines);
						ResearchText(Run.x + 10.0f * Scale, Y, 7.7f * Scale, aMore, Fade(Accent, 0.92f), Run.w - 20.0f * Scale, -1);
						break;
					}
					char aPerk[96];
					str_format(aPerk, sizeof(aPerk), "%s ×%d", Localize(PveCardDef(ID)->m_pName), m_aRunPerks[ID]);
					ResearchText(Run.x + 10.0f * Scale, Y, 7.8f * Scale, aPerk, Fade(Text, 0.82f), Run.w - 20.0f * Scale, -1);
					Y += 13.0f * Scale;
					PerkLines++;
				}
			}
		}
		else
		{
			const float GuideY = Progress.y + Progress.h + 8.0f * Scale;
			CUIRect Guide = {Details.x, GuideY, Details.w, max(0.0f, Buy.y - GuideY - 8.0f * Scale)};
			if(Guide.h >= 50.0f * Scale)
			{
				DrawPanel(Guide, Fade(Inset, 0.68f), 7.0f * Scale);
				ResearchText(Guide.x + 10.0f * Scale, Guide.y + 4.3f * Scale, 8.4f * Scale, Localize("How Research Works"), Fade(Accent, 1.0f));
				ResearchText(Guide.x + 10.0f * Scale, Guide.y + 19.5f * Scale, 7.6f * Scale, Localize("Earn points from PvE stages and contracts."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
				ResearchText(Guide.x + 10.0f * Scale, Guide.y + 31.5f * Scale, 7.6f * Scale, Localize("Unlock connected nodes in order."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
				ResearchText(Guide.x + 10.0f * Scale, Guide.y + 43.5f * Scale, 7.6f * Scale, Localize("Unlocked cards join future perk choices."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
			}
		}
	}
	TextRender()->TextColor(1, 1, 1, 1);
}

void CPveRoguelite::OnRender()
{
	TickTutorial();
	if(m_ChoiceActive && m_ChoiceDismissAt > 0 && time_get() >= m_ChoiceDismissAt)
	{
		m_ChoiceActive = false;
		m_ChoiceSequence = 0;
		m_ChoiceDismissAt = 0;
	}
	const bool WasResearchVisible = m_ResearchVisible;
	m_ResearchVisible = false;
	if(!WasResearchVisible)
	{
		m_ResearchAppearAmount = 0.0f;
		m_ResearchAnimTab = -1;
		for(int i = 0; i < 4; i++)
			m_aBranchExpand[i] = 0.0f;
		for(int i = 0; i < 3; i++)
			m_aRouteExpand[i] = 0.0f;
	}
	if(m_DebugResearchScreenshotFrames > 0)
	{
		m_pClient->m_pMenus->OpenResearchPage();
		m_ResearchAppearAmount = 1.0f;
		if(--m_DebugResearchScreenshotFrames == 0)
			Graphics()->TakeScreenshot(0);
	}
	if(m_DebugChoiceScreenshotFrames > 0 && --m_DebugChoiceScreenshotFrames == 0)
		Graphics()->TakeScreenshot(0);
	if(m_DebugBuildScreenshotFrames > 0)
	{
		if(m_DebugScreenshotPage >= 0)
			g_Config.m_UiPage = m_DebugScreenshotPage;
		if(--m_DebugBuildScreenshotFrames == 0)
		{
			Graphics()->TakeScreenshot(0);
			m_DebugScreenshotPage = -1;
		}
	}
	if(m_DebugGameScreenshotFrames > 0 && time_get() >= m_DebugGameScreenshotEarliestTime && Client()->State() == IClient::STATE_ONLINE && m_pClient->m_Snap.m_pLocalCharacter)
	{
		if(--m_DebugGameScreenshotFrames == 0)
		{
			Graphics()->TakeScreenshot(0);
			m_DebugGameScreenshotEarliestTime = 0;
		}
	}
	if(m_InvasionRetryResultActive)
		DrawInvasionRetryResult();
	else if(m_InvasionRetryVoteActive)
		DrawInvasionRetryVote();
	else if(m_ContractVoteActive)
		DrawSelectionOverlay(true);
	else if(m_ChoiceActive)
		DrawSelectionOverlay(false);
	else
	{
		m_AppearAmount = 0.0f;
		// Gameplay status panels sit above the inventory in component order.
		// Hide them while a focused overlay owns the screen so they cannot cover
		// inventory/build controls; selection and vote overlays still render above.
		if(!m_pClient->GameplayInputCaptured())
		{
			DrawContractHud();
			DrawBuildHud();
		}
	}
	DrawTutorialHud();
	DrawDroneWheel();
}

void CPveRoguelite::RenderMenuDebugOverlay()
{
	if(Client()->State() != IClient::STATE_ONLINE && m_InvasionRetryResultActive)
		DrawInvasionRetryResult();
	else if(Client()->State() != IClient::STATE_ONLINE && m_InvasionRetryVoteActive)
		DrawInvasionRetryVote();
	else if(Client()->State() != IClient::STATE_ONLINE && m_ContractVoteActive)
		DrawSelectionOverlay(true);
	else if(Client()->State() != IClient::STATE_ONLINE && m_ChoiceActive)
		DrawSelectionOverlay(false);
}

bool CPveRoguelite::OnInput(IInput::CEvent Event)
{
	if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 1 && m_TutorialNonce <= 0 && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		const char *pBind = m_pClient->m_pBinds->Get(Event.m_Key);
		const int Checkpoint = g_Config.m_ClTutorialCheckpoint;
		if(Checkpoint == 0)
		{
			if(str_comp(pBind, "+left") == 0 || str_comp(pBind, "+right") == 0 || str_comp(pBind, "+gamepadleft") == 0 || str_comp(pBind, "+gamepadright") == 0)
				m_TutorialMoveMask |= 1;
			if(str_comp(pBind, "+jump") == 0 || str_comp(pBind, "+gamepadjump") == 0)
				m_TutorialMoveMask |= 2;
			if(m_TutorialMoveMask == 3)
				AdvanceTutorial();
		}
		else if(Checkpoint == 1 && (str_comp(pBind, "+fire") == 0 || str_comp(pBind, "+gamepadfire") == 0))
		{
			if(++m_TutorialFireCount >= 3)
				AdvanceTutorial();
		}
	}
	if(!ChoiceActive() && !m_ResearchVisible && !m_pClient->GameplayInputCaptured() &&
		(Event.m_Flags & IInput::FLAG_PRESS) && m_aRunPerks[PVE_CARD_DRONE_CHASSIS] > 0)
	{
		int Module = PVE_DRONE_NONE;
		if(Event.m_Key == KEY_GAMEPAD_BUTTON_X)
			Module = PVE_DRONE_ASSAULT;
		else if(Event.m_Key == KEY_GAMEPAD_BUTTON_Y)
			Module = PVE_DRONE_GUARDIAN;
		else if(Event.m_Key == KEY_GAMEPAD_BUTTON_B)
			Module = PVE_DRONE_REPAIR;
		const int Card = Module == PVE_DRONE_ASSAULT ? PVE_CARD_ASSAULT_MODULE : (Module == PVE_DRONE_GUARDIAN ? PVE_CARD_GUARDIAN_MODULE : PVE_CARD_REPAIR_MODULE);
		if(Module != PVE_DRONE_NONE && m_aRunPerks[Card] > 0)
		{
			SendDroneModule(Module);
			return true;
		}
	}
	if(!ChoiceActive() && !m_ResearchVisible)
		return false;
	if(!(Event.m_Flags & IInput::FLAG_PRESS))
		return ChoiceActive();
	if(m_InvasionRetryResultActive)
		return true;
	if(m_InvasionRetryVoteActive)
	{
		int Direction = 0;
		if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT || Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
			Direction = -1;
		else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
			Direction = 1;
		if(Direction)
			m_FocusedChoice = (m_FocusedChoice + Direction + 2) % 2;
		else if(Event.m_Key == KEY_1 || Event.m_Key == KEY_KP_1)
		{
			m_FocusedChoice = PVE_INVASION_RETRY;
			SendInvasionRetryVote(PVE_INVASION_RETRY);
		}
		else if(Event.m_Key == KEY_2 || Event.m_Key == KEY_KP_2)
		{
			m_FocusedChoice = PVE_INVASION_RESET;
			SendInvasionRetryVote(PVE_INVASION_RESET);
		}
		else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A)
			SendInvasionRetryVote(m_FocusedChoice);
		else if(Event.m_Key == KEY_MOUSE_1)
			m_MouseTrigger = true;
		return true;
	}
	if(m_ResearchVisible && !ChoiceActive())
	{
		// Research shares the input stack with menus and inventory. Only consume
		// navigation owned by this page so Escape and player bindings propagate.
		const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
		const int OldTab = m_ResearchTab;
		bool Handled = true;
		if(Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
			m_ResearchTab = (m_ResearchTab + 2) % 3;
		else if(Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
			m_ResearchTab = (m_ResearchTab + 1) % 3;
		else if(Event.m_Key == KEY_TAB)
			m_ResearchTab = (m_ResearchTab + 1) % 3;
		else if(m_ResearchTab == PVE_TAB_MODE && (Event.m_Key == KEY_C || Event.m_Key == KEY_GAMEPAD_BUTTON_Y))
			CycleCheckpoint();
		else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A)
			BuySelectedResearch();
		else if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			// The page now exposes every category and route directly. Consume the
			// wheel while it has focus so it neither changes selection unexpectedly
			// nor leaks through to weapon switching behind the menu.
		}
		else if(pSelected)
		{
			int TargetBranch = pSelected->m_Branch;
			int TargetRoute = PveResearchRoute(pSelected);
			int TargetTier = pSelected->m_Tier;
			bool Navigate = true;
			if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT)
				TargetTier--;
			else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT)
				TargetTier++;
			else if(Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP)
			{
				if(TargetRoute > 0)
					TargetRoute--;
				else
				{
					TargetBranch--;
					TargetRoute = TargetBranch >= 0 ? PveResearchRouteCount(m_ResearchTab, TargetBranch) - 1 : 0;
				}
			}
			else if(Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN)
			{
				if(TargetRoute + 1 < PveResearchRouteCount(m_ResearchTab, TargetBranch))
					TargetRoute++;
				else
				{
					TargetBranch++;
					TargetRoute = 0;
				}
			}
			else
				Navigate = false;
			if(Navigate)
			{
				int Best = -1;
				int BestDistance = 999;
				for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				{
					const CPveCardDef *pDef = PveCardDef(ID);
					if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != TargetBranch || PveResearchRoute(pDef) != TargetRoute)
						continue;
					const int Dist = abs(pDef->m_Tier - TargetTier);
					if(Dist < BestDistance)
					{
						Best = ID;
						BestDistance = Dist;
					}
				}
				if(Best >= 0)
				{
					m_SelectedResearch = Best;
					m_ResearchBranch = PveCardDef(Best)->m_Branch;
					m_ResearchRoute = PveResearchRoute(PveCardDef(Best));
				}
			}
			else
				Handled = false;
		}
		else
			Handled = false;
		if(!Handled)
			return false;
		if(OldTab != m_ResearchTab)
		{
			m_ResearchBranch = 0;
			m_ResearchRoute = 0;
			m_SelectionPulse = 0.7f;
			for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
				{
					m_SelectedResearch = ID;
					break;
				}
		}
		return true;
	}
	const int Count = m_ContractVoteActive ? 2 : 3;
	int Direction = 0;
	if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT || Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
		Direction = -1;
	else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
		Direction = 1;
	if(Direction)
		m_FocusedChoice = (m_FocusedChoice + Direction + Count) % Count;
	else if(Event.m_Key == KEY_1 || Event.m_Key == KEY_KP_1)
	{
		m_FocusedChoice = 0;
		if(m_ContractVoteActive) SendContractVote(0); else SendChoice(0);
	}
	else if((Event.m_Key == KEY_2 || Event.m_Key == KEY_KP_2) && Count >= 2)
	{
		m_FocusedChoice = 1;
		if(m_ContractVoteActive) SendContractVote(1); else SendChoice(1);
	}
	else if((Event.m_Key == KEY_3 || Event.m_Key == KEY_KP_3) && Count >= 3)
	{
		m_FocusedChoice = 2;
		SendChoice(2);
	}
	else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A)
	{
		if(m_ContractVoteActive)
			SendContractVote(m_FocusedChoice);
		else
			SendChoice(m_FocusedChoice);
	}
	else if(Event.m_Key == KEY_MOUSE_1)
		m_MouseTrigger = true;
	return true;
}

bool CPveRoguelite::OnMouseMove(float x, float y)
{
	if(m_DroneWheelActive)
	{
		Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
		Input()->GetRelativePosition(&x, &y);
		m_DroneWheelMouse += vec2(x, y);
		if(length(m_DroneWheelMouse) > 170.0f)
			m_DroneWheelMouse = normalize(m_DroneWheelMouse) * 170.0f;
		return true;
	}
	if(!ChoiceActive())
		return false;
	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	Input()->GetRelativePosition(&x, &y);
	m_SelectorMouse += vec2(x, y) * 0.5f;
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 2.0f, 300.0f * Graphics()->ScreenAspect() - 6.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 2.0f, 294.0f);
	return true;
}

void CPveRoguelite::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_TUTORIALSTATE)
	{
		const CNetMsg_Sv_TutorialState *pMsg = (const CNetMsg_Sv_TutorialState *)pRawMsg;
		g_Config.m_ClTutorialChapter = pMsg->m_Chapter;
		g_Config.m_ClTutorialStep = pMsg->m_Step;
		g_Config.m_ClTutorialCompletedMask = pMsg->m_CompletedMask;
		m_TutorialProgress = pMsg->m_Progress;
		m_TutorialTarget = pMsg->m_Target;
		m_TutorialNonce = pMsg->m_Nonce;
		m_TutorialFlags = pMsg->m_Flags;
		if(pMsg->m_Flags & 2)
		{
			m_pClient->m_pMenus->HandleTutorialChapterCompleted(pMsg->m_Chapter, pMsg->m_CompletedMask);
		}
		else if(pMsg->m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && pMsg->m_Step >= 1)
		{
			m_pClient->m_pMenus->OpenTutorialRoomPractice();
		}
		else if(pMsg->m_Chapter == TUTORIAL_CHAPTER_BUILD && pMsg->m_Step == 2)
		{
			m_SelectedResearch = PVE_CARD_SERVO_LINK;
			const CPveCardDef *pDef = PveCardDef(m_SelectedResearch);
			if(pDef)
			{
				m_ResearchTab = pDef->m_Tab;
				m_ResearchBranch = pDef->m_Branch;
				m_ResearchRoute = PveResearchRoute(pDef);
			}
			m_pClient->m_pMenus->OpenResearchPage();
		}
		return;
	}
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		const CNetMsg_Sv_KillMsg *pMsg = (const CNetMsg_Sv_KillMsg *)pRawMsg;
		if(g_Config.m_ClTutorialActive && m_TutorialNonce <= 0 && g_Config.m_ClTutorialState == 1 && g_Config.m_ClTutorialCheckpoint == 2 && pMsg->m_Killer == m_pClient->m_Snap.m_LocalClientID)
			if(++m_TutorialKillCount >= 3)
				AdvanceTutorial();
	}
	else if(MsgType == NETMSGTYPE_SV_FORGERESULT)
	{
		const CNetMsg_Sv_ForgeResult *pMsg = (const CNetMsg_Sv_ForgeResult *)pRawMsg;
		if(g_Config.m_ClTutorialActive && m_TutorialNonce <= 0 && g_Config.m_ClTutorialState == 1 && g_Config.m_ClTutorialCheckpoint == 4 && pMsg->m_Result == FORGERESULT_SUCCESS)
			AdvanceTutorial();
	}
	else if(MsgType == NETMSGTYPE_SV_PVEPROGRESS)
	{
		if(!m_ProgressSent)
		{
			SyncProgress();
			return;
		}
		if(g_Config.m_ClTutorialActive)
			return;
		CNetMsg_Sv_PveProgress *pMsg = (CNetMsg_Sv_PveProgress *)pRawMsg;
		const unsigned long long Low = (unsigned int)pMsg->m_ResearchMask0 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask1 << 32);
		const unsigned long long High = (unsigned int)pMsg->m_ResearchMask2 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask3 << 32);
		g_Config.m_ClPveProgressVersion = pMsg->m_Version;
		g_Config.m_ClPveResearchPoints = pMsg->m_ResearchPoints;
		StoreResearchMask(CPveResearchMask(Low, High));
		g_Config.m_ClPveHighestInvasion = pMsg->m_HighestInvasion;
		g_Config.m_ClPvePreferredCheckpoint = pMsg->m_PreferredCheckpoint;
		SaveProgress();
	}
	else if(MsgType == NETMSGTYPE_SV_PVECHOICE)
	{
		CNetMsg_Sv_PveChoice *pMsg = (CNetMsg_Sv_PveChoice *)pRawMsg;
		const bool NewChoice = !m_ChoiceActive || m_ChoiceNonce != pMsg->m_Nonce || m_ChoiceSequence != pMsg->m_ChoiceSequence;
		m_ChoiceActive = true;
		m_ContractVoteActive = false;
		m_InvasionRetryVoteActive = false;
		m_InvasionRetryResultActive = false;
		m_ChoiceNonce = pMsg->m_Nonce;
		m_ChoiceSequence = pMsg->m_ChoiceSequence;
		m_ChoiceEndTick = pMsg->m_EndTick;
		m_ChoiceDismissAt = 0;
		m_aChoiceCards[0] = pMsg->m_Card0;
		m_aChoiceCards[1] = pMsg->m_Card1;
		m_aChoiceCards[2] = pMsg->m_Card2;
		m_aChoiceStacks[0] = pMsg->m_Stack0;
		m_aChoiceStacks[1] = pMsg->m_Stack1;
		m_aChoiceStacks[2] = pMsg->m_Stack2;
		if(g_Config.m_Debug)
		{
			char aBuf[192];
			str_format(aBuf, sizeof(aBuf), "offer sequence=%d nonce=%d cards=%d/%d/%d active=%d new=%d",
				pMsg->m_ChoiceSequence, pMsg->m_Nonce, pMsg->m_Card0, pMsg->m_Card1, pMsg->m_Card2, m_ChoiceActive, NewChoice);
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "pve", aBuf);
		}
		if(NewChoice)
		{
			m_MouseTrigger = false;
			m_FocusedChoice = 1;
			for(int i = 0; i < 3; i++)
				m_aCardFocus[i] = 0.0f;
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);
			m_AppearAmount = 0.0f;
			m_SelectionPulse = 0.0f;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVEPERK)
	{
		CNetMsg_Sv_PvePerk *pMsg = (CNetMsg_Sv_PvePerk *)pRawMsg;
		if(pMsg->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
		{
			if(g_Config.m_ClTutorialActive)
				m_TutorialPerkChosen = true;
			// Perk messages also restore the existing run after an Invasion map
			// change. Only this offer's result may dismiss the choice overlay.
			if(m_ChoiceActive && m_ChoiceSequence > 0 && pMsg->m_Choices >= m_ChoiceSequence)
			{
				// Keep the accepted card visible just long enough for the confirmation
				// pulse to read, then fade it during the final 80 ms.
				m_ChoiceNonce = 0;
				m_ChoiceDismissAt = time_get() + time_freq() * 220 / 1000;
			}
			if(pMsg->m_Card < NUM_PVE_CARDS)
				m_aRunPerks[pMsg->m_Card] = pMsg->m_Stacks;
			if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 1 && g_Config.m_ClTutorialCheckpoint == 5)
				AdvanceTutorial();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVECONTRACTVOTE)
	{
		CNetMsg_Sv_PveContractVote *pMsg = (CNetMsg_Sv_PveContractVote *)pRawMsg;
		const bool NewVote = !m_ContractVoteActive || m_ContractNonce != pMsg->m_Nonce;
		m_ContractVoteActive = true;
		m_ChoiceActive = false;
		m_InvasionRetryVoteActive = false;
		m_InvasionRetryResultActive = false;
		m_ContractNonce = pMsg->m_Nonce;
		m_ContractEndTick = pMsg->m_EndTick;
		m_aContractOptions[0] = pMsg->m_Contract0;
		m_aContractOptions[1] = pMsg->m_Contract1;
		m_aContractVotes[0] = pMsg->m_Votes0;
		m_aContractVotes[1] = pMsg->m_Votes1;
		if(NewVote)
		{
			m_MouseTrigger = false;
			m_SelectedContract = -1;
			m_FocusedChoice = 0;
			for(int i = 0; i < 3; i++)
				m_aCardFocus[i] = 0.0f;
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);
			m_AppearAmount = 0.0f;
			m_SelectionPulse = 0.0f;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVECONTRACTSTATUS)
	{
		CNetMsg_Sv_PveContractStatus *pMsg = (CNetMsg_Sv_PveContractStatus *)pRawMsg;
		m_ContractVoteActive = false;
		m_ActiveContract = pMsg->m_Contract;
		m_ContractState = pMsg->m_State;
		m_ContractProgress = pMsg->m_Progress;
		m_ContractTarget = pMsg->m_Target;
		m_ContractStatusEndTick = pMsg->m_EndTick;
	}
	else if(MsgType == NETMSGTYPE_SV_PVERESEARCHREWARD)
	{
		if(g_Config.m_ClTutorialActive)
			return;
		CNetMsg_Sv_PveResearchReward *pMsg = (CNetMsg_Sv_PveResearchReward *)pRawMsg;
		g_Config.m_ClPveResearchPoints = clamp(g_Config.m_ClPveResearchPoints + pMsg->m_Amount, 0, 999);
		g_Config.m_ClPveHighestInvasion = max(g_Config.m_ClPveHighestInvasion, pMsg->m_HighestInvasion);
		if(g_Config.m_ClPvePreferredCheckpoint > pMsg->m_UnlockedCheckpoint)
			g_Config.m_ClPvePreferredCheckpoint = pMsg->m_UnlockedCheckpoint;
		SaveProgress();
	}
	else if(MsgType == NETMSGTYPE_SV_PVEBUILDSTATE)
	{
		CNetMsg_Sv_PveBuildState *pMsg = (CNetMsg_Sv_PveBuildState *)pRawMsg;
		m_aWeaponResources[0] = pMsg->m_Focus;
		m_aWeaponResources[1] = pMsg->m_BlastCharge;
		m_aWeaponResources[2] = pMsg->m_Voltage;
		m_aWeaponResources[3] = pMsg->m_Fury;
		m_Barrier = pMsg->m_Barrier;
		m_VulnerableTargets = pMsg->m_VulnerableTargets;
		m_BleedingTargets = pMsg->m_BleedingTargets;
		m_LegendaryCard = pMsg->m_LegendaryCard;
		m_DroneModule = pMsg->m_DroneModule;
		m_DroneSwitchReadyTick = pMsg->m_DroneSwitchReadyTick;
	}
	else if(MsgType == NETMSGTYPE_SV_PVEINVASIONRETRYVOTE)
	{
		CNetMsg_Sv_PveInvasionRetryVote *pMsg = (CNetMsg_Sv_PveInvasionRetryVote *)pRawMsg;
		const bool NewVote = !m_InvasionRetryVoteActive || m_InvasionRetryNonce != pMsg->m_Nonce;
		m_ChoiceActive = false;
		m_ContractVoteActive = false;
		m_InvasionRetryResultActive = false;
		m_InvasionRetryVoteActive = true;
		m_InvasionRetryNonce = pMsg->m_Nonce;
		m_InvasionRetryEndTick = pMsg->m_EndTick;
		m_InvasionRetryFloor = pMsg->m_CurrentFloor;
		m_aInvasionRetryVotes[PVE_INVASION_RETRY] = pMsg->m_RetryVotes;
		m_aInvasionRetryVotes[PVE_INVASION_RESET] = pMsg->m_ResetVotes;
		if(NewVote)
		{
			m_MouseTrigger = false;
			m_SelectedInvasionRetry = -1;
			m_FocusedChoice = PVE_INVASION_RETRY;
			for(int i = 0; i < 3; i++)
				m_aCardFocus[i] = 0.0f;
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);
			m_AppearAmount = 0.0f;
			m_SelectionPulse = 0.0f;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVEINVASIONRETRYRESULT)
	{
		CNetMsg_Sv_PveInvasionRetryResult *pMsg = (CNetMsg_Sv_PveInvasionRetryResult *)pRawMsg;
		const bool NewResult = !m_InvasionRetryResultActive || m_InvasionRetryResult != pMsg->m_Result || m_InvasionRetryResultEndTick != pMsg->m_EndTick;
		m_ChoiceActive = false;
		m_ContractVoteActive = false;
		m_InvasionRetryVoteActive = false;
		m_InvasionRetryResultActive = true;
		m_InvasionRetryResult = pMsg->m_Result;
		m_InvasionRetryResultEndTick = pMsg->m_EndTick;
		str_copy(m_aInvasionRetryPlayerName, pMsg->m_pPlayerName, sizeof(m_aInvasionRetryPlayerName));
		if(NewResult)
		{
			m_MouseTrigger = false;
			m_AppearAmount = 0.0f;
			m_SelectionPulse = 0.0f;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVEVALIDATION)
	{
		CNetMsg_Sv_PveValidation *pMsg = (CNetMsg_Sv_PveValidation *)pRawMsg;
		m_ValidationCode = pMsg->m_Code;
		m_ValidationUntil = time_get() + time_freq() * 3;
	}
}
