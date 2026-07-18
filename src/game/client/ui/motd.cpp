

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>

#include <generated/protocol.h>
#include <generated/game_data.h>
#include <game/shared/core/localization.h>
#include <game/client/core/gameclient.h>
#include <game/client/ui/menus.h>

#include "motd.h"

void CMotd::Clear()
{
	m_ServerMotdTime = 0;
}

bool CMotd::IsActive() const
{
	return time_get() < m_ServerMotdTime;
}

void CMotd::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE || OldState == IClient::STATE_OFFLINE)
		Clear();
}

void CMotd::OnRender()
{
	if(!IsActive())
		return;

	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	const float w = clamp(Width*0.44f, 580.0f, 760.0f);
	CTextCursor Measure;
	TextRender()->SetCursor(&Measure, 0, 0, 24.0f, 0);
	Measure.m_LineWidth = w-64.0f;
	Measure.m_MaxLines = 18;
	TextRender()->TextEx(&Measure, m_aServerMotd, -1);
	const float h = clamp(142.0f+max(24.0f, Measure.m_Y), 230.0f, 700.0f);
	const float x = (Width-w)*0.5f;
	const float y = (Height-h)*0.5f;
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.01f, 0.015f, 0.02f, 0.46f);
	IGraphics::CQuadItem Backdrop(0, 0, Width, Height);
	Graphics()->QuadsDrawTL(&Backdrop, 1);
	Graphics()->SetColor(0, 0, 0, 0.46f);
	RenderTools()->DrawRoundRect(x+5.0f, y+6.0f, w, h, 20.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.68f);
	RenderTools()->DrawRoundRect(x-1.5f, y-1.5f, w+3.0f, h+3.0f, 20.5f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.99f);
	RenderTools()->DrawRoundRect(x, y, w, h, 19.0f);
	Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.62f);
	RenderTools()->DrawRoundRect(x+20.0f, y+68.0f, w-40.0f, h-118.0f, 12.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.96f);
	RenderTools()->DrawRoundRect(x, y+18.0f, 4.0f, h-36.0f, 2.0f);
	Graphics()->QuadsEnd();
	TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
	TextRender()->Text(0, x+28.0f, y+20.0f, 28.0f, Localize("MOTD"), -1);
	TextRender()->TextColor(Text.r, Text.g, Text.b, 0.94f);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, x+32.0f, y+88.0f, 24.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = w-64.0f;
	Cursor.m_MaxLines = 18;
	TextRender()->TextEx(&Cursor, m_aServerMotd, -1);
	char aClose[64];
	str_format(aClose, sizeof(aClose), "Esc · %s", Localize("Close"));
	const float CloseW = TextRender()->TextWidth(0, 17.0f, aClose, -1);
	TextRender()->TextColor(Text.r, Text.g, Text.b, 0.58f);
	TextRender()->Text(0, x+w-CloseW-26.0f, y+h-34.0f, 17.0f, aClose, -1);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CMotd::OnMessage(int MsgType, void *pRawMsg)
{
	(void)MsgType;
	(void)pRawMsg;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;
	/* MOTD is currently not part of the generated v7 game protocol. Keep the
	 * renderer ready, but do not reference the removed message type here. */
	/*
	if(MsgType == NETMSGTYPE_SV_MOTD)
	{
		CNetMsg_Sv_Motd *pMsg = (CNetMsg_Sv_Motd *)pRawMsg;

		// process escaping
		str_copy(m_aServerMotd, pMsg->m_pMessage, sizeof(m_aServerMotd));
		for(int i = 0; m_aServerMotd[i]; i++)
		{
			if(m_aServerMotd[i] == '\\')
			{
				if(m_aServerMotd[i+1] == 'n')
				{
					m_aServerMotd[i] = ' ';
					m_aServerMotd[i+1] = '\n';
					i++;
				}
			}
		}

		if(m_aServerMotd[0] && g_Config.m_ClMotdTime)
			m_ServerMotdTime = time_get()+time_freq()*g_Config.m_ClMotdTime;
		else
			m_ServerMotdTime = 0;
	}
	*/
}

bool CMotd::OnInput(IInput::CEvent Event)
{
	if(!IsActive())
		return false;

	if(Event.m_Flags&IInput::FLAG_PRESS && (Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_GAMEPAD_BUTTON_B))
	{
		Clear();
		return true;
	}

	// The MOTD is a focused modal. Do not let movement, weapon or inventory
	// bindings run underneath it while it is visible.
	return true;
}
