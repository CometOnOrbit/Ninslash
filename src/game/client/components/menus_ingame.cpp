

#include <base/math.h>

#include <engine/config.h>
#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/localization.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "menus.h"
#include "motd.h"
#include "voting.h"

void CMenus::RenderGame(CUIRect MainView)
{
	CUIRect Button, ButtonBar;
	MainView.HSplitTop(40.0f, &ButtonBar, &MainView);
	DrawMenuPanel(&ButtonBar, CUI::CORNER_ALL);

	// button bar
	ButtonBar.HSplitTop(8.0f, 0, &ButtonBar);
	ButtonBar.HSplitTop(24.0f, &ButtonBar, 0);
	ButtonBar.VMargin(8.0f, &ButtonBar);

	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Button);
	static int s_DisconnectButton = 0;
	if(DoButton_Menu(&s_DisconnectButton, Localize("Disconnect"), 0, &Button, BUTTONSTYLE_DANGER))
		Client()->Disconnect();

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			ButtonBar.VSplitLeft(8.0f, 0, &ButtonBar);
			ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
			static int s_SpectateButton = 0;
			if(DoButton_Menu(&s_SpectateButton, Localize("Spectate"), 0, &Button))
			{
				m_pClient->SendSwitchTeam(TEAM_SPECTATORS);
				SetActive(false);
			}
		}

		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION))
		{
			if (m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_RED)
			{
				ButtonBar.VSplitLeft(8.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
				static int s_SpectateButton = 0;
				if(DoButton_Menu(&s_SpectateButton, Localize("Join red"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_RED);
					SetActive(false);
				}
			}

			if (m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_BLUE)
			{
				ButtonBar.VSplitLeft(8.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
				static int s_SpectateButton = 0;
				if (DoButton_Menu(&s_SpectateButton, Localize("Join blue"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_BLUE);
					SetActive(false);
				}
			}
		}
		else
		{
			if (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION)
			{
				if (m_pClient->m_Snap.m_pLocalInfo->m_Team == TEAM_SPECTATORS)
				{
					ButtonBar.VSplitLeft(8.0f, 0, &ButtonBar);
					ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
					static int s_SpectateButton = 0;
					if(DoButton_Menu(&s_SpectateButton, Localize("Join game"), 0, &Button, BUTTONSTYLE_ACCENT))
					{
						m_pClient->SendSwitchTeam(0);
						SetActive(false);
					}
				}
			}
			else
			if (m_pClient->m_Snap.m_pLocalInfo->m_Team != 0)
			{
				ButtonBar.VSplitLeft(8.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(110.0f, &Button, &ButtonBar);
				static int s_SpectateButton = 0;
				if(DoButton_Menu(&s_SpectateButton, Localize("Join game"), 0, &Button, BUTTONSTYLE_ACCENT))
				{
					m_pClient->SendSwitchTeam(0);
					SetActive(false);
				}
			}
		}
	}
}

void CMenus::RenderPlayers(CUIRect MainView)
{
	CUIRect Button, ButtonBar, Options, Player;
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);

	// player options
	MainView.Margin(8.0f, &Options);
	DrawMenuInset(&Options, CUI::CORNER_ALL);
	Options.Margin(8.0f, &Options);
	Options.HSplitTop(22.0f, &Button, &Options);
	UI()->DoLabelScaled(&Button, Localize("Player options"), 14.0f, -1);
	DrawAccentUnderline(&Button);

	// headline
	Options.HSplitTop(8.0f, 0, &Options);
	Options.HSplitTop(22.0f, &ButtonBar, &Options);
	ButtonBar.VSplitRight(200.0f, &Player, &ButtonBar);
	UI()->DoLabelScaled(&Player, Localize("Player"), 12.0f, -1);

	ButtonBar.HMargin(1.0f, &ButtonBar);
	float Width = ButtonBar.h*2.0f;
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIICONS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_GUIICON_MUTE);
	IGraphics::CQuadItem QuadItem(Button.x, Button.y, Button.w, Button.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	ButtonBar.VSplitLeft(16.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIICONS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_GUIICON_FRIEND);
	QuadItem = IGraphics::CQuadItem(Button.x, Button.y, Button.w, Button.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// options
	static int s_aPlayerIDs[MAX_CLIENTS][2] = {{0}};
	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_pClient->m_Snap.m_paInfoByTeam[i])
			continue;

		// hide bots
		if(m_pClient->m_aClients[i].m_IsBot)
			continue;

		int Index = m_pClient->m_Snap.m_paInfoByTeam[i]->m_ClientID;
		if(Index == m_pClient->m_Snap.m_LocalClientID)
			continue;

		Options.HSplitTop(22.0f, &ButtonBar, &Options);
		if(Count++%2 == 0)
			RenderTools()->DrawUIRect(&ButtonBar, vec4(0.06f, 0.07f, 0.09f, 0.55f), CUI::CORNER_ALL, ms_ControlRounding);
		ButtonBar.VSplitRight(200.0f, &Player, &ButtonBar);

		// player info
		Player.VSplitLeft(28.0f, &Button, &Player);

		Player.HSplitTop(1.5f, 0, &Player);
		Player.VSplitMid(&Player, &Button);
		CTextCursor Cursor;
		char aNameBuf[MAX_NAME_LENGTH];
		TextRender()->SetCursor(&Cursor, Player.x, Player.y, 11.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Player.w;
		TextRender()->TextEx(&Cursor, m_pClient->GetPlayerLabel(Index, aNameBuf, sizeof(aNameBuf)), -1);

		TextRender()->SetCursor(&Cursor, Button.x,Button.y, 11.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Button.w;
		if(g_Config.m_ClShowsocial)
			TextRender()->TextEx(&Cursor, m_pClient->m_aClients[Index].m_aClan, -1);

		// ignore button
		ButtonBar.HMargin(2.0f, &ButtonBar);
		ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
		Button.VSplitLeft((Width-Button.h)/4.0f, 0, &Button);
		Button.VSplitLeft(Button.h, &Button, 0);
		if(m_pClient->EffectiveFilterChat() == 1 && !m_pClient->m_aClients[Index].m_Friend)
			DoButton_Toggle(&s_aPlayerIDs[Index][0], 1, &Button, false);
		else
			if(DoButton_Toggle(&s_aPlayerIDs[Index][0], m_pClient->m_aClients[Index].m_ChatIgnore, &Button, true))
				m_pClient->m_aClients[Index].m_ChatIgnore ^= 1;

		// friend button
		ButtonBar.VSplitLeft(16.0f, &Button, &ButtonBar);
		ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
		Button.VSplitLeft((Width-Button.h)/4.0f, 0, &Button);
		Button.VSplitLeft(Button.h, &Button, 0);
		if(DoButton_Toggle(&s_aPlayerIDs[Index][1], m_pClient->m_aClients[Index].m_Friend, &Button, true))
		{
			if(m_pClient->m_aClients[Index].m_Friend)
				m_pClient->Friends()->RemoveFriend(m_pClient->m_aClients[Index].m_aName, m_pClient->m_aClients[Index].m_aClan);
			else
				m_pClient->Friends()->AddFriend(m_pClient->m_aClients[Index].m_aName, m_pClient->m_aClients[Index].m_aClan);
		}
	}
}

void CMenus::RenderServerInfo(CUIRect MainView)
{
	if(!m_pClient->m_Snap.m_pLocalInfo)
		return;

	// fetch server info
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);

	CUIRect View, ServerInfo, GameInfo, Motd;

	float x = 0.0f;
	float y = 0.0f;

	char aBuf[1024];

	MainView.Margin(8.0f, &View);

	View.HSplitTop(View.h/2/UI()->Scale()-5.0f, &ServerInfo, &Motd);
	ServerInfo.VSplitLeft(View.w/2/UI()->Scale()-5.0f, &ServerInfo, &GameInfo);
	DrawMenuInset(&ServerInfo, CUI::CORNER_ALL);
	DrawAccentUnderline(&ServerInfo);

	ServerInfo.Margin(6.0f, &ServerInfo);

	x = 4.0f;
	y = 0.0f;

	TextRender()->Text(0, ServerInfo.x+x, ServerInfo.y+y, 14, Localize("Server info"), 250);
	y += 18.0f;

	mem_zero(aBuf, sizeof(aBuf));
	str_format(
		aBuf,
		sizeof(aBuf),
		"%s\n\n"
		"%s: %s\n"
		"%s: %d\n"
		"%s: %s\n"
		"%s: %s\n",
		CurrentServerInfo.m_aName,
		Localize("Address"), g_Config.m_UiServerAddress,
		Localize("Ping"), m_pClient->m_Snap.m_pLocalInfo->m_Latency,
		Localize("Version"), CurrentServerInfo.m_aVersion,
		Localize("Password"), CurrentServerInfo.m_Flags &1 ? Localize("Yes") : Localize("No")
	);

	TextRender()->Text(0, ServerInfo.x+x, ServerInfo.y+y, 11, aBuf, 250);

	{
		CUIRect Button;
		int IsFavorite = ServerBrowser()->IsFavorite(CurrentServerInfo.m_NetAddr);
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		static int s_AddFavButton = 0;
		if(DoButton_CheckBox(&s_AddFavButton, Localize("Favorite"), IsFavorite, &Button))
		{
			if(IsFavorite)
				ServerBrowser()->RemoveFavorite(CurrentServerInfo.m_NetAddr);
			else
				ServerBrowser()->AddFavorite(CurrentServerInfo.m_NetAddr);
		}
	}

	GameInfo.VSplitLeft(8.0f, 0x0, &GameInfo);
	DrawMenuInset(&GameInfo, CUI::CORNER_ALL);
	DrawAccentUnderline(&GameInfo);

	GameInfo.Margin(6.0f, &GameInfo);

	x = 4.0f;
	y = 0.0f;

	TextRender()->Text(0, GameInfo.x+x, GameInfo.y+y, 14, Localize("Game info"), 250);
	y += 18.0f;

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		mem_zero(aBuf, sizeof(aBuf));
		str_format(
			aBuf,
			sizeof(aBuf),
			"\n\n"
			"%s: %s\n"
			"%s: %s\n"
			"%s: %d\n"
			"%s: %d\n"
			"\n"
			"%s: %d/%d\n",
			Localize("Game type"), CurrentServerInfo.m_aGameType,
			Localize("Map"), CurrentServerInfo.m_aMap,
			Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit,
			Localize("Time limit"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit,
			Localize("Players"), m_pClient->m_Snap.m_NumPlayers, CurrentServerInfo.m_MaxClients
		);
		TextRender()->Text(0, GameInfo.x+x, GameInfo.y+y, 11, aBuf, 250);
	}

	Motd.HSplitTop(8.0f, 0, &Motd);
	DrawMenuInset(&Motd, CUI::CORNER_ALL);
	DrawAccentUnderline(&Motd);
	Motd.Margin(6.0f, &Motd);
	y = 0.0f;
	x = 4.0f;
	TextRender()->Text(0, Motd.x+x, Motd.y+y, 14, Localize("MOTD"), -1);
	y += 18.0f;
	TextRender()->Text(0, Motd.x+x, Motd.y+y, 11, m_pClient->m_pMotd->m_aServerMotd, (int)Motd.w);
}

void CMenus::RenderServerControlServer(CUIRect MainView)
{
	static int s_VoteList = 0;
	static float s_ScrollValue = 0;
	CUIRect List = MainView;
	UiDoListboxStart(&s_VoteList, &List, 18.0f, "", "", m_pClient->m_pVoting->m_NumVoteOptions, 1, m_CallvoteSelectedOption, s_ScrollValue);

	for(CVoteOptionClient *pOption = m_pClient->m_pVoting->m_pFirst; pOption; pOption = pOption->m_pNext)
	{
		CListboxItem Item = UiDoListboxNextItem(pOption);

		if(Item.m_Visible)
			UI()->DoLabelScaled(&Item.m_Rect, pOption->m_aDescription, 12.0f, -1);
	}

	m_CallvoteSelectedOption = UiDoListboxEnd(&s_ScrollValue, 0);
}

void CMenus::RenderServerControlKick(CUIRect MainView, bool FilterSpectators)
{
	int NumOptions = 0;
	int Selected = -1;
	static int aPlayerIDs[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_paInfoByTeam[i])
			continue;

		int Index = m_pClient->m_Snap.m_paInfoByTeam[i]->m_ClientID;
		if(Index == m_pClient->m_Snap.m_LocalClientID || (FilterSpectators && m_pClient->m_Snap.m_paInfoByTeam[i]->m_Team == TEAM_SPECTATORS))
			continue;
		if(m_CallvoteSelectedPlayer == Index)
			Selected = NumOptions;
		aPlayerIDs[NumOptions++] = Index;
	}

	static int s_VoteList = 0;
	static float s_ScrollValue = 0;
	CUIRect List = MainView;
	UiDoListboxStart(&s_VoteList, &List, 18.0f, "", "", NumOptions, 1, Selected, s_ScrollValue);

	for(int i = 0; i < NumOptions; i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&aPlayerIDs[i]);

		if(Item.m_Visible)
		{
			CTeeRenderInfo Info = m_pClient->m_aClients[aPlayerIDs[i]].m_RenderInfo;
			Info.m_Size = Item.m_Rect.h;
			Item.m_Rect.HSplitTop(2.0f, 0, &Item.m_Rect);
			Item.m_Rect.x +=Info.m_Size;
			char aNameBuf[MAX_NAME_LENGTH];
			UI()->DoLabelScaled(&Item.m_Rect, m_pClient->GetPlayerLabel(aPlayerIDs[i], aNameBuf, sizeof(aNameBuf)), 12.0f, -1);
		}
	}

	Selected = UiDoListboxEnd(&s_ScrollValue, 0);
	m_CallvoteSelectedPlayer = Selected != -1 ? aPlayerIDs[Selected] : -1;
}

void CMenus::RenderServerControl(CUIRect MainView)
{
	static int s_ControlPage = 0;

	CUIRect Bottom, Extended, TabBar, Button;
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.HSplitTop(4.0f, 0, &MainView);
	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.Margin(8.0f, &MainView);
	MainView.HSplitBottom(90.0f, &MainView, &Extended);

	// tab bar
	{
		TabBar.VSplitLeft(TabBar.w/3, &Button, &TabBar);
		static int s_Button0 = 0;
		if(DoButton_MenuTab(&s_Button0, Localize("Change settings"), s_ControlPage == 0, &Button, CUI::CORNER_TL))
			s_ControlPage = 0;

		TabBar.VSplitMid(&Button, &TabBar);
		static int s_Button1 = 0;
		if(DoButton_MenuTab(&s_Button1, Localize("Kick player"), s_ControlPage == 1, &Button, 0))
			s_ControlPage = 1;

		static int s_Button2 = 0;
		if(DoButton_MenuTab(&s_Button2, Localize("Move player to spectators"), s_ControlPage == 2, &TabBar, CUI::CORNER_TR))
			s_ControlPage = 2;
	}

	DrawMenuInset(&MainView, CUI::CORNER_ALL);

	// render page
	MainView.HSplitBottom(ms_ButtonHeight + 5*2, &MainView, &Bottom);
	Bottom.HMargin(5.0f, &Bottom);

	if(s_ControlPage == 0)
		RenderServerControlServer(MainView);
	else if(s_ControlPage == 1)
		RenderServerControlKick(MainView, false);
	else if(s_ControlPage == 2)
		RenderServerControlKick(MainView, true);

	// vote menu
	{
		CUIRect Button;
		Bottom.VSplitRight(120.0f, &Bottom, &Button);

		static int s_CallVoteButton = 0;
		if(DoButton_Menu(&s_CallVoteButton, Localize("Call vote"), 0, &Button))
		{
			if(s_ControlPage == 0)
				m_pClient->m_pVoting->CallvoteOption(m_CallvoteSelectedOption, m_aCallvoteReason);
			else if(s_ControlPage == 1)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_paPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_pVoting->CallvoteKick(m_CallvoteSelectedPlayer, m_aCallvoteReason);
					SetActive(false);
				}
			}
			else if(s_ControlPage == 2)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_paPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_pVoting->CallvoteSpectate(m_CallvoteSelectedPlayer, m_aCallvoteReason);
					SetActive(false);
				}
			}
			m_aCallvoteReason[0] = 0;
		}

		// render kick reason
		CUIRect Reason;
		Bottom.VSplitRight(40.0f, &Bottom, 0);
		Bottom.VSplitRight(160.0f, &Bottom, &Reason);
		Reason.HSplitTop(5.0f, 0, &Reason);
		const char *pLabel = Localize("Reason:");
		UI()->DoLabelScaled(&Reason, pLabel, 14.0f, -1);
		float w = TextRender()->TextWidth(0, 14.0f, pLabel, -1);
		Reason.VSplitLeft(w+10.0f, 0, &Reason);
		static float s_Offset = 0.0f;
		DoEditBox(&m_aCallvoteReason, &Reason, m_aCallvoteReason, sizeof(m_aCallvoteReason), 14.0f, &s_Offset, false, CUI::CORNER_ALL);

		// extended features (only available when authed in rcon)
		if(Client()->RconAuthed())
		{
			// background
			Extended.Margin(10.0f, &Extended);
			Extended.HSplitTop(20.0f, &Bottom, &Extended);
			Extended.HSplitTop(5.0f, 0, &Extended);

			// force vote
			Bottom.VSplitLeft(5.0f, 0, &Bottom);
			Bottom.VSplitLeft(120.0f, &Button, &Bottom);
			static int s_ForceVoteButton = 0;
			if(DoButton_Menu(&s_ForceVoteButton, Localize("Force vote"), 0, &Button))
			{
				if(s_ControlPage == 0)
					m_pClient->m_pVoting->CallvoteOption(m_CallvoteSelectedOption, m_aCallvoteReason, true);
				else if(s_ControlPage == 1)
				{
					if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
						m_pClient->m_Snap.m_paPlayerInfos[m_CallvoteSelectedPlayer])
					{
						m_pClient->m_pVoting->CallvoteKick(m_CallvoteSelectedPlayer, m_aCallvoteReason, true);
						SetActive(false);
					}
				}
				else if(s_ControlPage == 2)
				{
					if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
						m_pClient->m_Snap.m_paPlayerInfos[m_CallvoteSelectedPlayer])
					{
						m_pClient->m_pVoting->CallvoteSpectate(m_CallvoteSelectedPlayer, m_aCallvoteReason, true);
						SetActive(false);
					}
				}
				m_aCallvoteReason[0] = 0;
			}

			if(s_ControlPage == 0)
			{
				// remove vote
				Bottom.VSplitRight(10.0f, &Bottom, 0);
				Bottom.VSplitRight(120.0f, 0, &Button);
				static int s_RemoveVoteButton = 0;
				if(DoButton_Menu(&s_RemoveVoteButton, Localize("Remove"), 0, &Button))
					m_pClient->m_pVoting->RemovevoteOption(m_CallvoteSelectedOption);


				// add vote
				Extended.HSplitTop(20.0f, &Bottom, &Extended);
				Bottom.VSplitLeft(5.0f, 0, &Bottom);
				Bottom.VSplitLeft(250.0f, &Button, &Bottom);
				UI()->DoLabelScaled(&Button, Localize("Vote description:"), 14.0f, -1);

				Bottom.VSplitLeft(20.0f, 0, &Button);
				UI()->DoLabelScaled(&Button, Localize("Vote command:"), 14.0f, -1);

				static char s_aVoteDescription[64] = {0};
				static char s_aVoteCommand[512] = {0};
				Extended.HSplitTop(20.0f, &Bottom, &Extended);
				Bottom.VSplitRight(10.0f, &Bottom, 0);
				Bottom.VSplitRight(120.0f, &Bottom, &Button);
				static int s_AddVoteButton = 0;
				if(DoButton_Menu(&s_AddVoteButton, Localize("Add"), 0, &Button))
					if(s_aVoteDescription[0] != 0 && s_aVoteCommand[0] != 0)
						m_pClient->m_pVoting->AddvoteOption(s_aVoteDescription, s_aVoteCommand);

				Bottom.VSplitLeft(5.0f, 0, &Bottom);
				Bottom.VSplitLeft(250.0f, &Button, &Bottom);
				static float s_OffsetDesc = 0.0f;
				DoEditBox(&s_aVoteDescription, &Button, s_aVoteDescription, sizeof(s_aVoteDescription), 14.0f, &s_OffsetDesc, false, CUI::CORNER_ALL);

				Bottom.VMargin(20.0f, &Button);
				static float s_OffsetCmd = 0.0f;
				DoEditBox(&s_aVoteCommand, &Button, s_aVoteCommand, sizeof(s_aVoteCommand), 14.0f, &s_OffsetCmd, false, CUI::CORNER_ALL);
			}
		}
	}
}
