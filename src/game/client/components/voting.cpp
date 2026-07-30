

#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <base/vmath.h>
#include <game/client/render.h>
#include <game/client/components/menus.h>
#include "voting.h"

void CVoting::ConCallvote(IConsole::IResult *pResult, void *pUserData)
{
	CVoting *pSelf = (CVoting *)pUserData;
	pSelf->Callvote(
		pResult->GetString(0), pResult->GetString(1), pResult->NumArguments() > 2 ? pResult->GetString(2) : "");
}

void CVoting::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CVoting *pSelf = (CVoting *)pUserData;
	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->Vote(1);
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->Vote(-1);
}

void CVoting::ConDebugVote(IConsole::IResult *pResult, void *pUserData)
{
	CVoting *pSelf = (CVoting *)pUserData;
	pSelf->OnReset();
	const int Seconds = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 5, 60) : 25;
	str_copy(pSelf->m_aDescription, "Restart the current round", sizeof(pSelf->m_aDescription));
	str_copy(pSelf->m_aReason, "HUD layout preview", sizeof(pSelf->m_aReason));
	pSelf->m_Yes = 3;
	pSelf->m_No = 1;
	pSelf->m_Pass = 2;
	pSelf->m_Total = 6;
	pSelf->m_Closetime = time_get() + time_freq() * Seconds;
	pSelf->m_DebugVoteActive = true;
}

void CVoting::Callvote(const char *pType, const char *pValue, const char *pReason)
{
	CNetMsg_Cl_CallVote Msg = {0};
	Msg.m_Type = pType;
	Msg.m_Value = pValue;
	Msg.m_Reason = pReason;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CVoting::CallvoteSpectate(int ClientID, const char *pReason, bool ForceVote)
{
	if(ForceVote)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "set_team %d -1", ClientID);
		Client()->Rcon(aBuf);
	}
	else
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%d", ClientID);
		Callvote("spectate", aBuf, pReason);
	}
}

void CVoting::CallvoteKick(int ClientID, const char *pReason, bool ForceVote)
{
	if(ForceVote)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "force_vote kick %d %s", ClientID, pReason);
		Client()->Rcon(aBuf);
	}
	else
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%d", ClientID);
		Callvote("kick", aBuf, pReason);
	}
}

void CVoting::CallvoteOption(int OptionID, const char *pReason, bool ForceVote)
{
	CVoteOptionClient *pOption = m_pFirst;
	while(pOption && OptionID >= 0)
	{
		if(OptionID == 0)
		{
			if(ForceVote)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "force_vote option \"%s\" %s", pOption->m_aDescription, pReason);
				Client()->Rcon(aBuf);
			}
			else
				Callvote("option", pOption->m_aDescription, pReason);
			break;
		}

		OptionID--;
		pOption = pOption->m_pNext;
	}
}

void CVoting::RemovevoteOption(int OptionID)
{
	CVoteOptionClient *pOption = m_pFirst;
	while(pOption && OptionID >= 0)
	{
		if(OptionID == 0)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "remove_vote \"%s\"", pOption->m_aDescription);
			Client()->Rcon(aBuf);
			break;
		}

		OptionID--;
		pOption = pOption->m_pNext;
	}
}

void CVoting::AddvoteOption(const char *pDescription, const char *pCommand)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "add_vote \"%s\" %s", pDescription, pCommand);
	Client()->Rcon(aBuf);
}

void CVoting::Vote(int v)
{
	CNetMsg_Cl_Vote Msg = {v};
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

CVoting::CVoting()
{
	ClearOptions();

	m_Closetime = 0;
	m_aDescription[0] = 0;
	m_aReason[0] = 0;
	m_Yes = m_No = m_Pass = m_Total = 0;
	m_DisplayedYes = m_DisplayedNo = m_DisplayedPass = m_DisplayedTotal = 0.0f;
	m_LastAnimationTime = time_get();
	m_Voted = 0;
	m_DebugVoteActive = false;
}

void CVoting::AddOption(const char *pDescription)
{
	CVoteOptionClient *pOption;
	if(m_pRecycleFirst)
	{
		pOption = m_pRecycleFirst;
		m_pRecycleFirst = m_pRecycleFirst->m_pNext;
		if(m_pRecycleFirst)
			m_pRecycleFirst->m_pPrev = 0;
		else
			m_pRecycleLast = 0;
	}
	else
		pOption = (CVoteOptionClient *)m_Heap.Allocate(sizeof(CVoteOptionClient));

	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pLast = pOption;
	if(!m_pFirst)
		m_pFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	++m_NumVoteOptions;
}

void CVoting::ClearOptions()
{
	m_Heap.Reset();

	m_NumVoteOptions = 0;
	m_pFirst = 0;
	m_pLast = 0;

	m_pRecycleFirst = 0;
	m_pRecycleLast = 0;
}

void CVoting::OnReset()
{
	if(m_DebugVoteActive && m_Closetime > time_get())
		return;
	if(Client()->State() == IClient::STATE_LOADING) // do not reset active vote while connecting
		return;

	m_Closetime = 0;
	m_aDescription[0] = 0;
	m_aReason[0] = 0;
	m_Yes = m_No = m_Pass = m_Total = 0;
	m_DisplayedYes = m_DisplayedNo = m_DisplayedPass = m_DisplayedTotal = 0.0f;
	m_LastAnimationTime = time_get();
	m_Voted = 0;
	m_DebugVoteActive = false;
}

void CVoting::OnConsoleInit()
{
	Console()->Register("callvote", "ss?r", CFGFLAG_CLIENT, ConCallvote, this, "Call vote");
	Console()->Register("vote", "r", CFGFLAG_CLIENT, ConVote, this, "Vote yes/no");
	Console()->Register(
		"hud_debug_vote", "?i", CFGFLAG_CLIENT, ConDebugVote, this, "Preview the in-game vote HUD for 5-60 seconds");
}

void CVoting::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_VOTESET)
	{
		CNetMsg_Sv_VoteSet *pMsg = (CNetMsg_Sv_VoteSet *)pRawMsg;
		m_DebugVoteActive = false;
		if(pMsg->m_Timeout)
		{
			OnReset();
			m_pClient->SanitizeSocialString(pMsg->m_pDescription, m_aDescription, sizeof(m_aDescription));
			m_pClient->SanitizeSocialString(pMsg->m_pReason, m_aReason, sizeof(m_aReason));
			m_Closetime = time_get() + time_freq() * pMsg->m_Timeout;
		}
		else
			OnReset();
	}
	else if(MsgType == NETMSGTYPE_SV_VOTESTATUS)
	{
		CNetMsg_Sv_VoteStatus *pMsg = (CNetMsg_Sv_VoteStatus *)pRawMsg;
		if(pMsg->m_Type == 0)
		{
			m_Yes = pMsg->m_Yes;
			m_No = pMsg->m_No;
			m_Pass = pMsg->m_Pass;
			m_Total = pMsg->m_Total;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_VOTECLEAROPTIONS)
	{
		ClearOptions();
	}
	else if(MsgType == NETMSGTYPE_SV_VOTEOPTIONLISTADD)
	{
		CNetMsg_Sv_VoteOptionListAdd *pMsg = (CNetMsg_Sv_VoteOptionListAdd *)pRawMsg;
		int NumOptions = pMsg->m_NumOptions;
		for(int i = 0; i < NumOptions; ++i)
		{
			switch(i)
			{
				case 0:
					AddOption(pMsg->m_pDescription0);
					break;
				case 1:
					AddOption(pMsg->m_pDescription1);
					break;
				case 2:
					AddOption(pMsg->m_pDescription2);
					break;
				case 3:
					AddOption(pMsg->m_pDescription3);
					break;
				case 4:
					AddOption(pMsg->m_pDescription4);
					break;
				case 5:
					AddOption(pMsg->m_pDescription5);
					break;
				case 6:
					AddOption(pMsg->m_pDescription6);
					break;
				case 7:
					AddOption(pMsg->m_pDescription7);
					break;
				case 8:
					AddOption(pMsg->m_pDescription8);
					break;
				case 9:
					AddOption(pMsg->m_pDescription9);
					break;
				case 10:
					AddOption(pMsg->m_pDescription10);
					break;
				case 11:
					AddOption(pMsg->m_pDescription11);
					break;
				case 12:
					AddOption(pMsg->m_pDescription12);
					break;
				case 13:
					AddOption(pMsg->m_pDescription13);
					break;
				case 14:
					AddOption(pMsg->m_pDescription14);
			}
		}
	}
	else if(MsgType == NETMSGTYPE_SV_VOTEOPTIONADD)
	{
		CNetMsg_Sv_VoteOptionAdd *pMsg = (CNetMsg_Sv_VoteOptionAdd *)pRawMsg;
		AddOption(pMsg->m_pDescription);
	}
	else if(MsgType == NETMSGTYPE_SV_VOTEOPTIONREMOVE)
	{
		CNetMsg_Sv_VoteOptionRemove *pMsg = (CNetMsg_Sv_VoteOptionRemove *)pRawMsg;

		for(CVoteOptionClient *pOption = m_pFirst; pOption; pOption = pOption->m_pNext)
		{
			if(str_comp(pOption->m_aDescription, pMsg->m_pDescription) == 0)
			{
				// remove it from the list
				if(m_pFirst == pOption)
					m_pFirst = m_pFirst->m_pNext;
				if(m_pLast == pOption)
					m_pLast = m_pLast->m_pPrev;
				if(pOption->m_pPrev)
					pOption->m_pPrev->m_pNext = pOption->m_pNext;
				if(pOption->m_pNext)
					pOption->m_pNext->m_pPrev = pOption->m_pPrev;
				--m_NumVoteOptions;

				// add it to recycle list
				pOption->m_pNext = 0;
				pOption->m_pPrev = m_pRecycleLast;
				if(pOption->m_pPrev)
					pOption->m_pPrev->m_pNext = pOption;
				m_pRecycleLast = pOption;
				if(!m_pRecycleFirst)
					m_pRecycleLast = pOption;

				break;
			}
		}
	}
}

void CVoting::OnRender()
{
}

void CVoting::RenderBars(CUIRect Bars, bool Text)
{
	const int64 Now = time_get();
	const float Dt = clamp((float)((Now - m_LastAnimationTime) / (double)time_freq()), 0.0f, 0.05f);
	const float Blend = 1.0f - expf(-10.0f * Dt);
	m_DisplayedYes += (m_Yes - m_DisplayedYes) * Blend;
	m_DisplayedNo += (m_No - m_DisplayedNo) * Blend;
	m_DisplayedPass += (m_Pass - m_DisplayedPass) * Blend;
	m_DisplayedTotal += (m_Total - m_DisplayedTotal) * Blend;
	m_LastAnimationTime = Now;

	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Danger = CMenus::ThemeDanger();
	RenderTools()->DrawUIRect(&Bars, vec4(Deep.r, Deep.g, Deep.b, 0.92f), CUI::CORNER_ALL, Bars.h / 2.0f);

	if(m_Total)
	{
		const float DisplayedTotal = max(0.001f, m_DisplayedTotal);
		CUIRect PassArea = Bars;
		if(m_DisplayedYes > 0.001f)
		{
			CUIRect YesArea = Bars;
			YesArea.w *= clamp(m_DisplayedYes / DisplayedTotal, 0.0f, 1.0f);
			RenderTools()->DrawUIRect(&YesArea, vec4(0.25f, 0.78f, 0.43f, 0.92f), CUI::CORNER_ALL, Bars.h / 2.0f);

			if(Text)
			{
				char Buf[256];
				str_format(Buf, sizeof(Buf), "%d", m_Yes);
				UI()->DoLabel(&YesArea, Buf, Bars.h * 0.75f, 0);
			}

			PassArea.x += YesArea.w;
			PassArea.w -= YesArea.w;
		}

		if(m_DisplayedNo > 0.001f)
		{
			CUIRect NoArea = Bars;
			NoArea.w *= clamp(m_DisplayedNo / DisplayedTotal, 0.0f, 1.0f);
			NoArea.x = (Bars.x + Bars.w) - NoArea.w;
			RenderTools()->DrawUIRect(
				&NoArea, vec4(Danger.r, Danger.g, Danger.b, 0.92f), CUI::CORNER_ALL, Bars.h / 2.0f);

			if(Text)
			{
				char Buf[256];
				str_format(Buf, sizeof(Buf), "%d", m_No);
				UI()->DoLabel(&NoArea, Buf, Bars.h * 0.75f, 0);
			}

			PassArea.w -= NoArea.w;
		}

		if(m_DisplayedPass > 0.001f && PassArea.w > 0.0f)
		{
			RenderTools()->DrawUIRect(
				&PassArea, vec4(AccentDim.r, AccentDim.g, AccentDim.b, 0.48f), CUI::CORNER_ALL, Bars.h / 2.0f);
			if(Text)
			{
				char Buf[256];
				str_format(Buf, sizeof(Buf), "%d", m_Pass);
				UI()->DoLabel(&PassArea, Buf, Bars.h * 0.75f, 0);
			}
		}
	}

	CUIRect Majority = Bars;
	Majority.x += Majority.w * 0.5f - 0.35f;
	Majority.w = 0.7f;
	Majority.y += 0.6f;
	Majority.h -= 1.2f;
	RenderTools()->DrawUIRect(&Majority, vec4(1, 1, 1, 0.36f), CUI::CORNER_ALL, 0.3f);
}
