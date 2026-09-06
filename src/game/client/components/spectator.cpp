

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/game_data.h>
#include <generated/protocol.h>

#include <game/client/render.h>
#include <game/client/components/chat.h>
#include <game/client/customstuff.h>
#include <game/localization.h>
#include <game/weapons/weapon_catalog.h>

#include "hud_layout.h"
#include "spectator.h"

void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active &&
	   (pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK ||
		pSelf->DemoPlayer()->GetDemoType() == IDemoPlayer::DEMOTYPE_SERVER))
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	((CSpectator *)pUserData)->Spectate(pResult->GetInteger(0));
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int NewSpectatorID;
	bool GotNewSpectatorID = false;

	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
			   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = i;
			GotNewSpectatorID = true;
			break;
		}
	}
	else
	{
		for(int i = pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID + 1; i < MAX_CLIENTS; i++)
		{
			if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
			   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = i;
			GotNewSpectatorID = true;
			break;
		}

		if(!GotNewSpectatorID)
		{
			for(int i = 0; i < pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID; i++)
			{
				if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
				   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
				   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
				   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
					continue;

				NewSpectatorID = i;
				GotNewSpectatorID = true;
				break;
			}
		}
	}
	if(GotNewSpectatorID)
		pSelf->Spectate(NewSpectatorID);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int NewSpectatorID;
	bool GotNewSpectatorID = false;

	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
	{
		for(int i = MAX_CLIENTS - 1; i > -1; i--)
		{
			if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
			   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = i;
			GotNewSpectatorID = true;
			break;
		}
	}
	else
	{
		for(int i = pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID - 1; i > -1; i--)
		{
			if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
			   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
			   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
				continue;

			NewSpectatorID = i;
			GotNewSpectatorID = true;
			break;
		}

		if(!GotNewSpectatorID)
		{
			for(int i = MAX_CLIENTS - 1; i > pSelf->m_pClient->m_Snap.m_SpecInfo.m_SpectatorID; i--)
			{
				if((pSelf->CustomStuff()->IsBot(i) && pSelf->m_pClient->IsCoop()) ||
				   !pSelf->m_pClient->m_Snap.m_paPlayerInfos[i] ||
				   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
				   pSelf->m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
					continue;

				NewSpectatorID = i;
				GotNewSpectatorID = true;
				break;
			}
		}
	}
	if(GotNewSpectatorID)
		pSelf->Spectate(NewSpectatorID);
}

CSpectator::CSpectator()
{
	OnReset();
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register(
		"spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
}

bool CSpectator::OnMouseMove(float x, float y)
{
	if(!m_Active)
		return false;

	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	Input()->GetRelativePosition(&x, &y);
	const float HudScale = 300.0f / max(1, Graphics()->ScreenHeight());
	m_SelectorMouse += vec2(x, y) * HudScale;
	return true;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{
	// Highlight the killer briefly, then return to the view that was active
	// before the automatic director took over.
	if(m_AutoDirectorActive && Client()->GameTick() >= m_AutoDirectorEndTick)
	{
		const int ReturnID = m_AutoDirectorReturnID;
		m_AutoDirectorActive = false;
		m_AutoDirectorReturnID = NO_SELECTION;
		if(ReturnID >= SPEC_FREEVIEW && ReturnID < MAX_CLIENTS)
			SpectateInternal(ReturnID, true);
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_Active && !m_pClient->m_pChat->IsVisible())
		RenderStatsPanel();

	if(!m_Active)
	{
		if(m_WasActive)
		{
			if(m_SelectedSpectatorID != NO_SELECTION)
				Spectate(m_SelectedSpectatorID);
			m_WasActive = false;
		}
		return;
	}

	if(!m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	const bool FirstFrame = !m_WasActive;
	m_WasActive = true;
	m_SelectedSpectatorID = NO_SELECTION;

	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	const float Margin = HudLayout::SafeMargin(Width);
	const CUIRect Panel = {Margin, 8.0f, Width - Margin * 2.0f, Height - 16.0f};
	const int Columns = Width >= 420.0f ? 3 : (Width >= 300.0f ? 2 : 1);
	const float Gap = 6.0f;
	const float ColumnW = (Panel.w - 16.0f - Gap * (Columns - 1)) / Columns;
	const float RowHeight = Columns == 1 ? 14.0f : 20.0f;
	const float FontSize = Columns == 1 ? 6.5f : 7.0f;
	const float FreeY = Panel.y + 10.0f;
	const CUIRect FreeView = {Panel.x + 8.0f, FreeY, Panel.w - 16.0f, 20.0f};
	const float ListY = FreeView.y + FreeView.h + 8.0f;

	if(FirstFrame)
		m_SelectorMouse = vec2(Width * 0.5f, Height * 0.5f);
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 2.0f, Width - 6.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 2.0f, Height - 6.0f);

	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.01f, 0.015f, 0.02f, 0.92f);
	RenderTools()->DrawRoundRect(0.0f, 0.0f, Width, Height, 0.0f);
	Graphics()->SetColor(0.05f, 0.07f, 0.10f, 0.98f);
	RenderTools()->DrawRoundRect(Panel.x, Panel.y, Panel.w, Panel.h, 10.0f);
	Graphics()->QuadsEnd();

	const bool FreeHovered = m_SelectorMouse.x >= FreeView.x && m_SelectorMouse.x <= FreeView.x + FreeView.w &&
							 m_SelectorMouse.y >= FreeView.y && m_SelectorMouse.y <= FreeView.y + FreeView.h;
	if(FreeHovered)
		m_SelectedSpectatorID = SPEC_FREEVIEW;
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, FreeHovered ? 1.0f : 0.76f);
	TextRender()->Text(0, FreeView.x + 8.0f, FreeView.y + 5.0f, 8.0f, Localize("Free-View"), FreeView.w - 16.0f);

	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if((CustomStuff()->IsBot(i) && m_pClient->IsCoop()) || !m_pClient->m_Snap.m_paPlayerInfos[i] ||
		   m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
		   m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
			continue;

		const int Row = Count / Columns;
		const int Column = Count % Columns;
		const CUIRect Item = {Panel.x + 8.0f + Column * (ColumnW + Gap),
			ListY + Row * RowHeight,
			ColumnW,
			RowHeight - 2.0f};
		Count++;
		if(Item.y + Item.h > Panel.y + Panel.h - 6.0f)
			continue;
		const bool Hovered = m_SelectorMouse.x >= Item.x && m_SelectorMouse.x <= Item.x + Item.w &&
							 m_SelectorMouse.y >= Item.y && m_SelectorMouse.y <= Item.y + Item.h;
		const bool Current = m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == i;
		if(Hovered)
			m_SelectedSpectatorID = i;
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.18f, 0.45f, 0.65f, Current ? 0.52f : (Hovered ? 0.34f : 0.20f));
		RenderTools()->DrawRoundRect(Item.x, Item.y, Item.w, Item.h, 4.0f);
		Graphics()->QuadsEnd();

		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameDataObj &&
		   m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS &&
		   (m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed == i ||
			m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == i))
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();
			RenderTools()->SelectSprite(
				m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_RED ? SPRITE_FLAG_BLUE : SPRITE_FLAG_RED,
				SPRITE_FLAG_FLIP_X);
			IGraphics::CQuadItem Flag(Item.x + 4.0f, Item.y + 3.0f, 4.0f, 12.0f);
			Graphics()->QuadsDrawTL(&Flag, 1);
			Graphics()->QuadsEnd();
		}

		CTextCursor Cursor;
		TextRender()->SetCursor(
			&Cursor, Item.x + 12.0f, Item.y + (Item.h - FontSize) * 0.5f, FontSize,
			TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = max(4.0f, Item.w - 16.0f);
		Cursor.m_MaxLines = 1;
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Hovered || Current ? 1.0f : 0.72f);
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[i].m_aName, -1);
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem Cursor(m_SelectorMouse.x, m_SelectorMouse.y, 8.0f, 8.0f);
	Graphics()->QuadsDrawTL(&Cursor, 1);
	Graphics()->QuadsEnd();
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorID = NO_SELECTION;
	m_SelectorMouse = vec2(0.0f, 0.0f);
	m_AutoDirectorActive = false;
	m_AutoDirectorEndTick = 0;
	m_AutoDirectorReturnID = NO_SELECTION;
}

void CSpectator::CancelAutoDirector()
{
	m_AutoDirectorActive = false;
	m_AutoDirectorEndTick = 0;
	m_AutoDirectorReturnID = NO_SELECTION;
}

void CSpectator::SpectateInternal(int SpectatorID, bool Automatic)
{
	if(!Automatic)
		CancelAutoDirector();

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->m_DemoSpecID = clamp(SpectatorID, (int)SPEC_FREEVIEW, MAX_CLIENTS - 1);
		return;
	}

	if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SpectatorID)
		return;

	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpectatorID = SpectatorID;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CSpectator::Spectate(int SpectatorID)
{
	SpectateInternal(SpectatorID, false);
}

void CSpectator::OnKillEvent(const CNetMsg_Sv_KillMsg *pMsg)
{
	if(!pMsg || !g_Config.m_ClSpectatorDirector || !m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;
	const int Killer = pMsg->m_Killer;
	if(Killer < 0 || Killer >= MAX_CLIENTS || Killer == pMsg->m_Victim ||
	   !m_pClient->m_Snap.m_paPlayerInfos[Killer] ||
	   m_pClient->m_Snap.m_paPlayerInfos[Killer]->m_Team == TEAM_SPECTATORS ||
	   m_pClient->m_Snap.m_paPlayerInfos[Killer]->m_Spectating)
		return;

	const int Current = m_pClient->m_Snap.m_SpecInfo.m_SpectatorID;
	if(!m_AutoDirectorActive)
		m_AutoDirectorReturnID = Current;
	m_AutoDirectorActive = true;
	m_AutoDirectorEndTick = Client()->GameTick() + max(1, Client()->GameTickSpeed() * 2);
	SpectateInternal(Killer, true);
}

void CSpectator::RenderStatsPanel()
{
	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;
	const float PanelW = min(360.0f, Width * 0.29f);
	const float PanelY = HudLayout::ObjectiveTop;
	const float PanelMargin = HudLayout::SafeMargin(Width);
	const float RowH = 25.0f;
	const float HeaderH = 34.0f;
	const float RowFontSize = clamp(PanelW / 30.0f, 8.0f, 11.5f);
	const int MaxRows = max(1, min((int)MAX_CLIENTS, (int)((Height - PanelY - HeaderH - 14.0f) / RowH)));
	int RowCount = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
		// Keep eliminated players in the table: their K/D and streak values are
		// still authoritative and should remain visible while they wait to respawn.
		if(pInfo && pInfo->m_Team != TEAM_SPECTATORS)
			RowCount++;
	}
	if(RowCount <= 0)
		return;

	const float PanelH = HeaderH + min(RowCount, MaxRows) * RowH + 14.0f;
	const float X = PanelMargin;
	const float Y = PanelY;
	Graphics()->MapScreen(0, 0, Width, Height);
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.42f);
	RenderTools()->DrawRoundRect(X + 3.0f, Y + 4.0f, PanelW, PanelH, 12.0f);
	Graphics()->SetColor(0.05f, 0.07f, 0.10f, 0.94f);
	RenderTools()->DrawRoundRect(X, Y, PanelW, PanelH, 12.0f);
	Graphics()->SetColor(0.20f, 0.65f, 0.95f, 0.85f);
	RenderTools()->DrawRoundRect(X, Y, 3.0f, PanelH, 1.5f);
	Graphics()->QuadsEnd();
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.95f);
	CTextCursor HeaderCursor;
	TextRender()->SetCursor(
		&HeaderCursor, X + 12.0f, Y + 8.0f, 17.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	HeaderCursor.m_LineWidth = max(4.0f, PanelW - 24.0f);
	HeaderCursor.m_MaxLines = 1;
	TextRender()->TextEx(&HeaderCursor, Localize("Spectator stats"), -1);
	TextRender()->TextColor(0.65f, 0.76f, 0.86f, 0.9f);
	CTextCursor SubheaderCursor;
	TextRender()->SetCursor(
		&SubheaderCursor, X + 12.0f, Y + 27.0f, 10.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	SubheaderCursor.m_LineWidth = max(4.0f, PanelW - 24.0f);
	SubheaderCursor.m_MaxLines = 1;
	TextRender()->TextEx(&SubheaderCursor, Localize("K/D   Streak   Gold/Kits   Weapon"), -1);

	int Row = 0;
	for(int i = 0; i < MAX_CLIENTS && Row < MaxRows; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
		if(!pInfo || pInfo->m_Team == TEAM_SPECTATORS)
			continue;
		const float RowY = Y + HeaderH + Row * RowH;
		if(i == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.18f, 0.45f, 0.65f, 0.48f);
			RenderTools()->DrawRoundRect(X + 7.0f, RowY, PanelW - 14.0f, RowH - 2.0f, 5.0f);
			Graphics()->QuadsEnd();
		}
		char aWeapon[64] = "-";
		int DefinitionId = 0;
		int Level = 0;
		switch(clamp(pInfo->m_WeaponSlot, 0, 3))
		{
			case 0: DefinitionId = pInfo->m_Weapon1DefinitionId; Level = pInfo->m_Weapon1Level; break;
			case 1: DefinitionId = pInfo->m_Weapon2DefinitionId; Level = pInfo->m_Weapon2Level; break;
			case 2: DefinitionId = pInfo->m_Weapon3DefinitionId; Level = pInfo->m_Weapon3Level; break;
			default: DefinitionId = pInfo->m_Weapon4DefinitionId; Level = pInfo->m_Weapon4Level; break;
		}
		CWeaponSpec Spec;
		CWeaponDefinition Definition;
		if(CWeaponCatalog::TryFromProtocol(DefinitionId, Level, &Spec) &&
		   CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition))
			str_format(aWeapon, sizeof(aWeapon), "%s L%d", Localize(Definition.m_aNameKey), Level);
		char aLine[256];
		char aName[MAX_NAME_LENGTH + 1];
		const char *pLabel = m_pClient->GetPlayerLabel(i, aName, sizeof(aName));
		str_format(aLine,
			 sizeof(aLine),
			 "%s  %d/%d  %d/%d  G%d K%d  %s",
			 pLabel,
			 pInfo->m_Kills,
			 pInfo->m_Deaths,
			 pInfo->m_KillStreak,
			 pInfo->m_BestKillStreak,
			 pInfo->m_Gold,
			 pInfo->m_Kits,
			 aWeapon);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, i == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID ? 1.0f : 0.76f);
		CTextCursor Cursor;
		TextRender()->SetCursor(
			&Cursor, X + 12.0f, RowY + 5.0f, RowFontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = max(4.0f, PanelW - 24.0f);
		Cursor.m_MaxLines = 1;
		TextRender()->TextEx(&Cursor, aLine, -1);
		Row++;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
