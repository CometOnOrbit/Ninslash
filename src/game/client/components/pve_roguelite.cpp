#include <math.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>

#include "pve_roguelite.h"

namespace
{
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
}

CPveRoguelite::CPveRoguelite()
{
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		m_aNodeButtonIDs[i] = i;
	for(int i = 0; i < 3; i++)
		m_aTabButtonIDs[i] = i;
	for(int i = 0; i < 4; i++)
		m_aBranchButtonIDs[i] = i;
	m_BuyButtonID = 0;
	m_CheckpointButtonID = 0;
	m_DebugChoiceScreenshotFrames = 0;
	m_DebugResearchScreenshotFrames = 0;
	m_DebugBuildScreenshotFrames = 0;
	m_DebugScreenshotPage = -1;
	m_DebugBuildPreview = false;
	OnReset();
}

void CPveRoguelite::OnConsoleInit()
{
	Console()->Register("pve_debug_choice", "?i", CFGFLAG_CLIENT, ConDebugChoice, this, "Preview three perk cards from a starting card ID");
	Console()->Register("pve_debug_contract", "?i?i", CFGFLAG_CLIENT, ConDebugContract, this, "Preview a contract ID as vote/active/success/failure (state 0-3)");
	Console()->Register("pve_debug_research", "?i?i", CFGFLAG_CLIENT, ConDebugResearch, this, "Preview and capture research states (0-2) with an optional tab (0-2)");
	Console()->Register("pve_debug_build", "?i", CFGFLAG_CLIENT, ConDebugBuild, this, "Preview and capture the PvE build HUD with an optional drone module (1-3)");
	Console()->Register("pve_debug_screenshot", "?i", CFGFLAG_CLIENT, ConDebugScreenshot, this, "Capture the current client UI after initialization, optionally forcing a UI page");
	Console()->Register("pve_drone_module", "i", CFGFLAG_CLIENT, ConDroneModule, this, "Switch support drone module: 1 assault, 2 guardian, 3 repair");
}

void CPveRoguelite::ConDroneModule(IConsole::IResult *pResult, void *pUserData)
{
	((CPveRoguelite *)pUserData)->SendDroneModule(pResult->GetInteger(0));
}

void CPveRoguelite::ConDebugChoice(IConsole::IResult *pResult, void *pUserData)
{
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	const int Start = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, NUM_PVE_CARDS - 1) : 0;
	pSelf->m_ContractVoteActive = false;
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
	(void)pResult;
	CPveRoguelite *pSelf = (CPveRoguelite *)pUserData;
	pSelf->m_DebugBuildPreview = false;
	pSelf->m_DebugBuildScreenshotFrames = 12;
	pSelf->m_DebugScreenshotPage = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 1, 14) : -1;
}

void CPveRoguelite::OnReset()
{
	if(m_DebugChoiceScreenshotFrames <= 0)
	{
		m_ChoiceActive = false;
		m_ChoiceNonce = 0;
		m_ChoiceSequence = 0;
		m_ChoiceEndTick = 0;
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
	m_ValidationCode = 0;
	m_ValidationUntil = 0;
	if(m_DebugResearchScreenshotFrames <= 0)
	{
		m_SelectedResearch = PVE_CARD_FINISHER;
		m_ResearchTab = PVE_TAB_CORE;
		m_ResearchBranch = 0;
	}
	m_ResearchNonce = 1;
	if(m_DebugResearchScreenshotFrames <= 0)
		for(int i = 0; i < NUM_PVE_CARDS; i++)
			m_aRunPerks[i] = 0;
	m_SelectorMouse = vec2(150.0f, 150.0f);
	m_AppearAmount = 0.0f;
	m_ResearchAppearAmount = 0.0f;
	m_SelectionPulse = 0.0f;
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
	const float Alpha = clamp(m_AppearAmount, 0.0f, 1.0f);
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();

	CUIRect Screen = {0, 0, ScreenWidth, 300.0f};
	DrawPanel(Screen, vec4(Deep.r, Deep.g, Deep.b, 0.94f * Alpha), 0.0f);
	CUIRect Stage = {8.0f, 42.0f, ScreenWidth - 16.0f, 218.0f};
	DrawPanel(Stage, vec4(Inset.r, Inset.g, Inset.b, 0.96f * Alpha), 13.0f);
	CUIRect Line = {22.0f, 55.0f, ScreenWidth - 44.0f, 1.2f};
	DrawPanel(Line, vec4(Accent.r, Accent.g, Accent.b, 0.65f * Alpha), 0.6f);
	DrawText(ScreenWidth * 0.5f, 10.0f, 12.0f, Localize(ContractVote ? "Team Contract" : "Choose a Perk"), vec4(Text.r, Text.g, Text.b, Alpha), -1.0f, 0);

	const int EndTick = ContractVote ? m_ContractEndTick : m_ChoiceEndTick;
	const int Seconds = max(0, (EndTick - Client()->GameTick() + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
	char aTimer[64];
	str_format(aTimer, sizeof(aTimer), Localize("%d seconds remaining"), Seconds);
	const vec4 TimerColor = Seconds <= 3 ? Danger : Accent;
	CUIRect Timer = {ScreenWidth * 0.5f - 48.0f, 27.0f, 96.0f, 12.0f};
	DrawPanel(Timer, vec4(Panel.r, Panel.g, Panel.b, 0.96f * Alpha), 6.0f);
	DrawText(ScreenWidth * 0.5f, 29.3f, 6.4f, aTimer, vec4(TimerColor.r, TimerColor.g, TimerColor.b, Alpha), -1.0f, 0);

	const int Count = ContractVote ? 2 : 3;
	const float Gap = ContractVote ? 16.0f : 8.0f;
	const float MaxCardWidth = ContractVote ? 160.0f : 105.0f;
	const float CardWidth = min(MaxCardWidth, (Stage.w - 24.0f - Gap * (Count - 1)) / Count);
	const float TotalWidth = CardWidth * Count + Gap * (Count - 1);
	const float StartX = ScreenWidth * 0.5f - TotalWidth * 0.5f;
	int Hovered = -1;
	for(int i = 0; i < Count; i++)
	{
		CUIRect Hit = {StartX + i * (CardWidth + Gap), 66.0f, CardWidth, 178.0f};
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
		const bool Selected = ContractVote ? i == m_SelectedContract : (m_ChoiceNonce == 0 && i == m_FocusedChoice);
		const int ID = ContractVote ? m_aContractOptions[i] : m_aChoiceCards[i];
		const CPveContractDef *pContract = ContractVote ? PveContractDef(ID) : 0;
		const CPveCardDef *pCard = ContractVote ? 0 : PveCardDef(ID);
		vec4 CategoryColor = AccentDim;
		if(ContractVote || (pCard && pCard->m_Rarity == PVE_RARITY_RARE))
			CategoryColor = Accent;
		else if(pCard && pCard->m_Rarity == PVE_RARITY_EPIC)
			CategoryColor = vec4((Accent.r + Danger.r) * 0.5f, (Accent.g + Danger.g) * 0.5f, (Accent.b + Danger.b) * 0.5f, 1.0f);
		const float Scale = 1.0f + FocusAmount * 0.035f + (Selected ? m_SelectionPulse * 0.018f : 0.0f);
		CUIRect Card = {StartX + i * (CardWidth + Gap) - CardWidth * (Scale - 1.0f) * 0.5f,
			66.0f - FocusAmount * 3.0f, CardWidth * Scale, 178.0f * Scale};
		CUIRect Border = Card;
		Border.Margin(-1.4f, &Border);
		const vec4 BorderColor = Selected || Focused ? CategoryColor : AccentDim;
		DrawPanel(Border, vec4(BorderColor.r, BorderColor.g, BorderColor.b, (Focused ? 0.85f : 0.35f) * Alpha), 11.0f);
		DrawPanel(Card, vec4(Panel.r, Panel.g, Panel.b, 0.98f * Alpha), 9.0f);
		const char *pName = Localize(ContractVote ? (pContract ? pContract->m_pName : "Unknown contract") : PveChoiceName(ID));
		const char *pDescription = Localize(ContractVote ? (pContract ? pContract->m_pRule : "") : PveChoiceDescription(ID));

		CUIRect Badge = {Card.x + 8.0f, Card.y + 8.0f, Card.w - 16.0f, 14.0f};
		DrawPanel(Badge, vec4(Inset.r, Inset.g, Inset.b, 0.96f * Alpha), 7.0f);
		char aBadge[64];
		if(ContractVote)
			str_format(aBadge, sizeof(aBadge), Localize("RISK • %d VOTES"), m_aContractVotes[i]);
		else if(pCard)
			str_format(aBadge, sizeof(aBadge), Localize("%s • LEVEL %d/%d"), Localize(PveRarityName(pCard->m_Rarity)), m_aChoiceStacks[i], pCard->m_MaxStacks);
		else
			str_copy(aBadge, Localize("Immediate Supply"), sizeof(aBadge));
		DrawText(Card.x + Card.w * 0.5f, Card.y + 11.2f, 5.6f, aBadge, vec4(CategoryColor.r, CategoryColor.g, CategoryColor.b, Alpha), -1.0f, 0);
		float NameSize = Focused ? 9.2f : 8.5f;
		while(NameSize > 6.0f && TextRender()->TextWidth(0, NameSize, pName, -1) > Card.w - 16.0f)
			NameSize -= 0.3f;
		DrawText(Card.x + Card.w * 0.5f, Card.y + 32.0f, NameSize, pName, vec4(Text.r, Text.g, Text.b, Alpha), -1.0f, 0);
		const CPveUiIcon Icon = ContractVote ? CPveUiIcon() : PveChoiceIcon(ID, pCard);
		DrawIcon(Icon.m_Image, Icon.m_Sprite, Card.x + Card.w * 0.5f, Card.y + 61.0f, (Focused ? 24.0f : 21.0f) * Icon.m_Scale,
			vec4(Text.r, Text.g, Text.b, 0.82f * Alpha));
		DrawWrappedText(Card.x + 10.0f, Card.y + 78.0f, 6.2f, pDescription, vec4(Text.r, Text.g, Text.b, 0.78f * Alpha), Card.w - 20.0f, 4);
		if(ContractVote && pContract)
		{
			DrawText(Card.x + 10.0f, Card.y + 121.0f, 5.8f, Localize(pContract->m_pRisk), vec4(Danger.r, Danger.g, Danger.b, Alpha), Card.w - 20.0f, -1);
			DrawText(Card.x + 10.0f, Card.y + 140.0f, 5.8f, Localize("Reward: 1 Research Point"), vec4(Accent.r, Accent.g, Accent.b, Alpha), Card.w - 20.0f, -1);
		}
		else if(!ContractVote)
		{
			char aResult[80];
			if(pCard)
				str_format(aResult, sizeof(aResult), Localize("After selection: level %d/%d"), min(pCard->m_MaxStacks, m_aChoiceStacks[i] + 1), pCard->m_MaxStacks);
			else
				str_copy(aResult, Localize("Immediate effect"), sizeof(aResult));
			DrawText(Card.x + 10.0f, Card.y + 140.0f, 5.8f, aResult, vec4(Accent.r, Accent.g, Accent.b, Alpha), Card.w - 20.0f, -1);
		}
		CUIRect Button = {Card.x + 10.0f, Card.y + Card.h - 27.0f, Card.w - 20.0f, 18.0f};
		DrawPanel(Button, vec4((Focused ? Accent : Inset).r, (Focused ? Accent : Inset).g, (Focused ? Accent : Inset).b, 0.95f * Alpha), 7.0f);
		DrawText(Button.x + Button.w * 0.5f, Button.y + 5.1f, 6.5f, Localize(Selected ? (ContractVote ? "Voted" : "Selected") : "Select"), vec4(Text.r, Text.g, Text.b, Alpha), -1.0f, 0);
	}

	DrawText(ScreenWidth * 0.5f, 274.0f, 6.5f, Localize("Mouse • Arrow Keys • 1–3 • Gamepad"), vec4(Text.r, Text.g, Text.b, 0.65f * Alpha), -1.0f, 0);
	if(m_ValidationCode && time_get() < m_ValidationUntil)
		DrawText(ScreenWidth * 0.5f, 287.0f, 6.0f, Localize("The server rejected that selection."), vec4(Danger.r, Danger.g, Danger.b, Alpha), -1.0f, 0);

	Graphics()->TextureSet(-1);
	CUIRect Cursor = {m_SelectorMouse.x, m_SelectorMouse.y, 5.0f, 5.0f};
	DrawPanel(Cursor, vec4(Accent.r, Accent.g, Accent.b, Alpha), 2.5f);
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
	char aaLines[10][96];
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

void CPveRoguelite::RenderBuildDebug()
{
	if(m_DebugBuildPreview)
		DrawBuildHud();
}

bool CPveRoguelite::CanBuyResearch(int CardID, const CPveResearchMask &Mask) const
{
	const CPveCardDef *pDef = PveCardDef(CardID);
	return pDef && !pDef->m_Base && !PveCardIsUnlocked(CardID, Mask) && g_Config.m_ClPveResearchPoints >= pDef->m_ResearchCost &&
		Mask.PrerequisitesMet(CardID);
}

void CPveRoguelite::BuySelectedResearch()
{
	CPveResearchMask Mask = ParseResearchMask();
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
	if(Client()->State() == IClient::STATE_ONLINE && m_ProgressSent)
		SyncProgress();
}

void CPveRoguelite::RenderResearch(CUIRect MainView)
{
	m_ResearchVisible = true;
	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 4.0f);
	m_ResearchAppearAmount += (1.0f - m_ResearchAppearAmount) * (1.0f - expf(-11.0f * Dt));
	const float Alpha = clamp(m_ResearchAppearAmount, 0.0f, 1.0f);
	const float Scale = clamp(min(MainView.w / 780.0f, MainView.h / 520.0f), 0.82f, 1.08f);
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

	MainView.y += (1.0f - Alpha) * 6.0f * Scale;
	DrawPanel(MainView, Fade(AccentDim, 0.30f), 12.0f * Scale);
	MainView.Margin(1.4f * Scale, &MainView);
	DrawPanel(MainView, Fade(Deep, 0.99f), 10.5f * Scale);

	CUIRect Header, Body;
	MainView.Margin(8.0f * Scale, &MainView);
	MainView.HSplitTop(110.0f * Scale, &Header, &Body);
	CUIRect HeaderShadow = Header;
	HeaderShadow.y += 2.0f * Scale;
	DrawPanel(HeaderShadow, Fade(Deep, 0.62f), 10.0f * Scale);
	DrawPanel(Header, Fade(Panel, 0.98f), 9.0f * Scale);
	CUIRect HeaderEdge = {Header.x + 12.0f * Scale, Header.y + Header.h - 1.5f * Scale, Header.w - 24.0f * Scale, 1.5f * Scale};
	DrawPanel(HeaderEdge, Fade(Accent, 0.68f), 0.75f * Scale);
	CUIRect TitleMark = {Header.x + 12.0f * Scale, Header.y + 8.0f * Scale, 3.0f * Scale, 23.0f * Scale};
	DrawPanel(TitleMark, Fade(Accent, 0.95f), 1.5f * Scale);
	DrawText(Header.x + 23.0f * Scale, Header.y + 6.0f * Scale, 13.0f * Scale, Localize("Research"), Fade(Text, 1.0f));

	char aPoints[64];
	str_format(aPoints, sizeof(aPoints), Localize("%d Research Points"), g_Config.m_ClPveResearchPoints);
	const float PointsWidth = clamp(Header.w * 0.21f, 142.0f * Scale, 178.0f * Scale);
	CUIRect Points = {Header.x + Header.w - PointsWidth - 10.0f * Scale, Header.y + 7.0f * Scale, PointsWidth, 30.0f * Scale};
	DrawPanel(Points, Fade(AccentDim, 0.42f), 15.0f * Scale);
	CUIRect PointsInner = Points;
	PointsInner.Margin(1.2f * Scale, &PointsInner);
	DrawPanel(PointsInner, Fade(Inset, 0.98f), 14.0f * Scale);
	DrawSprite(CPveUiIcon(IMAGE_WEAPONS, SPRITE_PICKUP_BIGCOIN), Points.x + 17.0f * Scale, Points.y + Points.h * 0.5f, 15.0f * Scale, Accent, 1.0f);
	DrawText(Points.x + Points.w * 0.57f, Points.y + 7.0f * Scale, 9.2f * Scale, aPoints, Fade(Accent, 1.0f), -1.0f, 0);

	DrawText(Header.x + 13.0f * Scale, Header.y + 35.0f * Scale, 9.1f * Scale,
		Localize("Research unlocks perk cards; select them during a run to activate their effects."), Fade(Text, 0.92f), Header.w - 26.0f * Scale, -1);
	DrawText(Header.x + 13.0f * Scale, Header.y + 53.5f * Scale, 8.3f * Scale,
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
		CUIRect TabRect = {TabsX + Tab * (TabWidth + TabGap), Header.y + 75.0f * Scale, TabWidth, 27.0f * Scale};
		const bool Selected = Tab == m_ResearchTab;
		const bool Hovered = UI()->HotItem() == &m_aTabButtonIDs[Tab];
		CUIRect TabBorder = TabRect;
		TabBorder.Margin(-1.0f * Scale, &TabBorder);
		DrawPanel(TabBorder, Fade(Selected || Hovered ? Accent : AccentDim, Selected ? 0.78f : (Hovered ? 0.52f : 0.18f)), 7.0f * Scale);
		DrawPanel(TabRect, Fade(Selected ? AccentDim : Inset, Selected ? 0.62f : 0.96f), 6.0f * Scale);
		DrawSprite(aTabIcons[Tab], TabRect.x + 18.0f * Scale, TabRect.y + TabRect.h * 0.5f, 14.0f * Scale, Selected ? Accent : Text, Selected ? 1.0f : 0.62f);
		DrawText(TabRect.x + TabRect.w * 0.54f, TabRect.y + 5.8f * Scale, 9.2f * Scale, apTabs[Tab], Fade(Selected ? Text : AccentDim, 1.0f), -1.0f, 0);
		if(Selected)
		{
			CUIRect SelectedLine = {TabRect.x + 10.0f * Scale, TabRect.y + TabRect.h - 1.5f * Scale, TabRect.w - 20.0f * Scale, 1.5f * Scale};
			DrawPanel(SelectedLine, Fade(Accent, 1.0f), 0.75f * Scale);
		}
		if(UI()->DoButtonLogic(&m_aTabButtonIDs[Tab], &TabRect))
		{
			m_ResearchTab = Tab;
			m_ResearchBranch = 0;
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
		CUIRect Checkpoint = {Points.x - CheckpointWidth - 7.0f * Scale, Header.y + 7.0f * Scale, CheckpointWidth, 30.0f * Scale};
		const bool Hovered = UI()->HotItem() == &m_CheckpointButtonID;
		DrawPanel(Checkpoint, Fade(Hovered ? AccentDim : Inset, 0.98f), 8.0f * Scale);
		char aCheckpoint[64];
		str_format(aCheckpoint, sizeof(aCheckpoint), Localize("Checkpoint %d"), g_Config.m_ClPvePreferredCheckpoint);
		DrawText(Checkpoint.x + Checkpoint.w * 0.5f, Checkpoint.y + 6.9f * Scale, 8.6f * Scale, aCheckpoint, Fade(Accent, 1.0f), -1.0f, 0);
		if(UI()->DoButtonLogic(&m_CheckpointButtonID, &Checkpoint))
			CycleCheckpoint();
	}

	Body.HSplitTop(8.0f * Scale, 0, &Body);
	CUIRect Tree, Details;
	const float DetailWidth = clamp(Body.w * 0.32f, 250.0f * Scale, 294.0f * Scale);
	Body.VSplitRight(DetailWidth, &Tree, &Details);
	Tree.VSplitRight(8.0f * Scale, &Tree, 0);
	DrawPanel(Tree, Fade(Panel, 0.96f), 9.0f * Scale);
	DrawPanel(Details, Fade(Panel, 0.96f), 9.0f * Scale);
	const char *apTabDescriptions[3] = {
		Localize("Core research improves universal attack, survival, and logistics perks."),
		Localize("Weapon research unlocks specialization perks matched to your current weapon."),
		Localize("Mode research unlocks perks that appear only in the matching PvE mode.")};
	CUIRect TreeIntro = {Tree.x + 8.0f * Scale, Tree.y + 7.0f * Scale, Tree.w - 16.0f * Scale, 39.0f * Scale};
	DrawPanel(TreeIntro, Fade(Inset, 0.84f), 7.0f * Scale);
	CUIRect TreeIntroMark = {TreeIntro.x, TreeIntro.y + 7.0f * Scale, 2.0f * Scale, TreeIntro.h - 14.0f * Scale};
	DrawPanel(TreeIntroMark, Fade(Accent, 0.88f), 1.0f * Scale);
	DrawText(TreeIntro.x + 10.0f * Scale, TreeIntro.y + 4.0f * Scale, 9.0f * Scale, apTabs[m_ResearchTab], Fade(Text, 1.0f));
	DrawText(TreeIntro.x + 10.0f * Scale, TreeIntro.y + 19.5f * Scale, 8.2f * Scale, apTabDescriptions[m_ResearchTab], Fade(Text, 0.80f), TreeIntro.w - 20.0f * Scale, -1);
	CUIRect BranchArea = {Tree.x, Tree.y + 48.0f * Scale, Tree.w, Tree.h - 48.0f * Scale};
	int BranchCount = 0;
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
		if(!PveCardIsBase(ID) && PveCardDef(ID)->m_Tab == m_ResearchTab)
			BranchCount = max(BranchCount, PveCardDef(ID)->m_Branch + 1);
	BranchCount = max(1, BranchCount);
	m_ResearchBranch = clamp(m_ResearchBranch, 0, BranchCount - 1);
	const CPveResearchMask Mask = ParseResearchMask();
	const float BranchGap = 5.0f * Scale;
	const float BranchHeaderHeight = 31.0f * Scale;
	const float ExpandedHeight = max(BranchHeaderHeight,
		BranchArea.h - 8.0f * Scale - (BranchCount - 1) * (BranchHeaderHeight + BranchGap));
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
		const float BranchHeight = Expanded ? ExpandedHeight : BranchHeaderHeight;
		CUIRect BranchPanel = {BranchArea.x + 8.0f * Scale, BranchY, BranchArea.w - 16.0f * Scale, BranchHeight};
		DrawPanel(BranchPanel, Fade(Expanded ? Inset : Deep, Expanded ? 0.84f : 0.68f), 8.0f * Scale);
		CUIRect BranchHeader = {BranchPanel.x, BranchPanel.y, BranchPanel.w, BranchHeaderHeight};
		if(Expanded)
		{
			CUIRect HeaderLine = {BranchHeader.x + 8.0f * Scale, BranchHeader.y + BranchHeader.h - 1.5f * Scale, BranchHeader.w - 16.0f * Scale, 1.5f * Scale};
			DrawPanel(HeaderLine, Fade(Accent, 0.92f), 0.75f * Scale);
		}
		DrawSprite(PveBranchIcon(m_ResearchTab, Branch), BranchHeader.x + 17.0f * Scale,
			BranchHeader.y + BranchHeader.h * 0.5f, 15.0f * Scale, Expanded ? Accent : AccentDim, Expanded ? 1.0f : 0.72f);
		DrawText(BranchHeader.x + 34.0f * Scale, BranchHeader.y + 7.0f * Scale, 9.0f * Scale,
			Localize(pBranchName), Fade(Text, Expanded ? 1.0f : 0.78f));
		char aBranchProgress[32];
		str_format(aBranchProgress, sizeof(aBranchProgress), "%d / %d", BoughtCount, Count);
		DrawText(BranchHeader.x + BranchHeader.w - 34.0f * Scale, BranchHeader.y + 7.0f * Scale,
			8.0f * Scale, aBranchProgress, Fade(Accent, 0.90f), -1.0f, 1);
		DrawText(BranchHeader.x + BranchHeader.w - 13.0f * Scale, BranchHeader.y + 6.2f * Scale,
			9.0f * Scale, Expanded ? "−" : "+", Fade(Expanded ? Accent : Text, 0.90f), -1.0f, 0);
		if(UI()->DoButtonLogic(&m_aBranchButtonIDs[Branch], &BranchHeader))
		{
			m_ResearchBranch = Branch;
			int Best = aNodes[0];
			for(int n = 1; n < Count; n++)
				if(PveCardDef(aNodes[n])->m_Tier < PveCardDef(Best)->m_Tier)
					Best = aNodes[n];
			m_SelectedResearch = Best;
		}

		if(!Expanded)
		{
			BranchY += BranchHeight + BranchGap;
			continue;
		}

		CUIRect NodeArea = {BranchPanel.x + 7.0f * Scale, BranchHeader.y + BranchHeader.h + 6.0f * Scale,
			BranchPanel.w - 14.0f * Scale, BranchPanel.h - BranchHeader.h - 13.0f * Scale};
		DrawPanel(NodeArea, Fade(Deep, 0.62f), 7.0f * Scale);
		NodeArea.x += 6.0f * Scale;
		NodeArea.w -= 12.0f * Scale;
		NodeArea.y += 5.0f * Scale;
		NodeArea.h -= 10.0f * Scale;
		const int Rows = Count > 5 ? 2 : 1;
		const int Columns = (Count + Rows - 1) / Rows;
		const float NodeGap = 6.0f * Scale;
		const float NodeWidth = min(104.0f * Scale, (NodeArea.w - NodeGap * (Columns - 1)) / Columns);
		const float NodeHeight = min(72.0f * Scale, (NodeArea.h - NodeGap * (Rows - 1)) / Rows);
		auto NodeRect = [&](int Index) {
			const int GridRow = Index / Columns;
			const int GridColumn = Index % Columns;
			const int ItemsInRow = min(Columns, Count - GridRow * Columns);
			const float RowWidth = ItemsInRow * NodeWidth + (ItemsInRow - 1) * NodeGap;
			const float StartX = NodeArea.x + (NodeArea.w - RowWidth) * 0.5f;
			const float UsedHeight = Rows * NodeHeight + (Rows - 1) * NodeGap;
			const float StartY = NodeArea.y + (NodeArea.h - UsedHeight) * 0.5f;
			CUIRect Result = {StartX + GridColumn * (NodeWidth + NodeGap), StartY + GridRow * (NodeHeight + NodeGap), NodeWidth, NodeHeight};
			return Result;
		};
		for(int n = 1; n < Count; n++)
		{
			const CUIRect Previous = NodeRect(n - 1);
			const CUIRect Current = NodeRect(n);
			const vec4 LinkColor = PveCardIsUnlocked(aNodes[n - 1], Mask) ? Accent : AccentDim;
			const float LinkAlpha = PveCardIsUnlocked(aNodes[n - 1], Mask) ? 0.92f : 0.34f;
			if((n - 1) / Columns == n / Columns)
			{
				CUIRect Link = {Previous.x + Previous.w, Previous.y + Previous.h * 0.5f - Scale, Current.x - Previous.x - Previous.w, 2.0f * Scale};
				DrawPanel(Link, Fade(LinkColor, LinkAlpha), Scale);
			}
			else
			{
				const float MidY = (Previous.y + Previous.h + Current.y) * 0.5f;
				CUIRect LinkA = {Previous.x + Previous.w * 0.5f - Scale, Previous.y + Previous.h, 2.0f * Scale, MidY - Previous.y - Previous.h};
				CUIRect LinkB = {min(Previous.x + Previous.w * 0.5f, Current.x + Current.w * 0.5f), MidY - Scale,
					abs(Current.x + Current.w * 0.5f - Previous.x - Previous.w * 0.5f), 2.0f * Scale};
				CUIRect LinkC = {Current.x + Current.w * 0.5f - Scale, MidY, 2.0f * Scale, Current.y - MidY};
				DrawPanel(LinkA, Fade(LinkColor, LinkAlpha), Scale);
				DrawPanel(LinkB, Fade(LinkColor, LinkAlpha), Scale);
				DrawPanel(LinkC, Fade(LinkColor, LinkAlpha), Scale);
			}
		}
		UI()->ClipEnable(&NodeArea);
		for(int n = 0; n < Count; n++)
		{
			const int ID = aNodes[n];
			const CPveCardDef *pDef = PveCardDef(ID);
			CUIRect Node = NodeRect(n);
			const bool Bought = PveCardIsUnlocked(ID, Mask);
			const bool Available = CanBuyResearch(ID, Mask);
			const bool Selected = ID == m_SelectedResearch;
			const bool Hovered = UI()->HotItem() == &m_aNodeButtonIDs[ID];
			CUIRect Shadow = Node;
			Shadow.y += 2.0f * Scale;
			DrawPanel(Shadow, Fade(Deep, 0.72f), 8.0f * Scale);
			CUIRect Border = Node;
			Border.Margin((-1.2f - (Selected ? m_SelectionPulse * 1.2f : 0.0f)) * Scale, &Border);
			const vec4 StateColor = Bought || Available ? Accent : Danger;
			DrawPanel(Border, Fade(Selected || Hovered ? Accent : StateColor, Selected ? 0.98f : (Hovered ? 0.76f : 0.42f)), 8.0f * Scale);
			DrawPanel(Node, Fade(Bought ? Mix(Inset, AccentDim, 0.42f) : (Available ? Panel : Deep), Bought ? 0.98f : 0.94f), 7.0f * Scale);
			if(Selected || Hovered)
			{
				CUIRect NodeLine = {Node.x + 7.0f * Scale, Node.y + 2.5f * Scale, Node.w - 14.0f * Scale, 1.4f * Scale};
				DrawPanel(NodeLine, Fade(Accent, Selected ? 1.0f : 0.65f), 0.7f * Scale);
			}
			char aState[32];
			str_copy(aState, Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")), sizeof(aState));
			DrawText(Node.x + 7.0f * Scale, Node.y + 5.8f * Scale, 8.5f * Scale,
				Bought ? "✓" : (Available ? "+" : "×"), Fade(StateColor, 1.0f));
			char aCost[16];
			str_format(aCost, sizeof(aCost), "%d", pDef->m_ResearchCost);
			CUIRect Cost = {Node.x + Node.w - 26.0f * Scale, Node.y + 6.0f * Scale, 20.0f * Scale, 15.0f * Scale};
			DrawPanel(Cost, Fade(Inset, 0.96f), 6.5f * Scale);
			DrawText(Cost.x + Cost.w * 0.5f, Cost.y + 2.4f * Scale, 7.7f * Scale, aCost, Fade(Accent, 0.95f), -1.0f, 0);
			const char *pName = Localize(pDef->m_pName);
			const float NameWidth = Node.w - 13.0f * Scale;
			float NodeNameSize = 8.8f * Scale;
			while(NodeNameSize > 5.0f * Scale && TextRender()->TextWidth(0, NodeNameSize, pName, -1) > NameWidth)
				NodeNameSize -= 0.2f * Scale;
			DrawText(Node.x + Node.w * 0.5f, Node.y + 23.8f * Scale, NodeNameSize, pName, Fade(Text, 1.0f), -1.0f, 0);
			CUIRect StateBadge = {Node.x + 6.0f * Scale, Node.y + Node.h - 19.0f * Scale, Node.w - 12.0f * Scale, 14.0f * Scale};
			DrawPanel(StateBadge, Fade(Inset, Bought || Available ? 0.82f : 0.55f), 6.0f * Scale);
			DrawText(StateBadge.x + StateBadge.w * 0.5f, StateBadge.y + 1.7f * Scale, 8.0f * Scale, aState, Fade(StateColor, 0.95f), -1.0f, 0);
			if(UI()->DoButtonLogic(&m_aNodeButtonIDs[ID], &Node))
				m_SelectedResearch = ID;
		}
		UI()->ClipDisable();
		BranchY += BranchHeight + BranchGap;
	}

	const CPveCardDef *pSelected = PveCardDef(m_SelectedResearch);
	if(pSelected)
	{
		Details.Margin(9.0f * Scale, &Details);
		const bool Bought = PveCardIsUnlocked(m_SelectedResearch, Mask);
		const bool Available = CanBuyResearch(m_SelectedResearch, Mask);
		const vec4 StateColor = Bought || Available ? Accent : Danger;
		char aState[48];
		str_format(aState, sizeof(aState), "%s  %s", Bought ? "✓" : (Available ? "+" : "×"), Localize(Bought ? "PURCHASED" : (Available ? "AVAILABLE" : "LOCKED")));
		CUIRect DetailHeader = {Details.x, Details.y, Details.w, 23.0f * Scale};
		DrawText(DetailHeader.x + 2.0f * Scale, DetailHeader.y + 2.8f * Scale, 9.1f * Scale, Localize("Research Details"), Fade(Text, 0.96f));
		CUIRect DetailState = {DetailHeader.x + DetailHeader.w - 100.0f * Scale, DetailHeader.y, 100.0f * Scale, 22.0f * Scale};
		DrawPanel(DetailState, Fade(Inset, 0.92f), 8.0f * Scale);
		CUIRect DetailStateEdge = {DetailState.x, DetailState.y + 5.0f * Scale, 2.0f * Scale, DetailState.h - 10.0f * Scale};
		DrawPanel(DetailStateEdge, Fade(StateColor, 0.96f), 1.0f * Scale);
		DrawText(DetailState.x + DetailState.w * 0.5f, DetailState.y + 4.2f * Scale, 7.8f * Scale, aState, Fade(StateColor, 1.0f), -1.0f, 0);

		CUIRect Hero = {Details.x, DetailHeader.y + DetailHeader.h + 6.0f * Scale, Details.w, 74.0f * Scale};
		DrawPanel(Hero, Fade(Inset, 0.94f), 8.0f * Scale);
		CUIRect HeroEdge = {Hero.x, Hero.y + 7.0f * Scale, 2.0f * Scale, Hero.h - 14.0f * Scale};
		DrawPanel(HeroEdge, Fade(StateColor, 0.92f), 1.0f * Scale);
		CUIRect IconTile = {Hero.x + 10.0f * Scale, Hero.y + 12.0f * Scale, 50.0f * Scale, 50.0f * Scale};
		DrawPanel(IconTile, Fade(Deep, 0.86f), 8.0f * Scale);
		DrawSprite(PveCardIcon(pSelected), IconTile.x + IconTile.w * 0.5f, IconTile.y + IconTile.h * 0.5f, 36.0f * Scale, Text, 0.92f);
		const float HeroTextX = IconTile.x + IconTile.w + 10.0f * Scale;
		DrawText(HeroTextX, Hero.y + 9.0f * Scale, 10.7f * Scale, Localize(pSelected->m_pName), Fade(Text, 1.0f),
			Hero.x + Hero.w - HeroTextX - 9.0f * Scale, -1);
		char aMeta[96];
		str_format(aMeta, sizeof(aMeta), Localize("%s • TIER %d • COST %d"), Localize(PveRarityName(pSelected->m_Rarity)), pSelected->m_Tier, pSelected->m_ResearchCost);
		DrawText(HeroTextX, Hero.y + 46.8f * Scale, 8.2f * Scale, aMeta, Fade(Accent, 0.96f), Hero.x + Hero.w - HeroTextX - 9.0f * Scale, -1);

		float SectionY = Hero.y + Hero.h + 7.0f * Scale;
		DrawText(Details.x + 2.0f * Scale, SectionY, 8.9f * Scale, Localize("Effect"), Fade(Accent, 1.0f));
		CUIRect Effect = {Details.x, SectionY + 15.0f * Scale, Details.w, 68.0f * Scale};
		DrawPanel(Effect, Fade(Inset, 0.74f), 7.0f * Scale);
		DrawWrappedText(Effect.x + 10.0f * Scale, Effect.y + 7.5f * Scale, 8.7f * Scale, Localize(pSelected->m_pDescription), Fade(Text, 0.94f), Effect.w - 20.0f * Scale, 5);
		SectionY = Effect.y + Effect.h + 7.0f * Scale;
		CUIRect Rules = {Details.x, SectionY, Details.w, 50.0f * Scale};
		DrawPanel(Rules, Fade(Deep, 0.58f), 7.0f * Scale);
		char aStackRule[64];
		if(pSelected->m_MaxStacks == 1)
			str_copy(aStackRule, Localize("Unique perk"), sizeof(aStackRule));
		else
			str_format(aStackRule, sizeof(aStackRule), Localize("Stack limit: %d"), pSelected->m_MaxStacks);
		DrawText(Rules.x + 10.0f * Scale, Rules.y + 5.8f * Scale, 8.4f * Scale, aStackRule, Fade(Accent, 0.96f), Rules.w - 20.0f * Scale, -1);
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
		DrawText(Rules.x + 10.0f * Scale, Rules.y + 25.8f * Scale, 8.2f * Scale, aPrerequisite, Fade(Available || Bought ? Accent : Danger, 0.94f), Rules.w - 20.0f * Scale, -1);
		int UnlockedResearch = 0;
		for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
			if(!PveCardIsBase(ID) && PveCardIsUnlocked(ID, Mask))
				UnlockedResearch++;
		CUIRect Progress = {Details.x, Rules.y + Rules.h + 7.0f * Scale, Details.w, 38.0f * Scale};
		DrawPanel(Progress, Fade(Inset, 0.68f), 7.0f * Scale);
		DrawText(Progress.x + 10.0f * Scale, Progress.y + 4.8f * Scale, 8.3f * Scale, Localize("Research Progress"), Fade(Accent, 0.96f));
		char aProgress[32];
		str_format(aProgress, sizeof(aProgress), "%d / %d", UnlockedResearch, NUM_PVE_RESEARCH_CARDS);
		DrawText(Progress.x + Progress.w - 10.0f * Scale, Progress.y + 4.8f * Scale, 8.2f * Scale, aProgress, Fade(Text, 0.86f), -1.0f, 1);
		CUIRect ProgressBar = {Progress.x + 10.0f * Scale, Progress.y + 26.0f * Scale, Progress.w - 20.0f * Scale, 4.0f * Scale};
		DrawPanel(ProgressBar, Fade(Deep, 0.78f), 2.0f * Scale);
		if(UnlockedResearch > 0)
		{
			CUIRect ProgressFill = ProgressBar;
			ProgressFill.w *= UnlockedResearch / (float)NUM_PVE_RESEARCH_CARDS;
			DrawPanel(ProgressFill, Fade(Accent, 0.92f), 2.0f * Scale);
		}

		CUIRect Buy = {Details.x, Details.y + Details.h - 33.0f * Scale, Details.w, 32.0f * Scale};
		const bool BuyHovered = Available && UI()->HotItem() == &m_BuyButtonID;
		CUIRect BuyBorder = Buy;
		BuyBorder.Margin(-1.2f * Scale, &BuyBorder);
		DrawPanel(BuyBorder, Fade(Available ? Accent : (Bought ? AccentDim : Danger), Available ? 0.90f : 0.34f), 9.0f * Scale);
		DrawPanel(Buy, Fade(Available ? (BuyHovered ? AccentDim : Accent) : Inset, Available ? 0.96f : 0.76f), 8.0f * Scale);
		const bool PurchaseRejected = m_ValidationCode && time_get() < m_ValidationUntil;
		DrawText(Buy.x + Buy.w * 0.5f, Buy.y + 5.5f * Scale, 11.4f * Scale,
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
				DrawText(Run.x + 10.0f * Scale, Run.y + 4.8f * Scale, 8.4f * Scale, Localize("Current Run"), Fade(Accent, 1.0f));
				char aCount[24];
				str_format(aCount, sizeof(aCount), "%d", TotalPerks);
				CUIRect CountBadge = {Run.x + Run.w - 30.0f * Scale, Run.y + 5.0f * Scale, 20.0f * Scale, 16.0f * Scale};
				DrawPanel(CountBadge, Fade(Deep, 0.80f), 7.0f * Scale);
				DrawText(CountBadge.x + CountBadge.w * 0.5f, CountBadge.y + 2.6f * Scale, 7.6f * Scale, aCount, Fade(Text, 0.92f), -1.0f, 0);
				const int MaxLines = max(1, min(10, (int)((Run.h - 29.0f * Scale) / (13.0f * Scale))));
				float Y = Run.y + 26.0f * Scale;
				if(TotalPerks == 0)
					DrawText(Run.x + 10.0f * Scale, Y, 7.8f * Scale, Localize("No perks selected"), Fade(Text, 0.62f), Run.w - 20.0f * Scale, -1);
				for(int ID = 0; ID < NUM_PVE_CARDS && PerkLines < MaxLines; ID++)
				if(m_aRunPerks[ID] > 0)
				{
					if(PerkLines == MaxLines - 1 && TotalPerks > MaxLines)
					{
						char aMore[64];
						str_format(aMore, sizeof(aMore), Localize("+%d more perks"), TotalPerks - PerkLines);
						DrawText(Run.x + 10.0f * Scale, Y, 7.7f * Scale, aMore, Fade(Accent, 0.92f), Run.w - 20.0f * Scale, -1);
						break;
					}
					char aPerk[96];
					str_format(aPerk, sizeof(aPerk), "%s ×%d", Localize(PveCardDef(ID)->m_pName), m_aRunPerks[ID]);
					DrawText(Run.x + 10.0f * Scale, Y, 7.8f * Scale, aPerk, Fade(Text, 0.82f), Run.w - 20.0f * Scale, -1);
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
				DrawText(Guide.x + 10.0f * Scale, Guide.y + 4.3f * Scale, 8.4f * Scale, Localize("How Research Works"), Fade(Accent, 1.0f));
				DrawText(Guide.x + 10.0f * Scale, Guide.y + 19.5f * Scale, 7.6f * Scale, Localize("Earn points from PvE stages and contracts."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
				DrawText(Guide.x + 10.0f * Scale, Guide.y + 31.5f * Scale, 7.6f * Scale, Localize("Unlock connected nodes in order."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
				DrawText(Guide.x + 10.0f * Scale, Guide.y + 43.5f * Scale, 7.6f * Scale, Localize("Unlocked cards join future perk choices."), Fade(Text, 0.82f), Guide.w - 20.0f * Scale, -1);
			}
		}
	}
	TextRender()->TextColor(1, 1, 1, 1);
}

void CPveRoguelite::OnRender()
{
	const bool WasResearchVisible = m_ResearchVisible;
	m_ResearchVisible = false;
	if(!WasResearchVisible)
		m_ResearchAppearAmount = 0.0f;
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
	if(m_ContractVoteActive)
		DrawSelectionOverlay(true);
	else if(m_ChoiceActive)
		DrawSelectionOverlay(false);
	else
	{
		m_AppearAmount = 0.0f;
		DrawContractHud();
		DrawBuildHud();
	}
}

void CPveRoguelite::RenderMenuDebugOverlay()
{
	if(Client()->State() != IClient::STATE_ONLINE && m_ContractVoteActive)
		DrawSelectionOverlay(true);
	else if(Client()->State() != IClient::STATE_ONLINE && m_ChoiceActive)
		DrawSelectionOverlay(false);
}

bool CPveRoguelite::OnInput(IInput::CEvent Event)
{
	if(!ChoiceActive() && !m_ResearchVisible && (Event.m_Flags & IInput::FLAG_PRESS) && m_aRunPerks[PVE_CARD_DRONE_CHASSIS] > 0)
	{
		int Module = PVE_DRONE_NONE;
		if((g_Config.m_ClPveDroneAssault > 0 && Event.m_Key == g_Config.m_ClPveDroneAssault) || Event.m_Key == KEY_GAMEPAD_BUTTON_X)
			Module = PVE_DRONE_ASSAULT;
		else if((g_Config.m_ClPveDroneGuardian > 0 && Event.m_Key == g_Config.m_ClPveDroneGuardian) || Event.m_Key == KEY_GAMEPAD_BUTTON_Y)
			Module = PVE_DRONE_GUARDIAN;
		else if((g_Config.m_ClPveDroneRepair > 0 && Event.m_Key == g_Config.m_ClPveDroneRepair) || Event.m_Key == KEY_GAMEPAD_BUTTON_B)
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
		else if(pSelected)
		{
			int TargetBranch = pSelected->m_Branch;
			int TargetTier = pSelected->m_Tier;
			bool Navigate = true;
			if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT)
				TargetTier--;
			else if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT)
				TargetTier++;
			else if(Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP)
				TargetBranch--;
			else if(Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN)
				TargetBranch++;
			else
				Navigate = false;
			if(Navigate)
			{
				int Best = -1;
				int BestDistance = 999;
				for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
				{
					const CPveCardDef *pDef = PveCardDef(ID);
					if(pDef->m_Base || pDef->m_Tab != m_ResearchTab || pDef->m_Branch != TargetBranch)
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
	if(MsgType == NETMSGTYPE_SV_PVEPROGRESS)
	{
		if(!m_ProgressSent)
		{
			SyncProgress();
			return;
		}
		CNetMsg_Sv_PveProgress *pMsg = (CNetMsg_Sv_PveProgress *)pRawMsg;
		const unsigned long long Low = (unsigned int)pMsg->m_ResearchMask0 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask1 << 32);
		const unsigned long long High = (unsigned int)pMsg->m_ResearchMask2 | ((unsigned long long)(unsigned int)pMsg->m_ResearchMask3 << 32);
		g_Config.m_ClPveProgressVersion = pMsg->m_Version;
		g_Config.m_ClPveResearchPoints = pMsg->m_ResearchPoints;
		StoreResearchMask(CPveResearchMask(Low, High));
		g_Config.m_ClPveHighestInvasion = pMsg->m_HighestInvasion;
		g_Config.m_ClPvePreferredCheckpoint = pMsg->m_PreferredCheckpoint;
	}
	else if(MsgType == NETMSGTYPE_SV_PVECHOICE)
	{
		CNetMsg_Sv_PveChoice *pMsg = (CNetMsg_Sv_PveChoice *)pRawMsg;
		const bool NewChoice = !m_ChoiceActive || m_ChoiceNonce != pMsg->m_Nonce || m_ChoiceSequence != pMsg->m_ChoiceSequence;
		m_ChoiceActive = true;
		m_ContractVoteActive = false;
		m_ChoiceNonce = pMsg->m_Nonce;
		m_ChoiceSequence = pMsg->m_ChoiceSequence;
		m_ChoiceEndTick = pMsg->m_EndTick;
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
			m_FocusedChoice = 1;
			for(int i = 0; i < 3; i++)
				m_aCardFocus[i] = 0.0f;
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);
			m_AppearAmount = 0.0f;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVEPERK)
	{
		CNetMsg_Sv_PvePerk *pMsg = (CNetMsg_Sv_PvePerk *)pRawMsg;
		if(pMsg->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
		{
			// Perk messages also restore the existing run after an Invasion map
			// change. Only this offer's result may dismiss the choice overlay.
			if(m_ChoiceActive && m_ChoiceSequence > 0 && pMsg->m_Choices >= m_ChoiceSequence)
			{
				m_ChoiceActive = false;
				m_ChoiceSequence = 0;
			}
			if(pMsg->m_Card < NUM_PVE_CARDS)
				m_aRunPerks[pMsg->m_Card] = pMsg->m_Stacks;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_PVECONTRACTVOTE)
	{
		CNetMsg_Sv_PveContractVote *pMsg = (CNetMsg_Sv_PveContractVote *)pRawMsg;
		const bool NewVote = !m_ContractVoteActive || m_ContractNonce != pMsg->m_Nonce;
		m_ContractVoteActive = true;
		m_ChoiceActive = false;
		m_ContractNonce = pMsg->m_Nonce;
		m_ContractEndTick = pMsg->m_EndTick;
		m_aContractOptions[0] = pMsg->m_Contract0;
		m_aContractOptions[1] = pMsg->m_Contract1;
		m_aContractVotes[0] = pMsg->m_Votes0;
		m_aContractVotes[1] = pMsg->m_Votes1;
		if(NewVote)
		{
			m_SelectedContract = -1;
			m_FocusedChoice = 0;
			for(int i = 0; i < 3; i++)
				m_aCardFocus[i] = 0.0f;
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);
			m_AppearAmount = 0.0f;
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
		CNetMsg_Sv_PveResearchReward *pMsg = (CNetMsg_Sv_PveResearchReward *)pRawMsg;
		g_Config.m_ClPveResearchPoints = clamp(g_Config.m_ClPveResearchPoints + pMsg->m_Amount, 0, 999);
		g_Config.m_ClPveHighestInvasion = max(g_Config.m_ClPveHighestInvasion, pMsg->m_HighestInvasion);
		if(g_Config.m_ClPvePreferredCheckpoint > pMsg->m_UnlockedCheckpoint)
			g_Config.m_ClPvePreferredCheckpoint = pMsg->m_UnlockedCheckpoint;
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
	else if(MsgType == NETMSGTYPE_SV_PVEVALIDATION)
	{
		CNetMsg_Sv_PveValidation *pMsg = (CNetMsg_Sv_PveValidation *)pRawMsg;
		m_ValidationCode = pMsg->m_Code;
		m_ValidationUntil = time_get() + time_freq() * 3;
	}
}
