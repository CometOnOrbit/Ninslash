

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/game_data.h>
#include <generated/protocol.h>

#include <game/client/render.h>
#include <game/client/customstuff.h>
#include <game/localization.h>
#include <game/weapon_catalog.h>

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

	Input()->GetRelativePosition(&x, &y);
	m_SelectorMouse += vec2(x, y);
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

	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
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

	m_WasActive = true;
	m_SelectedSpectatorID = NO_SELECTION;

	// draw background
	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	RenderTools()->DrawRoundRect(Width / 2.0f - 300.0f, Height / 2.0f - 300.0f, 600.0f, 600.0f, 20.0f);
	Graphics()->QuadsEnd();

	// clamp mouse position to selector area
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, -280.0f, 280.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, -280.0f, 280.0f);

	// draw selections
	float FontSize = 20.0f;
	float StartY = -190.0f;
	float LineHeight = 60.0f;
	bool Selected = false;

	if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == SPEC_FREEVIEW)
	{
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
		RenderTools()->DrawRoundRect(Width / 2.0f - 280.0f, Height / 2.0f - 280.0f, 270.0f, 60.0f, 20.0f);
		Graphics()->QuadsEnd();
	}

	if(m_SelectorMouse.x >= -280.0f && m_SelectorMouse.x <= -10.0f && m_SelectorMouse.y >= -280.0f &&
	   m_SelectorMouse.y <= -220.0f)
	{
		m_SelectedSpectatorID = SPEC_FREEVIEW;
		Selected = true;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.5f);
	TextRender()->Text(0, Width / 2.0f - 240.0f, Height / 2.0f - 265.0f, FontSize, Localize("Free-View"), -1);

	float x = -270.0f, y = StartY;
	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if((CustomStuff()->IsBot(i) && m_pClient->IsCoop()) || !m_pClient->m_Snap.m_paPlayerInfos[i] ||
		   m_pClient->m_Snap.m_paPlayerInfos[i]->m_Spectating ||
		   m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_SPECTATORS)
			continue;

		if(++Count % 9 == 0)
		{
			x += 290.0f;
			y = StartY;
		}

		if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID == i)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
			RenderTools()->DrawRoundRect(Width / 2.0f + x - 10.0f, Height / 2.0f + y - 10.0f, 270.0f, 60.0f, 20.0f);
			Graphics()->QuadsEnd();
		}

		Selected = false;
		if(m_SelectorMouse.x >= x - 10.0f && m_SelectorMouse.x <= x + 260.0f && m_SelectorMouse.y >= y - 10.0f &&
		   m_SelectorMouse.y <= y + 50.0f)
		{
			m_SelectedSpectatorID = i;
			Selected = true;
		}
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Selected ? 1.0f : 0.5f);
		TextRender()->Text(
			0, Width / 2.0f + x + 50.0f, Height / 2.0f + y + 5.0f, FontSize, m_pClient->m_aClients[i].m_aName, 220.0f);

		// flag
		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameDataObj &&
		   m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS &&
		   (m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed == m_pClient->m_Snap.m_paPlayerInfos[i]->m_ClientID ||
			m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == m_pClient->m_Snap.m_paPlayerInfos[i]->m_ClientID))
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();

			RenderTools()->SelectSprite(m_pClient->m_Snap.m_paPlayerInfos[i]->m_Team == TEAM_RED ? SPRITE_FLAG_BLUE
																								 : SPRITE_FLAG_RED,
										SPRITE_FLAG_FLIP_X);

			float Size = LineHeight;
			IGraphics::CQuadItem QuadItem(
				Width / 2.0f + x - LineHeight / 5.0f, Height / 2.0f + y - LineHeight / 3.0f, Size / 2.0f, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// CTeeRenderInfo TeeInfo = m_pClient->m_aClients[i].m_RenderInfo;
		// RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f),
		// vec2(Width/2.0f+x+20.0f, Height/2.0f+y+20.0f));

		y += LineHeight;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	// draw cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x + Width / 2.0f, m_SelectorMouse.y + Height / 2.0f, 48.0f, 48.0f);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorID = NO_SELECTION;
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
	const float RowH = 25.0f;
	const float HeaderH = 34.0f;
	const int MaxRows = max(1, min((int)MAX_CLIENTS, (int)((Height - 28.0f) / RowH) - 1));
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
	const float X = Width - PanelW - 16.0f;
	const float Y = 14.0f;
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
	TextRender()->Text(0, X + 12.0f, Y + 8.0f, 17.0f, Localize("Spectator stats"), PanelW - 24.0f);
	TextRender()->TextColor(0.65f, 0.76f, 0.86f, 0.9f);
	TextRender()->Text(0, X + 12.0f, Y + 27.0f, 10.0f, Localize("K/D   Streak   Gold/Kits   Weapon"), PanelW - 24.0f);

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
		TextRender()->Text(0, X + 12.0f, RowY + 5.0f, 11.5f, aLine, PanelW - 24.0f);
		Row++;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
