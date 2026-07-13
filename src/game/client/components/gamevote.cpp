#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>
#include <game/client/components/menus.h>

#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>

#include "skins.h"

#include <game/client/customstuff.h>
#include <game/client/customstuff/playerinfo.h>

#include "gamevote.h"

#include <math.h>

void CGameVoteDisplay::OnReset()
{
	m_GameVoteCount = 0;
	
	for (int i = 0; i < MAX_GAME_VOTES; i++)
	{
		m_aGameVoteDetails[i].m_Valid = false;
		m_aGameVoteDetails[i].m_Votes = 0;
	}
	
	m_MouseTrigger = false;
	m_SelectorMouse = vec2(150, 150);
	m_Selected = -1;
	m_Focused = 0;
	m_CarouselPosition = 0.0f;
	m_AppearAmount = 0.0f;
	m_SelectionPulse = 0.0f;
	m_TimeLeft = 0;
	m_VoteDuration = 0;
	m_LastVoteMessageTime = time_get();
}

bool CGameVoteDisplay::OnInput(IInput::CEvent Event)
{
	if(!IsActive())
		return false;

	if(Event.m_Flags&IInput::FLAG_PRESS)
	{
		int Direction = 0;
		if(Event.m_Key == KEY_MOUSE_WHEEL_UP)
			Direction = -1;
		else if(Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
			Direction = 1;

		if(Direction)
		{
			int Next = m_Focused + Direction;
			while(Next >= 0 && Next < m_GameVoteCount && !m_aGameVoteDetails[Next].m_Valid)
				Next += Direction;
			if(Next >= 0 && Next < m_GameVoteCount)
				m_Focused = Next;
			return true;
		}
	}
	
	if(Event.m_Key == KEY_MOUSE_1)
	{
		if(Event.m_Flags&IInput::FLAG_PRESS)
			m_MouseTrigger = true;
		return true;
	}

	return false;
}

bool CGameVoteDisplay::OnMouseMove(float x, float y)
{
	if(!IsActive())
		return false;

	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	
	Input()->GetRelativePosition(&x, &y);
	m_SelectorMouse += vec2(x,y)*0.5f;

	return true;
}

void CGameVoteDisplay::RenderMouse()
{
	// cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x, m_SelectorMouse.y, 16, 16);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CGameVoteDisplay::SendVote()
{
	m_SelectionPulse = 1.0f;
	CNetMsg_Cl_VoteGameMode Msg;
	Msg.m_Vote = m_Selected;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CGameVoteDisplay::OnRender()
{
	if(m_GameVoteCount <= 0)
		return;

	if(m_TimeLeft + (m_TimeLeftTick-Client()->GameTick())/Client()->GameTickSpeed() < 0 &&
		time_get() > m_LastVoteMessageTime + time_freq()*8)
	{
		OnReset();
		return;
	}

	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f*Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);

	m_Focused = clamp(m_Focused, 0, m_GameVoteCount - 1);
	const float ScrollDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_CarouselPosition += (m_Focused - m_CarouselPosition) * (1.0f - expf(-12.0f * ScrollDt));
	if(fabsf(m_CarouselPosition - m_Focused) < 0.0005f)
		m_CarouselPosition = m_Focused;
	m_AppearAmount += (1.0f - m_AppearAmount) * (1.0f - expf(-8.0f * ScrollDt));
	if(m_AppearAmount > 0.999f)
		m_AppearAmount = 1.0f;
	m_SelectionPulse = max(0.0f, m_SelectionPulse - ScrollDt*1.8f);

	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 3.0f, ScreenWidth-5.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 3.0f, 295.0f);

	Graphics()->BlendNormal();

	const float Appear = clamp(m_AppearAmount, 0.0f, 1.0f);
	const vec4 ColorAccent = CMenus::ThemeAccent();
	const vec4 ColorAccentDim = CMenus::ThemeAccentDim();
	const vec4 ColorText = CMenus::ThemeText();
	const vec4 ColorMuted = vec4(
		CMenus::ThemeText().r * 0.45f + CMenus::ThemeBgPanel().r * 0.55f,
		CMenus::ThemeText().g * 0.45f + CMenus::ThemeBgPanel().g * 0.55f,
		CMenus::ThemeText().b * 0.45f + CMenus::ThemeBgPanel().b * 0.55f,
		1.0f);
	const vec4 ColorBgDeep = CMenus::ThemeBgDeep();
	const vec4 ColorBgPanel = CMenus::ThemeBgPanel();
	const vec4 ColorBgInset = CMenus::ThemeBgInset();

	auto DrawRoundedRect = [&](vec2 Center, vec2 HalfSize, vec4 Color, float Rounding) {
		if(HalfSize.x <= 0.0f || HalfSize.y <= 0.0f || Color.a <= 0.0f)
			return;
		CUIRect Rect;
		Rect.x = Center.x-HalfSize.x;
		Rect.y = Center.y-HalfSize.y;
		Rect.w = HalfSize.x*2.0f;
		Rect.h = HalfSize.y*2.0f;
		RenderTools()->DrawUIRect(&Rect, Color, CUI::CORNER_ALL, min(Rounding, min(HalfSize.x, HalfSize.y)));
	};

	auto DrawGradientRect = [&](float X, float Y, float Width, float Height, vec4 Top, vec4 Bottom) {
		if(Width <= 0.0f || Height <= 0.0f)
			return;
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		IGraphics::CColorVertex aColors[4] = {
			IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
			IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
		Graphics()->SetColorVertex(aColors, 4);
		IGraphics::CFreeformItem Item(X, Y, X+Width, Y, X, Y+Height, X+Width, Y+Height);
		Graphics()->QuadsDrawFreeform(&Item, 1);
		Graphics()->QuadsEnd();
	};

	auto DrawCenteredText = [&](float CenterX, float Y, float FontSize, const char *pText, vec4 Color) {
		const float Width = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.45f*Color.a);
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
		TextRender()->Text(0, CenterX-Width*0.5f, Y, FontSize, pText, -1);
	};

	// Dim the world and frame the carousel as one continuous selection stage.
	DrawGradientRect(0.0f, 0.0f, ScreenWidth, 300.0f,
		vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.92f*Appear),
		vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.95f*Appear));
	const vec2 StageHalf(min(ScreenWidth*0.5f-10.0f, 255.0f), 99.0f);
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 157.0f), StageHalf+vec2(2.0f, 3.0f), vec4(0.0f, 0.0f, 0.0f, 0.42f*Appear), 14.0f);
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 154.0f), StageHalf, vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.88f*Appear), 13.0f);
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 56.0f), vec2(StageHalf.x-13.0f, 0.7f), vec4(ColorAccent.r, ColorAccent.g, ColorAccent.b, 0.35f*Appear), 0.7f);
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 154.0f), vec2(StageHalf.x-18.0f, 0.55f), vec4(ColorAccentDim.r, ColorAccentDim.g, ColorAccentDim.b, 0.12f*Appear), 0.55f);

	DrawCenteredText(ScreenWidth*0.5f, 10.0f, 11.0f, Localize("Choose the next mode"), vec4(ColorText.r, ColorText.g, ColorText.b, Appear));

	int Time = m_TimeLeft + (m_TimeLeftTick-Client()->GameTick())/Client()->GameTickSpeed();
	const bool ChangingMap = Time < 0;
	Time = max(0, Time);
	char aTimer[64];
	if(ChangingMap)
		str_copy(aTimer, Localize("Server is changing map"), sizeof(aTimer));
	else
		str_format(aTimer, sizeof(aTimer), Localize("Vote ends in %d..."), Time);
	const float TimerFontSize = 6.5f;
	const float TimerTextWidth = TextRender()->TextWidth(0, TimerFontSize, aTimer, -1);
	const vec4 TimerColor = Time <= 5 && !ChangingMap ? CMenus::ThemeDanger() : ColorAccent;
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 37.0f), vec2(TimerTextWidth*0.5f+10.0f, 8.0f), vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.94f*Appear), 8.0f);
	DrawCenteredText(ScreenWidth*0.5f, 33.4f, TimerFontSize, aTimer, vec4(TimerColor.r, TimerColor.g, TimerColor.b, Appear));

	const float TimerBarWidth = min(116.0f, ScreenWidth*0.30f);
	const float TimerRatio = m_VoteDuration > 0 ? clamp(Time/(float)m_VoteDuration, 0.0f, 1.0f) : 0.0f;
	DrawRoundedRect(vec2(ScreenWidth*0.5f, 51.0f), vec2(TimerBarWidth*0.5f, 1.1f), vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.55f*Appear), 1.1f);
	if(TimerRatio > 0.0f)
	{
		const float FillWidth = TimerBarWidth*TimerRatio;
		DrawRoundedRect(vec2(ScreenWidth*0.5f-TimerBarWidth*0.5f+FillWidth*0.5f, 51.0f), vec2(FillWidth*0.5f, 1.1f), vec4(TimerColor.r, TimerColor.g, TimerColor.b, 0.95f*Appear), 1.1f);
	}

	const float CarouselCenterX = 150.0f*Aspect;
	const float CarouselCenterY = 153.0f;
	const float CardSpacing = clamp(ScreenWidth*0.285f, 118.0f, 146.0f);

	auto GetCardLayout = [&](int Index, vec2 &Center, float &Scale, float &Alpha, float &Distance) -> bool {
		const float Offset = Index - m_CarouselPosition;
		Distance = fabsf(Offset);
		if(Distance > 3.25f)
			return false;

		Scale = clamp(1.0f-Distance*0.205f, 0.50f, 1.0f);
		Alpha = clamp(1.0f-Distance*0.31f, 0.10f, 1.0f)*Appear;
		Center = vec2(CarouselCenterX+Offset*CardSpacing, CarouselCenterY+min(Distance, 2.5f)*5.0f+(1.0f-Appear)*18.0f);
		const float HalfWidth = 92.0f*Scale;
		return Center.x + HalfWidth > 0.0f && Center.x - HalfWidth < ScreenWidth;
	};

	int HoveredCard = -1;
	int HoveredButton = -1;
	float HoveredCardDistance = 1000.0f;
	float HoveredButtonDistance = 1000.0f;
	for(int i = 0; i < m_GameVoteCount; i++)
	{
		if(!m_aGameVoteDetails[i].m_Valid)
			continue;

		vec2 Center;
		float Scale, Alpha, Distance;
		if(!GetCardLayout(i, Center, Scale, Alpha, Distance))
			continue;

		const vec2 FrameHalf = vec2(88.0f, 88.0f)*Scale;
		const vec2 ButtonCenter = Center + vec2(0.0f, 69.0f*Scale);
		const vec2 ButtonHalf = vec2(61.0f, 10.5f)*Scale;
		if(abs(m_SelectorMouse.x-Center.x) < FrameHalf.x && abs(m_SelectorMouse.y-Center.y) < FrameHalf.y && Distance < HoveredCardDistance)
		{
			HoveredCard = i;
			HoveredCardDistance = Distance;
		}
		if(abs(m_SelectorMouse.x-ButtonCenter.x) < ButtonHalf.x && abs(m_SelectorMouse.y-ButtonCenter.y) < ButtonHalf.y && Distance < HoveredButtonDistance)
		{
			HoveredButton = i;
			HoveredButtonDistance = Distance;
		}
	}

	if(m_MouseTrigger)
	{
		if(HoveredButton >= 0)
		{
			m_Focused = HoveredButton;
			m_Selected = HoveredButton;
			SendVote();
		}
		else if(HoveredCard >= 0)
			m_Focused = HoveredCard;
		m_MouseTrigger = false;
	}

	auto DrawCard = [&](int i) {
		vec2 Center;
		float Scale, Alpha, Distance;
		if(!m_aGameVoteDetails[i].m_Valid || !GetCardLayout(i, Center, Scale, Alpha, Distance))
			return;

		const bool Focused = i == m_Focused;
		const bool Selected = i == m_Selected;
		const bool Hovered = i == HoveredCard;
		const bool ButtonHovered = i == HoveredButton;
		if(Hovered)
		{
			Center.y -= 1.8f*Scale;
			Scale *= 1.018f;
		}

		const vec4 Accent = Selected ? ColorAccent : ColorAccentDim;
		const vec2 FrameHalf = vec2(88.0f, 88.0f)*Scale;
		const vec2 ImageCenter = Center + vec2(0.0f, -40.5f*Scale);
		const vec2 ImageHalf = vec2(80.0f, 39.5f)*Scale;
		const vec2 ButtonCenter = Center + vec2(0.0f, 69.0f*Scale);
		const vec2 ButtonHalf = vec2(61.0f, 10.5f)*Scale;
		const float Pulse = m_SelectionPulse*(0.5f+0.5f*sinf((1.0f-m_SelectionPulse)*18.0f));

		DrawRoundedRect(Center+vec2(0.0f, 4.0f*Scale), FrameHalf+vec2(3.5f, 3.5f)*Scale, vec4(0.0f, 0.0f, 0.0f, 0.42f*Alpha), 13.0f*Scale);
		if(Focused || Selected)
			DrawRoundedRect(Center, FrameHalf+vec2(4.0f, 4.0f)*Scale, vec4(Accent.r, Accent.g, Accent.b, (0.13f+0.18f*Pulse)*Alpha), 14.0f*Scale);

		const float BorderAlpha = Selected ? 0.90f : (Focused ? 0.78f : (Hovered ? 0.40f : 0.13f));
		DrawRoundedRect(Center, FrameHalf+vec2(1.3f, 1.3f)*Scale, vec4(Accent.r, Accent.g, Accent.b, BorderAlpha*Alpha), 11.5f*Scale);
		DrawRoundedRect(Center, FrameHalf, vec4(0.030f, 0.048f, 0.055f, 0.98f*Alpha), 10.5f*Scale);
		if(Focused || Selected)
			DrawRoundedRect(vec2(Center.x, Center.y-FrameHalf.y+3.3f*Scale), vec2(FrameHalf.x-11.0f*Scale, 1.0f*Scale), vec4(Accent.r, Accent.g, Accent.b, 0.85f*Alpha), 1.0f*Scale);

		DrawRoundedRect(ImageCenter, ImageHalf+vec2(1.4f, 1.4f)*Scale, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.75f*Alpha), 6.5f*Scale);
		Graphics()->TextureSet(m_aGameVoteDetails[i].m_Texture);
		Graphics()->QuadsBegin();
		const float ImageBrightness = Focused || Selected ? 1.0f : 0.72f;
		Graphics()->SetColor(ImageBrightness, ImageBrightness, ImageBrightness, Alpha);
		Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 1, 1, 1);
		IGraphics::CFreeformItem Image(
			ImageCenter.x-ImageHalf.x, ImageCenter.y-ImageHalf.y,
			ImageCenter.x+ImageHalf.x, ImageCenter.y-ImageHalf.y,
			ImageCenter.x-ImageHalf.x, ImageCenter.y+ImageHalf.y,
			ImageCenter.x+ImageHalf.x, ImageCenter.y+ImageHalf.y);
		Graphics()->QuadsDrawFreeform(&Image, 1);
		Graphics()->QuadsEnd();
		DrawGradientRect(ImageCenter.x-ImageHalf.x, ImageCenter.y-5.0f*Scale, ImageHalf.x*2.0f, ImageHalf.y+5.0f*Scale,
			vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.0f), vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.88f*Alpha));

		char aVotes[24];
		str_format(aVotes, sizeof(aVotes), "%d", m_aGameVoteDetails[i].m_Votes);
		const float VoteFontSize = 6.5f*Scale;
		const float VoteTextWidth = TextRender()->TextWidth(0, VoteFontSize, aVotes, -1);
		const vec2 VoteBadgeHalf(max(13.0f*Scale, VoteTextWidth*0.5f+8.0f*Scale), 7.0f*Scale);
		const vec2 VoteBadgeCenter(ImageCenter.x-ImageHalf.x+VoteBadgeHalf.x+5.0f*Scale, ImageCenter.y-ImageHalf.y+VoteBadgeHalf.y+5.0f*Scale);
		DrawRoundedRect(VoteBadgeCenter, VoteBadgeHalf, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.88f*Alpha), 7.0f*Scale);
		DrawRoundedRect(vec2(VoteBadgeCenter.x-VoteBadgeHalf.x+6.2f*Scale, VoteBadgeCenter.y), vec2(2.0f, 2.0f)*Scale, vec4(Accent.r, Accent.g, Accent.b, Alpha), 2.0f*Scale);
		DrawCenteredText(VoteBadgeCenter.x+2.2f*Scale, VoteBadgeCenter.y-VoteFontSize*0.56f, VoteFontSize, aVotes, vec4(ColorText.r, ColorText.g, ColorText.b, Alpha));

		float NameSize = 10.0f*Scale;
		const float MinNameSize = 6.0f*Scale;
		while(NameSize > MinNameSize && TextRender()->TextWidth(0, NameSize, m_aGameVoteDetails[i].m_aName, -1) > 148.0f*Scale)
			NameSize -= 0.35f*Scale;
		DrawCenteredText(Center.x, Center.y+5.0f*Scale, NameSize, m_aGameVoteDetails[i].m_aName,
			vec4(ColorText.r, ColorText.g, ColorText.b, Alpha));

		float DescriptionSize = 5.6f*Scale;
		const float MinDescriptionSize = 4.0f*Scale;
		while(DescriptionSize > MinDescriptionSize && TextRender()->TextWidth(0, DescriptionSize, m_aGameVoteDetails[i].m_aDescription, -1) > 150.0f*Scale)
			DescriptionSize -= 0.25f*Scale;
		DrawCenteredText(Center.x, Center.y+23.0f*Scale, DescriptionSize, m_aGameVoteDetails[i].m_aDescription,
			vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, Alpha));

		const vec4 ButtonBorder = Selected ? ColorAccent : ColorAccentDim;
		DrawRoundedRect(ButtonCenter, ButtonHalf+vec2(1.1f, 1.1f)*Scale, vec4(ButtonBorder.r, ButtonBorder.g, ButtonBorder.b, (Focused || ButtonHovered || Selected ? 0.80f : 0.22f)*Alpha), 7.0f*Scale);
		vec4 ButtonColor;
		if(Selected)
			ButtonColor = vec4(ColorAccent.r, ColorAccent.g, ColorAccent.b, 0.96f*Alpha);
		else if(ButtonHovered)
			ButtonColor = vec4(ColorAccentDim.r, ColorAccentDim.g, ColorAccentDim.b, 0.98f*Alpha);
		else if(Focused)
			ButtonColor = vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.96f*Alpha);
		else
			ButtonColor = vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.92f*Alpha);
		DrawRoundedRect(ButtonCenter, ButtonHalf, ButtonColor, 6.0f*Scale);

		const char *pButtonText = Selected ? Localize("Selected") : Localize("Select");
		const float ButtonFontSize = 7.0f*Scale;
		DrawCenteredText(ButtonCenter.x, ButtonCenter.y-ButtonFontSize*0.56f, ButtonFontSize, pButtonText,
			vec4(1.0f, 1.0f, 1.0f, Alpha));
	};

	// Distant cards are painted first; the focused card always owns the foreground.
	for(int Band = 3; Band >= 0; Band--)
	{
		for(int i = 0; i < m_GameVoteCount; i++)
		{
			if(i == m_Focused || !m_aGameVoteDetails[i].m_Valid)
				continue;
			vec2 Center;
			float Scale, Alpha, Distance;
			if(GetCardLayout(i, Center, Scale, Alpha, Distance) && min(3, (int)Distance) == Band)
				DrawCard(i);
		}
	}
	if(m_Focused >= 0 && m_Focused < m_GameVoteCount)
		DrawCard(m_Focused);

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);

	if(m_GameVoteCount > 1)
	{
		const int DotCount = min(m_GameVoteCount, 9);
		const int DotStart = clamp(m_Focused-DotCount/2, 0, m_GameVoteCount-DotCount);
		const float DotSpacing = 7.0f;
		const float DotStartX = ScreenWidth*0.5f-(DotCount-1)*DotSpacing*0.5f;
		for(int Dot = 0; Dot < DotCount; Dot++)
		{
			const int Index = DotStart+Dot;
			const bool Focused = Index == m_Focused;
			const bool Selected = Index == m_Selected;
			const vec4 DotColor = Selected ? ColorAccent : (Focused ? ColorAccentDim : vec4(0.46f, 0.50f, 0.49f, 1.0f));
			DrawRoundedRect(vec2(DotStartX+Dot*DotSpacing, 263.0f), Focused ? vec2(4.0f, 1.4f) : vec2(1.4f, 1.4f),
				vec4(DotColor.r, DotColor.g, DotColor.b, (Focused || Selected ? 0.95f : 0.38f)*Appear), 1.4f);
		}

		char aBuf[96];
		str_format(aBuf, sizeof(aBuf), Localize("Mouse wheel to browse modes | %d / %d"), m_Focused+1, m_GameVoteCount);
		const float HintFontSize = 6.5f;
		const float HintWidth = TextRender()->TextWidth(0, HintFontSize, aBuf, -1);
		DrawRoundedRect(vec2(ScreenWidth*0.5f, 283.0f), vec2(HintWidth*0.5f+11.0f, 8.0f), vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.88f*Appear), 8.0f);
		DrawCenteredText(ScreenWidth*0.5f, 279.4f, HintFontSize, aBuf, vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, Appear));
	}

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
	RenderMouse();
}

void CGameVoteDisplay::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_GAMEVOTESTATUS)
	{
		CNetMsg_Sv_GameVoteStatus *pMsg = (CNetMsg_Sv_GameVoteStatus *)pRawMsg;
		if(pMsg->m_Index >= 0 && pMsg->m_Index < MAX_GAME_VOTES)
			m_aGameVoteDetails[pMsg->m_Index].m_Votes = pMsg->m_Votes;
		m_LastVoteMessageTime = time_get();
	}
	
	if(MsgType == NETMSGTYPE_SV_GAMEVOTE)
	{
		CNetMsg_Sv_GameVote *pMsg = (CNetMsg_Sv_GameVote *)pRawMsg;
		m_LastVoteMessageTime = time_get();
		
		int i = pMsg->m_Index;
		if(i < 0 || i >= MAX_GAME_VOTES)
			return;

		m_TimeLeft = pMsg->m_TimeLeft;
		m_TimeLeftTick = Client()->GameTick();
		m_VoteDuration = max(m_VoteDuration, pMsg->m_TimeLeft);

		if(m_aGameVoteDetails[i].m_Valid)
			return;

		if(m_GameVoteCount == 0)
			m_SelectorMouse = vec2(150.0f*Graphics()->ScreenAspect(), 150.0f);
		
		m_aGameVoteDetails[i].m_Valid = true;
		m_aGameVoteDetails[i].m_Texture = m_pClient->m_pSkins->GetGameVote(m_pClient->m_pSkins->FindGameVote(pMsg->m_pImage))->m_Texture;
		
		str_copy(m_aGameVoteDetails[i].m_aName, pMsg->m_pName, sizeof(m_aGameVoteDetails[i].m_aName));
		str_copy(m_aGameVoteDetails[i].m_aDescription, pMsg->m_pDescription, sizeof(m_aGameVoteDetails[i].m_aDescription));
		
		m_GameVoteCount = max(m_GameVoteCount, i+1);
		
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 10.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, m_aGameVoteDetails[i].m_aName, -1);
			m_aGameVoteDetails[i].m_NameWidth = Cursor.m_X/2;
		}
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 6.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, m_aGameVoteDetails[i].m_aDescription, -1);
			m_aGameVoteDetails[i].m_DescriptionWidth = Cursor.m_X/2;
		}
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 10.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, "0", -1);
			m_aGameVoteDetails[i].m_VotesWidth = Cursor.m_X/2;
		}
	}
}
