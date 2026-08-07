

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>

#include <game/client/components/scoreboard.h>
#include <game/client/components/sounds.h>
#include <game/localization.h>

#include <game/client/customstuff.h>
#include <game/client/customstuff/playerinfo.h>

#include <generated/game_data.h>

#include "chat.h"
#include "hud_layout.h"
#include "inventory.h"

CChat::CChat()
{
	ClearLines();
	OnReset();
}

void CChat::OnInit()
{
}

void CChat::ClearLines()
{
	for(int i = 0; i < MAX_LINES; i++)
	{
		m_aLines[i].m_Time = 0;
		m_aLines[i].m_aText[0] = 0;
		m_aLines[i].m_aName[0] = 0;
	}
	m_CurrentLine = 0;
}

void CChat::OnReset()
{
	m_Mode = MODE_NONE;
	m_Show = false;
	m_Filtered = false;
	m_InputUpdate = false;
	m_ChatStringOffset = 0;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = 0x0;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_WhisperTarget = -1;
	m_LastWhisperFrom = -1;

	for(int i = 0; i < CHAT_NUM; ++i)
		m_aLastSoundPlayed[i] = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_CONNECTING && OldState != IClient::STATE_CONNECTING)
	{
		m_Mode = MODE_NONE;
		ClearLines();
	}
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Say(MODE_ALL, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Say(MODE_TEAM, pResult->GetString(0));
}

void CChat::ConWhisper(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->EnableMode(MODE_WHISPER);
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat *)pUserData)->EnableMode(MODE_ALL);
	else if(str_comp(pMode, "team") == 0)
		((CChat *)pUserData)->EnableMode(MODE_TEAM);
	else if(str_comp(pMode, "whisper") == 0)
		((CChat *)pUserData)->EnableMode(MODE_WHISPER);
	else
		((CChat *)pUserData)
			->Console()
			->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all, team or whisper as mode");
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("whisper", "", CFGFLAG_CLIENT, ConWhisper, this, "Open whisper chat");
	Console()->Register("chat", "s", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team/whisper mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
}

bool CChat::OnInput(IInput::CEvent Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_GAMEPAD_BUTTON_B))
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
		m_pClient->OnRelease();
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_Input.GetString()[0])
		{
			bool AddEntry = false;

			if(m_LastChatSend + time_freq() < time_get())
			{
				int Target = m_Mode == MODE_WHISPER ? m_WhisperTarget : -1;
				Say(m_Mode, m_Input.GetString(), Target);
				AddEntry = true;
			}
			else if(m_PendingChatCounter < 3)
			{
				++m_PendingChatCounter;
				AddEntry = true;
			}

			if(AddEntry)
			{
				CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + m_Input.GetLength());
				pEntry->m_Mode = m_Mode;
				pEntry->m_Target = m_Mode == MODE_WHISPER ? m_WhisperTarget : -1;
				mem_copy(pEntry->m_aText, m_Input.GetString(), m_Input.GetLength() + 1);
			}
		}
		m_pHistoryEntry = 0x0;
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
		m_pClient->OnRelease();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		if(m_Mode == MODE_WHISPER)
		{
			int Start = m_WhisperTarget < 0 ? 0 : m_WhisperTarget + 1;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				int Index = (Start + i) % MAX_CLIENTS;
				if(Index != m_pClient->m_Snap.m_LocalClientID && m_pClient->m_Snap.m_paPlayerInfos[Index])
				{
					m_WhisperTarget = Index;
					break;
				}
			}
		}
		else
		{
			// fill the completion buffer
			if(m_CompletionChosen < 0)
			{
				const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
				for(int Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
					;
				m_PlaceholderOffset = pCursor - m_Input.GetString();

				for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
					++m_PlaceholderLength;

				str_copy(m_aCompletionBuffer,
						 m_Input.GetString() + m_PlaceholderOffset,
						 min(static_cast<int>(sizeof(m_aCompletionBuffer)), m_PlaceholderLength + 1));
			}

			// find next possible name
			const char *pCompletionString = 0;
			m_CompletionChosen = (m_CompletionChosen + 1) % (2 * MAX_CLIENTS);
			for(int i = 0; i < 2 * MAX_CLIENTS; ++i)
			{
				int SearchType = ((m_CompletionChosen + i) % (2 * MAX_CLIENTS)) / MAX_CLIENTS;
				int Index = (m_CompletionChosen + i) % MAX_CLIENTS;
				if(!m_pClient->m_Snap.m_paPlayerInfos[Index])
					continue;

				bool Found = false;
				if(SearchType == 1)
				{
					if(str_comp_nocase_num(m_pClient->m_aClients[Index].m_aName,
										   m_aCompletionBuffer,
										   str_length(m_aCompletionBuffer)) &&
					   str_find_nocase(m_pClient->m_aClients[Index].m_aName, m_aCompletionBuffer))
						Found = true;
				}
				else if(!str_comp_nocase_num(
							m_pClient->m_aClients[Index].m_aName, m_aCompletionBuffer, str_length(m_aCompletionBuffer)))
					Found = true;

				if(Found)
				{
					pCompletionString = m_pClient->m_aClients[Index].m_aName;
					m_CompletionChosen = Index + SearchType * MAX_CLIENTS;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[256];
				// add part before the name
				str_copy(aBuf, m_Input.GetString(), min(static_cast<int>(sizeof(aBuf)), m_PlaceholderOffset + 1));

				// add the name
				str_append(aBuf, pCompletionString, sizeof(aBuf));

				// add seperator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator, sizeof(aBuf));

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength, sizeof(aBuf));

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_OldChatStringLength = m_Input.GetLength();
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
				m_InputUpdate = true;
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB)
			m_CompletionChosen = -1;

		m_OldChatStringLength = m_Input.GetLength();
		m_Input.ProcessInput(Event);
		m_InputUpdate = true;
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
			m_pHistoryEntry = m_History.Last();

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
		else
			m_Input.Clear();
	}

	return true;
}

void CChat::EnableMode(int Mode)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(Mode == MODE_WHISPER && g_Config.m_ClDisableWhisper)
		return;

	if(m_Mode == MODE_NONE)
	{
		m_Mode = Mode;

		if(Mode == MODE_WHISPER)
		{
			m_WhisperTarget = -1;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(i != m_pClient->m_Snap.m_LocalClientID && m_pClient->m_Snap.m_paPlayerInfos[i])
				{
					m_WhisperTarget = i;
					break;
				}
			}
		}

		m_Input.Clear();
		m_Input.Activate(CHAT);
		Input()->ClearEvents();
		m_CompletionChosen = -1;
	}
	else
		m_Input.Activate(CHAT);
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		if(pMsg->m_Mode == CHATMODE_WHISPER)
		{
			if(g_Config.m_ClDisableWhisper)
				return;
			if(pMsg->m_ClientID >= 0 && pMsg->m_ClientID != m_pClient->m_Snap.m_LocalClientID)
				m_LastWhisperFrom = pMsg->m_ClientID;
		}

		AddLine(pMsg->m_ClientID, pMsg->m_Mode, pMsg->m_pMessage);
	}
}

void CChat::AddLine(int ClientID, int Mode, const char *pLine)
{
	const int FilterChat = m_pClient->EffectiveFilterChat();
	if(*pLine == 0 || (ClientID != -1 && (m_pClient->m_aClients[ClientID].m_aName[0] == '\0' || // unknown client
										  m_pClient->m_aClients[ClientID].m_ChatIgnore || FilterChat == 2 ||
										  (m_pClient->m_Snap.m_LocalClientID != ClientID && FilterChat == 1 &&
										   !m_pClient->m_aClients[ClientID].m_Friend))))
		return;

	// trim right and set maximum length to 128 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = 0;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) &&
		   (Code < 0x2028 || Code > 0x202F) && (Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) &&
		   (Code < 0xFE00 || Code > 0xFE0F) && Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
		{
			pEnd = 0;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;

		if(++Length >= 127)
		{
			*(const_cast<char *>(pStr)) = 0;
			break;
		}
	}
	if(pEnd != 0)
		*(const_cast<char *>(pEnd)) = 0;

	bool Highlighted = false;
	char *p = const_cast<char *>(pLine);
	while(*p)
	{
		Highlighted = false;
		pLine = p;
		// find line seperator and strip multiline
		while(*p)
		{
			if(*p++ == '\n')
			{
				*(p - 1) = 0;
				break;
			}
		}

		m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;
		m_aLines[m_CurrentLine].m_Time = time_get();
		m_aLines[m_CurrentLine].m_YOffset[0] = -1.0f;
		m_aLines[m_CurrentLine].m_YOffset[1] = -1.0f;
		m_aLines[m_CurrentLine].m_ClientID = ClientID;
		m_aLines[m_CurrentLine].m_Mode = Mode;
		m_aLines[m_CurrentLine].m_NameColor = -2;

		// check for highlighted name
		const char *pHL = str_find_nocase(pLine, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName);
		if(pHL)
		{
			int Length = str_length(m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName);
			if((pLine == pHL || pHL[-1] == ' ') &&
			   (pHL[Length] == 0 || pHL[Length] == ' ' || (pHL[Length] == ':' && pHL[Length + 1] == ' ')))
				Highlighted = true;
		}
		m_aLines[m_CurrentLine].m_Highlighted = Highlighted;

		if(ClientID == -1) // server message
		{
			str_copy(m_aLines[m_CurrentLine].m_aName, "*** ", sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), "%s", pLine);
		}
		else
		{
			char aNameBuf[MAX_NAME_LENGTH];
			if(m_pClient->m_aClients[ClientID].m_Team == TEAM_SPECTATORS)
				m_aLines[m_CurrentLine].m_NameColor = TEAM_SPECTATORS;

			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION)
			{
				if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
			}
			else if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
			{
				if(m_pClient->m_aClients[ClientID].m_Team == TEAM_RED)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_RED;
				else if(m_pClient->m_aClients[ClientID].m_Team == TEAM_BLUE)
					m_aLines[m_CurrentLine].m_NameColor = TEAM_BLUE;
			}

			str_copy(m_aLines[m_CurrentLine].m_aName,
					 m_pClient->GetPlayerLabel(ClientID, aNameBuf, sizeof(aNameBuf)),
					 sizeof(m_aLines[m_CurrentLine].m_aName));
			str_format(m_aLines[m_CurrentLine].m_aText, sizeof(m_aLines[m_CurrentLine].m_aText), ": %s", pLine);
		}

		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s", m_aLines[m_CurrentLine].m_aName, m_aLines[m_CurrentLine].m_aText);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
						 m_aLines[m_CurrentLine].m_Mode == CHATMODE_TEAM
							 ? "teamchat"
							 : (m_aLines[m_CurrentLine].m_Mode == CHATMODE_WHISPER ? "whisper" : "chat"),
						 aBuf);
	}

	// play sound
	int64 Now = time_get();
	if(ClientID == -1)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 0);
			m_aLastSoundPlayed[CHAT_SERVER] = Now;
		}
	}
	else if(Highlighted)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0);
			m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;

			// desktop notification
			if(g_Config.m_ClShowNotifications)
			{
				char aBuf[768];
				str_format(aBuf,
						   sizeof(aBuf),
						   "notify-send \"Ninslash\" \"%s: %s\" --icon=ninslash &",
						   m_aLines[m_CurrentLine].m_aName,
						   m_aLines[m_CurrentLine].m_aText);
				if(system(aBuf))
				{ /* ignore result */
				}
			}
		}
	}
	else
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 0);
			m_aLastSoundPlayed[CHAT_CLIENT] = Now;
		}
	}
}

void CChat::OnRender()
{
	// if chat is disabled, don't render anything
	if(g_Config.m_ClShowChat == 0)
		return;

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time_get())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				Say(pEntry->m_Mode, pEntry->m_aText, pEntry->m_Target);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	if(m_pClient->m_pInventory->IsVisible())
		return;

	float Width = 300.0f * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, 300.0f);
	float x = 5.0f;
	float y = HudLayout::ChatInputTop(300.0f);
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		const char *pModeLabel = 0;
		vec4 ModeColor = vec4(1.0f, 1.0f, 1.0f, 0.35f);
		if(m_Mode == MODE_ALL)
		{
			pModeLabel = Localize("All");
			ModeColor = vec4(1.0f, 1.0f, 1.0f, 0.35f);
		}
		else if(m_Mode == MODE_TEAM)
		{
			pModeLabel = Localize("Team");
			ModeColor = vec4(0.2f, 0.8f, 0.2f, 0.35f);
		}
		else if(m_Mode == MODE_WHISPER)
		{
			pModeLabel = Localize("Whisper");
			ModeColor = vec4(0.2f, 0.8f, 0.8f, 0.35f);
		}
		else
			pModeLabel = Localize("Chat");

		float ModeLabelWidth = TextRender()->TextWidth(0, 8.0f, pModeLabel, -1);
		CUIRect ModeRect = {x, y, ModeLabelWidth + 4.0f, 8.0f};
		RenderTools()->DrawUIRect(&ModeRect, ModeColor, CUI::CORNER_ALL, 2.0f);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, x + 2.0f, y, 8.0f, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Width - 190.0f;
		Cursor.m_MaxLines = 2;

		TextRender()->TextEx(&Cursor, pModeLabel, -1);

		if(m_Mode == MODE_WHISPER && m_WhisperTarget >= 0)
		{
			char aTarget[128];
			str_format(aTarget, sizeof(aTarget), " (%s)", m_pClient->m_aClients[m_WhisperTarget].m_aName);
			TextRender()->TextEx(&Cursor, aTarget, -1);
		}

		TextRender()->TextEx(&Cursor, ": ", -1);

		// show filter status hint
		{
			m_Filtered = false;
			char aFilterHint[64] = {0};
			const int FilterChat = m_pClient->EffectiveFilterChat();
			if(FilterChat == 1)
			{
				str_copy(aFilterHint, "[Friends]", sizeof(aFilterHint));
				m_Filtered = true;
			}
			else if(FilterChat == 2)
			{
				str_copy(aFilterHint, "[No chat]", sizeof(aFilterHint));
				m_Filtered = true;
			}
			if(g_Config.m_ClShowChatTeamMembersOnly)
			{
				str_copy(aFilterHint, "[Team only]", sizeof(aFilterHint));
				m_Filtered = true;
			}
			if(!g_Config.m_ClShowChatSystem)
			{
				if(aFilterHint[0])
					str_append(aFilterHint, " ", sizeof(aFilterHint));
				str_append(aFilterHint, "[-Sys]", sizeof(aFilterHint));
				m_Filtered = true;
			}
			if(aFilterHint[0])
				TextRender()->TextEx(&Cursor, aFilterHint, -1);
		}

		// check if the visible text has to be moved
		if(m_InputUpdate)
		{
			if(m_ChatStringOffset > 0 && m_Input.GetLength() < m_OldChatStringLength)
				m_ChatStringOffset = max(0, m_ChatStringOffset - (m_OldChatStringLength - m_Input.GetLength()));

			if(m_ChatStringOffset > m_Input.GetCursorOffset())
				m_ChatStringOffset -= m_ChatStringOffset - m_Input.GetCursorOffset();
			else
			{
				CTextCursor Temp = Cursor;
				Temp.m_Flags = 0;
				TextRender()->TextEx(
					&Temp, m_Input.GetString() + m_ChatStringOffset, m_Input.GetCursorOffset() - m_ChatStringOffset);
				TextRender()->TextEx(&Temp, "|", -1);
				while(Temp.m_LineCount > 2)
				{
					++m_ChatStringOffset;
					Temp = Cursor;
					Temp.m_Flags = 0;
					TextRender()->TextEx(&Temp,
										 m_Input.GetString() + m_ChatStringOffset,
										 m_Input.GetCursorOffset() - m_ChatStringOffset);
					TextRender()->TextEx(&Temp, "|", -1);
				}
			}
			m_InputUpdate = false;
		}

		TextRender()->TextEx(
			&Cursor, m_Input.GetString() + m_ChatStringOffset, m_Input.GetCursorOffset() - m_ChatStringOffset);
		CLineInput::SetCompositionWindowPosition(vec2(Cursor.m_X, Cursor.m_Y + 8.0f), 8.0f);
		if(Input()->HasComposition())
		{
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
			TextRender()->TextEx(&Cursor, Input()->GetComposition(), -1);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
		static float MarkerOffset = TextRender()->TextWidth(0, 8.0f, "|", -1) / 3;
		CTextCursor Marker = Cursor;
		Marker.m_X -= MarkerOffset;
		TextRender()->TextEx(&Marker, "|", -1);
		TextRender()->TextEx(&Cursor, m_Input.GetString() + m_Input.GetCursorOffset(), -1);
		m_Input.MarkRendered();
	}

	y -= 8.0f;

	int64 Now = time_get();
	float LineWidth = 200.0f;
	const bool ShowHistory = m_Show || m_Mode != MODE_NONE;
	float HeightLimit = (g_Config.m_ClShowChat == 2 || !ShowHistory) ? 200.0f : 50.0f;
	if(m_pClient->m_pScoreboard->Active())
	{
		const CUIRect &ScoreboardRect = m_pClient->m_pScoreboard->GetScoreboardRect();
		const float ScoreboardWidth = 400.0f * 3.0f * Graphics()->ScreenAspect();
		const float ScoreboardHeight = 400.0f * 3.0f;
		const float ScaleX = Width / ScoreboardWidth;
		const float ScaleY = 300.0f / ScoreboardHeight;

		const float SbLeft = ScoreboardRect.x * ScaleX;
		const float SbBottom = (ScoreboardRect.y + ScoreboardRect.h) * ScaleY;
		const float ReducedLineWidth = min(SbLeft - 5.0f - x, LineWidth);
		const float ReducedHeightLimit = max(SbBottom + 5.0f, HeightLimit);
		if(ReducedLineWidth * (y - HeightLimit) >= LineWidth * (y - ReducedHeightLimit))
			LineWidth = ReducedLineWidth;
		else
			HeightLimit = ReducedHeightLimit;
	}
	float Begin = x;
	float FontSize = 6.0f;
	CTextCursor Cursor;
	int OffsetType = m_pClient->m_pScoreboard->Active() ? 1 : 0;
	for(int i = 0; i < MAX_LINES; i++)
	{
		int r = ((m_CurrentLine - i) + MAX_LINES) % MAX_LINES;
		if(m_aLines[r].m_Time == 0)
			break;
		if(Now > m_aLines[r].m_Time + 16 * time_freq() && !ShowHistory)
			break;

		// get the y offset (calculate it if we haven't done that yet)
		if(m_aLines[r].m_YOffset[OffsetType] < 0.0f)
		{
			TextRender()->SetCursor(&Cursor, Begin, 0.0f, FontSize, 0);
			Cursor.m_LineWidth = LineWidth;
			TextRender()->TextEx(&Cursor, m_aLines[r].m_aName, -1);
			TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
			m_aLines[r].m_YOffset[OffsetType] = Cursor.m_Y + Cursor.m_FontSize;
		}
		y -= m_aLines[r].m_YOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		// filter messages based on settings
		if(g_Config.m_ClShowChatTeamMembersOnly && m_aLines[r].m_ClientID != -1)
		{
			if(m_pClient->m_aClients[m_aLines[r].m_ClientID].m_Team !=
			   m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Team)
				continue;
		}
		if(!g_Config.m_ClShowChatSystem && m_aLines[r].m_ClientID == -1)
			continue;

		float Blend = Now > m_aLines[r].m_Time + 14 * time_freq() && !ShowHistory
						  ? 1.0f - (Now - m_aLines[r].m_Time - 14 * time_freq()) / (2.0f * time_freq())
						  : 1.0f;

		// reset the cursor
		TextRender()->SetCursor(&Cursor, Begin, y, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;

		// render name
		if(m_aLines[r].m_ClientID == -1)
			TextRender()->TextColor(1.0f, 1.0f, 0.5f, Blend); // system
		else if(m_aLines[r].m_Mode == CHATMODE_TEAM)
			TextRender()->TextColor(0.45f, 0.9f, 0.45f, Blend); // team message
		else if(m_aLines[r].m_Mode == CHATMODE_WHISPER)
			TextRender()->TextColor(0.3f, 0.9f, 0.9f, Blend); // whisper message
		else if(m_aLines[r].m_NameColor == TEAM_RED)
			TextRender()->TextColor(1.0f, 0.4f, 0.1f, Blend); // red
		else if(m_aLines[r].m_NameColor == TEAM_BLUE)
		{
			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION)
				TextRender()->TextColor(0.5f, 0.5f, 0.5f, Blend); // gray
			else
				TextRender()->TextColor(0.1f, 0.4f, 1.0f, Blend); // blue
		}
		else if(m_aLines[r].m_NameColor == TEAM_SPECTATORS)
			TextRender()->TextColor(0.75f, 0.5f, 0.75f, Blend); // spectator
		else
			TextRender()->TextColor(0.8f, 0.8f, 0.8f, Blend);

		TextRender()->TextEx(&Cursor, m_aLines[r].m_aName, -1);

		// render line
		if(m_aLines[r].m_ClientID == -1)
			TextRender()->TextColor(1.0f, 1.0f, 0.5f, Blend); // system
		else if(m_aLines[r].m_Highlighted)
			TextRender()->TextColor(1.0f, 0.5f, 0.5f, Blend); // highlighted
		else if(m_aLines[r].m_Mode == CHATMODE_TEAM)
			TextRender()->TextColor(0.65f, 1.0f, 0.65f, Blend); // team message
		else if(m_aLines[r].m_Mode == CHATMODE_WHISPER)
			TextRender()->TextColor(0.5f, 1.0f, 1.0f, Blend); // whisper message
		else
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, Blend);

		TextRender()->TextEx(&Cursor, m_aLines[r].m_aText, -1);
	}

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CChat::Say(int Mode, const char *pLine, int Target)
{
	m_LastChatSend = time_get();

	const char *pMsg = pLine;
	int SendMode = Mode;
	int SendTarget = Target;

	if(pLine[0] == '/')
	{
		if(!str_comp_nocase_num(pLine, "/w ", 3) || !str_comp_nocase_num(pLine, "/whisper ", 10))
		{
			const char *pRest = pLine + (pLine[1] == 'w' ? 3 : 10);
			char aName[MAX_NAME_LENGTH];
			const char *pSpace = str_find(pRest, " ");
			if(!pSpace)
				return;
			int NameLen = (int)(pSpace - pRest);
			if(NameLen >= (int)sizeof(aName))
				NameLen = (int)sizeof(aName) - 1;
			str_copy(aName, pRest, NameLen + 1);
			pMsg = pSpace + 1;
			SendMode = MODE_WHISPER;
			SendTarget = -1;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_pClient->m_aClients[i].m_aName[0] && !str_comp_nocase(m_pClient->m_aClients[i].m_aName, aName))
				{
					SendTarget = i;
					break;
				}
			}
			if(SendTarget < 0)
				return;
		}
		else if(!str_comp_nocase_num(pLine, "/r ", 3))
		{
			if(m_LastWhisperFrom < 0)
				return;
			pMsg = pLine + 3;
			SendMode = MODE_WHISPER;
			SendTarget = m_LastWhisperFrom;
		}
	}

	if(g_Config.m_ClDisableWhisper && SendMode == MODE_WHISPER)
		return;

	int ChatMode = CHATMODE_ALL;
	if(SendMode == MODE_WHISPER)
		ChatMode = CHATMODE_WHISPER;
	else if(SendMode == MODE_TEAM)
		ChatMode = CHATMODE_TEAM;

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Mode = ChatMode;
	Msg.m_Target = ChatMode == CHATMODE_WHISPER ? SendTarget : -1;
	Msg.m_pMessage = pMsg;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}
