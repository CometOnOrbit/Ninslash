

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>

#include <cstring>

#include <generated/game_data.h>
#include <generated/protocol.h>

#include <game/shared/core/localization.h>
#include <game/client/core/gameclient.h>
#include <game/client/render/render.h>
#include <game/client/resources/countryflags.h>
#include <game/client/ui/motd.h>
#include <game/client/ui/menus.h>

#include <game/client/state/customstuff.h>

#include "scoreboard.h"
#include "gamevote.h"
#include "hud.h"
#include "pve_roguelite.h"


CScoreboard::CScoreboard()
{
	m_DebugActive = false;
	OnReset();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	((CScoreboard *)pUserData)->m_Active = pResult->GetInteger(0) != 0;
}

void CScoreboard::ConDebugScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	((CScoreboard *)pUserData)->m_DebugActive = !pResult->NumArguments() || pResult->GetInteger(0) != 0;
}

void CScoreboard::OnReset()
{
	m_Active = false;
	mem_zero(&m_TotalRect, sizeof(m_TotalRect));
}

void CScoreboard::OnRelease()
{
	m_Active = false;
}

void CScoreboard::OnConsoleInit()
{
	Console()->Register("+scoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
	Console()->Register("+gamepadscoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
	Console()->Register("scoreboard_debug_show", "?i", CFGFLAG_CLIENT, ConDebugScoreboard, this, "Keep the scoreboard visible for UI regression screenshots");
}

float CScoreboard::RenderGoals(float x, float y, float w)
{
	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return 0.0f;
	char aaItems[3][96];
	int NumItems = 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit)
		str_format(aaItems[NumItems++], sizeof(aaItems[0]), "%s: %d", Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit);
	if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit)
		str_format(aaItems[NumItems++], sizeof(aaItems[0]), Localize("Time limit: %d min"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit);
	if(m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum > 1 && m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent)
		str_format(aaItems[NumItems++], sizeof(aaItems[0]), "%s %d/%d", Localize("Round"), m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent, m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum);
	if(NumItems == 0)
		return 0.0f;

	const float h = 48.0f;
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.36f);
	RenderTools()->DrawRoundRect(x+2.0f, y+3.0f, w, h, 12.0f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.96f);
	RenderTools()->DrawRoundRect(x, y, w, h, 12.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.92f);
	RenderTools()->DrawRoundRect(x, y+8.0f, 3.0f, h-16.0f, 1.5f);
	Graphics()->QuadsEnd();
	const float ItemW = (w-24.0f-(NumItems-1)*8.0f)/NumItems;
	for(int i = 0; i < NumItems; i++)
	{
		const float ItemX = x+12.0f+i*(ItemW+8.0f);
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.72f);
		RenderTools()->DrawRoundRect(ItemX, y+8.0f, ItemW, h-16.0f, 8.0f);
		Graphics()->QuadsEnd();
		float FontSize = 18.0f;
		while(FontSize > 13.0f && TextRender()->TextWidth(0, FontSize, aaItems[i], -1) > ItemW-16.0f)
			FontSize -= 0.5f;
		const float TextW = TextRender()->TextWidth(0, FontSize, aaItems[i], -1);
		TextRender()->Text(0, ItemX+(ItemW-TextW)*0.5f, y+(h-FontSize)*0.5f-1.0f, FontSize, aaItems[i], -1);
	}
	return h;
}

float CScoreboard::RenderSpectators(float x, float y, float w)
{
	char aBuffer[1024*4];
	char aBuf[MAX_NAME_LENGTH];
	aBuffer[0] = 0;
	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;

		if(Count)
			str_append(aBuffer, ", ", sizeof(aBuffer));
		str_append(aBuffer, m_pClient->GetPlayerLabel(pInfo->m_ClientID, aBuf, sizeof(aBuf)), sizeof(aBuffer));
		Count++;
	}
	if(Count == 0)
		return 0.0f;

	const float FontSize = 18.0f;
	const float LineWidth = w-34.0f;
	CTextCursor Measure;
	TextRender()->SetCursor(&Measure, 0, 0, FontSize, 0);
	Measure.m_LineWidth = LineWidth;
	Measure.m_MaxLines = 3;
	TextRender()->TextEx(&Measure, aBuffer, -1);
	const float TextHeight = max(FontSize, Measure.m_Y);
	const float h = clamp(42.0f+TextHeight, 68.0f, 116.0f);
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Accent = CMenus::ThemeAccent();
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.36f);
	RenderTools()->DrawRoundRect(x+2.0f, y+3.0f, w, h, 12.0f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.96f);
	RenderTools()->DrawRoundRect(x, y, w, h, 12.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.90f);
	RenderTools()->DrawRoundRect(x, y+8.0f, 3.0f, h-16.0f, 1.5f);
	Graphics()->QuadsEnd();
	TextRender()->Text(0, x+16.0f, y+10.0f, 20.0f, Localize("Spectators"), w-32.0f);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, x+16.0f, y+36.0f, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = LineWidth;
	Cursor.m_MaxLines = 3;
	TextRender()->TextEx(&Cursor, aBuffer, -1);
	return h;
}

float CScoreboard::RenderScoreboard(float x, float y, float w, int Team, const char *pTitle)
{
	if(Team == TEAM_SPECTATORS)
		return 0.0f;

	const CNetObj_PlayerInfo *apPlayers[MAX_CLIENTS];
	int PlayerCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];
		if(!pInfo || pInfo->m_Team != Team || pInfo->m_ClientID < 0 || pInfo->m_ClientID >= MAX_CLIENTS)
			continue;
		if((m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_COOP) && m_pClient->m_aClients[pInfo->m_ClientID].m_IsBot)
			continue;
		apPlayers[PlayerCount++] = pInfo;
	}
	const int Columns = PlayerCount > 18 ? 2 : 1;
	const int Rows = max(1, (PlayerCount+Columns-1)/Columns);
	const float RowHeight = PlayerCount == 0 ? 0.0f : clamp(596.0f/Rows, 22.0f, PlayerCount <= 8 ? 54.0f : 44.0f);
	const float HeaderHeight = 62.0f;
	const float TableHeaderHeight = 34.0f;
	const float h = HeaderHeight+TableHeaderHeight+Rows*RowHeight+12.0f;
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	vec4 TeamAccent = Accent;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
		TeamAccent = Team == TEAM_RED ? vec4(0.92f, 0.24f, 0.30f, 1.0f) : vec4(0.26f, 0.42f, 0.92f, 1.0f);

	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.48f);
	RenderTools()->DrawRoundRect(x+4.0f, y+5.0f, w, h, 18.0f);
	Graphics()->SetColor(TeamAccent.r, TeamAccent.g, TeamAccent.b, 0.66f);
	RenderTools()->DrawRoundRect(x-1.5f, y-1.5f, w+3.0f, h+3.0f, 18.5f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.985f);
	RenderTools()->DrawRoundRect(x, y, w, h, 17.0f);
	Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.84f);
	RenderTools()->DrawRoundRect(x, y, w, HeaderHeight, 17.0f);
	Graphics()->SetColor(TeamAccent.r, TeamAccent.g, TeamAccent.b, 0.96f);
	RenderTools()->DrawRoundRect(x, y+12.0f, 4.0f, HeaderHeight-24.0f, 2.0f);
	Graphics()->QuadsEnd();

	float TitleFontsize = 32.0f;
	if(!pTitle)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			pTitle = Localize("Game over");
		else
			pTitle = Localize("Score board");
	}
	while(TitleFontsize > 24.0f && TextRender()->TextWidth(0, TitleFontsize, pTitle, -1) > w*0.55f)
		TitleFontsize -= 1.0f;
	TextRender()->TextColor(Text.r, Text.g, Text.b, 1.0f);
	TextRender()->Text(0, x+20.0f, y+12.0f, TitleFontsize, pTitle, -1);

	char aBuf[128] = {0};
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		if(m_pClient->m_Snap.m_pGameDataObj)
		{
			int Score = Team == TEAM_RED ? m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed : m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
	}
	else
	{
		if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW &&
			m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID])
		{
			int Score = m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID]->m_Score;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
		else if(m_pClient->m_Snap.m_pLocalInfo)
		{
			if(g_Config.m_ClHideSelfScore)
				aBuf[0] = 0;
			else
			{
				int Score = m_pClient->m_Snap.m_pLocalInfo->m_Score;
				str_format(aBuf, sizeof(aBuf), "%d", Score);
			}
		}
	}
	float tw = TextRender()->TextWidth(0, TitleFontsize, aBuf, -1);
	if (!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_INFECTION) && aBuf[0])
		TextRender()->Text(0, x+w-tw-20.0f, y+12.0f, TitleFontsize, aBuf, -1);
	char aPlayers[64];
	str_format(aPlayers, sizeof(aPlayers), "%s: %d", Localize("Players"), PlayerCount);
	const float PlayersW = TextRender()->TextWidth(0, 15.0f, aPlayers, -1)+22.0f;
	const float PlayersX = x+w-PlayersW-18.0f-(aBuf[0] ? tw+18.0f : 0.0f);
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(AccentDim.r, AccentDim.g, AccentDim.b, 0.62f);
	RenderTools()->DrawRoundRect(PlayersX, y+18.0f, PlayersW, 25.0f, 12.0f);
	Graphics()->QuadsEnd();
	TextRender()->Text(0, PlayersX+11.0f, y+22.0f, 15.0f, aPlayers, -1);

	const float InnerX = x+14.0f;
	const float InnerW = w-28.0f;
	const float ColumnGap = Columns > 1 ? 12.0f : 0.0f;
	const float ColumnW = (InnerW-ColumnGap*(Columns-1))/Columns;
	const bool CompactColumns = Columns > 1;
	const float HeaderY = y+HeaderHeight;
	for(int Column = 0; Column < Columns; Column++)
	{
		const float ColumnX = InnerX+Column*(ColumnW+ColumnGap);
		const float ScoreW = CompactColumns ? 50.0f : 64.0f;
		const float TeeW = CompactColumns ? 42.0f : 52.0f;
		const float IdW = !CompactColumns && g_Config.m_ClScoreboardUserId ? 38.0f : 0.0f;
		const float PingW = CompactColumns ? 52.0f : 62.0f;
		const float CountryW = CompactColumns ? 0.0f : 54.0f;
		const float ClanW = !CompactColumns && g_Config.m_ClShowsocial ? clamp(ColumnW*0.20f, 90.0f, 155.0f) : 0.0f;
		const float ScoreX = ColumnX;
		const float TeeX = ScoreX+ScoreW;
		const float IdX = TeeX+TeeW;
		const float NameX = IdX+IdW;
		const float PingX = ColumnX+ColumnW-PingW;
		const float CountryX = PingX-CountryW;
		const float ClanX = CountryX-ClanW;
		const float NameW = max(44.0f, ClanX-NameX-8.0f);
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.54f);
		RenderTools()->DrawRoundRect(ColumnX, HeaderY+4.0f, ColumnW, TableHeaderHeight-8.0f, 8.0f);
		Graphics()->QuadsEnd();
		TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 0.92f);
		const float HeaderFont = 14.0f;
		tw = TextRender()->TextWidth(0, HeaderFont, Localize("Score"), -1);
		TextRender()->Text(0, ScoreX+ScoreW-tw-5.0f, HeaderY+10.0f, HeaderFont, Localize("Score"), -1);
		if(IdW > 0.0f)
			TextRender()->Text(0, IdX+3.0f, HeaderY+10.0f, HeaderFont, Localize("ID"), -1);
		TextRender()->Text(0, NameX+4.0f, HeaderY+10.0f, HeaderFont, Localize("Name"), -1);
		if(ClanW > 0.0f)
			TextRender()->Text(0, ClanX+6.0f, HeaderY+10.0f, HeaderFont, Localize("Clan"), -1);
		tw = TextRender()->TextWidth(0, HeaderFont, Localize("Ping"), -1);
		TextRender()->Text(0, PingX+PingW-tw-4.0f, HeaderY+10.0f, HeaderFont, Localize("Ping"), -1);
		TextRender()->TextColor(1, 1, 1, 1);

		for(int Row = 0; Row < Rows; Row++)
		{
			const int Index = Column*Rows+Row;
			if(Index >= PlayerCount)
				break;
			const CNetObj_PlayerInfo *pInfo = apPlayers[Index];
			const int ClientID = pInfo->m_ClientID;
			const float RowY = HeaderY+TableHeaderHeight+Row*RowHeight;
			const bool Focused = pInfo->m_Local || (m_pClient->m_Snap.m_SpecInfo.m_Active && ClientID == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			if(Focused)
				Graphics()->SetColor(TeamAccent.r, TeamAccent.g, TeamAccent.b, 0.28f);
			else if(Row&1)
				Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.30f);
			else
				Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.18f);
			RenderTools()->DrawRoundRect(ColumnX, RowY+2.0f, ColumnW, max(1.0f, RowHeight-4.0f), 8.0f);
			if(Focused)
			{
				Graphics()->SetColor(TeamAccent.r, TeamAccent.g, TeamAccent.b, 0.96f);
				RenderTools()->DrawRoundRect(ColumnX, RowY+8.0f, 3.0f, max(1.0f, RowHeight-16.0f), 1.5f);
			}
			Graphics()->QuadsEnd();

			const float FontSize = RowHeight >= 42.0f ? 19.0f : (RowHeight >= 30.0f ? 16.0f : 13.0f);
			const float TextY = RowY+(RowHeight-FontSize)*0.5f-1.0f;
			const float Dim = pInfo->m_Spectating ? 0.54f : 1.0f;
			TextRender()->TextColor(Text.r*Dim, Text.g*Dim, Text.b*Dim, 1.0f);
			if(g_Config.m_ClHideSelfScore && pInfo->m_Local)
				aBuf[0] = 0;
			else
				str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Score, -9999, 99999));
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, ScoreX+ScoreW-tw-5.0f, TextY, FontSize, aBuf, -1);

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS && m_pClient->m_Snap.m_pGameDataObj &&
				(m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed == ClientID || m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == ClientID))
			{
				Graphics()->BlendNormal();
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
				Graphics()->QuadsBegin();
				RenderTools()->SelectSprite(pInfo->m_Team==TEAM_RED ? SPRITE_FLAG_BLUE : SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);
				const float Size = min(30.0f, RowHeight-6.0f);
				IGraphics::CQuadItem QuadItem(TeeX+2.0f, RowY+(RowHeight-Size)*0.5f, Size/2.0f, Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
			CTeeRenderInfo TeeInfo = m_pClient->m_aClients[ClientID].m_RenderInfo;
			TeeInfo.m_Size = clamp(RowHeight-8.0f, 16.0f, 40.0f);
			RenderTools()->RenderPortrait(&TeeInfo, vec2(TeeX+TeeW*0.5f, RowY+RowHeight*0.5f+TeeInfo.m_Size*0.55f), 0);

			if(IdW > 0.0f)
			{
				str_format(aBuf, sizeof(aBuf), "%d", ClientID);
				TextRender()->Text(0, IdX+3.0f, TextY, FontSize, aBuf, -1);
			}
			char aNameBuf[MAX_NAME_LENGTH+16];
			const char *pLabel = m_pClient->GetPlayerLabel(ClientID, aNameBuf, sizeof(aNameBuf));
			if(CompactColumns && g_Config.m_ClScoreboardUserId && g_Config.m_ClShowsocial)
			{
				char aCompactName[MAX_NAME_LENGTH+16];
				str_format(aCompactName, sizeof(aCompactName), "%d · %s", ClientID, pLabel);
				str_copy(aNameBuf, aCompactName, sizeof(aNameBuf));
				pLabel = aNameBuf;
			}
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, NameX+4.0f, TextY, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = NameW-8.0f;
			Cursor.m_MaxLines = 1;
			TextRender()->TextEx(&Cursor, pLabel, -1);
			if(ClanW > 0.0f)
			{
				TextRender()->SetCursor(&Cursor, ClanX+5.0f, TextY, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = ClanW-10.0f;
				Cursor.m_MaxLines = 1;
				TextRender()->TextEx(&Cursor, m_pClient->m_aClients[ClientID].m_aClan, -1);
			}
			if(!m_pClient->m_aClients[ClientID].m_IsBot)
			{
				if(CountryW > 0.0f)
				{
					vec4 FlagColor(1.0f, 1.0f, 1.0f, 0.68f*Dim);
					const float FlagH = min(24.0f, RowHeight-10.0f);
					m_pClient->m_pCountryFlags->Render(m_pClient->m_aClients[ClientID].m_Country, &FlagColor,
						CountryX+5.0f, RowY+(RowHeight-FlagH)*0.5f, CountryW-10.0f, FlagH);
				}
				str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Latency, 0, 1000));
				const vec4 PingColor = pInfo->m_Latency >= 180 ? Danger : (pInfo->m_Latency >= 80 ? Accent : Text);
				TextRender()->TextColor(PingColor.r*Dim, PingColor.g*Dim, PingColor.b*Dim, 1.0f);
				tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
				TextRender()->Text(0, PingX+PingW-tw-4.0f, TextY, FontSize, aBuf, -1);
			}
			TextRender()->TextColor(1, 1, 1, 1);
		}
	}
	return h;
}

void CScoreboard::RenderRecordingNotification(float x)
{
	if(!m_pClient->DemoRecorder()->IsRecording())
		return;

	//draw the box
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	vec4 Panel = CMenus::ThemeBgPanel();
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.90f);
	RenderTools()->DrawRoundRectExt(x, 0.0f, 180.0f, 50.0f, 15.0f, CUI::CORNER_B);
	Graphics()->QuadsEnd();

	//draw the red dot
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	RenderTools()->DrawRoundRect(x+20, 15.0f, 20.0f, 20.0f, 10.0f);
	Graphics()->QuadsEnd();

	//draw the text
	char aBuf[64];
	int Seconds = m_pClient->DemoRecorder()->Length();
	str_format(aBuf, sizeof(aBuf), Localize("REC %3d:%02d"), Seconds/60, Seconds%60);
	TextRender()->Text(0, x+50.0f, 10.0f, 20.0f, aBuf, -1);
}

void CScoreboard::OnRender()
{
	if(!Active())
		return;
	
	if (m_pClient->m_pGameVoteDisplay->IsActive())
		return;
	if (m_pClient->m_pPveRoguelite->ChoiceActive())
		return;
	
	// if the score board is active, then we should clear the motd message aswell
	if(m_pClient->m_pMotd->IsActive())
		m_pClient->m_pMotd->Clear();


	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;

	Graphics()->MapScreen(0, 0, Width, Height);
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.01f, 0.015f, 0.02f, 0.42f);
	IGraphics::CQuadItem Backdrop(0, 0, Width, Height);
	Graphics()->QuadsDrawTL(&Backdrop, 1);
	Graphics()->QuadsEnd();

	const bool Teams = m_pClient->m_Snap.m_pGameInfoObj && (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS);
	const float Gap = 14.0f;
	const float w = Teams ? clamp((Width-120.0f-Gap)*0.5f, 560.0f, 720.0f) : clamp(Width*0.56f, 760.0f, 920.0f);
	const float PanelY = 92.0f;
	float ScoreboardHeight = 0.0f;
	float PanelLeft = Width*0.5f-w*0.5f;
	float TotalWidth = w;

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS))
			ScoreboardHeight = RenderScoreboard(PanelLeft, PanelY, w, 0, 0);
		else
		{
			const char *pRedClanName = GetClanName(TEAM_RED);
			const char *pBlueClanName = GetClanName(TEAM_BLUE);

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER && m_pClient->m_Snap.m_pGameDataObj)
			{
				char aText[256];
				str_copy(aText, Localize("Draw!"), sizeof(aText));

				if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue)
				{
					if(pRedClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pRedClanName);
					else
						str_copy(aText, Localize("Red team wins!"), sizeof(aText));
				}
				else if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed)
				{
					if(pBlueClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pBlueClanName);
					else
						str_copy(aText, Localize("Blue team wins!"), sizeof(aText));
				}

				const vec4 Accent = CMenus::ThemeAccent();
				TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
				float TextW = TextRender()->TextWidth(0, 48.0f, aText, -1);
				TextRender()->Text(0, Width/2-TextW/2, 25.0f, 48.0f, aText, -1);
				TextRender()->TextColor(1, 1, 1, 1);
			}

			PanelLeft = Width*0.5f-w-Gap*0.5f;
			TotalWidth = 2.0f*w+Gap;
			float RedHeight = 0.0f;
			float BlueHeight = 0.0f;
			if (m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_INFECTION)
			{
				RedHeight = RenderScoreboard(PanelLeft, PanelY, w, TEAM_RED, Localize("The Living"));
				BlueHeight = RenderScoreboard(PanelLeft+w+Gap, PanelY, w, TEAM_BLUE, Localize("The Dead"));
			}
			else
			{
				RedHeight = RenderScoreboard(PanelLeft, PanelY, w, TEAM_RED, pRedClanName ? pRedClanName : Localize("Red team"));
				BlueHeight = RenderScoreboard(PanelLeft+w+Gap, PanelY, w, TEAM_BLUE, pBlueClanName ? pBlueClanName : Localize("Blue team"));
			}
			ScoreboardHeight = max(RedHeight, BlueHeight);
		}
	}

	const float FooterW = min(920.0f, TotalWidth);
	const float FooterX = Width*0.5f-FooterW*0.5f;
	float FooterY = PanelY+ScoreboardHeight+12.0f;
	const float GoalsHeight = RenderGoals(FooterX, FooterY, FooterW);
	if(GoalsHeight > 0.0f)
		FooterY += GoalsHeight+10.0f;
	const float SpectatorHeight = RenderSpectators(FooterX, FooterY, FooterW);
	if(SpectatorHeight > 0.0f)
		FooterY += SpectatorHeight+10.0f;
	RenderRecordingNotification((Width/7)*4);

	// server & map info
	{
		CServerInfo Info = {0};
		Client()->GetServerInfo(&Info);
		const vec4 Panel = CMenus::ThemeBgPanel();
		const vec4 Inset = CMenus::ThemeBgInset();
		const vec4 Accent = CMenus::ThemeAccent();
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.94f);
		RenderTools()->DrawRoundRect(FooterX, FooterY, FooterW, 40.0f, 11.0f);
		Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.62f);
		RenderTools()->DrawRoundRect(FooterX+8.0f, FooterY+7.0f, FooterW-16.0f, 26.0f, 8.0f);
		Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.90f);
		RenderTools()->DrawRoundRect(FooterX, FooterY+8.0f, 3.0f, 24.0f, 1.5f);
		Graphics()->QuadsEnd();
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.66f);
		TextRender()->Text(0, FooterX+18.0f, FooterY+11.0f, 16.0f, Info.m_aName[0] ? Info.m_aName : "-", FooterW*0.55f);
		char aMap[192];
		str_format(aMap, sizeof(aMap), "%s: %s", Localize("Map"), Info.m_aMap[0] ? Info.m_aMap : "-");
		const float MapW = TextRender()->TextWidth(0, 16.0f, aMap, -1);
		TextRender()->Text(0, FooterX+FooterW-MapW-18.0f, FooterY+11.0f, 16.0f, aMap, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
	FooterY += 40.0f;
	m_TotalRect.x = min(PanelLeft, FooterX);
	m_TotalRect.w = max(PanelLeft+TotalWidth, FooterX+FooterW)-m_TotalRect.x;
	m_TotalRect.y = PanelY;
	m_TotalRect.h = FooterY-PanelY;
	m_pClient->m_pHud->RenderObjective();
}

bool CScoreboard::Active()
{
	if(m_DebugActive)
		return true;
	// if we activly wanna look on the scoreboard
	if(m_Active)
		return true;

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
	{
		// we are not a spectator, check if we are dead
		//if(!m_pClient->m_Snap.m_pLocalCharacter)
		//	return true;
	}

	// if the game is over
	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetClanName(int Team)
{
	int ClanPlayers = 0;
	const char *pClanName = 0;
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;
		if(pInfo->m_ClientID < 0 || pInfo->m_ClientID >= MAX_CLIENTS || m_pClient->m_aClients[pInfo->m_ClientID].m_IsBot)
			continue;

		if(!pClanName)
		{
			pClanName = m_pClient->m_aClients[pInfo->m_ClientID].m_aClan;
			ClanPlayers++;
		}
		else
		{
			if(str_comp(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, pClanName) == 0)
				ClanPlayers++;
			else
				return 0;
		}
	}

	if(ClanPlayers > 1 && pClanName[0])
		return pClanName;
	else
		return 0;
}
