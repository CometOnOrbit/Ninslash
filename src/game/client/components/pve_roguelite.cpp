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

#include "hud_layout.h"
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

	CPveUiIcon(int Image = IMAGE_WEAPONS, int Sprite = SPRITE_PICKUP_ARMOR, float Scale = 1.0f)
		: m_Image(Image), m_Sprite(Sprite), m_Scale(Scale)
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
	static const char *s_apCore[4][2] = {{"Combat Foundation", "Vulnerable Mastery"},
										 {"Defense Foundation", "Barrier Mastery"},
										 {"Logistics Foundation", "War Economy"},
										 {"Drone Modules", "Drone Firmware"}};
	static const char *s_apWeapon[4][3] = {{"Firearm Foundation", "Focus Mastery", "Vulnerable Tactics"},
											 {"Explosive Foundation", "Blast Mastery", "Siege Tactics"},
											 {"Electric Foundation", "Voltage Mastery", "Grid Tactics"},
											 {"Melee Foundation", "Fury Mastery", "Wound Tactics"}};
	static const char *s_apMode[3][3] = {{"Invasion Foundation", "Deep Run", "Exploration"},
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
		Array[NumItems++] = IGraphics::CFreeformItem(x,
													 y,
													 x + cosf(a1) * r,
													 y + sinf(a1) * r,
													 x + cosf(a3) * r,
													 y + sinf(a3) * r,
													 x + cosf(a2) * r,
													 y + sinf(a2) * r);
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
		Array[NumItems++] = IGraphics::CFreeformItem(x,
													 y,
													 x + cosf(A1) * r,
													 y + sinf(A1) * r,
													 x + cosf(A3) * r,
													 y + sinf(A3) * r,
													 x + cosf(A2) * r,
													 y + sinf(A2) * r);
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
} // namespace

CPveRoguelite::CPveRoguelite()
{
	m_ProgressStorageWritable = true;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		m_aNodeButtonIDs[i] = i;
	for(int i = 0; i < 3; i++)
		m_aTabButtonIDs[i] = i;
	for(int i = 0; i < 4; i++)
	{
		m_aBranchButtonIDs[i] = i;
		m_aModuleButtonIDs[i] = 100 + i;
	}
	for(int Branch = 0; Branch < 4; Branch++)
		for(int Route = 0; Route < 3; Route++)
			m_aRouteButtonIDs[Branch][Route] = Branch * 3 + Route;
	m_BuyButtonID = 0;
	m_CheckpointButtonID = 0;
	m_ResearchBackButtonID = 0;
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
		const EPveProgressLoadResult BackupResult =
			CPveProgressStorage::Load(Storage(), &Data, "pve_progress.json.bak");
		if(BackupResult != PVE_PROGRESS_LOAD_MISSING)
		{
			Result = BackupResult;
			UsedBackup = BackupResult == PVE_PROGRESS_LOAD_OK;
		}
	}
	if(Result == PVE_PROGRESS_LOAD_FUTURE_VERSION)
	{
		m_ProgressStorageWritable = false;
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
						 "pve",
						 "pve_progress.json was created by a newer game version; it will not be overwritten");
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
	Console()->Register("pve_debug_choice",
						"?i",
						CFGFLAG_CLIENT,
						ConDebugChoice,
						this,
						"Preview three perk cards from a starting card ID");
	Console()->Register("pve_debug_contract",
						"?i?i",
						CFGFLAG_CLIENT,
						ConDebugContract,
						this,
						"Preview a contract ID as vote/active/success/failure (state 0-3)");
	Console()->Register("pve_debug_invasion_retry",
						"?i",
						CFGFLAG_CLIENT,
						ConDebugInvasionRetry,
						this,
						"Preview the Invasion retry vote or result (state 0-3)");
	Console()->Register("pve_debug_research",
						"?i?i",
						CFGFLAG_CLIENT,
						ConDebugResearch,
						this,
						"Preview and capture research states (0-2) with an optional tab (0-2)");
	Console()->Register("pve_debug_build",
						"?i",
						CFGFLAG_CLIENT,
						ConDebugBuild,
						this,
						"Preview and capture the PvE build HUD with an optional drone module (1-3)");
	Console()->Register("pve_debug_screenshot",
						"?i",
						CFGFLAG_CLIENT,
						ConDebugScreenshot,
						this,
						"Capture the current client UI after initialization, optionally forcing a UI page");
	Console()->Register("pve_debug_game_screenshot",
						"?i?i",
						CFGFLAG_CLIENT,
						ConDebugGameScreenshot,
						this,
						"Capture gameplay after a local character appears: optional frame and millisecond delays");
	Console()->Register("pve_drone_module",
						"i",
						CFGFLAG_CLIENT,
						ConDroneModule,
						this,
						"Switch support drone module: 1 assault, 2 guardian, 3 repair");
	Console()->Register(
		"+dronewheel", "", CFGFLAG_CLIENT, ConKeyDroneWheel, this, "Hold to open the drone command wheel");
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
		if(pSelf->ChoiceActive() || pSelf->m_ResearchVisible || pSelf->m_pClient->GameplayInputCaptured() ||
		   pSelf->m_pClient->m_pBuildPlacement->Active())
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
		pSelf->m_ContractState = State == 1 ? PVE_CONTRACT_STATE_ACTIVE
											: (State == 2 ? PVE_CONTRACT_STATE_SUCCESS : PVE_CONTRACT_STATE_FAILED);
		pSelf->m_ContractProgress = State == 1 ? 1 : 3;
		pSelf->m_ContractTarget = 3;
		pSelf->m_ContractStatusEndTick =
			State == 1 ? pSelf->Client()->GameTick() + pSelf->Client()->GameTickSpeed() * 30 : 0;
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
	pSelf->m_DroneModule = pResult->NumArguments()
							   ? clamp(pResult->GetInteger(0), (int)PVE_DRONE_ASSAULT, (int)PVE_DRONE_REPAIR)
							   : PVE_DRONE_GUARDIAN;
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
	static const char *s_apChapterNames[6] = {"First Deployment",
											  "Combat and Recovery",
											  "PvE Mission",
											  "Forge and Build",
											  "Build and Growth",
											  "Multiplayer Ready"};
	char aText[256];
	const int Chapter = clamp(g_Config.m_ClTutorialChapter, 1, 6);
	const int Step = clamp(g_Config.m_ClTutorialStep, 0, 9);
	switch(Chapter)
	{
		case 1:
			str_format(aText,
					   sizeof(aText),
					   Step == 0   ? Localize("Move with %s, then jump with %s")
					   : Step == 1 ? Localize("Aim and fire with %s")
								   : Localize("Switch weapons and hit the training target."),
					   Step == 0 ? pMove : pFire,
					   pJump);
			break;
		case 2:
			str_copy(aText,
					 Localize(Step == 0	  ? "Defeat the marked enemies and watch your ammunition."
							  : Step == 1 ? "Take controlled damage, then collect health."
										  : "Respawn near the current objective and finish the encounter."),
					 sizeof(aText));
			break;
		case 3:
			str_copy(aText,
					 Localize(Step == 0 ? "Follow the radar marker. Stay near the switch for 2 seconds to activate it."
							  : Step == 1 ? "Stay near the next marked switch for 2 seconds to secure the defense area."
							  : Step == 2 ? "Activate the next marked switch and watch the HUD progress."
										  : "Activate the final marked switch to open the extraction route."),
					 sizeof(aText));
			break;
		case 4:
			str_format(aText,
					   sizeof(aText),
					   Step == 0   ? Localize("Collect the marked sandbox materials.")
					   : Step == 1 ? Localize("Open Forge with %s and craft the recommended weapon.")
								   : Localize("Build a defense and survive the controlled wave."),
					   pForge);
			break;
		case 5:
			if(Step == 1)
				str_format(
					aText, sizeof(aText), Localize("Hold %s, choose a drone module, then release to switch."), pDrone);
			else
				str_copy(aText,
						 Localize(Step == 0 ? "Choose one of the three run perks."
											: "Purchase the highlighted sandbox research node. Tutorial points do not "
											  "affect your save."),
						 sizeof(aText));
			break;
		default:
			str_copy(aText,
					 Localize(Step == 0	  ? "Complete the short bot PvP objective."
							  : Step == 1 ? "Configure and create the simulated room."
										  : "Filter the simulated room list and join a room."),
					 sizeof(aText));
			break;
	}
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	CUIRect Hud = {ScreenWidth * 0.5f - 100.0f, 12.0f, 200.0f, 34.0f};
	DrawPanel(Hud, vec4(Panel.r, Panel.g, Panel.b, 0.94f), 6.0f);
	CUIRect Edge = {Hud.x, Hud.y, 2.0f, Hud.h};
	DrawPanel(Edge, Accent, 1.0f);
	char aTitle[128];
	str_format(aTitle,
			   sizeof(aTitle),
			   Localize("CHAPTER %d/6 · %s · %d/%d"),
			   Chapter,
			   Localize(s_apChapterNames[Chapter - 1]),
			   m_TutorialProgress,
			   max(1, m_TutorialTarget));
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
	str_format(g_Config.m_ClPveResearchMask,
			   sizeof(g_Config.m_ClPveResearchMask),
			   "%016llX%016llX",
			   Mask.m_aWords[1],
			   Mask.m_aWords[0]);
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
		str_format(aBuf,
				   sizeof(aBuf),
				   "progress points=%d mask=%016llX%016llX checkpoint=%d state=%d",
				   Msg.m_ResearchPoints,
				   Mask.m_aWords[1],
				   Mask.m_aWords[0],
				   Msg.m_PreferredCheckpoint,
				   Client()->State());
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
	if(Client()->State() != IClient::STATE_ONLINE || Module < PVE_DRONE_ASSAULT || Module > PVE_DRONE_REPAIR ||
	   Client()->GameTick() < m_DroneSwitchReadyTick)
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
	TextRender()->TextOutlineColor(
		CMenus::ThemeBgDeep().r, CMenus::ThemeBgDeep().g, CMenus::ThemeBgDeep().b, 0.45f * Color.a);
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
	float CursorX = X;
	const float TextWidth = TextRender()->TextWidth(0, ActualSize, pText, -1);
	if(TextWidth <= MaxWidth)
	{
		if(Align == 0)
			CursorX -= TextWidth * 0.5f;
		else if(Align > 0)
			CursorX -= TextWidth;
	}
	else if(Align == 0)
		CursorX -= MaxWidth * 0.5f;
	else if(Align > 0)
		CursorX -= MaxWidth;
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, CursorX, Y, ActualSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = MaxWidth;
	TextRender()->TextEx(&Cursor, pText, -1);
}

void CPveRoguelite::DrawWrappedText(
	float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines)
{
	TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
	TextRender()->TextOutlineColor(
		CMenus::ThemeBgDeep().r, CMenus::ThemeBgDeep().g, CMenus::ThemeBgDeep().b, 0.45f * Color.a);
	float ActualSize = Size;
	while(ActualSize > Size * 0.76f && TextRender()->TextLineCount(0, ActualSize, pText, MaxWidth) > MaxLines)
		ActualSize -= 0.25f;
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, X, Y, ActualSize, TEXTFLAG_RENDER);
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
	DrawText(ScreenWidth * 0.5f,
			 10.0f,
			 12.0f,
			 Localize(ContractVote ? "Team Contract" : "Choose a Perk"),
			 vec4(Text.r, Text.g, Text.b, Alpha),
			 -1.0f,
			 0);

	const int EndTick = ContractVote ? m_ContractEndTick : m_ChoiceEndTick;
	const int Seconds =
		max(0, (EndTick - Client()->GameTick() + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
	char aTimer[64];
	str_format(aTimer, sizeof(aTimer), Localize("%d seconds remaining"), Seconds);
	const float WarningPulse =
		Seconds <= 3 ? 0.82f + 0.18f * sinf((float)time_get() / (float)time_freq() * 7.0f) : 1.0f;
	const vec4 TimerColor = Seconds <= 3 ? Danger : Accent;
	CUIRect Timer = {ScreenWidth * 0.5f - 48.0f, 27.0f, 96.0f, 12.0f};
	DrawPanel(Timer, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 6.0f);
	DrawText(ScreenWidth * 0.5f,
			 29.3f,
			 6.4f,
			 aTimer,
			 vec4(TimerColor.r, TimerColor.g, TimerColor.b, Alpha * WarningPulse),
			 -1.0f,
			 0);

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
		if(m_SelectorMouse.x >= Hit.x && m_SelectorMouse.x <= Hit.x + Hit.w && m_SelectorMouse.y >= Hit.y &&
		   m_SelectorMouse.y <= Hit.y + Hit.h)
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
						66.0f + (1.0f - CardEntry) * 9.0f - FocusAmount * 2.0f -
							(Selected ? ConfirmPulse * 0.8f : 0.0f),
						CardWidth * Scale,
						178.0f * Scale};
		CUIRect Border = Card;
		Border.Margin(-1.4f, &Border);
		const vec4 BorderColor = Selected || Focused ? CategoryColor : AccentDim;
		const float BorderAlpha = Selected ? 0.92f : (Focused ? 0.78f : 0.24f);
		DrawPanel(
			Border,
			vec4(
				BorderColor.r, BorderColor.g, BorderColor.b, min(1.0f, BorderAlpha + ConfirmPulse * 0.08f) * CardAlpha),
			11.0f);
		DrawPanel(Card, vec4(Panel.r, Panel.g, Panel.b, 0.98f * CardAlpha), 9.0f);
		const char *pName =
			Localize(ContractVote ? (pContract ? pContract->m_pName : "Unknown contract") : PveChoiceName(ID));
		const char *pDescription =
			Localize(ContractVote ? (pContract ? pContract->m_pRule : "") : PveChoiceDescription(ID));

		CUIRect Badge = {Card.x + 8.0f, Card.y + 8.0f, Card.w - 16.0f, 14.0f};
		DrawPanel(Badge, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 7.0f);
		char aBadge[64];
		if(ContractVote)
			str_format(aBadge, sizeof(aBadge), Localize("RISK • %d VOTES"), m_aContractVotes[i]);
		else if(pCard)
			str_format(aBadge,
					   sizeof(aBadge),
					   Localize("%d · %s · Lv. %d/%d"),
					   i + 1,
					   Localize(PveRarityName(pCard->m_Rarity)),
					   m_aChoiceStacks[i],
					   pCard->m_MaxStacks);
		else
			str_format(aBadge, sizeof(aBadge), Localize("%d · Supply"), i + 1);
		DrawText(Card.x + Card.w * 0.5f,
				 Card.y + 11.2f,
				 5.6f,
				 aBadge,
				 vec4(CategoryColor.r, CategoryColor.g, CategoryColor.b, CardAlpha),
				 -1.0f,
				 0);
		float NameSize = 8.5f + FocusAmount * 0.7f;
		while(NameSize > 6.0f && TextRender()->TextWidth(0, NameSize, pName, -1) > Card.w - 16.0f)
			NameSize -= 0.3f;
		DrawText(
			Card.x + Card.w * 0.5f, Card.y + 32.0f, NameSize, pName, vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		const CPveUiIcon Icon = ContractVote ? CPveUiIcon() : PveChoiceIcon(ID, pCard);
		DrawIcon(Icon.m_Image,
				 Icon.m_Sprite,
				 Card.x + Card.w * 0.5f,
				 Card.y + 61.0f,
				 (21.0f + FocusAmount * 3.0f) * Icon.m_Scale,
				 vec4(Text.r, Text.g, Text.b, 0.82f * CardAlpha));
		const vec4 DescriptionText = vec4(0.72f, 0.74f, 0.76f, ContractVote ? 0.72f * CardAlpha : 0.64f * CardAlpha);
		DrawWrappedText(
			Card.x + 10.0f, Card.y + 82.0f, 6.2f, pDescription, DescriptionText, Card.w - 20.0f, ContractVote ? 4 : 3);
		if(ContractVote && pContract)
		{
			DrawText(Card.x + 10.0f,
					 Card.y + 121.0f,
					 5.8f,
					 Localize(pContract->m_pRisk),
					 vec4(Danger.r, Danger.g, Danger.b, CardAlpha),
					 Card.w - 20.0f,
					 -1);
			DrawText(Card.x + 10.0f,
					 Card.y + 140.0f,
					 5.8f,
					 Localize("Reward: 1 Research Point"),
					 vec4(Accent.r, Accent.g, Accent.b, CardAlpha),
					 Card.w - 20.0f,
					 -1);
		}
		if(ContractVote)
		{
			CUIRect Button = {Card.x + 10.0f, Card.y + Card.h - 27.0f, Card.w - 20.0f, 18.0f};
			DrawPanel(Button,
					  vec4((Focused ? Accent : Inset).r,
						   (Focused ? Accent : Inset).g,
						   (Focused ? Accent : Inset).b,
						   0.95f * CardAlpha),
					  7.0f);
			DrawText(Button.x + Button.w * 0.5f,
					 Button.y + 5.1f,
					 6.5f,
					 Localize(Selected ? "Voted" : "Vote"),
					 vec4(Text.r, Text.g, Text.b, CardAlpha),
					 -1.0f,
					 0);
		}
		else if(Selected)
			DrawText(Card.x + Card.w * 0.5f,
					 Card.y + Card.h - 23.0f,
					 6.4f + ConfirmPulse * 0.35f,
					 Localize("Selected"),
					 vec4(Accent.r, Accent.g, Accent.b, CardAlpha),
					 -1.0f,
					 0);
	}

	if(ContractVote)
		DrawText(ScreenWidth * 0.5f,
				 274.0f,
				 6.5f,
				 Localize("Mouse • Arrow Keys • 1–3 • Gamepad"),
				 vec4(Text.r, Text.g, Text.b, 0.65f * Alpha),
				 -1.0f,
				 0);
	if(m_ValidationCode && time_get() < m_ValidationUntil)
		DrawText(ScreenWidth * 0.5f,
				 287.0f,
				 6.0f,
				 Localize("The server rejected that selection."),
				 vec4(Danger.r, Danger.g, Danger.b, Alpha),
				 -1.0f,
				 0);

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
	CUIRect Stage = {10.0f, 51.0f + EntryOffset * 0.35f, ScreenWidth - 20.0f, 171.0f};
	DrawPanel(Stage, vec4(Inset.r, Inset.g, Inset.b, 0.97f * Alpha), 13.0f);
	CUIRect TopLine = {Stage.x + 13.0f, Stage.y + 9.0f, (Stage.w - 26.0f) * Entry, 1.2f};
	DrawPanel(TopLine, vec4(Danger.r, Danger.g, Danger.b, 0.68f * Alpha), 0.6f);
	DrawText(ScreenWidth * 0.5f,
			 8.0f,
			 12.5f,
			 Localize("The expedition has reached its limit"),
			 vec4(Text.r, Text.g, Text.b, Alpha),
			 -1.0f,
			 0);
	DrawText(ScreenWidth * 0.5f,
			 26.0f,
			 6.4f,
			 Localize("Five failures. Decide the fate of this run."),
			 vec4(Text.r, Text.g, Text.b, 0.72f * Alpha),
			 -1.0f,
			 0);

	char aFloor[48];
	str_format(aFloor, sizeof(aFloor), Localize("Floor %d"), m_InvasionRetryFloor);
	CUIRect Floor = {Stage.x + 14.0f, Stage.y + 16.0f, 58.0f, 15.0f};
	DrawPanel(Floor, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 7.0f);
	DrawText(
		Floor.x + Floor.w * 0.5f, Floor.y + 4.0f, 6.3f, aFloor, vec4(Accent.r, Accent.g, Accent.b, Alpha), -1.0f, 0);
	const int Seconds = max(
		0, (m_InvasionRetryEndTick - Client()->GameTick() + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
	char aTimer[64];
	str_format(aTimer, sizeof(aTimer), Localize("%d seconds remaining"), Seconds);
	CUIRect Timer = {Stage.x + Stage.w - 92.0f, Stage.y + 16.0f, 78.0f, 15.0f};
	DrawPanel(Timer, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 7.0f);
	const float WarningPulse =
		Seconds <= 3 ? 0.82f + 0.18f * sinf((float)time_get() / (float)time_freq() * 7.0f) : 1.0f;
	const vec4 TimerColor = Seconds <= 3 ? Danger : Accent;
	DrawText(Timer.x + Timer.w * 0.5f,
			 Timer.y + 4.0f,
			 6.0f,
			 aTimer,
			 vec4(TimerColor.r, TimerColor.g, TimerColor.b, Alpha * WarningPulse),
			 -1.0f,
			 0);

	const float Gap = 10.0f;
	const float CardWidth = min(195.0f, (Stage.w - 30.0f - Gap) * 0.5f);
	const float StartX = ScreenWidth * 0.5f - CardWidth - Gap * 0.5f;
	int Hovered = -1;
	for(int i = 0; i < 2; i++)
	{
		const float CardEntry = UiStagger(Alpha, i);
		CUIRect Hit = {StartX + i * (CardWidth + Gap), 85.0f + (1.0f - CardEntry) * 6.0f, CardWidth, 54.0f};
		if(m_SelectorMouse.x >= Hit.x && m_SelectorMouse.x <= Hit.x + Hit.w && m_SelectorMouse.y >= Hit.y &&
		   m_SelectorMouse.y <= Hit.y + Hit.h)
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
							85.0f + (1.0f - CardEntry) * 6.0f - FocusAmount * 1.0f -
								(Selected ? ConfirmPulse * 0.4f : 0.0f),
							CardWidth * Scale,
							54.0f * Scale};
		CUIRect Border = Card;
		Border.Margin(-1.5f, &Border);
		const float BorderAlpha = Selected ? 0.92f : (Focused ? 0.78f : 0.24f);
		DrawPanel(
			Border,
			vec4(
				ChoiceColor.r, ChoiceColor.g, ChoiceColor.b, min(1.0f, BorderAlpha + ConfirmPulse * 0.08f) * CardAlpha),
			11.0f);
		DrawPanel(Card, vec4(Panel.r, Panel.g, Panel.b, 0.98f * CardAlpha), 9.0f);
		char aVotes[64];
		str_format(aVotes, sizeof(aVotes), Localize("%d votes"), m_aInvasionRetryVotes[i]);
		CUIRect VoteBadge = {Card.x + 8.0f, Card.y + 8.0f, 48.0f, 14.0f};
		DrawPanel(VoteBadge, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 7.0f);
		DrawText(VoteBadge.x + VoteBadge.w * 0.5f,
				 VoteBadge.y + 3.8f,
				 5.8f,
				 aVotes,
				 vec4(ChoiceColor.r, ChoiceColor.g, ChoiceColor.b, CardAlpha),
				 -1.0f,
				 0);
		char aKey[8];
		str_format(aKey, sizeof(aKey), "%d", i + 1);
		CUIRect Key = {Card.x + Card.w - 25.0f, Card.y + 8.0f, 16.0f, 14.0f};
		DrawPanel(Key, vec4(Inset.r, Inset.g, Inset.b, 0.96f * CardAlpha), 6.0f);
		DrawText(Key.x + Key.w * 0.5f, Key.y + 3.8f, 5.8f, aKey, vec4(Text.r, Text.g, Text.b, CardAlpha), -1.0f, 0);
		float NameSize = 7.8f + FocusAmount * 0.5f;
		const char *pName = Localize(apNames[i]);
		while(NameSize > 6.2f && TextRender()->TextWidth(0, NameSize, pName, -1) > Card.w - 92.0f)
			NameSize -= 0.3f;
		DrawText(Card.x + 66.0f,
				 Card.y + 10.0f,
				 NameSize,
				 pName,
				 vec4(Text.r, Text.g, Text.b, CardAlpha),
				 Card.w - 94.0f,
				 -1);
		CUIRect Button = {Card.x + Card.w - 54.0f, Card.y + 29.0f, 46.0f, 17.0f};
		const vec4 ButtonColor = Focused || Selected ? ChoiceColor : AccentDim;
		DrawPanel(Button, vec4(ButtonColor.r, ButtonColor.g, ButtonColor.b, 0.94f * CardAlpha), 7.0f);
		DrawText(Button.x + Button.w * 0.5f,
				 Button.y + 5.0f,
			 5.8f + (Selected ? ConfirmPulse * 0.3f : 0.0f),
				 Localize(Selected ? "Voted" : "Vote"),
				 vec4(Text.r, Text.g, Text.b, CardAlpha),
				 -1.0f,
				 0);
	}

	DrawText(ScreenWidth * 0.5f,
			 240.0f,
			 6.0f,
			 Localize("Choose 1 or 2  ·  Arrows / gamepad  ·  Enter / A to vote"),
			 vec4(Text.r, Text.g, Text.b, 0.68f * Alpha),
			 -1.0f,
			 0);

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
	CUIRect Glow = {ScreenWidth * 0.5f - min(230.0f, ScreenWidth - 42.0f) * 0.5f,
					111.0f + (1.0f - Entry) * 6.0f,
					min(230.0f, ScreenWidth - 42.0f),
					76.0f};
	DrawPanel(Glow, vec4(ResultColor.r, ResultColor.g, ResultColor.b, 0.14f * Pulse * Alpha), 18.0f);
	CUIRect Core = Glow;
	Core.Margin(4.0f, &Core);
	DrawPanel(Core, vec4(Panel.r, Panel.g, Panel.b, 0.95f * Alpha), 15.0f);

	char aHeadline[128];
	if(Retry)
		str_format(aHeadline,
				   sizeof(aHeadline),
				   Localize("%s did not succumb."),
				   m_aInvasionRetryPlayerName[0] ? m_aInvasionRetryPlayerName : Localize("The team"));
	else if(m_InvasionRetryResult == PVE_INVASION_RETRY_RESULT_FINAL_FAILURE)
		str_copy(aHeadline, Localize("What a pity..."), sizeof(aHeadline));
	else
		str_copy(aHeadline, Localize("See you next time."), sizeof(aHeadline));
	float HeadlineSize = Retry ? 18.0f : 21.0f;
	while(HeadlineSize > 14.0f && TextRender()->TextWidth(0, HeadlineSize, aHeadline, -1) > Glow.w - 20.0f)
		HeadlineSize -= 0.5f;
	DrawText(ScreenWidth * 0.5f,
			 Glow.y + 20.0f,
			 HeadlineSize,
			 aHeadline,
			 vec4(ResultColor.r, ResultColor.g, ResultColor.b, Alpha),
			 -1.0f,
			 0);
	const char *pSubtitle = Retry ? "The expedition continues." : "Returning all players to Floor 1.";
	DrawText(ScreenWidth * 0.5f,
			 Glow.y + 51.0f,
			 6.8f,
			 Localize(pSubtitle),
			 vec4(Text.r, Text.g, Text.b, 0.72f * Alpha),
			 -1.0f,
			 0);
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
		str_format(aStatus,
				   sizeof(aStatus),
				   Localize("%d seconds • %d/%d"),
				   (m_ContractStatusEndTick - Client()->GameTick()) / Client()->GameTickSpeed(),
				   m_ContractProgress,
				   m_ContractTarget);
	else
		str_format(aStatus, sizeof(aStatus), Localize("Active • %d/%d"), m_ContractProgress, m_ContractTarget);
	DrawText(Hud.x + 8.0f, Hud.y + 20.0f, 5.7f, aStatus, vec4(StateColor.r, StateColor.g, StateColor.b, 0.95f));
	(void)Inset;
}

void CPveRoguelite::DrawBuildHud()
{
	if(Client()->State() != IClient::STATE_ONLINE && !m_DebugBuildPreview)
		return;
	bool HasState = m_Barrier > 0 || m_VulnerableTargets > 0 || m_BleedingTargets > 0 || m_LegendaryCard >= 0 ||
					m_DroneModule != PVE_DRONE_NONE;
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
			str_format(
				aaLines[Lines++], sizeof(aaLines[0]), "%s %d/10", Localize(apResources[i]), m_aWeaponResources[i]);
	if(m_VulnerableTargets > 0 || m_BleedingTargets > 0)
		str_format(aaLines[Lines++],
				   sizeof(aaLines[0]),
				   "%s %d  •  %s %d",
				   Localize("Vulnerable"),
				   m_VulnerableTargets,
				   Localize("Bleed"),
				   m_BleedingTargets);
	if(m_LegendaryCard >= 0 && PveCardDef(m_LegendaryCard))
		str_format(aaLines[Lines++],
				   sizeof(aaLines[0]),
				   "%s: %s",
				   Localize("Legendary"),
				   Localize(PveCardDef(m_LegendaryCard)->m_pName));
	if(m_DroneModule != PVE_DRONE_NONE)
	{
		const char *apModules[4] = {"None", "Assault Module", "Guardian Module", "Repair Module"};
		const int Cooldown = max(0, m_DroneSwitchReadyTick - Client()->GameTick());
		if(Cooldown > 0)
			str_format(aaLines[Lines++],
					   sizeof(aaLines[0]),
					   "%s: %s  %.1fs",
					   Localize("Drone"),
					   Localize(apModules[m_DroneModule]),
					   Cooldown / (float)Client()->GameTickSpeed());
		else
			str_format(
				aaLines[Lines++], sizeof(aaLines[0]), "%s: %s", Localize("Drone"), Localize(apModules[m_DroneModule]));
		if(m_DroneHealth > 0)
			str_format(aaLines[Lines++], sizeof(aaLines[0]), "%s %d/40", Localize("Drone integrity"), m_DroneHealth);
		if((m_DroneState == PVE_DRONE_STATE_DISABLED || m_DroneState == PVE_DRONE_STATE_REBUILDING) &&
		   m_DroneActionTick > Client()->GameTick())
			str_format(aaLines[Lines++],
					   sizeof(aaLines[0]),
					   "%s %.1fs",
					   Localize(m_DroneState == PVE_DRONE_STATE_DISABLED ? "EMP disabled" : "Rebuilding"),
					   (m_DroneActionTick - Client()->GameTick()) / (float)Client()->GameTickSpeed());
		else if(!m_DroneTutorialSeen)
		{
			char aKeyHelp[96];
			const char *pWheelKey = m_pClient->m_pBinds->GetKey("+dronewheel");
			str_format(
				aKeyHelp, sizeof(aKeyHelp), Localize("Hold %s: drone command wheel"), pWheelKey[0] ? pWheelKey : "?");
			str_copy(aaLines[Lines++], aKeyHelp, sizeof(aaLines[0]));
		}
	}
	if(m_ValidationCode == PVE_VALIDATION_MODULE_LOCKED && time_get() < m_ValidationUntil)
		str_copy(aaLines[Lines++], Localize("Drone module not owned"), sizeof(aaLines[0]));
	else if(m_ValidationCode == PVE_VALIDATION_MODULE_COOLDOWN && time_get() < m_ValidationUntil)
		str_copy(aaLines[Lines++], Localize("Drone switch cooling down"), sizeof(aaLines[0]));
	const int Columns = max(1, min(Lines, HudLayout::BuildEffectsColumns));
	float MaxTextWidth = 0.0f;
	for(int i = 0; i < Lines; i++)
		MaxTextWidth = max(MaxTextWidth, TextRender()->TextWidth(0, 5.8f, aaLines[i], -1));
	const float MaxWidth = min(HudLayout::BuildEffectsWidth, ScreenWidth - HudLayout::BuildEffectsLeft * 2.0f);
	const float PreferredWidth = 16.0f + Columns * (MaxTextWidth + 6.0f);
	const float Width = clamp(PreferredWidth, min(78.0f, MaxWidth), MaxWidth);
	const int Rows = (Lines + Columns - 1) / Columns;
	CUIRect Hud = {HudLayout::BuildEffectsLeft,
		HudLayout::BuildEffectsTop,
		Width,
		12.0f + Rows * HudLayout::BuildEffectsRowHeight};
	DrawPanel(Hud, vec4(Panel.r, Panel.g, Panel.b, 0.90f), 8.0f);
	CUIRect Edge = {Hud.x, Hud.y, 2.0f, Hud.h};
	DrawPanel(Edge, Accent, 1.0f);
	const float ColumnWidth = (Hud.w - 16.0f) / Columns;
	for(int i = 0; i < Lines; i++)
	{
		const int Column = i % Columns;
		const int Row = i / Columns;
		DrawText(Hud.x + 8.0f + Column * ColumnWidth,
			Hud.y + 6.0f + Row * HudLayout::BuildEffectsRowHeight,
			5.8f,
			aaLines[i],
			Text,
			ColumnWidth - 6.0f,
			-1);
	}
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
		const bool Disabled =
			pDrone->m_State == PVE_DRONE_STATE_DISABLED || pDrone->m_State == PVE_DRONE_STATE_REBUILDING;
		const vec4 Color = Disabled ? vec4(0.35f, 0.45f, 0.55f, 0.65f)
									: (pDrone->m_Module == PVE_DRONE_ASSAULT
										   ? vec4(1.0f, 0.35f, 0.25f, 1.0f)
										   : (pDrone->m_Module == PVE_DRONE_GUARDIAN ? vec4(0.25f, 0.65f, 1.0f, 1.0f)
																					 : vec4(0.3f, 1.0f, 0.55f, 1.0f)));
		const float Intra = Client()->IntraGameTick();
		const vec2 DronePos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pDrone->m_X, pDrone->m_Y), Intra);
		const vec2 TargetPos =
			mix(vec2(pPrev->m_TargetX, pPrev->m_TargetY), vec2(pDrone->m_TargetX, pDrone->m_TargetY), Intra);
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
				DrawIcon(IMAGE_WEAPONS,
						 SPRITE_PICKUP_ARMOR,
						 DronePos.x + cosf(Angle) * 280.0f,
						 DronePos.y + sinf(Angle) * 280.0f,
						 9.0f,
						 vec4(0.25f, 0.65f, 1.0f, 0.22f));
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
			pAnim = pDrone->m_Module == PVE_DRONE_GUARDIAN
						? "shield"
						: (pDrone->m_Module == PVE_DRONE_REPAIR ? "repair" : "attack");

		const float Time = (Client()->PrevGameTick() + Client()->IntraGameTick()) / (float)Client()->GameTickSpeed() +
						   Item.m_ID * 0.11f;
		if(pDrone->m_State == PVE_DRONE_STATE_DISABLED)
			Graphics()->ShaderBegin(SHADER_ELECTRIC, 0.85f);
		const int DroneAtlas = pDrone->m_Module == PVE_DRONE_GUARDIAN
								   ? ATLAS_LOST_PROTOCOL_PVE_DRONE_GUARDIAN
								   : (pDrone->m_Module == PVE_DRONE_REPAIR ? ATLAS_LOST_PROTOCOL_PVE_DRONE_REPAIR
																		   : ATLAS_LOST_PROTOCOL_PVE_DRONE_ASSAULT);

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
			while(SmoothDelta > 180.0f)
				SmoothDelta -= 360.0f;
			while(SmoothDelta < -180.0f)
				SmoothDelta += 360.0f;
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
		vec4(1.0f, 0.35f, 0.25f, 1.0f), vec4(0.25f, 0.65f, 1.0f, 1.0f), vec4(0.3f, 1.0f, 0.55f, 1.0f)};
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
		Graphics()->SetColor(min(1.0f, Base.r * Bright), min(1.0f, Base.g * Bright), min(1.0f, Base.b * Bright), Alpha);
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
		DrawText(X,
				 Y,
				 Selected ? 18.0f : 14.0f,
				 Localize(apAllNames[aUnlocked[i]]),
				 Selected ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(0.85f, 0.9f, 0.95f, 0.9f),
				 -1.0f,
				 0);
	}

	DrawText(Cx,
			 Cy - 10.0f,
			 14.0f,
			 Localize(Client()->GameTick() < m_DroneSwitchReadyTick
						  ? "Drone switch cooling down"
						  : (m_DroneWheelSelected >= 0 ? "Release to switch" : "Aim to select")),
			 vec4(1.0f, 1.0f, 1.0f, 0.85f),
			 -1.0f,
			 0);

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
	const int MaxCheckpoint =
		g_Config.m_ClPveHighestInvasion >= 10 ? (g_Config.m_ClPveHighestInvasion / 10) * 10 + 1 : 1;
	g_Config.m_ClPvePreferredCheckpoint += 10;
	if(g_Config.m_ClPvePreferredCheckpoint > MaxCheckpoint)
		g_Config.m_ClPvePreferredCheckpoint = 1;
	SaveProgress();
	if(Client()->State() == IClient::STATE_ONLINE && m_ProgressSent)
		SyncProgress();
}

void CPveRoguelite::RenderResearch(CUIRect MainView)
{
	if(MainView.w < 720.0f || MainView.h < 430.0f)
	{
		RenderResearchLegacy(MainView);
		return;
	}
	RenderResearchCommandCenter(MainView);
}

void CPveRoguelite::RenderResearchCommandCenter(CUIRect MainView)
{
	m_ResearchVisible = true;
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 4.0f);
	m_ResearchAppearAmount += (1.0f - m_ResearchAppearAmount) * (1.0f - expf(-10.0f * Dt));
	const float Alpha = clamp(m_ResearchAppearAmount, 0.0f, 1.0f);
	const float Time = (float)time_get() / (float)time_freq();
	const float Wave = 0.5f + 0.5f * sinf(Time * 2.2f);
	const float WaveFast = 0.5f + 0.5f * sinf(Time * 4.0f);
	const float Scale = clamp(min(MainView.w / 1180.0f, MainView.h / 680.0f), 0.84f, 1.10f);
	const float LayoutScale = Scale / max(0.01f, UI()->Scale());
	const bool Compact = MainView.w < 1120.0f || MainView.h < 660.0f || UI()->Scale() > 1.15f;
	const float Font = Compact ? 1.14f : 1.20f;
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	const vec4 PurchasedColor = AccentDim;
	const vec4 AvailableColor = CMenus::ThemeResearchAvailable();
	const vec4 LockedColor = CMenus::ThemeResearchLocked();
	const vec4 MutedText = CMenus::ThemeTextMuted();
	auto Fade = [&](const vec4 &Color, float Opacity)
	{
		return vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha);
	};
	auto Mix = [&](const vec4 &A, const vec4 &B, float Amount)
	{
		return vec4(A.r + (B.r - A.r) * Amount,
			A.g + (B.g - A.g) * Amount,
			A.b + (B.b - A.b) * Amount,
			A.a + (B.a - A.a) * Amount);
	};
	auto ResearchText = [&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth = -1.0f, int Align = -1)
	{
		DrawText(X, Y, Size * Font, pText, Color, MaxWidth, Align);
	};
	auto ResearchWrapped = [&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines)
	{
		DrawWrappedText(X, Y, Size * Font, pText, Color, MaxWidth, MaxLines);
	};
	auto DrawHint = [&](const CUIRect &Rect, const char *pText, const vec4 &Color)
	{
		DrawPanel(Rect, Fade(Inset, 0.48f + 0.08f * WaveFast), 8.0f * Scale);
		CUIRect Edge = Rect;
		Edge.h = 1.0f * Scale;
		DrawPanel(Edge, Fade(Color, 0.54f + 0.16f * WaveFast), 0.5f * Scale);
		ResearchText(Rect.x + 8.0f * Scale,
			Rect.y + 4.0f * Scale,
			6.7f,
			pText,
			Fade(Color, 0.90f),
			Rect.w - 16.0f * Scale);
	};
	auto DrawSprite = [&](const CPveUiIcon &Icon, float X, float Y, float Size, const vec4 &Color, float Opacity)
	{
		DrawIcon(Icon.m_Image,
			Icon.m_Sprite,
			X,
			Y,
			Size * Icon.m_Scale,
			vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha));
	};
	const char *apTabs[3] = {Localize("Core"), Localize("Weapons"), Localize("Modes")};
	const char *apBranchNames[4];
	if(m_ResearchTab == PVE_TAB_CORE)
	{
		const char *apNames[4] = {"Attack", "Survival", "Logistics", "Drone"};
		for(int Branch = 0; Branch < 4; Branch++)
			apBranchNames[Branch] = apNames[Branch];
	}
	else if(m_ResearchTab == PVE_TAB_WEAPON)
	{
		const char *apNames[4] = {"Firearms", "Explosives", "Electric", "Melee"};
		for(int Branch = 0; Branch < 4; Branch++)
			apBranchNames[Branch] = apNames[Branch];
	}
	else
	{
		const char *apNames[3] = {"Invasion", "Horde", "Extraction"};
		for(int Branch = 0; Branch < 3; Branch++)
			apBranchNames[Branch] = apNames[Branch];
	}

	const CPveResearchMask Mask = TutorialResearchActive() ? CPveResearchMask() : ParseResearchMask();
	int BranchCount = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
			BranchCount = max(BranchCount, PveCardDef(ID)->m_Branch + 1);
	BranchCount = max(1, BranchCount);
	m_ResearchBranch = clamp(m_ResearchBranch, 0, BranchCount - 1);
	int aBranchCount[4] = {};
	int aBranchBought[4] = {};
	int aBranchPreview[4];
	for(int Branch = 0; Branch < 4; Branch++)
		aBranchPreview[Branch] = -1;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		const CPveCardDef *pDef = PveCardDef(ID);
		if(pDef->m_Base || pDef->m_Tab != m_ResearchTab)
			continue;
		const int Branch = clamp(pDef->m_Branch, 0, 3);
		aBranchCount[Branch]++;
		if(PveCardIsUnlocked(ID, Mask))
			aBranchBought[Branch]++;
		const int Rank = CanBuyResearch(ID, Mask) ? 0 : (PveCardIsUnlocked(ID, Mask) ? 2 : 1);
		if(aBranchPreview[Branch] < 0)
			aBranchPreview[Branch] = ID;
		else
		{
			const CPveCardDef *pCurrent = PveCardDef(aBranchPreview[Branch]);
			const int CurrentRank = CanBuyResearch(aBranchPreview[Branch], Mask)
				? 0
				: (PveCardIsUnlocked(aBranchPreview[Branch], Mask) ? 2 : 1);
			if(Rank < CurrentRank || (Rank == CurrentRank && pDef->m_Tier < pCurrent->m_Tier))
				aBranchPreview[Branch] = ID;
		}
	}

	auto SelectResearchRoute = [&](int Branch, int Route)
	{
		m_ResearchBranch = clamp(Branch, 0, BranchCount - 1);
		m_ResearchRoute = Route;
		int BestID = -1;
		int BestRank = 3;
		int BestTier = 999;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			const CPveCardDef *pDef = PveCardDef(ID);
			if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != Branch ||
				PveResearchRoute(pDef) != Route)
				continue;
			const int Rank = CanBuyResearch(ID, Mask) ? 0 : (PveCardIsUnlocked(ID, Mask) ? 2 : 1);
			if(BestID < 0 || Rank < BestRank || (Rank == BestRank && pDef->m_Tier < BestTier))
			{
				BestID = ID;
				BestRank = Rank;
				BestTier = pDef->m_Tier;
			}
		}
		if(BestID >= 0)
			m_SelectedResearch = BestID;
		m_SelectionPulse = 0.65f;
	};
	if(m_SelectedResearch < 0 || !PveCardDef(m_SelectedResearch) || PveCardDef(m_SelectedResearch)->m_Tab != m_ResearchTab)
		SelectResearchRoute(m_ResearchBranch, 0);

	MainView.y += (1.0f - Alpha) * 10.0f * Scale;
	DrawPanel(MainView, Fade(Deep, 0.22f), 0.0f);
	CUIRect Canvas = MainView;
	Canvas.Margin(16.0f * LayoutScale, &Canvas);
	CUIRect Header, Body;
	Canvas.HSplitTop(72.0f * LayoutScale, &Header, &Body);
	DrawPanel(Header, Fade(Panel, 0.48f), 12.0f * Scale);
	CUIRect HeaderEdge = {Header.x + 16.0f * Scale,
		Header.y + Header.h - 2.0f * Scale,
		Header.w - 32.0f * Scale,
		2.0f * Scale};
	DrawPanel(HeaderEdge, Fade(Accent, 0.58f + 0.25f * Wave), 1.0f * Scale);

	CUIRect Back = {Header.x + 10.0f * Scale,
		Header.y + 16.0f * Scale,
		120.0f * Scale,
		36.0f * Scale};
	const float BackHover = m_pClient->m_pMenus->AnimHover(&m_ResearchBackButtonID);
	DrawPanel(Back, Fade(BackHover > 0.2f ? AccentDim : Inset, 0.74f + BackHover * 0.18f), 12.0f * Scale);
	CUIRect BackEdge = Back;
	BackEdge.w = 2.0f * Scale;
	DrawPanel(BackEdge, Fade(BackHover > 0.2f ? Accent : AccentDim, 0.86f), 1.0f * Scale);
	ResearchText(Back.x + 60.0f * Scale,
		Back.y + 8.0f * Scale,
		8.8f,
		Localize("Back"),
		Fade(BackHover > 0.2f ? Text : AccentDim, 1.0f),
		-1.0f,
		0);
	if(UI()->DoButtonLogic(&m_ResearchBackButtonID, &Back))
		m_pClient->m_pMenus->CloseResearchPage();

	ResearchText(Header.x + 154.0f * Scale, Header.y + 9.0f * Scale, 18.0f, Localize("Research"), Fade(Text, 1.0f));
	ResearchText(Header.x + 154.0f * Scale,
		Header.y + 34.0f * Scale,
		8.6f,
		Localize("NEURAL CONSTELLATION"),
		Fade(Accent, 0.92f));
	ResearchText(Header.x + 154.0f * Scale,
		Header.y + 49.0f * Scale,
		7.8f,
		apTabs[m_ResearchTab],
		Fade(Text, 0.78f));

	char aPoints[64];
	str_format(aPoints,
		sizeof(aPoints),
		Localize("%d Research Points"),
		TutorialResearchActive() ? 99 : g_Config.m_ClPveResearchPoints);
	const float PointsW = clamp(188.0f * Scale, 166.0f * Scale, 224.0f * Scale);
	CUIRect Points = {Header.x + Header.w - PointsW - 12.0f * Scale,
		Header.y + 16.0f * Scale,
		PointsW,
		36.0f * Scale};
	DrawPanel(Points, Fade(AccentDim, 0.34f + 0.14f * Wave), 14.0f * Scale);
	CUIRect PointsInner = Points;
	PointsInner.Margin(1.2f * LayoutScale, &PointsInner);
	DrawPanel(PointsInner, Fade(Inset, 0.94f), 13.0f * Scale);
	DrawSprite(CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_BIGCOIN),
		Points.x + 17.0f * Scale,
		Points.y + Points.h * 0.5f,
		16.0f * Scale * (1.0f + 0.04f * WaveFast),
		Accent,
		0.92f);
	ResearchText(Points.x + Points.w * 0.58f,
		Points.y + 8.0f * Scale,
		9.4f,
		aPoints,
		Fade(Accent, 1.0f),
		-1.0f,
		0);
	if(m_ResearchTab == PVE_TAB_MODE)
	{
		CUIRect Checkpoint = {Points.x - 132.0f * Scale,
			Points.y + 3.0f * Scale,
			122.0f * Scale,
			30.0f * Scale};
		DrawPanel(Checkpoint, Fade(Inset, 0.86f), 12.0f * Scale);
		char aCheckpoint[64];
		str_format(aCheckpoint, sizeof(aCheckpoint), Localize("Checkpoint %d"), g_Config.m_ClPvePreferredCheckpoint);
		ResearchText(Checkpoint.x + Checkpoint.w * 0.5f,
			Checkpoint.y + 7.0f * Scale,
			8.3f,
			aCheckpoint,
			Fade(Accent, 1.0f),
			-1.0f,
			0);
		if(UI()->DoButtonLogic(&m_CheckpointButtonID, &Checkpoint))
			CycleCheckpoint();
	}

	Body.HSplitTop(10.0f * LayoutScale, 0, &Body);
	Body.HSplitBottom(42.0f * LayoutScale, &Body, &Canvas);
	CUIRect Footer = Canvas;
	DrawPanel(Footer, Fade(Panel, 0.38f), 10.0f * Scale);
	ResearchText(Footer.x + 16.0f * Scale,
		Footer.y + 9.0f * Scale,
		8.2f,
		Localize("HOVER OR CLICK FOR DETAILS"),
		Fade(MutedText, 0.86f));
	const char *apLegend[] = {"PURCHASED", "AVAILABLE", "LOCKED"};
	const vec4 aLegendColors[3] = {PurchasedColor, AvailableColor, LockedColor};
	float LegendX = Footer.x + Footer.w - 254.0f * Scale;
	for(int i = 0; i < 3; i++)
	{
		CUIRect Dot = {LegendX, Footer.y + 14.0f * Scale, 6.0f * Scale, 6.0f * Scale};
		DrawPanel(Dot, Fade(aLegendColors[i], 0.92f), 3.0f * Scale);
		ResearchText(LegendX + 12.0f * Scale,
			Footer.y + 8.0f * Scale,
			7.0f,
			Localize(apLegend[i]),
			Fade(aLegendColors[i], 0.88f));
		LegendX += 78.0f * Scale;
	}

	CUIRect Workspace = Body;
	const float SidebarW = clamp((Compact ? 178.0f : 202.0f) * Scale, 166.0f * Scale, 218.0f * Scale);
	const float DetailsW = clamp((Compact ? 286.0f : 318.0f) * Scale, 266.0f * Scale, 338.0f * Scale);
	CUIRect Sidebar, Center, Details;
	Workspace.VSplitLeft(SidebarW / max(0.01f, UI()->Scale()), &Sidebar, &Center);
	Center.VSplitRight(DetailsW / max(0.01f, UI()->Scale()), &Center, &Details);
	Sidebar.Margin(4.0f * LayoutScale, &Sidebar);
	Center.Margin(4.0f * LayoutScale, &Center);
	Details.Margin(4.0f * LayoutScale, &Details);

	DrawPanel(Sidebar, Fade(Panel, 0.60f), 12.0f * Scale);
	CUIRect SidebarInner = Sidebar;
	SidebarInner.Margin(10.0f * LayoutScale, &SidebarInner);
	CUIRect SidebarTitle, SidebarBody;
	SidebarInner.HSplitTop(30.0f * LayoutScale, &SidebarTitle, &SidebarBody);
	ResearchText(SidebarTitle.x + 2.0f * Scale,
		SidebarTitle.y + 3.0f * Scale,
		10.5f,
		Localize("Research"),
		Fade(Text, 0.96f));
	for(int Tab = 0; Tab < 3; Tab++)
	{
		CUIRect TabRect;
		SidebarBody.HSplitTop(38.0f * LayoutScale, &TabRect, &SidebarBody);
		const bool Selected = Tab == m_ResearchTab;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aTabButtonIDs[Tab]);
		const float Selection = m_pClient->m_pMenus->AnimSelected(&m_aTabButtonIDs[Tab], Selected, 14.0f);
		DrawPanel(TabRect,
			Fade(Mix(Inset, AccentDim, Selection * 0.72f + Hover * 0.10f), 0.54f + Selection * 0.18f + Hover * 0.10f),
			11.0f * Scale);
		CUIRect TabEdge = TabRect;
		TabEdge.w = 2.0f * Scale;
		DrawPanel(TabEdge, Fade(Mix(AccentDim, Accent, max(Selection, Hover * 0.72f)), 0.46f + Selection * 0.46f), 1.0f * Scale);
		ResearchText(TabRect.x + 14.0f * Scale,
			TabRect.y + 9.0f * Scale,
			9.6f,
			apTabs[Tab],
			Fade(Mix(AccentDim, Text, Selection), 1.0f));
		if(UI()->DoButtonLogic(&m_aTabButtonIDs[Tab], &TabRect))
		{
			m_ResearchTab = Tab;
			m_ResearchBranch = 0;
			m_ResearchRoute = 0;
			for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == Tab)
				{
					m_SelectedResearch = ID;
					break;
				}
		}
	}
	CUIRect RailDivider = SidebarBody;
	RailDivider.h = 1.0f * Scale;
	DrawPanel(RailDivider, Fade(AccentDim, 0.40f), 0.5f * Scale);
	SidebarBody.y += 8.0f * Scale;
	SidebarBody.h -= 8.0f * Scale;
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		CUIRect BranchRect;
		SidebarBody.HSplitTop((Compact ? 52.0f : 58.0f) * LayoutScale, &BranchRect, &SidebarBody);
		const bool Selected = Branch == m_ResearchBranch;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aBranchButtonIDs[Branch]);
		const float Selection = m_pClient->m_pMenus->AnimSelected(&m_aBranchButtonIDs[Branch], Selected, 12.0f);
		DrawPanel(BranchRect,
			Fade(Mix(Deep, Inset, Selection * 0.78f + Hover * 0.08f), 0.54f + Selection * 0.26f + Hover * 0.10f),
			10.0f * Scale);
		CUIRect BranchEdge = BranchRect;
		BranchEdge.w = 2.0f * Scale;
		DrawPanel(BranchEdge,
			Fade(Mix(AccentDim, Accent, max(Selection, Hover * 0.72f)), 0.42f + Selection * 0.52f + Hover * 0.14f),
			1.0f * Scale);
		DrawSprite(PveBranchIcon(m_ResearchTab, Branch),
			BranchRect.x + 19.0f * Scale,
			BranchRect.y + 19.0f * Scale,
			16.0f * Scale,
			Mix(AccentDim, Accent, Selection),
			0.75f + Selection * 0.25f + Hover * 0.16f);
		ResearchText(BranchRect.x + 35.0f * Scale,
			BranchRect.y + 8.0f * Scale,
			9.3f,
			Localize(apBranchNames[Branch]),
			Fade(Mix(AccentDim, Text, Selection), 1.0f),
			BranchRect.w - 86.0f * Scale);
		char aProgress[24];
		str_format(aProgress, sizeof(aProgress), "%d / %d", aBranchBought[Branch], aBranchCount[Branch]);
		ResearchText(BranchRect.x + BranchRect.w - 10.0f * Scale,
			BranchRect.y + 10.0f * Scale,
			7.5f,
			aProgress,
			Fade(Mix(AccentDim, Accent, Selection), 0.92f),
			-1.0f,
			1);
		if(aBranchPreview[Branch] >= 0)
		{
			const CPveCardDef *pPreview = PveCardDef(aBranchPreview[Branch]);
			ResearchText(BranchRect.x + 35.0f * Scale,
				BranchRect.y + 31.0f * Scale,
				7.3f,
				Localize(pPreview->m_pName),
				Fade(PveCardIsUnlocked(aBranchPreview[Branch], Mask) ? PurchasedColor :
					(CanBuyResearch(aBranchPreview[Branch], Mask) ? AvailableColor : MutedText),
					0.90f),
				BranchRect.w - 45.0f * Scale);
		}
		ResearchText(BranchRect.x + BranchRect.w - 9.0f * Scale,
			BranchRect.y + BranchRect.h - 10.0f * Scale,
			7.2f,
			">",
			Fade(Mix(AccentDim, Accent, max(Selection, Hover * 0.72f)), 0.72f + Hover * 0.20f),
			-1.0f,
			1);
		if(UI()->DoButtonLogic(&m_aBranchButtonIDs[Branch], &BranchRect))
			SelectResearchRoute(Branch, 0);
	}

	CUIRect ModuleBay, RouteDeck;
	Center.HSplitTop((Compact ? 198.0f : 210.0f) * LayoutScale, &ModuleBay, &RouteDeck);
	DrawPanel(ModuleBay, Fade(Panel, 0.58f), 12.0f * Scale);
	CUIRect ModuleHeader, ModuleCanvas;
	ModuleBay.HSplitTop(30.0f * LayoutScale, &ModuleHeader, &ModuleCanvas);
	ResearchText(ModuleHeader.x + 12.0f * Scale,
		ModuleHeader.y + 5.0f * Scale,
		10.2f,
		Localize("NEURAL CONSTELLATION"),
		Fade(Text, 0.96f));
	const float BranchHintW = min(112.0f * Scale, ModuleHeader.w * 0.36f);
	CUIRect BranchHint = {ModuleHeader.x + ModuleHeader.w - BranchHintW - 12.0f * Scale,
		ModuleHeader.y + 4.0f * Scale,
		BranchHintW,
		20.0f * Scale};
	DrawHint(BranchHint, Localize("SELECT A BRANCH"), Accent);
	ModuleCanvas.Margin(8.0f * LayoutScale, &ModuleCanvas);
	const float ScanY = ModuleCanvas.y + fmodf(Time * 28.0f, max(1.0f, ModuleCanvas.h));
	CUIRect ScanLine = {ModuleCanvas.x, ScanY, ModuleCanvas.w, 1.0f * Scale};
	UI()->ClipEnable(&ModuleCanvas);
	DrawPanel(ScanLine, Fade(Accent, 0.05f + 0.03f * WaveFast), 0.5f * Scale);
	UI()->ClipDisable();
	const float CoreX = ModuleCanvas.x + ModuleCanvas.w * 0.5f;
	const float CoreY = ModuleCanvas.y + ModuleCanvas.h * 0.5f;
	const float CoreSize = min(64.0f * Scale, ModuleCanvas.h * 0.42f);
	IGraphics::CLineItem aModuleLinks[4];
	int ModuleLinkCount = 0;
	CUIRect aModuleRects[4];
	const float ModuleW = min(184.0f * Scale, ModuleCanvas.w * 0.31f);
	const float ModuleH = min(48.0f * Scale, ModuleCanvas.h * 0.30f);
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		float X = ModuleCanvas.x + 8.0f * Scale;
		float Y = ModuleCanvas.y + ModuleCanvas.h * 0.5f - ModuleH * 0.5f;
		if(Branch == 0)
		{
			X = CoreX - ModuleW * 0.5f;
			Y = ModuleCanvas.y;
		}
		else if(Branch == 1)
		{
			X = ModuleCanvas.x + ModuleCanvas.w - ModuleW;
		}
		else if(Branch == 2)
		{
			X = ModuleCanvas.x;
		}
		else
		{
			X = CoreX - ModuleW * 0.5f;
			Y = ModuleCanvas.y + ModuleCanvas.h - ModuleH;
		}
		aModuleRects[Branch] = {X, Y, ModuleW, ModuleH};
		const CUIRect &Module = aModuleRects[Branch];
		const float ModuleCx = Module.x + Module.w * 0.5f;
		const float ModuleCy = Module.y + Module.h * 0.5f;
		aModuleLinks[ModuleLinkCount++] = IGraphics::CLineItem(CoreX, CoreY, ModuleCx, ModuleCy);
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	for(int Branch = 0; Branch < ModuleLinkCount; Branch++)
	{
		const float Selection = m_pClient->m_pMenus->AnimSelected(
			&m_aModuleButtonIDs[Branch], Branch == m_ResearchBranch, 12.0f);
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aModuleButtonIDs[Branch]);
		const vec4 LinkColor = Mix(AccentDim, Accent, max(Selection, Hover * 0.72f));
		Graphics()->SetColor(LinkColor.r,
			LinkColor.g,
			LinkColor.b,
			(0.20f + Selection * 0.42f + Hover * 0.10f) * Alpha);
		Graphics()->LinesDraw(&aModuleLinks[Branch], 1);
	}
	Graphics()->LinesEnd();

	const float CoreGlow = CoreSize * 1.42f;
	CUIRect CoreGlowRect = {CoreX - CoreGlow * 0.5f, CoreY - CoreGlow * 0.5f, CoreGlow, CoreGlow};
	DrawPanel(CoreGlowRect, Fade(Accent, 0.10f + 0.08f * Wave), CoreGlow * 0.5f);
	CUIRect CoreRect = {CoreX - CoreSize * 0.5f, CoreY - CoreSize * 0.5f, CoreSize, CoreSize};
	DrawPanel(CoreRect, Fade(Accent, 0.78f + 0.12f * WaveFast), CoreSize * 0.5f);
	CUIRect CoreInner = CoreRect;
	CoreInner.Margin(3.0f * LayoutScale, &CoreInner);
	DrawPanel(CoreInner, Fade(Deep, 0.96f), CoreInner.w * 0.5f);
	ResearchText(CoreX, CoreY - 8.0f * Scale, 9.4f, apTabs[m_ResearchTab], Fade(Text, 0.98f), -1.0f, 0);
	char aCoreProgress[32];
	int TabBought = 0;
	int TabCards = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
		{
			TabCards++;
			if(PveCardIsUnlocked(ID, Mask))
				TabBought++;
		}
	str_format(aCoreProgress, sizeof(aCoreProgress), "%d / %d", TabBought, TabCards);
	ResearchText(CoreX, CoreY + 9.0f * Scale, 7.6f, aCoreProgress, Fade(Accent, 0.94f), -1.0f, 0);

	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		const CUIRect &Module = aModuleRects[Branch];
		const bool Selected = Branch == m_ResearchBranch;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aModuleButtonIDs[Branch]);
		const float Selection = m_pClient->m_pMenus->AnimSelected(&m_aModuleButtonIDs[Branch], Selected, 12.0f);
		const int PreviewID = aBranchPreview[Branch];
		const bool Available = PreviewID >= 0 && CanBuyResearch(PreviewID, Mask);
		const bool Bought = PreviewID >= 0 && PveCardIsUnlocked(PreviewID, Mask);
		const vec4 StateColor = Bought ? PurchasedColor : (Available ? AvailableColor : LockedColor);
		const vec4 FocusColor = Mix(StateColor, Accent, Selection * 0.72f + Hover * 0.18f);
		CUIRect Glow = Module;
		Glow.Margin(-2.0f * Scale, &Glow);
		DrawPanel(Glow, Fade(FocusColor, 0.08f + Selection * 0.16f + Hover * 0.08f), 13.0f * Scale);
		DrawPanel(Module, Fade(Mix(Inset, Panel, Selection * 0.76f), 0.84f + Selection * 0.08f), 12.0f * Scale);
		CUIRect Edge = Module;
		Edge.w = 2.0f * Scale;
		DrawPanel(Edge, Fade(FocusColor, 0.60f + Selection * 0.36f + Hover * 0.18f), 1.0f * Scale);
		DrawSprite(PveBranchIcon(m_ResearchTab, Branch),
			Module.x + 20.0f * Scale,
			Module.y + Module.h * 0.5f,
			20.0f * Scale,
			FocusColor,
			0.86f + 0.14f * max(Selection, Hover));
		ResearchText(Module.x + 38.0f * Scale,
			Module.y + 8.0f * Scale,
			9.7f,
			Localize(apBranchNames[Branch]),
			Fade(Mix(AccentDim, Text, Selection), 1.0f),
			Module.w - 86.0f * Scale);
		char aModuleProgress[24];
		str_format(aModuleProgress, sizeof(aModuleProgress), "%d / %d", aBranchBought[Branch], aBranchCount[Branch]);
		ResearchText(Module.x + Module.w - 10.0f * Scale,
			Module.y + 9.0f * Scale,
			7.3f,
			aModuleProgress,
			Fade(StateColor, 0.96f),
			-1.0f,
			1);
		if(PreviewID >= 0)
		{
			ResearchText(Module.x + 38.0f * Scale,
				Module.y + 31.0f * Scale,
				7.7f,
				Localize(PveCardDef(PreviewID)->m_pName),
				Fade(Mix(StateColor, Accent, Selection * 0.36f), 0.92f),
				Module.w - 48.0f * Scale);
		}
		ResearchText(Module.x + Module.w - 9.0f * Scale,
			Module.y + Module.h - 10.0f * Scale,
			7.2f,
			">",
			Fade(FocusColor, 0.72f + Hover * 0.20f),
			-1.0f,
			1);
		CUIRect ModuleHit = Module;
		if(UI()->DoButtonLogic(&m_aModuleButtonIDs[Branch], &ModuleHit))
			SelectResearchRoute(Branch, 0);
	}

	RouteDeck.Margin(6.0f * LayoutScale, &RouteDeck);
	DrawPanel(RouteDeck, Fade(Panel, 0.56f), 12.0f * Scale);
	CUIRect DeckHeader, DeckColumns;
	RouteDeck.HSplitTop(30.0f * LayoutScale, &DeckHeader, &DeckColumns);
	ResearchText(DeckHeader.x + 12.0f * Scale,
		DeckHeader.y + 5.0f * Scale,
		10.0f,
		Localize(apBranchNames[m_ResearchBranch]),
		Fade(Text, 0.96f));
	const float PerkHintW = min(98.0f * Scale, DeckHeader.w * 0.32f);
	CUIRect PerkHint = {DeckHeader.x + DeckHeader.w - PerkHintW - 12.0f * Scale,
		DeckHeader.y + 4.0f * Scale,
		PerkHintW,
		20.0f * Scale};
	DrawHint(PerkHint, Localize("SELECT A PERK"), AvailableColor);
	ResearchText(DeckHeader.x + 12.0f * Scale,
		DeckHeader.y + 18.0f * Scale,
		7.4f,
		Localize("Research unlocks perk cards; select them during a run to activate their effects."),
		Fade(MutedText, 0.70f),
		DeckHeader.w - PerkHintW - 36.0f * Scale);
	const int RouteCount = PveResearchRouteCount(m_ResearchTab, m_ResearchBranch);
	int aRouteCards[3][16] = {};
	int aRouteCardCount[3] = {};
	for(int Tier = 0; Tier < 16; Tier++)
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			const CPveCardDef *pDef = PveCardDef(ID);
			if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != m_ResearchBranch || pDef->m_Tier != Tier)
				continue;
			const int Route = clamp(PveResearchRoute(pDef), 0, RouteCount - 1);
			if(aRouteCardCount[Route] < 16)
				aRouteCards[Route][aRouteCardCount[Route]++] = ID;
		}
	int MaxRouteCards = 1;
	for(int Route = 0; Route < RouteCount; Route++)
		MaxRouteCards = max(MaxRouteCards, aRouteCardCount[Route]);
	const float ColumnGap = 7.0f * Scale;
	const float ColumnW = (DeckColumns.w - ColumnGap * (RouteCount - 1)) / (float)RouteCount;
	const float RouteHeaderH = 24.0f * Scale;
	const float CardGap = 3.0f * Scale;
	const float CardH = clamp((DeckColumns.h - RouteHeaderH - 8.0f * Scale) / MaxRouteCards - CardGap,
		25.0f * Scale,
		44.0f * Scale);
	for(int Route = 0; Route < RouteCount; Route++)
	{
		CUIRect Column = {DeckColumns.x + Route * (ColumnW + ColumnGap), DeckColumns.y, ColumnW, DeckColumns.h};
		const bool RouteSelected = Route == m_ResearchRoute;
		const float RouteHover = m_pClient->m_pMenus->AnimHover(&m_aRouteButtonIDs[m_ResearchBranch][Route]);
		const float RouteSelection = m_pClient->m_pMenus->AnimSelected(
			&m_aRouteButtonIDs[m_ResearchBranch][Route], RouteSelected, 12.0f);
		DrawPanel(Column, Fade(Deep, 0.38f), 9.0f * Scale);
		CUIRect ColumnGlow = Column;
		ColumnGlow.Margin(-1.0f * Scale, &ColumnGlow);
		DrawPanel(ColumnGlow,
			Fade(Mix(AccentDim, Accent, RouteSelection * 0.72f + RouteHover * 0.18f),
				0.018f + RouteSelection * 0.045f + RouteHover * 0.018f),
			9.0f * Scale);
		CUIRect ColumnEdge = Column;
		ColumnEdge.w = 1.0f * Scale;
		DrawPanel(ColumnEdge,
			Fade(Mix(AccentDim, Accent, RouteSelection * 0.72f + RouteHover * 0.18f),
				0.18f + RouteSelection * 0.34f + RouteHover * 0.16f),
			0.5f * Scale);
		CUIRect RouteLabel = {Column.x + 5.0f * Scale, Column.y + 5.0f * Scale, Column.w - 10.0f * Scale, RouteHeaderH - 4.0f * Scale};
		DrawPanel(RouteLabel,
			Fade(Mix(Inset, Accent, RouteSelection * 0.54f + RouteHover * 0.12f),
				0.56f + RouteSelection * 0.18f + RouteHover * 0.08f),
			8.0f * Scale);
		ResearchText(RouteLabel.x + 8.0f * Scale,
			RouteLabel.y + 4.0f * Scale,
			8.2f,
			Localize(PveResearchRouteName(m_ResearchTab, m_ResearchBranch, Route)),
			Fade(Mix(AccentDim, Text, max(RouteSelection, RouteHover)), 1.0f),
			RouteLabel.w - 28.0f * Scale);
		ResearchText(RouteLabel.x + RouteLabel.w - 8.0f * Scale,
			RouteLabel.y + 4.0f * Scale,
			7.2f,
			">",
			Fade(Mix(AccentDim, Accent, max(RouteSelection, RouteHover)), 0.76f + RouteHover * 0.16f),
			-1.0f,
			1);
		if(UI()->DoButtonLogic(&m_aRouteButtonIDs[m_ResearchBranch][Route], &RouteLabel))
			SelectResearchRoute(m_ResearchBranch, Route);
		for(int CardIndex = 0; CardIndex < aRouteCardCount[Route]; CardIndex++)
		{
			const int ID = aRouteCards[Route][CardIndex];
			const CPveCardDef *pDef = PveCardDef(ID);
			const bool Bought = PveCardIsUnlocked(ID, Mask);
			const bool Available = CanBuyResearch(ID, Mask);
			const bool Selected = ID == m_SelectedResearch;
			const vec4 StateColor = Bought ? PurchasedColor : (Available ? AvailableColor : LockedColor);
			const float Selection = m_pClient->m_pMenus->AnimSelected(&m_aNodeButtonIDs[ID], Selected, 14.0f);
			CUIRect Card = {Column.x + 5.0f * Scale,
				Column.y + RouteHeaderH + 8.0f * Scale + CardIndex * (CardH + CardGap),
				Column.w - 10.0f * Scale,
				CardH};
			const float Hover = m_pClient->m_pMenus->AnimHover(&m_aNodeButtonIDs[ID]);
			const float Pressed = m_pClient->m_pMenus->AnimPressed(&m_aNodeButtonIDs[ID]);
			const vec4 FocusColor = Mix(StateColor, Accent, Selection * 0.76f + Hover * 0.20f);
			CUIRect Glow = Card;
			Glow.Margin(-1.2f * Scale, &Glow);
			DrawPanel(Glow,
				Fade(FocusColor, 0.05f + Selection * 0.18f + (Available ? 0.05f : 0.0f) + Hover * 0.05f),
				8.0f * Scale);
			const vec4 CardSurface = Mix(Bought ? Mix(Inset, PurchasedColor, 0.24f) : Deep,
				Panel,
				Selection * 0.52f + Hover * 0.10f);
			DrawPanel(Card,
				Fade(CardSurface, 0.82f + Selection * 0.08f - Pressed * 0.04f),
				8.0f * Scale);
			CUIRect Edge = Card;
			Edge.w = 2.0f * Scale;
			DrawPanel(Edge, Fade(FocusColor, 0.54f + Selection * 0.42f + Hover * 0.20f), 1.0f * Scale);
			const float IconSize = min(20.0f * Scale, Card.h - 8.0f * Scale);
			CUIRect IconTile = {Card.x + 8.0f * Scale,
				Card.y + (Card.h - IconSize) * 0.5f,
				IconSize,
				IconSize};
			DrawPanel(IconTile, Fade(Deep, 0.86f), 6.0f * Scale);
			DrawSprite(PveCardIcon(pDef),
				IconTile.x + IconTile.w * 0.5f,
				IconTile.y + IconTile.h * 0.5f,
				IconSize * 0.70f,
				Text,
				Bought ? 0.92f : 0.68f);
			ResearchText(Card.x + 34.0f * Scale,
				Card.y + 6.0f * Scale,
				Compact ? 8.0f : 8.6f,
				Localize(pDef->m_pName),
				Fade(Mix(Bought ? Text : MutedText, Text, Selection), 1.0f),
				Card.w - 64.0f * Scale);
			char aTier[12];
			str_format(aTier, sizeof(aTier), "T%d", pDef->m_Tier);
			ResearchText(Card.x + Card.w - 8.0f * Scale,
				Card.y + 6.0f * Scale,
				7.0f,
				aTier,
				Fade(StateColor, 0.92f),
				-1.0f,
				1);
			char aStatus[24];
			str_copy(aStatus, Bought ? "✓" : (Available ? "+" : "×"), sizeof(aStatus));
			ResearchText(Card.x + 34.0f * Scale,
				Card.y + Card.h - 9.0f * Scale,
				7.0f,
				aStatus,
				Fade(StateColor, 0.95f));
			ResearchText(Card.x + Card.w - 8.0f * Scale,
				Card.y + Card.h - 9.0f * Scale,
				7.2f,
				">",
				Fade(FocusColor, 0.72f + Hover * 0.20f),
				-1.0f,
				1);
			if(UI()->DoButtonLogic(&m_aNodeButtonIDs[ID], &Card))
			{
				m_SelectedResearch = ID;
				m_ResearchBranch = pDef->m_Branch;
				m_ResearchRoute = Route;
				m_SelectionPulse = 0.7f;
			}
		}
	}

	DrawPanel(Details, Fade(Panel, 0.72f), 12.0f * Scale);
	CUIRect Detail = Details;
	Detail.Margin(12.0f * LayoutScale, &Detail);
	const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
	if(pSelected)
	{
		const bool Bought = PveCardIsUnlocked(m_SelectedResearch, Mask);
		const bool Available = CanBuyResearch(m_SelectedResearch, Mask);
		const vec4 StateColor = Bought ? PurchasedColor : (Available ? AvailableColor : LockedColor);
		const float DetailTransition = 1.0f - clamp(m_SelectionPulse / 0.7f, 0.0f, 1.0f);
		const vec4 DetailStateColor = Mix(Accent, StateColor, DetailTransition);
		char aState[48];
		str_format(aState,
			sizeof(aState),
			"%s  %s",
			Bought ? "✓" : (Available ? "+" : "×"),
			Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")));
		CUIRect DetailHeader, DetailBody;
		Detail.HSplitTop(28.0f * LayoutScale, &DetailHeader, &DetailBody);
		ResearchText(DetailHeader.x,
			DetailHeader.y + 3.0f * Scale,
			11.0f,
			Localize("Research Details"),
			Fade(Text, 0.98f));
		CUIRect StatePill = {DetailHeader.x + DetailHeader.w - 128.0f * Scale,
			DetailHeader.y,
			128.0f * Scale,
			24.0f * Scale};
		DrawPanel(StatePill, Fade(Inset, 0.90f), 10.0f * Scale);
		CUIRect StateEdge = StatePill;
		StateEdge.w = 2.0f * Scale;
		DrawPanel(StateEdge, Fade(DetailStateColor, 0.94f), 1.0f * Scale);
		ResearchText(StatePill.x + StatePill.w * 0.5f,
			StatePill.y + 5.0f * Scale,
			7.6f,
			aState,
			Fade(DetailStateColor, 1.0f),
			-1.0f,
			0);

		CUIRect Hero;
		DetailBody.HSplitTop(92.0f * LayoutScale, &Hero, &DetailBody);
		DrawPanel(Hero, Fade(Inset, 0.82f), 10.0f * Scale);
		CUIRect HeroEdge = Hero;
		HeroEdge.w = 2.0f * Scale;
		DrawPanel(HeroEdge, Fade(DetailStateColor, 0.92f), 1.0f * Scale);
		CUIRect HeroIcon = {Hero.x + 12.0f * Scale, Hero.y + 15.0f * Scale, 58.0f * Scale, 58.0f * Scale};
		DrawPanel(HeroIcon, Fade(Deep, 0.88f), 10.0f * Scale);
		DrawSprite(PveCardIcon(pSelected),
			HeroIcon.x + HeroIcon.w * 0.5f,
			HeroIcon.y + HeroIcon.h * 0.5f,
			42.0f * Scale,
			Text,
			0.94f);
		const float HeroTextX = HeroIcon.x + HeroIcon.w + 12.0f * Scale;
		ResearchText(HeroTextX,
			Hero.y + 15.0f * Scale,
			11.5f,
			Localize(pSelected->m_pName),
			Fade(Text, 1.0f),
			Hero.x + Hero.w - HeroTextX - 10.0f * Scale);
		char aMeta[96];
		str_format(aMeta,
			sizeof(aMeta),
			Localize("%s • TIER %d • COST %d"),
			Localize(PveRarityName(pSelected->m_Rarity)),
			pSelected->m_Tier,
			pSelected->m_ResearchCost);
		ResearchText(HeroTextX,
			Hero.y + 49.0f * Scale,
			8.2f,
			aMeta,
			Fade(Accent, 0.96f),
			Hero.x + Hero.w - HeroTextX - 10.0f * Scale);

		DetailBody.HSplitTop(8.0f * LayoutScale, 0, &DetailBody);
		ResearchText(DetailBody.x,
			DetailBody.y + 2.0f * Scale,
			9.4f,
			Localize("Effect"),
			Fade(Accent, 1.0f));
		DetailBody.y += 22.0f * Scale;
		CUIRect Effect;
		DetailBody.HSplitTop(82.0f * LayoutScale, &Effect, &DetailBody);
		DrawPanel(Effect, Fade(Inset, 0.68f), 9.0f * Scale);
		const char *pEffectText = Localize(Compact ? pSelected->m_pShortDescription : pSelected->m_pDescription);
		ResearchWrapped(Effect.x + 12.0f * Scale,
			Effect.y + 10.0f * Scale,
			8.4f,
			pEffectText,
			Fade(MutedText, 0.92f),
			Effect.w - 20.0f * Scale,
			3);
		DetailBody.HSplitTop(8.0f * LayoutScale, 0, &DetailBody);
		CUIRect Rules;
		DetailBody.HSplitTop(62.0f * LayoutScale, &Rules, &DetailBody);
		DrawPanel(Rules, Fade(Deep, 0.56f), 9.0f * Scale);
		char aRule[64];
		if(pSelected->m_MaxStacks == 1)
			str_copy(aRule, Localize("Unique perk"), sizeof(aRule));
		else
			str_format(aRule, sizeof(aRule), Localize("Stack limit: %d"), pSelected->m_MaxStacks);
		ResearchText(Rules.x + 12.0f * Scale,
			Rules.y + 9.0f * Scale,
			8.7f,
			aRule,
			Fade(Accent, 0.96f));
		char aPrerequisite[128];
		if(pSelected->m_NumPrerequisites > 0)
		{
			str_copy(aPrerequisite, Localize("Requires:"), sizeof(aPrerequisite));
			str_append(aPrerequisite, " ", sizeof(aPrerequisite));
			for(int i = 0; i < pSelected->m_NumPrerequisites; i++)
			{
				if(i > 0)
					str_append(aPrerequisite, ", ", sizeof(aPrerequisite));
				str_append(aPrerequisite,
					Localize(PveCardDef(pSelected->m_aPrerequisites[i])->m_pName),
					sizeof(aPrerequisite));
			}
		}
		else
			str_copy(aPrerequisite, Localize("No prerequisite"), sizeof(aPrerequisite));
		ResearchWrapped(Rules.x + 12.0f * Scale,
			Rules.y + 31.0f * Scale,
			8.0f,
			aPrerequisite,
			Fade(Available || Bought ? Accent : Danger, 0.94f),
			Rules.w - 24.0f * Scale,
			2);

		int UnlockedResearch = 0;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(!PveCardIsBase(ID) && PveCardIsUnlocked(ID, Mask))
				UnlockedResearch++;
		const float ProgressRatio = UnlockedResearch / (float)max(1, (int)NUM_PVE_RESEARCH_CARDS);
		const float BuyHeight = 42.0f * Scale;
		CUIRect Buy = {Detail.x,
			Detail.y + Detail.h - BuyHeight,
			Detail.w,
			BuyHeight};
		CUIRect Progress = {Detail.x,
			Buy.y - 54.0f * Scale,
			Detail.w,
			46.0f * Scale};
		DrawPanel(Progress, Fade(Inset, 0.60f), 9.0f * Scale);
		ResearchText(Progress.x + 12.0f * Scale,
			Progress.y + 8.0f * Scale,
			8.2f,
			Localize("Research Progress"),
			Fade(Accent, 0.96f));
		char aProgress[32];
		str_format(aProgress, sizeof(aProgress), "%d / %d", UnlockedResearch, NUM_PVE_RESEARCH_CARDS);
		ResearchText(Progress.x + Progress.w - 12.0f * Scale,
			Progress.y + 8.0f * Scale,
			8.2f,
			aProgress,
			Fade(Text, 0.86f),
			-1.0f,
			1);
		CUIRect ProgressBar = {Progress.x + 12.0f * Scale,
			Progress.y + 29.0f * Scale,
			Progress.w - 24.0f * Scale,
			4.0f * Scale};
		DrawPanel(ProgressBar, Fade(Deep, 0.82f), 2.0f * Scale);
		CUIRect ProgressFill = ProgressBar;
		ProgressFill.w *= clamp(ProgressRatio, 0.0f, 1.0f);
		if(ProgressFill.w > 0.0f)
			DrawPanel(ProgressFill, Fade(Accent, 0.88f), 2.0f * Scale);

		const float BuyHover = Available ? m_pClient->m_pMenus->AnimHover(&m_BuyButtonID) : 0.0f;
		const float BuyPressed = Available ? m_pClient->m_pMenus->AnimPressed(&m_BuyButtonID) : 0.0f;
		CUIRect BuyBorder = Buy;
		BuyBorder.Margin((-1.2f - BuyHover * 0.8f + BuyPressed * 0.8f) * LayoutScale, &BuyBorder);
		const vec4 BuyStateColor = Available ? Accent : (Bought ? AccentDim : Danger);
		DrawPanel(BuyBorder,
			Fade(BuyStateColor, Available ? 0.56f + BuyHover * 0.28f : 0.34f),
			BuyBorder.h * 0.5f);
		DrawPanel(Buy,
			Fade(Mix(Inset, BuyStateColor, Available ? 0.78f + BuyHover * 0.16f : 0.0f),
				Available ? 0.82f + BuyHover * 0.12f - BuyPressed * 0.06f : 0.76f),
			Buy.h * 0.5f);
		ResearchText(Buy.x + Buy.w * 0.5f,
			Buy.y + 10.0f * Scale,
			11.0f,
			Localize(Bought ? "Unlocked for future choices" : (Available ? "Purchase" : "Locked")),
			Fade(m_ValidationCode && time_get() < m_ValidationUntil ? Danger : Text, 1.0f),
			-1.0f,
			0);
		if(Available && UI()->DoButtonLogic(&m_BuyButtonID, &Buy))
			BuySelectedResearch();
	}
	TextRender()->TextColor(1, 1, 1, 1);
}

void CPveRoguelite::RenderResearchLegacy(CUIRect MainView)
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
		m_aBranchExpand[i] +=
			(((i == m_ResearchBranch) ? 1.0f : 0.0f) - m_aBranchExpand[i]) * (1.0f - expf(-11.0f * Dt));
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
	const vec4 PurchasedColor = AccentDim;
	const vec4 AvailableColor = CMenus::ThemeResearchAvailable();
	const vec4 LockedColor = CMenus::ThemeResearchLocked();
	const vec4 MutedText = CMenus::ThemeTextMuted();
	auto Fade = [&](const vec4 &Color, float Opacity)
	{
		return vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha);
	};
	auto Mix = [&](const vec4 &A, const vec4 &B, float Amount)
	{
		return vec4(A.r + (B.r - A.r) * Amount,
					A.g + (B.g - A.g) * Amount,
					A.b + (B.b - A.b) * Amount,
					A.a + (B.a - A.a) * Amount);
	};
	auto DrawSprite = [&](const CPveUiIcon &Icon, float X, float Y, float Size, const vec4 &Color, float Opacity)
	{
		DrawIcon(Icon.m_Image,
				 Icon.m_Sprite,
				 X,
				 Y,
				 Size * Icon.m_Scale,
				 vec4(Color.r, Color.g, Color.b, Color.a * Opacity * Alpha));
	};
	auto SoftToward = [&](float &Value, float Target, float Speed)
	{
		Value += (Target - Value) * (1.0f - expf(-Speed * Dt));
	};
	// Research copy was tuned small for dense trees; bump readable size without
	// rewriting every layout rect.
	const float Font = 1.18f;
	auto ResearchText =
		[&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth = -1.0f, int Align = -1)
	{
		DrawText(X, Y, Size * Font, pText, Color, MaxWidth, Align);
	};
	auto ResearchWrapped =
		[&](float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines)
	{
		DrawWrappedText(X, Y, Size * Font, pText, Color, MaxWidth, MaxLines);
	};

	MainView.y += (1.0f - Alpha) * 8.0f * Scale;
	// Research now lives on the shared open secondary-page canvas. Keep only
	// restrained corner/edge depth here instead of enclosing the whole page in
	// another opaque panel.
	DrawPanel(MainView, Fade(AccentDim, 0.11f + 0.06f * Wave), 12.0f * Scale);
	MainView.Margin(1.0f * LayoutScale, &MainView);
	const float UnderlayScrim = Client()->State() == IClient::STATE_ONLINE ? 0.62f : 0.20f;
	DrawPanel(MainView, Fade(Deep, UnderlayScrim), 10.5f * Scale);

	CUIRect Header, Body;
	MainView.Margin(8.0f * LayoutScale, &MainView);
	MainView.HSplitTop((Compact ? 102.0f : 116.0f) * LayoutScale, &Header, &Body);
	CUIRect HeaderShadow = Header;
	HeaderShadow.y += 2.0f * Scale;
	DrawPanel(HeaderShadow, Fade(Deep, 0.24f), 10.0f * Scale);
	DrawPanel(Header, Fade(Panel, 0.58f), 9.0f * Scale);
	CUIRect HeaderEdge = {
		Header.x + 12.0f * Scale, Header.y + Header.h - 1.5f * Scale, Header.w - 24.0f * Scale, 1.5f * Scale};
	DrawPanel(HeaderEdge, Fade(Accent, 0.52f + 0.28f * Wave), 0.75f * Scale);
	CUIRect TitleMark = {Header.x + 12.0f * Scale, Header.y + 8.0f * Scale, 3.0f * Scale, 23.0f * Scale};
	TitleMark.y += Wave * 0.6f * Scale;
	DrawPanel(TitleMark, Fade(Accent, 0.78f + 0.22f * WaveFast), 1.5f * Scale);
	ResearchText(
		Header.x + 23.0f * Scale, Header.y + 6.0f * Scale, 13.0f * Scale, Localize("Research"), Fade(Text, 1.0f));

	char aPoints[64];
	str_format(aPoints,
			   sizeof(aPoints),
			   Localize("%d Research Points"),
			   TutorialResearchActive() ? 99 : g_Config.m_ClPveResearchPoints);
	const float PointsWidth = clamp(Header.w * 0.23f, 156.0f * Scale, 196.0f * Scale);
	CUIRect Points = {Header.x + Header.w - PointsWidth - 10.0f * Scale,
					  Header.y + 7.0f * Scale,
					  PointsWidth,
					  (Compact ? 28.0f : 32.0f) * Scale};
	DrawPanel(Points, Fade(AccentDim, 0.34f + 0.16f * Wave), 15.0f * Scale);
	CUIRect PointsInner = Points;
	PointsInner.Margin(1.2f * LayoutScale, &PointsInner);
	DrawPanel(PointsInner, Fade(Inset, 0.98f), 14.0f * Scale);
	const float CoinPulse = 1.0f + 0.06f * WaveFast;
	DrawSprite(CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_BIGCOIN),
			   Points.x + 17.0f * Scale,
			   Points.y + Points.h * 0.5f,
			   15.0f * Scale * CoinPulse,
			   Accent,
			   0.88f + 0.12f * Wave);
	ResearchText(Points.x + Points.w * 0.57f,
				 Points.y + (Compact ? 5.3f : 7.0f) * Scale,
				 9.2f * Scale,
				 aPoints,
				 Fade(Accent, 1.0f),
				 -1.0f,
				 0);

	ResearchText(Header.x + 13.0f * Scale,
				 Header.y + (Compact ? 31.0f : 35.0f) * Scale,
				 9.1f * Scale,
				 Localize("Research unlocks perk cards; select them during a run to activate their effects."),
				 Fade(Text, 0.92f),
				 Header.w - 26.0f * Scale,
				 -1);
	ResearchText(Header.x + 13.0f * Scale,
				 Header.y + (Compact ? 46.0f : 53.5f) * Scale,
				 8.3f * Scale,
				 Localize("Base cards are always available • Rare and Epic perks are unique"),
				 Fade(Accent, 0.88f),
				 Header.w - 26.0f * Scale,
				 -1);

	const char *apTabs[3] = {Localize("Core"), Localize("Weapons"), Localize("Modes")};
	const float TabStart = Header.x + 18.0f * Scale;
	const float TabAreaWidth = min(Header.w * 0.58f, 430.0f * Scale);
	const float TabGap = 8.0f * Scale;
	const float TabWidth = (TabAreaWidth - TabGap * 2.0f) / 3.0f;
	const float MapY = Header.y + (Compact ? 65.0f : 72.0f) * Scale;
	IGraphics::CLineItem aMapLines[2] = {
		IGraphics::CLineItem(TabStart + TabWidth,
							 MapY + 12.0f * Scale,
							 TabStart + TabWidth + TabGap,
							 MapY + 12.0f * Scale),
		IGraphics::CLineItem(TabStart + TabWidth * 2.0f + TabGap,
							 MapY + 12.0f * Scale,
							 TabStart + TabWidth * 2.0f + TabGap * 2.0f,
							 MapY + 12.0f * Scale)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(AccentDim.r, AccentDim.g, AccentDim.b, 0.42f * Alpha);
	Graphics()->LinesDraw(aMapLines, 2);
	Graphics()->LinesEnd();
	for(int Tab = 0; Tab < 3; Tab++)
	{
		CUIRect TabRect = {TabStart + Tab * (TabWidth + TabGap),
						   MapY,
						   TabWidth,
						   (Compact ? 24.0f : 27.0f) * Scale};
		const bool Selected = Tab == m_ResearchTab;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aTabButtonIDs[Tab]);
		const float Lift = (Selected ? 1.2f : 0.0f) + Hover * 1.4f;
		TabRect.y -= Lift * Scale;
		CUIRect TabBorder = TabRect;
		TabBorder.Margin((-1.0f - Hover * 0.4f) * LayoutScale, &TabBorder);
		DrawPanel(TabBorder,
				  Fade(Selected || Hover > 0.2f ? Accent : AccentDim, Selected ? 0.78f : (0.18f + Hover * 0.42f)),
				  11.0f * Scale);
		DrawPanel(TabRect, Fade(Selected ? AccentDim : Inset, Selected ? 0.62f : 0.96f), 10.0f * Scale);
		const float TabGutter = 18.0f * Scale;
		CUIRect StatusDot = {TabRect.x + 7.0f * Scale,
							 TabRect.y + TabRect.h * 0.5f - 4.0f * Scale,
							 8.0f * Scale,
							 8.0f * Scale};
		DrawPanel(StatusDot,
				  Fade(Selected ? Accent : AccentDim, Selected ? 1.0f : (0.55f + Hover * 0.25f)),
				  4.0f * Scale);
		ResearchText(TabRect.x + TabGutter + 8.0f * Scale,
						 TabRect.y + (Compact ? 5.0f : 6.2f) * Scale,
						 (Compact ? 8.0f : 9.2f) * Scale,
						 apTabs[Tab],
					 Fade(Selected ? Text : AccentDim, 1.0f),
					 -1.0f,
					 0);
		if(Selected)
		{
			CUIRect SelectedLine = {TabRect.x + TabGutter + 4.0f * Scale,
									 TabRect.y + TabRect.h - 1.5f * Scale,
									 TabRect.w - TabGutter - 10.0f * Scale,
									 1.5f * Scale};
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
		CUIRect Checkpoint = {Points.x - CheckpointWidth - 7.0f * Scale,
							  Header.y + 7.0f * Scale,
							  CheckpointWidth,
							  (Compact ? 26.0f : 30.0f) * Scale};
		const bool Hovered = UI()->HotItem() == &m_CheckpointButtonID;
		DrawPanel(Checkpoint, Fade(Hovered ? AccentDim : Inset, 0.98f), Checkpoint.h * 0.5f);
		char aCheckpoint[64];
		str_format(aCheckpoint, sizeof(aCheckpoint), Localize("Checkpoint %d"), g_Config.m_ClPvePreferredCheckpoint);
		ResearchText(Checkpoint.x + Checkpoint.w * 0.5f,
					 Checkpoint.y + (Compact ? 5.1f : 6.9f) * Scale,
					 8.6f * Scale,
					 aCheckpoint,
					 Fade(Accent, 1.0f),
					 -1.0f,
					 0);
		if(UI()->DoButtonLogic(&m_CheckpointButtonID, &Checkpoint))
			CycleCheckpoint();
	}

	Body.HSplitTop(8.0f * LayoutScale, 0, &Body);
	CUIRect Tree, Details;
	const float DetailWidth = clamp(Body.w * 0.30f, 244.0f * Scale, 282.0f * Scale);
	Body.VSplitRight(DetailWidth / max(0.01f, UI()->Scale()), &Tree, &Details);
	Tree.VSplitRight(8.0f * LayoutScale, &Tree, 0);
	DrawPanel(Tree, Fade(Panel, 0.68f), 9.0f * Scale);
	DrawPanel(Details, Fade(Panel, 0.96f), 9.0f * Scale);
	const char *apTabDescriptions[3] = {
		Localize("Core research improves universal attack, survival, and logistics perks."),
		Localize("Weapon research unlocks specialization perks matched to your current weapon."),
		Localize("Mode research unlocks perks that appear only in the matching PvE mode.")};
	const CPveResearchMask Mask = TutorialResearchActive() ? CPveResearchMask() : ParseResearchMask();

	CUIRect StarMap = Tree;
	StarMap.Margin(7.0f * LayoutScale, &StarMap);
	CUIRect MapHeader, MapCanvas;
	StarMap.HSplitTop((Compact ? 48.0f : 56.0f) * Scale, &MapHeader, &MapCanvas);
	DrawPanel(MapHeader, Fade(Inset, 0.72f), 8.0f * Scale);
	CUIRect HeaderMark = {MapHeader.x + 9.0f * Scale,
						 MapHeader.y + 8.0f * Scale,
						 2.0f * Scale,
						 MapHeader.h - 16.0f * Scale};
	DrawPanel(HeaderMark, Fade(Accent, 0.76f + 0.20f * WaveFast), 1.0f * Scale);
	ResearchText(MapHeader.x + 17.0f * Scale,
				 MapHeader.y + 5.0f * Scale,
				 9.8f * Scale,
				 Localize("NEURAL CONSTELLATION"),
				 Fade(Text, 0.98f));
	ResearchWrapped(MapHeader.x + 17.0f * Scale,
					 MapHeader.y + (Compact ? 21.0f : 27.0f) * Scale,
					 7.4f * Scale,
					 apTabDescriptions[m_ResearchTab],
					 Fade(Text, 0.72f),
					 MapHeader.w - 192.0f * Scale,
					 2);
	CUIRect Legend = {MapHeader.x + MapHeader.w - 174.0f * Scale,
					 MapHeader.y + 7.0f * Scale,
					 164.0f * Scale,
					 MapHeader.h - 14.0f * Scale};
	const char *apLegend[] = {"PURCHASED", "AVAILABLE", "LOCKED"};
	const vec4 aLegendColors[3] = {PurchasedColor, AvailableColor, LockedColor};
	for(int LegendIndex = 0; LegendIndex < 3; LegendIndex++)
	{
		CUIRect LegendRow;
		Legend.HSplitTop(Legend.h / (3 - LegendIndex), &LegendRow, &Legend);
		CUIRect LegendDot = {LegendRow.x, LegendRow.y + LegendRow.h * 0.5f - 2.0f * Scale, 4.0f * Scale, 4.0f * Scale};
		DrawPanel(LegendDot, Fade(aLegendColors[LegendIndex], 0.86f), 2.0f * Scale);
		ResearchText(LegendRow.x + 9.0f * Scale,
					 LegendRow.y + 1.0f * Scale,
					 6.5f * Scale,
					 Localize(apLegend[LegendIndex]),
					 Fade(aLegendColors[LegendIndex], 0.90f));
	}

	MapCanvas.Margin(5.0f * LayoutScale, &MapCanvas);
	DrawPanel(MapCanvas, Fade(Deep, 0.64f), 8.0f * Scale);
	const float CanvasLeft = MapCanvas.x + 10.0f * Scale;
	const float CanvasTop = MapCanvas.y + 8.0f * Scale;
	const float CanvasRight = MapCanvas.x + MapCanvas.w - 10.0f * Scale;
	const float CanvasBottom = MapCanvas.y + MapCanvas.h - 8.0f * Scale;
	const float CanvasWidth = CanvasRight - CanvasLeft;
	const float CanvasHeight = CanvasBottom - CanvasTop;
	const float NetworkLeft = CanvasLeft + CanvasWidth * 0.29f;
	const float NetworkWidth = CanvasRight - NetworkLeft - 6.0f * Scale;
	const float CenterX = CanvasLeft + (Compact ? 8.0f : 10.0f) * Scale;
	const float CenterY = CanvasTop + CanvasHeight * 0.50f;
	const float MapRadius = min(CanvasWidth * 0.31f, CanvasHeight * 0.44f);
	const float NodeDiameter = (Compact ? 21.0f : 24.0f) * Scale;
	const float NodeRadius = NodeDiameter * 0.5f;

	for(int Star = 0; Star < 26; Star++)
	{
		const float StarX = CanvasLeft + fmodf((float)(Star * 47 + 19), max(1.0f, CanvasRight - CanvasLeft));
		const float StarY = CanvasTop + fmodf((float)(Star * 73 + 11), max(1.0f, CanvasBottom - CanvasTop));
		const float StarSize = (Star % 5 == 0 ? 2.0f : 1.0f) * Scale;
		CUIRect StarDot = {StarX, StarY, StarSize, StarSize};
		DrawPanel(StarDot, Fade(AccentDim, Star % 5 == 0 ? 0.26f : 0.12f), StarSize * 0.5f);
	}

	int BranchCount = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
			BranchCount = max(BranchCount, PveCardDef(ID)->m_Branch + 1);
	BranchCount = max(1, BranchCount);
	m_ResearchBranch = clamp(m_ResearchBranch, 0, BranchCount - 1);
	int aTierCounts[4][16] = {};
	int aTierSlots[4][16] = {};
	int aRouteCounts[4][3] = {};
	int aRouteBoughtCounts[4][3] = {};
	int aRouteMinTier[4][3];
	int aRouteMaxTier[4][3];
	for(int Branch = 0; Branch < 4; Branch++)
		for(int Route = 0; Route < 3; Route++)
		{
			aRouteMinTier[Branch][Route] = 99;
			aRouteMaxTier[Branch][Route] = 0;
		}
	int MaxTier = 1;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		const CPveCardDef *pDef = PveCardDef(ID);
		if(!pDef->m_Base && pDef->m_Tab == m_ResearchTab)
		{
			const int Branch = clamp(pDef->m_Branch, 0, 3);
			const int Tier = clamp(pDef->m_Tier, 0, 15);
			const int Route = clamp(PveResearchRoute(pDef), 0, 2);
			aTierCounts[Branch][Tier]++;
			aRouteCounts[Branch][Route]++;
			aRouteMinTier[Branch][Route] = min(aRouteMinTier[Branch][Route], pDef->m_Tier);
			aRouteMaxTier[Branch][Route] = max(aRouteMaxTier[Branch][Route], pDef->m_Tier);
			if(PveCardIsUnlocked(ID, Mask))
				aRouteBoughtCounts[Branch][Route]++;
			MaxTier = max(MaxTier, pDef->m_Tier);
		}
	}

	const float InnerRadius = MapRadius * 0.22f;
	const float LaneSpacing = CanvasHeight / (BranchCount + 1.0f);
	CUIRect aNodeRects[NUM_PVE_CARDS];
	bool aNodeVisible[NUM_PVE_CARDS] = {};
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		const CPveCardDef *pDef = PveCardDef(ID);
		if(pDef->m_Base || pDef->m_Tab != m_ResearchTab)
			continue;
		const int Branch = clamp(pDef->m_Branch, 0, 3);
		const int RouteCount = PveResearchRouteCount(m_ResearchTab, Branch);
		const int Route = clamp(PveResearchRoute(pDef), 0, RouteCount - 1);
		const float TierAmount = (pDef->m_Tier - 1) / (float)max(1, MaxTier - 1);
		const int TierSlot = aTierSlots[Branch][clamp(pDef->m_Tier, 0, 15)]++;
		const int TierCount = aTierCounts[Branch][clamp(pDef->m_Tier, 0, 15)];
		const float LaneY = CanvasTop + CanvasHeight * (Branch + 1) / (BranchCount + 1.0f);
		const float RouteOffset = (Route - (RouteCount - 1) * 0.5f) * NodeDiameter * 0.74f;
		const float SlotOffset = (TierSlot - (TierCount - 1) * 0.5f) * NodeDiameter * 0.45f;
		const float StaggerX = ((pDef->m_Tier + Branch) % 2 ? NodeDiameter * 0.42f : 0.0f);
		const float TierX = NetworkLeft + (NetworkWidth - NodeDiameter) * TierAmount + StaggerX;
		float X = TierX - NodeRadius;
		float Y = LaneY + RouteOffset + SlotOffset - NodeRadius;
		X = clamp(X, NetworkLeft - NodeRadius, CanvasRight - NodeDiameter);
		Y = clamp(Y, CanvasTop, CanvasBottom - NodeDiameter);
		aNodeRects[ID] = {X, Y, NodeDiameter, NodeDiameter};
		aNodeVisible[ID] = true;
	}

	for(int Ring = 1; Ring <= 3; Ring++)
	{
		const float Radius = InnerRadius + MapRadius * 0.24f * Ring;
		IGraphics::CLineItem aRing[48];
		const int Segments = 24;
		for(int Segment = 0; Segment < Segments; Segment++)
		{
			const float A0 = -pi * 0.5f + pi * Segment / Segments;
			const float A1 = -pi * 0.5f + pi * (Segment + 1) / Segments;
			aRing[Segment * 2] = IGraphics::CLineItem(CenterX + cosf(A0) * Radius,
													 CenterY + sinf(A0) * Radius,
													 CenterX + cosf(A1) * Radius,
													 CenterY + sinf(A1) * Radius);
		}
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(AccentDim.r, AccentDim.g, AccentDim.b, (0.10f + Ring * 0.025f) * Alpha);
		Graphics()->LinesDraw(aRing, Segments * 2);
		Graphics()->LinesEnd();
	}
	IGraphics::CLineItem aSpokes[4];
	int SpokeCount = 0;
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		const float LaneY = CanvasTop + CanvasHeight * (Branch + 1) / (BranchCount + 1.0f);
		aSpokes[SpokeCount++] = IGraphics::CLineItem(CenterX + NodeRadius,
													 CenterY,
													 NetworkLeft - 8.0f * Scale,
													 LaneY);
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(AccentDim.r, AccentDim.g, AccentDim.b, 0.18f * Alpha);
	Graphics()->LinesDraw(aSpokes, SpokeCount);
	Graphics()->LinesEnd();
	IGraphics::CLineItem aLaneLines[4];
	int LaneCount = 0;
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		const float LaneY = CanvasTop + CanvasHeight * (Branch + 1) / (BranchCount + 1.0f);
		aLaneLines[LaneCount++] = IGraphics::CLineItem(NetworkLeft,
													 LaneY,
													 CanvasRight,
													 LaneY);
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(AccentDim.r, AccentDim.g, AccentDim.b, 0.08f * Alpha);
	Graphics()->LinesDraw(aLaneLines, LaneCount);
	Graphics()->LinesEnd();

	IGraphics::CLineItem aLockedLinks[NUM_PVE_CARDS * 3];
	IGraphics::CLineItem aAvailableLinks[NUM_PVE_CARDS * 3];
	IGraphics::CLineItem aPurchasedLinks[NUM_PVE_CARDS * 3];
	int LinkCount = 0;
	int AvailableLinkCount = 0;
	int PurchasedLinkCount = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		if(!aNodeVisible[ID])
			continue;
		const CPveCardDef *pDef = PveCardDef(ID);
		for(int Prerequisite = 0; Prerequisite < pDef->m_NumPrerequisites; Prerequisite++)
		{
			const int PreviousID = pDef->m_aPrerequisites[Prerequisite];
			const CUIRect Previous = aNodeVisible[PreviousID] ? aNodeRects[PreviousID]
																		 : CUIRect{CenterX - 1.0f, CenterY - 1.0f, 2.0f, 2.0f};
			const CUIRect Current = aNodeRects[ID];
			const IGraphics::CLineItem Link(Previous.x + Previous.w * 0.5f,
										 Previous.y + Previous.h * 0.5f,
										 Current.x + Current.w * 0.5f,
										 Current.y + Current.h * 0.5f);
			if(PveCardIsUnlocked(ID, Mask))
			{
				if(PurchasedLinkCount < (int)(sizeof(aPurchasedLinks) / sizeof(aPurchasedLinks[0])))
					aPurchasedLinks[PurchasedLinkCount++] = Link;
			}
			else if(CanBuyResearch(ID, Mask))
			{
				if(AvailableLinkCount < (int)(sizeof(aAvailableLinks) / sizeof(aAvailableLinks[0])))
					aAvailableLinks[AvailableLinkCount++] = Link;
			}
			else if(LinkCount < (int)(sizeof(aLockedLinks) / sizeof(aLockedLinks[0])))
				aLockedLinks[LinkCount++] = Link;
		}
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(LockedColor.r, LockedColor.g, LockedColor.b, 0.28f * Alpha);
	Graphics()->LinesDraw(aLockedLinks, LinkCount);
	Graphics()->SetColor(AvailableColor.r, AvailableColor.g, AvailableColor.b, (0.70f + 0.18f * WaveFast) * Alpha);
	Graphics()->LinesDraw(aAvailableLinks, AvailableLinkCount);
	Graphics()->SetColor(PurchasedColor.r, PurchasedColor.g, PurchasedColor.b, (0.66f + 0.22f * WaveFast) * Alpha);
	Graphics()->LinesDraw(aPurchasedLinks, PurchasedLinkCount);
	Graphics()->LinesEnd();

	const char *pBranchNames[4];
	if(m_ResearchTab == PVE_TAB_CORE)
	{
		const char *apNames[4] = {"Attack", "Survival", "Logistics", "Drone"};
		for(int Branch = 0; Branch < 4; Branch++)
			pBranchNames[Branch] = apNames[Branch];
	}
	else if(m_ResearchTab == PVE_TAB_WEAPON)
	{
		const char *apNames[4] = {"Firearms", "Explosives", "Electric", "Melee"};
		for(int Branch = 0; Branch < 4; Branch++)
			pBranchNames[Branch] = apNames[Branch];
	}
	else
	{
		const char *apNames[3] = {"Invasion", "Horde", "Extraction"};
		for(int Branch = 0; Branch < 3; Branch++)
			pBranchNames[Branch] = apNames[Branch];
	}
	const float RouteTagHeight = (Compact ? 13.0f : 15.0f) * Scale;
	const float BranchRailHeight = min(LaneSpacing - 8.0f * Scale, (Compact ? 72.0f : 78.0f) * Scale);
	const auto TierCenterX = [&](int Tier)
	{
		const float TierAmount = (Tier - 1) / (float)max(1, MaxTier - 1);
		return NetworkLeft + (NetworkWidth - NodeDiameter) * TierAmount;
	};
	const auto SelectResearchRoute = [&](int Branch, int Route)
	{
		m_ResearchBranch = Branch;
		m_ResearchRoute = Route;
		int BestID = -1;
		int BestRank = 3;
		int BestTier = 999;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			const CPveCardDef *pDef = PveCardDef(ID);
			if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != Branch ||
				PveResearchRoute(pDef) != Route)
				continue;
			const int Rank = CanBuyResearch(ID, Mask) ? 0 : (PveCardIsUnlocked(ID, Mask) ? 2 : 1);
			if(BestID < 0 || Rank < BestRank || (Rank == BestRank && pDef->m_Tier < BestTier))
			{
				BestID = ID;
				BestRank = Rank;
				BestTier = pDef->m_Tier;
			}
		}
		if(BestID >= 0)
			m_SelectedResearch = BestID;
		m_SelectionPulse = 0.55f;
	};
	for(int Branch = 0; Branch < BranchCount; Branch++)
	{
		const float LaneY = CanvasTop + LaneSpacing * (Branch + 1);
		CUIRect BranchRail = {CanvasLeft + 3.0f * Scale,
						  LaneY - BranchRailHeight * 0.5f,
						  NetworkLeft - CanvasLeft - 14.0f * Scale,
						  BranchRailHeight};
		const bool BranchSelected = Branch == m_ResearchBranch;
		const float BranchHover = m_pClient->m_pMenus->AnimHover(&m_aBranchButtonIDs[Branch]);
		DrawPanel(BranchRail,
				  Fade(BranchSelected ? Inset : Deep, 0.70f + BranchHover * 0.12f),
				  BranchRail.h * 0.34f);
		CUIRect BranchEdge = BranchRail;
		BranchEdge.w = 2.0f * Scale;
		DrawPanel(BranchEdge,
				  Fade(BranchSelected || BranchHover > 0.2f ? Accent : AccentDim,
					   BranchSelected ? 0.92f : 0.52f + BranchHover * 0.28f),
				  Scale);
		CUIRect BranchHeader = {BranchRail.x + 8.0f * Scale,
								BranchRail.y + 4.0f * Scale,
								BranchRail.w - 16.0f * Scale,
								17.0f * Scale};
		DrawSprite(PveBranchIcon(m_ResearchTab, Branch),
					   BranchHeader.x + 7.0f * Scale,
					   BranchHeader.y + BranchHeader.h * 0.5f,
					   (11.0f + BranchHover) * Scale,
					   BranchSelected ? Accent : AccentDim,
					   0.76f + 0.24f * (BranchSelected ? 1.0f : BranchHover));
		ResearchText(BranchHeader.x + 17.0f * Scale,
					 BranchHeader.y + 2.0f * Scale,
					 (Compact ? 7.1f : 7.8f) * Scale,
					 Localize(pBranchNames[Branch]),
					 Fade(BranchSelected ? Text : AccentDim, 0.96f),
					 BranchHeader.w - 36.0f * Scale,
					 -1);
		int BranchCountBought = 0;
		int BranchNodeCount = 0;
		for(int Route = 0; Route < 3; Route++)
		{
			BranchCountBought += aRouteBoughtCounts[Branch][Route];
			BranchNodeCount += aRouteCounts[Branch][Route];
		}
		char aBranchProgress[32];
		str_format(aBranchProgress, sizeof(aBranchProgress), "%d / %d", BranchCountBought, BranchNodeCount);
		ResearchText(BranchHeader.x + BranchHeader.w,
					 BranchHeader.y + 3.0f * Scale,
					 5.8f * Scale,
					 aBranchProgress,
					 Fade(Accent, 0.84f),
					 -1.0f,
					 1);
		CUIRect BranchHit = BranchHeader;
		BranchHit.y -= 2.0f * Scale;
		BranchHit.h += 4.0f * Scale;
		if(UI()->DoButtonLogic(&m_aBranchButtonIDs[Branch], &BranchHit))
		{
			SelectResearchRoute(Branch, 0);
		}

		int PreviewID = -1;
		int PreviewRank = 3;
		int PreviewTier = 999;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		{
			const CPveCardDef *pDef = PveCardDef(ID);
			if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != Branch)
				continue;
			const int Rank = CanBuyResearch(ID, Mask) ? 0 : (PveCardIsUnlocked(ID, Mask) ? 2 : 1);
			if(PreviewID < 0 || Rank < PreviewRank || (Rank == PreviewRank && pDef->m_Tier < PreviewTier))
			{
				PreviewID = ID;
				PreviewRank = Rank;
				PreviewTier = pDef->m_Tier;
			}
		}
		if(PreviewID >= 0)
		{
			const CPveCardDef *pPreview = PveCardDef(PreviewID);
			const bool PreviewBought = PveCardIsUnlocked(PreviewID, Mask);
			const bool PreviewAvailable = CanBuyResearch(PreviewID, Mask);
			const bool PreviewSelected = PreviewID == m_SelectedResearch;
			const float PreviewHover = m_pClient->m_pMenus->AnimHover(&m_aNodeButtonIDs[PreviewID]);
			const vec4 PreviewState = PreviewBought ? PurchasedColor : (PreviewAvailable ? AvailableColor : LockedColor);
			const float PreviewTop = BranchRail.y + 22.0f * Scale;
			const float PreviewHeight = max(18.0f * Scale, BranchRail.y + BranchRail.h - PreviewTop - 3.0f * Scale);
			const float PreviewLeftInset = (Compact ? 18.0f : 25.0f) * Scale;
			CUIRect Preview = {BranchRail.x + PreviewLeftInset,
								PreviewTop,
								BranchRail.w - PreviewLeftInset - 7.0f * Scale,
								PreviewHeight};
			CUIRect PreviewGlow = Preview;
			PreviewGlow.Margin(-2.0f * Scale, &PreviewGlow);
			DrawPanel(PreviewGlow,
					  Fade(PreviewSelected ? Accent : PreviewState,
						   PreviewSelected ? 0.24f : (PreviewAvailable ? 0.16f : 0.08f)),
					  Preview.h * 0.26f);
			CUIRect PreviewBorder = Preview;
			PreviewBorder.Margin(-0.8f * LayoutScale, &PreviewBorder);
			DrawPanel(PreviewBorder,
					  Fade(PreviewSelected || PreviewHover > 0.15f ? Accent : PreviewState,
						   PreviewSelected ? 0.90f : (PreviewAvailable ? 0.72f : 0.46f)),
					  Preview.h * 0.26f);
			DrawPanel(Preview, Fade(PreviewAvailable ? Panel : Inset, PreviewAvailable ? 0.98f : 0.94f), Preview.h * 0.26f);
			const float PreviewIconSize = min(12.0f * Scale, max(8.0f * Scale, Preview.h - 8.0f * Scale));
			CUIRect PreviewIcon = {Preview.x + 4.0f * Scale,
									Preview.y + 4.0f * Scale,
									PreviewIconSize,
									PreviewIconSize};
			DrawPanel(PreviewIcon, Fade(Deep, 0.86f), PreviewIcon.w * 0.26f);
			DrawSprite(PveCardIcon(pPreview),
					   PreviewIcon.x + PreviewIcon.w * 0.5f,
					   PreviewIcon.y + PreviewIcon.h * 0.5f,
					   PreviewIcon.w * 0.72f,
					   Text,
					   0.92f);
			const float PreviewTextX = PreviewIcon.x + PreviewIcon.w + 4.0f * Scale;
			const float PreviewTextWidth = Preview.x + Preview.w - PreviewTextX - 4.0f * Scale;
			ResearchText(PreviewTextX,
							 Preview.y + 3.0f * Scale,
							 6.6f * Scale,
							 Localize(pPreview->m_pName),
							 Fade(Text, 1.0f),
							 PreviewTextWidth,
							 -1);
			const float PreviewFooterY = Preview.y + Preview.h - 4.8f * Scale;
			const float PreviewBodyY = Preview.y + 15.5f * Scale;
			const float PreviewBodyLineHeight = 3.8f * Scale * Font;
			const float PreviewBodyHeight = PreviewFooterY - PreviewBodyY - 2.0f * Scale;
			const int PreviewBodyLines = clamp((int)(PreviewBodyHeight / max(0.01f, PreviewBodyLineHeight)), 0, 2);
			if(PreviewBodyLines > 0)
			{
				ResearchWrapped(PreviewTextX,
								 PreviewBodyY,
								 3.8f * Scale,
								 Localize(pPreview->m_pShortDescription),
								 Fade(Text, 0.70f),
								 PreviewTextWidth,
								 PreviewBodyLines);
			}
			char aPreviewCost[48];
			str_format(aPreviewCost, sizeof(aPreviewCost), Localize("%d Research Points"), pPreview->m_ResearchCost);
			const float PreviewFooterWidth = max(20.0f * Scale, (Preview.w - 18.0f * Scale) * 0.5f);
			ResearchText(Preview.x + 6.0f * Scale,
							 PreviewFooterY,
							 4.2f * Scale,
							 aPreviewCost,
							 Fade(PreviewAvailable ? AvailableColor : AccentDim, 0.92f),
							 PreviewFooterWidth,
							 -1);
			ResearchText(Preview.x + Preview.w - 10.0f * Scale,
							 PreviewFooterY,
							 4.2f * Scale,
							 Localize(PreviewBought ? "PURCHASED" : (PreviewAvailable ? "AVAILABLE" : "LOCKED")),
							 Fade(PreviewState, 1.0f),
							 PreviewFooterWidth,
							 1);
			if(UI()->DoButtonLogic(&m_aNodeButtonIDs[PreviewID], &Preview))
			{
				m_SelectedResearch = PreviewID;
				m_ResearchBranch = Branch;
				m_ResearchRoute = PveResearchRoute(pPreview);
				m_SelectionPulse = 0.7f;
			}
		}

		const int RouteCount = PveResearchRouteCount(m_ResearchTab, Branch);
		for(int Route = 0; Route < RouteCount; Route++)
		{
			if(aRouteCounts[Branch][Route] <= 0)
				continue;
			const float RouteStart = max(NetworkLeft, TierCenterX(aRouteMinTier[Branch][Route]) - NodeRadius - 2.0f * Scale);
			const float RouteEnd = min(CanvasRight, TierCenterX(aRouteMaxTier[Branch][Route]) + NodeRadius + 2.0f * Scale);
			CUIRect RouteTag = {RouteStart,
								LaneY - LaneSpacing * 0.38f - RouteTagHeight * 0.5f,
														max(44.0f * Scale, RouteEnd - RouteStart),
														RouteTagHeight};
			const bool RouteSelected = BranchSelected && Route == m_ResearchRoute;
			const float RouteHover = m_pClient->m_pMenus->AnimHover(&m_aRouteButtonIDs[Branch][Route]);
			DrawPanel(RouteTag,
					  Fade(RouteSelected ? AccentDim : (RouteHover > 0.18f ? Panel : Inset),
						   RouteSelected ? 0.64f : (0.48f + RouteHover * 0.18f)),
					  RouteTagHeight * 0.5f);
			CUIRect RouteEdge = RouteTag;
			RouteEdge.h = 1.0f * Scale;
			DrawPanel(RouteEdge,
					  Fade(RouteSelected || RouteHover > 0.18f ? Accent : AccentDim,
						   RouteSelected ? 0.90f : 0.42f + RouteHover * 0.32f),
					  0.5f * Scale);
			ResearchText(RouteTag.x + 6.0f * Scale,
						 RouteTag.y + (Compact ? 2.2f : 3.2f) * Scale,
						 6.2f * Scale,
						 Localize(PveResearchRouteName(m_ResearchTab, Branch, Route)),
						 Fade(RouteSelected || RouteHover > 0.18f ? Text : AccentDim, 0.92f),
						 RouteTag.w - 12.0f * Scale,
						 -1);
			if(UI()->DoButtonLogic(&m_aRouteButtonIDs[Branch][Route], &RouteTag))
				SelectResearchRoute(Branch, Route);
		}
	}

	const float HubGlowDiameter = NodeDiameter * 1.35f;
	CUIRect HubGlow = {CenterX - HubGlowDiameter * 0.5f,
						 CenterY - HubGlowDiameter * 0.5f,
						 HubGlowDiameter,
						 HubGlowDiameter};
	DrawPanel(HubGlow, Fade(Accent, 0.12f + 0.08f * Wave), HubGlow.w * 0.5f);
	CUIRect Hub = {CenterX - NodeRadius, CenterY - NodeRadius, NodeDiameter, NodeDiameter};
	DrawPanel(Hub, Fade(Accent, 0.72f + 0.16f * WaveFast), Hub.w * 0.5f);
	CUIRect HubInner = Hub;
	HubInner.Margin(2.5f * Scale, &HubInner);
	DrawPanel(HubInner, Fade(Deep, 0.96f), HubInner.w * 0.5f);
	ResearchText(CenterX,
				 CenterY - 3.8f * Scale,
				 6.6f * Scale,
				 apTabs[m_ResearchTab],
				 Fade(Text, 0.96f),
				 -1.0f,
				 0);
	IGraphics::CLineItem RootConnector(CenterX,
									 CenterY + NodeRadius,
									 CenterX,
									 CenterY + NodeRadius + 7.0f * Scale);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.72f * Alpha);
	Graphics()->LinesDraw(&RootConnector, 1);
	Graphics()->LinesEnd();
	ResearchText(CenterX,
				 CenterY + NodeRadius + 9.0f * Scale,
				 5.8f * Scale,
				 Localize("ROOT"),
				 Fade(Accent, 0.88f),
				 -1.0f,
				 0);

	UI()->ClipEnable(&MapCanvas);
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		if(!aNodeVisible[ID])
			continue;
		const CPveCardDef *pDef = PveCardDef(ID);
		const CUIRect Anchor = aNodeRects[ID];
		const bool Bought = PveCardIsUnlocked(ID, Mask);
		const bool Available = CanBuyResearch(ID, Mask);
		const bool Selected = ID == m_SelectedResearch;
		const float Hover = m_pClient->m_pMenus->AnimHover(&m_aNodeButtonIDs[ID]);
		const float Pulse = Selected ? 0.78f + 0.22f * WaveFast : 1.0f;
		const float NodeScale = 1.0f + Hover * 0.06f + (Selected ? m_SelectionPulse * 0.025f : 0.0f);
		CUIRect Node = Anchor;
		Node.x -= Node.w * (NodeScale - 1.0f) * 0.5f;
		Node.y -= Node.h * (NodeScale - 1.0f) * 0.5f + Hover * 1.5f * Scale;
		Node.w *= NodeScale;
		Node.h *= NodeScale;
		const vec4 StateColor = Bought ? PurchasedColor : (Available ? AvailableColor : LockedColor);
		CUIRect Glow = Node;
		Glow.Margin(-4.0f * Scale, &Glow);
		DrawPanel(Glow,
				  Fade(Selected ? Accent : StateColor,
					   Selected ? 0.34f * Pulse : (Bought ? 0.14f : 0.08f)),
				  Glow.w * 0.5f);
		DrawPanel(Node,
				  Fade(Bought ? Mix(Inset, PurchasedColor, 0.46f) : Deep, 0.96f),
				  Node.w * 0.5f);
		CUIRect Ring = Node;
		Ring.Margin(-1.3f * LayoutScale, &Ring);
		DrawPanel(Ring,
				  Fade(Selected || Hover > 0.15f ? Accent : StateColor,
					   Selected ? 0.96f * Pulse : (Bought ? 0.72f : 0.42f)),
				  Ring.w * 0.5f);
		DrawSprite(PveCardIcon(pDef),
				   Node.x + Node.w * 0.5f,
				   Node.y + Node.h * 0.5f,
				   Node.w * 0.58f,
				   Text,
				   Bought ? 0.88f : 0.62f);
		char aTier[8];
		str_format(aTier, sizeof(aTier), "T%d", pDef->m_Tier);
		ResearchText(Node.x + Node.w * 0.5f,
					 Node.y + Node.h + 2.0f * Scale,
					 5.6f * Scale,
					 aTier,
					 Fade(StateColor, 0.88f),
					 -1.0f,
					 0);
		if(Bought && Hover > 0.25f)
			ResearchText(Node.x + Node.w * 0.5f,
						 Node.y - 9.0f * Scale,
						 5.9f * Scale,
						 Localize(pDef->m_pName),
						 Fade(Text, 0.82f),
						 72.0f * Scale,
						 0);
	}
	UI()->ClipDisable();

	UI()->ClipEnable(&MapCanvas);
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		if(!aNodeVisible[ID])
			continue;
		const CPveCardDef *pDef = PveCardDef(ID);
		const CUIRect Anchor = aNodeRects[ID];
		CUIRect InputRect = Anchor;
		InputRect.Margin(-3.0f * Scale, &InputRect);
		if(UI()->DoButtonLogic(&m_aNodeButtonIDs[ID], &InputRect))
		{
			m_SelectedResearch = ID;
			m_ResearchBranch = pDef->m_Branch;
			m_ResearchRoute = PveResearchRoute(pDef);
			m_SelectionPulse = 0.7f;
		}
	}
	UI()->ClipDisable();

	const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
	if(pSelected)
	{
		Details.Margin((Compact ? 7.0f : 9.0f) * LayoutScale, &Details);
		const bool Bought = PveCardIsUnlocked(m_SelectedResearch, Mask);
		const bool Available = CanBuyResearch(m_SelectedResearch, Mask);
		const vec4 StateColor = Bought ? PurchasedColor : (Available ? AvailableColor : LockedColor);
		char aState[48];
		str_format(aState,
				   sizeof(aState),
				   "%s  %s",
				   Bought ? "✓" : (Available ? "+" : "×"),
				   Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")));
		CUIRect DetailHeader = {Details.x, Details.y, Details.w, (Compact ? 20.0f : 23.0f) * Scale};
		ResearchText(DetailHeader.x + 2.0f * Scale,
					 DetailHeader.y + 2.8f * Scale,
					 9.1f * Scale,
					 Localize("Research Details"),
					 Fade(Text, 0.96f));
		CUIRect DetailState = {DetailHeader.x + DetailHeader.w - 112.0f * Scale,
							   DetailHeader.y,
							   112.0f * Scale,
							   (Compact ? 20.0f : 23.0f) * Scale};
		DrawPanel(DetailState, Fade(Inset, 0.92f), 8.0f * Scale);
		CUIRect DetailStateEdge = {
			DetailState.x, DetailState.y + 5.0f * Scale, 2.0f * Scale, DetailState.h - 10.0f * Scale};
		DrawPanel(DetailStateEdge, Fade(StateColor, (0.82f + 0.18f * WaveFast)), 1.0f * Scale);
		ResearchText(DetailState.x + DetailState.w * 0.5f,
					 DetailState.y + 4.2f * Scale,
					 7.8f * Scale,
					 aState,
					 Fade(StateColor, 1.0f),
					 -1.0f,
					 0);

		CUIRect Hero = {Details.x,
						DetailHeader.y + DetailHeader.h + (Compact ? 4.0f : 6.0f) * Scale,
						Details.w,
						(Compact ? 60.0f : 74.0f) * Scale};
		DrawPanel(Hero, Fade(Inset, 0.94f), 8.0f * Scale);
		CUIRect HeroEdge = {Hero.x, Hero.y + 7.0f * Scale, 2.0f * Scale, Hero.h - 14.0f * Scale};
		DrawPanel(HeroEdge, Fade(StateColor, (0.78f + 0.18f * Wave)), 1.0f * Scale);
		CUIRect IconTile = {Hero.x + 10.0f * Scale,
							Hero.y + (Compact ? 10.0f : 12.0f) * Scale,
							(Compact ? 40.0f : 50.0f) * Scale,
							(Compact ? 40.0f : 50.0f) * Scale};
		const float IconPulse = 1.0f + (Available && !Bought ? 0.04f * WaveFast : 0.0f);
		DrawPanel(IconTile, Fade(Deep, 0.86f), 8.0f * Scale);
		DrawSprite(PveCardIcon(pSelected),
				   IconTile.x + IconTile.w * 0.5f,
				   IconTile.y + IconTile.h * 0.5f,
				   (Compact ? 30.0f : 36.0f) * Scale * IconPulse,
				   Text,
				   0.92f);
		const float HeroTextX = IconTile.x + IconTile.w + 10.0f * Scale;
		ResearchText(HeroTextX,
					 Hero.y + 9.0f * Scale,
					 10.7f * Scale,
					 Localize(pSelected->m_pName),
					 Fade(Text, 1.0f),
					 Hero.x + Hero.w - HeroTextX - 9.0f * Scale,
					 -1);
		char aMeta[96];
		str_format(aMeta,
				   sizeof(aMeta),
				   Localize("%s • TIER %d • COST %d"),
				   Localize(PveRarityName(pSelected->m_Rarity)),
				   pSelected->m_Tier,
				   pSelected->m_ResearchCost);
		ResearchText(HeroTextX,
					 Hero.y + (Compact ? 37.0f : 46.8f) * Scale,
					 8.2f * Scale,
					 aMeta,
					 Fade(Accent, 0.96f),
					 Hero.x + Hero.w - HeroTextX - 9.0f * Scale,
					 -1);

		float SectionY = Hero.y + Hero.h + (Compact ? 4.0f : 7.0f) * Scale;
		ResearchText(Details.x + 2.0f * Scale, SectionY, 8.9f * Scale, Localize("Effect"), Fade(Accent, 1.0f));
		CUIRect Effect = {
			Details.x, SectionY + (Compact ? 13.0f : 15.0f) * Scale, Details.w, (Compact ? 50.0f : 68.0f) * Scale};
		DrawPanel(Effect, Fade(Inset, 0.74f), 7.0f * Scale);
		ResearchWrapped(Effect.x + 10.0f * Scale,
						Effect.y + 7.5f * Scale,
						8.7f * Scale,
						Localize(pSelected->m_pDescription),
						vec4(MutedText.r, MutedText.g, MutedText.b, MutedText.a * 0.65f),
						Effect.w - 20.0f * Scale,
						5);
		SectionY = Effect.y + Effect.h + (Compact ? 4.0f : 7.0f) * Scale;
		CUIRect Rules = {Details.x, SectionY, Details.w, (Compact ? 42.0f : 50.0f) * Scale};
		DrawPanel(Rules, Fade(Deep, 0.58f), 7.0f * Scale);
		char aStackRule[64];
		if(pSelected->m_MaxStacks == 1)
			str_copy(aStackRule, Localize("Unique perk"), sizeof(aStackRule));
		else
			str_format(aStackRule, sizeof(aStackRule), Localize("Stack limit: %d"), pSelected->m_MaxStacks);
		ResearchText(Rules.x + 10.0f * Scale,
					 Rules.y + 5.8f * Scale,
					 8.4f * Scale,
					 aStackRule,
					 Fade(Accent, 0.96f),
					 Rules.w - 20.0f * Scale,
					 -1);
		char aPrerequisite[128];
		if(pSelected->m_NumPrerequisites > 0)
		{
			str_copy(aPrerequisite, Localize("Requires:"), sizeof(aPrerequisite));
			str_append(aPrerequisite, " ", sizeof(aPrerequisite));
			for(int i = 0; i < pSelected->m_NumPrerequisites; i++)
			{
				if(i > 0)
					str_append(aPrerequisite, ", ", sizeof(aPrerequisite));
				str_append(aPrerequisite,
						   Localize(PveCardDef(pSelected->m_aPrerequisites[i])->m_pName),
						   sizeof(aPrerequisite));
			}
		}
		else
			str_copy(aPrerequisite, Localize("No prerequisite"), sizeof(aPrerequisite));
		ResearchText(Rules.x + 10.0f * Scale,
					 Rules.y + (Compact ? 21.5f : 25.8f) * Scale,
					 8.2f * Scale,
					 aPrerequisite,
					 Fade(Available || Bought ? Accent : Danger, 0.94f),
					 Rules.w - 20.0f * Scale,
					 -1);
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
			ResearchText(Progress.x + 10.0f * Scale,
						 Progress.y + 4.0f * Scale,
						 8.3f * Scale,
						 Localize("Research Progress"),
						 Fade(Accent, 0.96f));
		char aProgress[32];
		str_format(aProgress, sizeof(aProgress), "%d / %d", UnlockedResearch, NUM_PVE_RESEARCH_CARDS);
		if(ShowProgress)
			ResearchText(Progress.x + Progress.w - 10.0f * Scale,
						 Progress.y + 4.0f * Scale,
						 8.2f * Scale,
						 aProgress,
						 Fade(Text, 0.86f),
						 -1.0f,
						 1);
		CUIRect ProgressBar = {Progress.x + 10.0f * Scale,
							   Progress.y + Progress.h - 8.0f * Scale,
							   Progress.w - 20.0f * Scale,
							   4.0f * Scale};
		if(ShowProgress)
			DrawPanel(ProgressBar, Fade(Deep, 0.78f), 2.0f * Scale);
		if(ShowProgress && m_ResearchProgressDisplay > 0.001f)
		{
			CUIRect ProgressFill = ProgressBar;
			ProgressFill.w *= clamp(m_ResearchProgressDisplay, 0.0f, 1.0f);
			DrawPanel(ProgressFill, Fade(Accent, (0.82f + 0.14f * Wave)), 2.0f * Scale);
			if(ProgressFill.w > 4.0f * Scale)
			{
				CUIRect ProgressTip = {ProgressFill.x + ProgressFill.w - 3.0f * Scale,
									   ProgressFill.y - Scale,
									   3.0f * Scale,
									   ProgressFill.h + 2.0f * Scale};
				DrawPanel(ProgressTip, Fade(Text, (0.35f + 0.35f * WaveFast)), Scale);
			}
		}

		const float BuyHover = Available ? m_pClient->m_pMenus->AnimHover(&m_BuyButtonID) : 0.0f;
		const float BuyPulse = Available ? (0.82f + 0.18f * WaveFast) : 1.0f;
		CUIRect BuyBorder = Buy;
		BuyBorder.Margin((-1.2f - BuyHover * 0.8f) * LayoutScale, &BuyBorder);
		DrawPanel(BuyBorder,
				  Fade(Available ? Accent : (Bought ? AccentDim : Danger), (Available ? 0.90f * BuyPulse : 0.34f)),
				  BuyBorder.h * 0.5f);
		DrawPanel(Buy,
				  Fade(Available ? (BuyHover > 0.35f ? AccentDim : Accent) : Inset, (Available ? 0.96f : 0.76f)),
				  Buy.h * 0.5f);
		const bool PurchaseRejected = m_ValidationCode && time_get() < m_ValidationUntil;
		ResearchText(Buy.x + Buy.w * 0.5f,
					 Buy.y + (Compact ? 4.3f : 5.5f) * Scale,
					 (Compact ? 10.5f : 11.4f) * Scale,
					 Localize(PurchaseRejected
								  ? "Purchase rejected"
								  : (Bought ? "Unlocked for future choices" : (Available ? "Purchase" : "Locked"))),
					 Fade(PurchaseRejected ? Danger : Text, 1.0f),
					 -1.0f,
					 0);
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
				ResearchText(Run.x + 10.0f * Scale,
							 Run.y + 4.8f * Scale,
							 8.4f * Scale,
							 Localize("Current Run"),
							 Fade(Accent, 1.0f));
				char aCount[24];
				str_format(aCount, sizeof(aCount), "%d", TotalPerks);
				CUIRect CountBadge = {
					Run.x + Run.w - 30.0f * Scale, Run.y + 5.0f * Scale, 20.0f * Scale, 16.0f * Scale};
				DrawPanel(CountBadge, Fade(Deep, 0.80f), 7.0f * Scale);
				ResearchText(CountBadge.x + CountBadge.w * 0.5f,
							 CountBadge.y + 2.6f * Scale,
							 7.6f * Scale,
							 aCount,
							 Fade(Text, 0.92f),
							 -1.0f,
							 0);
				const int MaxLines = max(1, min(10, (int)((Run.h - 29.0f * Scale) / (13.0f * Scale))));
				float Y = Run.y + 26.0f * Scale;
				if(TotalPerks == 0)
					ResearchText(Run.x + 10.0f * Scale,
								 Y,
								 7.8f * Scale,
								 Localize("No perks selected"),
								 Fade(Text, 0.62f),
								 Run.w - 20.0f * Scale,
								 -1);
				for(int ID = 0; ID < NUM_PVE_CARDS && PerkLines < MaxLines; ID++)
					if(m_aRunPerks[ID] > 0)
					{
						if(PerkLines == MaxLines - 1 && TotalPerks > MaxLines)
						{
							char aMore[64];
							str_format(aMore, sizeof(aMore), Localize("+%d more perks"), TotalPerks - PerkLines);
							ResearchText(Run.x + 10.0f * Scale,
										 Y,
										 7.7f * Scale,
										 aMore,
										 Fade(Accent, 0.92f),
										 Run.w - 20.0f * Scale,
										 -1);
							break;
						}
						char aPerk[96];
						str_format(aPerk, sizeof(aPerk), "%s ×%d", Localize(PveCardDef(ID)->m_pName), m_aRunPerks[ID]);
						ResearchText(Run.x + 10.0f * Scale,
									 Y,
									 7.8f * Scale,
									 aPerk,
									 Fade(Text, 0.82f),
									 Run.w - 20.0f * Scale,
									 -1);
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
				ResearchText(Guide.x + 10.0f * Scale,
							 Guide.y + 4.3f * Scale,
							 8.4f * Scale,
							 Localize("How Research Works"),
							 Fade(Accent, 1.0f));
				ResearchText(Guide.x + 10.0f * Scale,
							 Guide.y + 19.5f * Scale,
							 7.6f * Scale,
							 Localize("Earn points from PvE stages and contracts."),
							 Fade(Text, 0.82f),
							 Guide.w - 20.0f * Scale,
							 -1);
				ResearchText(Guide.x + 10.0f * Scale,
							 Guide.y + 31.5f * Scale,
							 7.6f * Scale,
							 Localize("Unlock connected nodes in order."),
							 Fade(Text, 0.82f),
							 Guide.w - 20.0f * Scale,
							 -1);
				ResearchText(Guide.x + 10.0f * Scale,
							 Guide.y + 43.5f * Scale,
							 7.6f * Scale,
							 Localize("Unlocked cards join future perk choices."),
							 Fade(Text, 0.82f),
							 Guide.w - 20.0f * Scale,
							 -1);
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
	if(m_DebugGameScreenshotFrames > 0 && time_get() >= m_DebugGameScreenshotEarliestTime &&
	   Client()->State() == IClient::STATE_ONLINE && m_pClient->m_Snap.m_pLocalCharacter)
	{
		if(--m_DebugGameScreenshotFrames == 0)
		{
			Graphics()->TakeScreenshot(0);
			m_DebugGameScreenshotEarliestTime = 0;
		}
	}
	if(m_pClient->m_pMenus->IsResearchPageActive())
		return;

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
	if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialState == 1 && m_TutorialNonce <= 0 &&
	   (Event.m_Flags & IInput::FLAG_PRESS))
	{
		const char *pBind = m_pClient->m_pBinds->Get(Event.m_Key);
		const int Checkpoint = g_Config.m_ClTutorialCheckpoint;
		if(Checkpoint == 0)
		{
			if(str_comp(pBind, "+left") == 0 || str_comp(pBind, "+right") == 0 ||
			   str_comp(pBind, "+gamepadleft") == 0 || str_comp(pBind, "+gamepadright") == 0)
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
		const int Card = Module == PVE_DRONE_ASSAULT
							 ? PVE_CARD_ASSAULT_MODULE
							 : (Module == PVE_DRONE_GUARDIAN ? PVE_CARD_GUARDIAN_MODULE : PVE_CARD_REPAIR_MODULE);
		if(Module != PVE_DRONE_NONE && m_aRunPerks[Card] > 0)
		{
			SendDroneModule(Module);
			return true;
		}
	}
	if(!ChoiceActive() && !m_ResearchVisible)
		return false;
	if(!(Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_REPEAT)))
		return ChoiceActive();
	if(!(Event.m_Flags & IInput::FLAG_PRESS) && !m_ResearchVisible)
		return ChoiceActive();
	if(m_InvasionRetryResultActive)
		return true;
	if(m_InvasionRetryVoteActive)
	{
		int Direction = 0;
		if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT || Event.m_Key == KEY_GAMEPAD_AXIS_LEFT ||
		   Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
			Direction = -1;
		else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT || Event.m_Key == KEY_GAMEPAD_AXIS_RIGHT ||
				Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
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
		else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A ||
				 Event.m_Key == KEY_GAMEPAD_BUTTON_START)
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
		if(Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT && (Event.m_Flags & IInput::FLAG_PRESS))
			m_ResearchTab = (m_ResearchTab + 2) % 3;
		else if(Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT && (Event.m_Flags & IInput::FLAG_PRESS))
			m_ResearchTab = (m_ResearchTab + 1) % 3;
		else if(Event.m_Key == KEY_TAB)
			m_ResearchTab = (m_ResearchTab + 1) % 3;
		else if(m_ResearchTab == PVE_TAB_MODE && (Event.m_Key == KEY_C || Event.m_Key == KEY_GAMEPAD_BUTTON_Y))
			CycleCheckpoint();
		else if((Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A ||
				  Event.m_Key == KEY_GAMEPAD_BUTTON_START) &&
			(Event.m_Flags & IInput::FLAG_PRESS))
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
			if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT ||
				Event.m_Key == KEY_GAMEPAD_AXIS_LEFT)
				TargetTier--;
			else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT ||
					Event.m_Key == KEY_GAMEPAD_AXIS_RIGHT)
				TargetTier++;
			else if(Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP ||
					Event.m_Key == KEY_GAMEPAD_AXIS_UP)
			{
				if(TargetRoute > 0)
					TargetRoute--;
				else
				{
					TargetBranch--;
					TargetRoute = TargetBranch >= 0 ? PveResearchRouteCount(m_ResearchTab, TargetBranch) - 1 : 0;
				}
			}
			else if(Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN ||
					Event.m_Key == KEY_GAMEPAD_AXIS_DOWN)
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
					if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != TargetBranch ||
					   PveResearchRoute(pDef) != TargetRoute)
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
	if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT || Event.m_Key == KEY_GAMEPAD_AXIS_LEFT ||
	   Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
		Direction = -1;
	else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT || Event.m_Key == KEY_GAMEPAD_AXIS_RIGHT ||
			Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
		Direction = 1;
	if(Direction)
		m_FocusedChoice = (m_FocusedChoice + Direction + Count) % Count;
	else if(Event.m_Key == KEY_1 || Event.m_Key == KEY_KP_1)
	{
		m_FocusedChoice = 0;
		if(m_ContractVoteActive)
			SendContractVote(0);
		else
			SendChoice(0);
	}
	else if((Event.m_Key == KEY_2 || Event.m_Key == KEY_KP_2) && Count >= 2)
	{
		m_FocusedChoice = 1;
		if(m_ContractVoteActive)
			SendContractVote(1);
		else
			SendChoice(1);
	}
	else if((Event.m_Key == KEY_3 || Event.m_Key == KEY_KP_3) && Count >= 3)
	{
		m_FocusedChoice = 2;
		SendChoice(2);
	}
	else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A ||
			Event.m_Key == KEY_GAMEPAD_BUTTON_START)
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
		if(g_Config.m_ClTutorialActive && m_TutorialNonce <= 0 && g_Config.m_ClTutorialState == 1 &&
		   g_Config.m_ClTutorialCheckpoint == 2 && pMsg->m_Killer == m_pClient->m_Snap.m_LocalClientID)
			if(++m_TutorialKillCount >= 3)
				AdvanceTutorial();
	}
	else if(MsgType == NETMSGTYPE_SV_FORGERESULT)
	{
		const CNetMsg_Sv_ForgeResult *pMsg = (const CNetMsg_Sv_ForgeResult *)pRawMsg;
		if(g_Config.m_ClTutorialActive && m_TutorialNonce <= 0 && g_Config.m_ClTutorialState == 1 &&
		   g_Config.m_ClTutorialCheckpoint == 4 && pMsg->m_Result == FORGERESULT_SUCCESS)
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
		const unsigned long long Low =
			(unsigned int)pMsg->m_ResearchMask0 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask1 << 32);
		const unsigned long long High =
			(unsigned int)pMsg->m_ResearchMask2 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask3 << 32);
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
		const bool NewChoice =
			!m_ChoiceActive || m_ChoiceNonce != pMsg->m_Nonce || m_ChoiceSequence != pMsg->m_ChoiceSequence;
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
			str_format(aBuf,
					   sizeof(aBuf),
					   "offer sequence=%d nonce=%d cards=%d/%d/%d active=%d new=%d",
					   pMsg->m_ChoiceSequence,
					   pMsg->m_Nonce,
					   pMsg->m_Card0,
					   pMsg->m_Card1,
					   pMsg->m_Card2,
					   m_ChoiceActive,
					   NewChoice);
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
		const bool NewResult = !m_InvasionRetryResultActive || m_InvasionRetryResult != pMsg->m_Result ||
							   m_InvasionRetryResultEndTick != pMsg->m_EndTick;
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
