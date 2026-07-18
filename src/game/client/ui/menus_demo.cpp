


#include <cstdio>

#include <base/math.h>

#include <engine/demo.h>
#include <engine/keys.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/storage.h>

#include <game/client/render/render.h>
#include <game/client/core/gameclient.h>
#include <game/shared/core/localization.h>

#include <game/client/ui/ui.h>

#include <generated/game_data.h>

#include <game/client/resources/maplayers.h>
#include "menus.h"

int CMenus::DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	RenderTools()->DrawUIRect(pRect, vec4(0.95f,0.58f,0.18f, Checked ? 0.10f : 0.38f)*ButtonColorMul(pID), CUI::CORNER_ALL, 5.0f);
	UI()->DoLabel(pRect, pText, 14.0f, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_Sprite(const void *pID, int ImageID, int SpriteID, int Checked, const CUIRect *pRect, int Corners)
{
	RenderTools()->DrawUIRect(pRect, Checked ? vec4(0.12f, 0.13f, 0.16f, 0.10f) : vec4(0.95f, 0.58f, 0.18f, 0.38f)*ButtonColorMul(pID), Corners, 5.0f);
	Graphics()->TextureSet(g_pData->m_aImages[ImageID].m_Id);
	Graphics()->QuadsBegin();
	if(!Checked)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(SpriteID);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return UI()->DoButtonLogic(pID, "", Checked, pRect);
}

void CMenus::RenderDemoPlayer(CUIRect MainView)
{
	if(Client()->IsRecordingVideo())
		return;

	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();

	const float SeekBarHeight = 15.0f;
	const float ButtonbarHeight = 20.0f;
	const float JumpBarHeight = 20.0f;
	const float NameBarHeight = 20.0f;
	const float Margins = 5.0f;
	float TotalHeight;

	if(m_MenuActive)
		TotalHeight = SeekBarHeight+ButtonbarHeight+JumpBarHeight+NameBarHeight+Margins*5;
	else
		TotalHeight = SeekBarHeight+Margins*2;

	MainView.HSplitBottom(TotalHeight, 0, &MainView);
	MainView.VSplitLeft(50.0f, 0, &MainView);
	MainView.VSplitRight(450.0f, &MainView, 0);

	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_T, 10.0f);

	MainView.Margin(5.0f, &MainView);

	CUIRect SeekBar, ButtonBar, JumpBar, NameBar;

	int CurrentTick = pInfo->m_CurrentTick - pInfo->m_FirstTick;
	int TotalTicks = pInfo->m_LastTick - pInfo->m_FirstTick;

	if(m_MenuActive)
	{
		MainView.HSplitTop(SeekBarHeight, &SeekBar, &ButtonBar);
		ButtonBar.HSplitTop(Margins, 0, &ButtonBar);
		ButtonBar.HSplitTop(ButtonbarHeight, &ButtonBar, &JumpBar);
		JumpBar.HSplitTop(Margins, 0, &JumpBar);
		JumpBar.HSplitTop(JumpBarHeight, &JumpBar, &NameBar);
		NameBar.HSplitTop(Margins, 0, &NameBar);
		NameBar.HSplitTop(NameBarHeight, &NameBar, &NameBar);
		NameBar.HSplitTop(4.0f, 0, &NameBar);
	}
	else
		SeekBar = MainView;

	// do seekbar
	{
		static int s_SeekBarID = 0;
		void *id = &s_SeekBarID;
		char aBuffer[128];

		// draw seek bar
		RenderTools()->DrawUIRect(&SeekBar, vec4(0.03f,0.04f,0.05f,0.65f), CUI::CORNER_ALL, 5.0f);

		// draw filled bar
		float Amount = CurrentTick/(float)TotalTicks;
		CUIRect FilledBar = SeekBar;
		FilledBar.w = 10.0f + (FilledBar.w-10.0f)*Amount;
		RenderTools()->DrawUIRect(&FilledBar, vec4(0.95f,0.58f,0.18f,0.65f), CUI::CORNER_ALL, 5.0f);

		// draw markers
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			float Ratio = (pInfo->m_aTimelineMarkers[i]-pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(SeekBar.x + (SeekBar.w-10.0f)*Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw time
		str_format(aBuffer, sizeof(aBuffer), "%d:%02d / %d:%02d",
			CurrentTick/SERVER_TICK_SPEED/60, (CurrentTick/SERVER_TICK_SPEED)%60,
			TotalTicks/SERVER_TICK_SPEED/60, (TotalTicks/SERVER_TICK_SPEED)%60);
		UI()->DoLabel(&SeekBar, aBuffer, SeekBar.h*0.70f, 0);

		// do the logic
		int Inside = UI()->MouseInside(&SeekBar);

		if(UI()->ActiveItem() == id)
		{
			if(!UI()->MouseButton(0))
				UI()->SetActiveItem(0);
			else
			{
				static float PrevAmount = 0.0f;
				float Amount = (UI()->MouseX()-SeekBar.x)/(float)SeekBar.w;
				if(Amount > 0.0f && Amount < 1.0f && absolute(PrevAmount-Amount) >= 0.01f)
				{
					PrevAmount = Amount;
					m_pClient->OnReset();
					m_pClient->m_SuppressEvents = true;
					DemoPlayer()->SetPos(Amount);
					m_pClient->m_SuppressEvents = false;
					m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
					m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
				}
			}
		}
		else if(UI()->HotItem() == id)
		{
			if(UI()->MouseButton(0))
				UI()->SetActiveItem(id);
		}

		if(Inside)
			UI()->SetHotItem(id);
	}

	if(CurrentTick == TotalTicks)
	{
		m_pClient->OnReset();
		DemoPlayer()->Pause();
		DemoPlayer()->SetPos(0);
	}

	bool IncreaseDemoSpeed = false, DecreaseDemoSpeed = false;

	if(m_MenuActive)
	{
		// do buttons
		CUIRect Button;

		// combined play and pause button
		ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
		static int s_PlayPauseButton = 0;
		if(!pInfo->m_Paused)
		{
			if(DoButton_Sprite(&s_PlayPauseButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_PAUSE, false, &Button, CUI::CORNER_ALL))
				DemoPlayer()->Pause();
		}
		else
		{
			if(DoButton_Sprite(&s_PlayPauseButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_PLAY, false, &Button, CUI::CORNER_ALL))
				DemoPlayer()->Unpause();
		}

		// stop button

		ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
		ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
		static int s_ResetButton = 0;
		if(DoButton_Sprite(&s_ResetButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_STOP, false, &Button, CUI::CORNER_ALL))
		{
			m_pClient->OnReset();
			DemoPlayer()->Pause();
			DemoPlayer()->SetPos(0);
		}

		// slowdown
		ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
		ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
		static int s_SlowDownButton = 0;
		if(DoButton_Sprite(&s_SlowDownButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_SLOWER, 0, &Button, CUI::CORNER_ALL) || Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN))
			DecreaseDemoSpeed = true;

		// fastforward
		ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
		ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
		static int s_FastForwardButton = 0;
		if(DoButton_Sprite(&s_FastForwardButton, IMAGE_DEMOBUTTONS, SPRITE_DEMOBUTTON_FASTER, 0, &Button, CUI::CORNER_ALL))
			IncreaseDemoSpeed = true;

		// speed meter
		ButtonBar.VSplitLeft(Margins*3, 0, &ButtonBar);
		char aBuffer[64];
		if(pInfo->m_Speed >= 1.0f)
			str_format(aBuffer, sizeof(aBuffer), "x%.0f", pInfo->m_Speed);
		else
			str_format(aBuffer, sizeof(aBuffer), "x%.2f", pInfo->m_Speed);
		UI()->DoLabel(&ButtonBar, aBuffer, Button.h*0.7f, -1);

		// slice button
		{
			ButtonBar.VSplitRight(10.0f, &ButtonBar, 0);
			ButtonBar.VSplitRight(ButtonbarHeight*2.5f, &ButtonBar, &Button);
			static int s_SliceButton = 0;
			const char *pSliceText = Localize("Slice");
			if(m_DemoSliceState == 1)
				pSliceText = Localize("Set End");
			else if(m_DemoSliceState == 2)
				pSliceText = Localize("Slice...");
			if(DoButton_DemoPlayer(&s_SliceButton, pSliceText, 0, &Button))
			{
				if(m_DemoSliceState == 0)
				{
					m_DemoSliceState = 1;
					m_DemoSliceStartTick = pInfo->m_CurrentTick;
				}
				else if(m_DemoSliceState == 1)
				{
					m_DemoSliceState = 2;
					m_DemoSliceEndTick = pInfo->m_CurrentTick;
					if(m_DemoSliceEndTick < m_DemoSliceStartTick)
					{
						int Tmp = m_DemoSliceStartTick;
						m_DemoSliceStartTick = m_DemoSliceEndTick;
						m_DemoSliceEndTick = Tmp;
					}
					m_Popup = POPUP_SLICE_DEMO;
					m_DemoSliceState = 0;
				}
			}
		}

		// close button
		ButtonBar.VSplitRight(10.0f, &ButtonBar, 0);
		ButtonBar.VSplitRight(ButtonbarHeight*3, &ButtonBar, &Button);
		static int s_ExitButton = 0;
		if(DoButton_DemoPlayer(&s_ExitButton, Localize("Close"), 0, &Button))
			Client()->Disconnect();

		// jump bar: quick jumps + time input
		{
			CUIRect JumpBtn, Label, GoButton;
			JumpBar.VSplitLeft(ButtonbarHeight*2.0f, &JumpBtn, &JumpBar);
			static int s_JumpBack1m = 0;
			if(DoButton_DemoPlayer(&s_JumpBack1m, "<1m", 0, &JumpBtn) && TotalTicks > 0)
			{
				int targetTick = pInfo->m_CurrentTick - 60 * SERVER_TICK_SPEED;
				if(targetTick < pInfo->m_FirstTick) targetTick = pInfo->m_FirstTick;
				m_pClient->OnReset();
				m_pClient->m_SuppressEvents = true;
				DemoPlayer()->SetPos((float)(targetTick - pInfo->m_FirstTick) / (float)TotalTicks);
				m_pClient->m_SuppressEvents = false;
				m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
				m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
			}

			JumpBar.VSplitLeft(Margins, 0, &JumpBar);
			JumpBar.VSplitLeft(ButtonbarHeight*2.0f, &JumpBtn, &JumpBar);
			static int s_JumpFwd1m = 0;
			if(DoButton_DemoPlayer(&s_JumpFwd1m, ">>1m", 0, &JumpBtn) && TotalTicks > 0)
			{
				int targetTick = pInfo->m_CurrentTick + 60 * SERVER_TICK_SPEED;
				if(targetTick > pInfo->m_LastTick) targetTick = pInfo->m_LastTick;
				m_pClient->OnReset();
				m_pClient->m_SuppressEvents = true;
				DemoPlayer()->SetPos((float)(targetTick - pInfo->m_FirstTick) / (float)TotalTicks);
				m_pClient->m_SuppressEvents = false;
				m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
				m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
			}

			JumpBar.VSplitLeft(Margins, 0, &JumpBar);
			JumpBar.VSplitLeft(ButtonbarHeight*2.0f, &JumpBtn, &JumpBar);
			static int s_JumpFwd5m = 0;
			if(DoButton_DemoPlayer(&s_JumpFwd5m, ">>5m", 0, &JumpBtn) && TotalTicks > 0)
			{
				int targetTick = pInfo->m_CurrentTick + 300 * SERVER_TICK_SPEED;
				if(targetTick > pInfo->m_LastTick) targetTick = pInfo->m_LastTick;
				m_pClient->OnReset();
				m_pClient->m_SuppressEvents = true;
				DemoPlayer()->SetPos((float)(targetTick - pInfo->m_FirstTick) / (float)TotalTicks);
				m_pClient->m_SuppressEvents = false;
				m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
				m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
			}

			JumpBar.VSplitLeft(Margins*2, 0, &JumpBar);
			JumpBar.VSplitLeft(50.0f, &Label, &JumpBar);
			UI()->DoLabelScaled(&Label, Localize("Jump:"), 12.0f, -1);
			JumpBar.VSplitRight(30.0f, &JumpBar, &GoButton);
			JumpBar.VSplitRight(5.0f, &JumpBar, 0);
			static char s_aTimeBuf[8] = {0};
			static float s_TimeOffset = 0.0f;
			DoEditBox(&s_aTimeBuf, &JumpBar, s_aTimeBuf, sizeof(s_aTimeBuf), 12.0f, &s_TimeOffset);

			static int s_GoButton = 0;
			if((DoButton_DemoPlayer(&s_GoButton, Localize("Go"), 0, &GoButton) || (m_EnterPressed && UI()->ActiveItem() == &s_aTimeBuf)) && TotalTicks > 0)
			{
				m_EnterPressed = false;
				int mins = 0, secs = 0;
				if(sscanf(s_aTimeBuf, "%d:%d", &mins, &secs) == 2 || sscanf(s_aTimeBuf, "%d.%d", &mins, &secs) == 2)
				{
					int targetTime = mins * 60 + secs;
					int targetTick = pInfo->m_FirstTick + targetTime * SERVER_TICK_SPEED;
					if(targetTick < pInfo->m_FirstTick) targetTick = pInfo->m_FirstTick;
					if(targetTick > pInfo->m_LastTick) targetTick = pInfo->m_LastTick;
					m_pClient->OnReset();
					m_pClient->m_SuppressEvents = true;
					DemoPlayer()->SetPos((float)(targetTick - pInfo->m_FirstTick) / (float)TotalTicks);
					m_pClient->m_SuppressEvents = false;
					m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
					m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
				}
			}
		}

		// demo name
		char aDemoName[64] = {0};
		DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), Localize("Demofile: %s"), aDemoName);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, NameBar.x, NameBar.y, Button.h*0.5f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = MainView.w;
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	// number keys 0-9 for quick position jump (0%-90%)
	if(m_MenuActive && !UI()->ActiveItem() && TotalTicks > 0)
	{
		for(int k = KEY_0; k <= KEY_9; k++)
		{
			if(Input()->KeyPresses(k))
			{
				int percent = (k == KEY_0) ? 0 : (k - KEY_0) * 10;
				float pos = percent / 100.0f;
				m_pClient->OnReset();
				m_pClient->m_SuppressEvents = true;
				DemoPlayer()->SetPos(pos);
				m_pClient->m_SuppressEvents = false;
				m_pClient->m_pMapLayersBackGround->EnvelopeUpdate();
				m_pClient->m_pMapLayersForeGround->EnvelopeUpdate();
			}
		}
	}

	if(IncreaseDemoSpeed || Input()->KeyPresses(KEY_MOUSE_WHEEL_UP))
	{
		if(pInfo->m_Speed < 0.1f) DemoPlayer()->SetSpeed(0.1f);
		else if(pInfo->m_Speed < 0.25f) DemoPlayer()->SetSpeed(0.25f);
		else if(pInfo->m_Speed < 0.5f) DemoPlayer()->SetSpeed(0.5f);
		else if(pInfo->m_Speed < 0.75f) DemoPlayer()->SetSpeed(0.75f);
		else if(pInfo->m_Speed < 1.0f) DemoPlayer()->SetSpeed(1.0f);
		else if(pInfo->m_Speed < 2.0f) DemoPlayer()->SetSpeed(2.0f);
		else if(pInfo->m_Speed < 4.0f) DemoPlayer()->SetSpeed(4.0f);
		else DemoPlayer()->SetSpeed(8.0f);
	}
	else if(DecreaseDemoSpeed || Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN))
	{
		if(pInfo->m_Speed > 4.0f) DemoPlayer()->SetSpeed(4.0f);
		else if(pInfo->m_Speed > 2.0f) DemoPlayer()->SetSpeed(2.0f);
		else if(pInfo->m_Speed > 1.0f) DemoPlayer()->SetSpeed(1.0f);
		else if(pInfo->m_Speed > 0.75f) DemoPlayer()->SetSpeed(0.75f);
		else if(pInfo->m_Speed > 0.5f) DemoPlayer()->SetSpeed(0.5f);
		else if(pInfo->m_Speed > 0.25f) DemoPlayer()->SetSpeed(0.25f);
		else if(pInfo->m_Speed > 0.1f) DemoPlayer()->SetSpeed(0.1f);
		else DemoPlayer()->SetSpeed(0.05f);
	}
}

static CUIRect gs_ListBoxOriginalView;
static CUIRect gs_ListBoxView;
static float gs_ListBoxRowHeight;
static int gs_ListBoxItemIndex;
static int gs_ListBoxSelectedIndex;
static int gs_ListBoxNewSelected;
static int gs_ListBoxDoneEvents;
static int gs_ListBoxNumItems;
static int gs_ListBoxItemsPerRow;
static float gs_ListBoxScrollValue;
static float gs_ListBoxScrollTarget;
static float gs_ListBoxAnimTime;
static float gs_ListBoxAnimInit;
static const void *gs_pListBoxScrollID;
static bool gs_ListBoxItemActivated;

void CMenus::UiDoListboxStart(const void *pID, const CUIRect *pRect, float RowHeight, const char *pTitle, const char *pBottomText, int NumItems,
								int ItemsPerRow, int SelectedIndex, float ScrollValue)
{
	CUIRect Scroll, Row;
	CUIRect View = *pRect;
	CUIRect Header, Footer;

	// draw header
	View.HSplitTop(ms_ListheaderHeight, &Header, &View);
	DrawSectionHeader(&Header, CUI::CORNER_T);
	UI()->DoLabel(&Header, pTitle, min(Header.h*ms_FontmodHeight, 12.0f), 0);

	// draw footers
	View.HSplitBottom(ms_ListheaderHeight, &View, &Footer);
	DrawMenuInset(&Footer, CUI::CORNER_B);
	Footer.VSplitLeft(10.0f, 0, &Footer);
	UI()->DoLabel(&Footer, pBottomText, min(Header.h*ms_FontmodHeight, 12.0f), 0);

	// background
	DrawMenuInset(&View, 0);

	// prepare the scroll
	View.VSplitRight(15, &View, &Scroll);

	// setup the variables
	gs_ListBoxOriginalView = View;
	gs_ListBoxSelectedIndex = SelectedIndex;
	gs_ListBoxNewSelected = SelectedIndex;
	gs_ListBoxItemIndex = 0;
	gs_ListBoxRowHeight = RowHeight;
	gs_ListBoxNumItems = NumItems;
	gs_ListBoxItemsPerRow = ItemsPerRow;
	gs_ListBoxDoneEvents = 0;
	gs_ListBoxItemActivated = false;

	if(gs_pListBoxScrollID != pID)
	{
		gs_pListBoxScrollID = pID;
		gs_ListBoxScrollValue = ScrollValue;
		gs_ListBoxScrollTarget = ScrollValue;
		gs_ListBoxAnimInit = ScrollValue;
		gs_ListBoxAnimTime = 0.0f;
	}
	else
	{
		gs_ListBoxScrollValue = ScrollValue;
	}

	// do the scrollbar
	View.HSplitTop(gs_ListBoxRowHeight, &Row, 0);

	int NumViewable = (int)(gs_ListBoxOriginalView.h/Row.h) + 1;
	int Num = (NumItems+gs_ListBoxItemsPerRow-1)/gs_ListBoxItemsPerRow-NumViewable+1;
	if(Num < 0)
		Num = 0;

	// Match CScrollRegion: 0.5s cubic ease-out, ~60px per wheel notch
	const float AnimDuration = 0.5f;
	if(Num > 0)
	{
		const float MaxScrollPx = Num * Row.h;
		const float ScrollUnitNorm = clamp(60.0f / MaxScrollPx, 0.05f, 1.0f);

		if(UI()->MouseInside(&View))
		{
			if(Input()->KeyPresses(KEY_MOUSE_WHEEL_UP))
			{
				gs_ListBoxAnimTime = AnimDuration;
				gs_ListBoxAnimInit = gs_ListBoxScrollValue;
				gs_ListBoxScrollTarget -= ScrollUnitNorm;
			}
			if(Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN))
			{
				gs_ListBoxAnimTime = AnimDuration;
				gs_ListBoxAnimInit = gs_ListBoxScrollValue;
				gs_ListBoxScrollTarget += ScrollUnitNorm;
			}
		}

		gs_ListBoxScrollTarget = clamp(gs_ListBoxScrollTarget, 0.0f, 1.0f);

		if(fabs(gs_ListBoxAnimInit - gs_ListBoxScrollTarget) < 0.0005f)
			gs_ListBoxAnimTime = 0.0f;

		if(gs_ListBoxAnimTime > 0.0f)
		{
			gs_ListBoxAnimTime -= Client()->RenderFrameTime();
			if(gs_ListBoxAnimTime < 0.0f)
				gs_ListBoxAnimTime = 0.0f;
			const float AnimProgress = 1.0f - powf(gs_ListBoxAnimTime / AnimDuration, 3.0f);
			gs_ListBoxScrollValue = gs_ListBoxAnimInit + (gs_ListBoxScrollTarget - gs_ListBoxAnimInit) * AnimProgress;
		}
		else
			gs_ListBoxScrollValue = gs_ListBoxScrollTarget;
	}
	else
	{
		gs_ListBoxScrollValue = 0.0f;
		gs_ListBoxScrollTarget = 0.0f;
		gs_ListBoxAnimTime = 0.0f;
	}

	gs_ListBoxScrollValue = clamp(gs_ListBoxScrollValue, 0.0f, 1.0f);

	Scroll.HMargin(5.0f, &Scroll);
	float BarValue = DoScrollbarV(pID, &Scroll, gs_ListBoxScrollValue);
	if(fabs(BarValue - gs_ListBoxScrollValue) > 0.0001f)
	{
		gs_ListBoxScrollValue = BarValue;
		gs_ListBoxScrollTarget = BarValue;
		gs_ListBoxAnimInit = BarValue;
		gs_ListBoxAnimTime = 0.0f;
	}

	// the list
	gs_ListBoxView = gs_ListBoxOriginalView;
	gs_ListBoxView.VMargin(5.0f, &gs_ListBoxView);
	UI()->ClipEnable(&gs_ListBoxView);
	gs_ListBoxView.y -= gs_ListBoxScrollValue*Num*Row.h;
}

CMenus::CListboxItem CMenus::UiDoListboxNextRow()
{
	static CUIRect s_RowView;
	CListboxItem Item = {0};
	if(gs_ListBoxItemIndex%gs_ListBoxItemsPerRow == 0)
		gs_ListBoxView.HSplitTop(gs_ListBoxRowHeight /*-2.0f*/, &s_RowView, &gs_ListBoxView);

	s_RowView.VSplitLeft(s_RowView.w/(gs_ListBoxItemsPerRow-gs_ListBoxItemIndex%gs_ListBoxItemsPerRow)/(UI()->Scale()), &Item.m_Rect, &s_RowView);

	Item.m_Visible = 1;
	//item.rect = row;

	Item.m_HitRect = Item.m_Rect;

	//CUIRect select_hit_box = item.rect;

	if(gs_ListBoxSelectedIndex == gs_ListBoxItemIndex)
		Item.m_Selected = 1;

	// make sure that only those in view can be selected
	if(Item.m_Rect.y+Item.m_Rect.h > gs_ListBoxOriginalView.y)
	{

		if(Item.m_HitRect.y < Item.m_HitRect.y) // clip the selection
		{
			Item.m_HitRect.h -= gs_ListBoxOriginalView.y-Item.m_HitRect.y;
			Item.m_HitRect.y = gs_ListBoxOriginalView.y;
		}

	}
	else
		Item.m_Visible = 0;

	// check if we need to do more
	if(Item.m_Rect.y > gs_ListBoxOriginalView.y+gs_ListBoxOriginalView.h)
		Item.m_Visible = 0;

	gs_ListBoxItemIndex++;
	return Item;
}

CMenus::CListboxItem CMenus::UiDoListboxNextItem(const void *pId, bool Selected)
{
	int ThisItemIndex = gs_ListBoxItemIndex;
	if(Selected)
	{
		if(gs_ListBoxSelectedIndex == gs_ListBoxNewSelected)
			gs_ListBoxNewSelected = ThisItemIndex;
		gs_ListBoxSelectedIndex = ThisItemIndex;
	}

	CListboxItem Item = UiDoListboxNextRow();

	if(Item.m_Visible && UI()->DoButtonLogic(pId, "", gs_ListBoxSelectedIndex == gs_ListBoxItemIndex, &Item.m_HitRect))
		gs_ListBoxNewSelected = ThisItemIndex;

	// process input, regard selected index
	if(gs_ListBoxSelectedIndex == ThisItemIndex)
	{
		if(!gs_ListBoxDoneEvents)
		{
			gs_ListBoxDoneEvents = 1;

			if(m_EnterPressed || (UI()->ActiveItem() == pId && Input()->MouseDoubleClick()))
			{
				gs_ListBoxItemActivated = true;
				UI()->SetActiveItem(0);
			}
			else
			{
				for(int i = 0; i < m_NumInputEvents; i++)
				{
					int NewIndex = -1;
					if(m_aInputEvents[i].m_Flags&IInput::FLAG_PRESS)
					{
						if(m_aInputEvents[i].m_Key == KEY_DOWN) NewIndex = gs_ListBoxNewSelected + 1;
						if(m_aInputEvents[i].m_Key == KEY_UP) NewIndex = gs_ListBoxNewSelected - 1;
					}
					if(NewIndex > -1 && NewIndex < gs_ListBoxNumItems)
					{
						// scroll
						float Offset = (NewIndex/gs_ListBoxItemsPerRow-gs_ListBoxNewSelected/gs_ListBoxItemsPerRow)*gs_ListBoxRowHeight;
						int Scroll = gs_ListBoxOriginalView.y > Item.m_Rect.y+Offset ? -1 :
										gs_ListBoxOriginalView.y+gs_ListBoxOriginalView.h < Item.m_Rect.y+Item.m_Rect.h+Offset ? 1 : 0;
						if(Scroll)
						{
							int NumViewable = (int)(gs_ListBoxOriginalView.h/gs_ListBoxRowHeight) + 1;
							int ScrollNum = (gs_ListBoxNumItems+gs_ListBoxItemsPerRow-1)/gs_ListBoxItemsPerRow-NumViewable+1;
							if(Scroll < 0)
							{
								int Num = (gs_ListBoxOriginalView.y-Item.m_Rect.y-Offset+gs_ListBoxRowHeight-1.0f)/gs_ListBoxRowHeight;
								gs_ListBoxScrollTarget -= (1.0f/ScrollNum)*Num;
							}
							else
							{
								int Num = (Item.m_Rect.y+Item.m_Rect.h+Offset-(gs_ListBoxOriginalView.y+gs_ListBoxOriginalView.h)+gs_ListBoxRowHeight-1.0f)/
									gs_ListBoxRowHeight;
								gs_ListBoxScrollTarget += (1.0f/ScrollNum)*Num;
							}
							gs_ListBoxScrollTarget = clamp(gs_ListBoxScrollTarget, 0.0f, 1.0f);
							gs_ListBoxAnimTime = 0.5f;
							gs_ListBoxAnimInit = gs_ListBoxScrollValue;
						}

						gs_ListBoxNewSelected = NewIndex;
					}
				}
			}
		}

		//selected_index = i;
		CUIRect r = Item.m_Rect;
		r.Margin(1.5f, &r);
		RenderTools()->DrawUIRect(&r, vec4(0.18f, 0.66f, 0.46f, 0.5f), CUI::CORNER_ALL, 4.0f);
	}

	return Item;
}

int CMenus::UiDoListboxEnd(float *pScrollValue, bool *pItemActivated)
{
	UI()->ClipDisable();
	if(pScrollValue)
		*pScrollValue = gs_ListBoxScrollValue;
	if(pItemActivated)
		*pItemActivated = gs_ListBoxItemActivated;
	return gs_ListBoxNewSelected;
}

int CMenus::DemolistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	int Length = str_length(pName);
	if((pName[0] == '.' && (pName[1] == 0 ||
		(pName[1] == '.' && pName[2] == 0 && !str_comp(pSelf->m_aCurrentDemoFolder, "demos")))) ||
		(!IsDir && (Length < 5 || str_comp(pName+Length-5, ".demo"))))
		return 0;

	CDemoItem Item;
	str_copy(Item.m_aFilename, pName, sizeof(Item.m_aFilename));
	if(IsDir)
	{
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pName);
		Item.m_Valid = false;
	}
	else
	{
		str_copy(Item.m_aName, pName, min(static_cast<int>(sizeof(Item.m_aName)), Length-4));
		Item.m_InfosLoaded = false;
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_StorageType = StorageType;
	pSelf->m_lDemos.add_unsorted(Item);

	return 0;
}

void CMenus::DemolistPopulate()
{
	m_lDemos.clear();
	if(!str_comp(m_aCurrentDemoFolder, "demos"))
		m_DemolistStorageType = IStorage::TYPE_ALL;
	Storage()->ListDirectory(m_DemolistStorageType, m_aCurrentDemoFolder, DemolistFetchCallback, this);
	m_lDemos.sort_range();
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	m_DemolistSelectedIndex = Reset ? m_lDemos.size() > 0 ? 0 : -1 :
										m_DemolistSelectedIndex >= m_lDemos.size() ? m_lDemos.size()-1 : m_DemolistSelectedIndex;
	m_DemolistSelectedIsDir = m_DemolistSelectedIndex < 0 ? false : m_lDemos[m_DemolistSelectedIndex].m_IsDir;
}

void CMenus::RenderDemoList(CUIRect MainView)
{
	static int s_Inited = 0;
	if(!s_Inited)
	{
		DemolistPopulate();
		DemolistOnUpdate(true);
		s_Inited = 1;
	}

	char aFooterLabel[128] = {0};
	if(m_DemolistSelectedIndex >= 0)
	{
		CDemoItem *Item = &m_lDemos[m_DemolistSelectedIndex];
		if(str_comp(Item->m_aFilename, "..") == 0)
			str_copy(aFooterLabel, Localize("Parent Folder"), sizeof(aFooterLabel));
		else if(m_DemolistSelectedIsDir)
			str_copy(aFooterLabel, Localize("Folder"), sizeof(aFooterLabel));
		else
		{
			if(!Item->m_InfosLoaded)
			{
				char aBuffer[512];
				str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_aCurrentDemoFolder, Item->m_aFilename);
				Item->m_Valid = DemoPlayer()->GetDemoInfo(Storage(), aBuffer, Item->m_StorageType, &Item->m_Info);
				Item->m_InfosLoaded = true;
			}
			if(!Item->m_Valid)
				str_copy(aFooterLabel, Localize("Invalid Demo"), sizeof(aFooterLabel));
			else
				str_copy(aFooterLabel, Localize("Demo details"), sizeof(aFooterLabel));
		}
	}

	// render background
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_ALL, 10.0f);
	MainView.Margin(10.0f, &MainView);

	CUIRect ButtonBar, RefreshRect, PlayRect, DeleteRect, RenameRect, RenderRect, FileIcon, ListBox;
	MainView.HSplitBottom(ms_ButtonHeight+5.0f, &MainView, &ButtonBar);
	ButtonBar.HSplitTop(5.0f, 0, &ButtonBar);
	ButtonBar.VSplitRight(130.0f, &ButtonBar, &PlayRect);
	ButtonBar.VSplitLeft(130.0f, &RefreshRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(120.0f, &DeleteRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(120.0f, &RenameRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(130.0f, &RenderRect, &ButtonBar);
	MainView.HSplitBottom(140.0f, &ListBox, &MainView);

	// render demo info
	MainView.VMargin(5.0f, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, 0);
	RenderTools()->DrawUIRect(&MainView, vec4(0.03f,0.04f,0.05f,0.22f), CUI::CORNER_B, 4.0f);
	if(!m_DemolistSelectedIsDir && m_DemolistSelectedIndex >= 0 && m_lDemos[m_DemolistSelectedIndex].m_Valid)
	{
		CUIRect Left, Right, Labels;
		MainView.Margin(20.0f, &MainView);
		MainView.VSplitMid(&Labels, &MainView);

		// left side
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Created:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aTimestamp, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Type:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aType, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Length:"), 14.0f, -1);
		int Length = ((m_lDemos[m_DemolistSelectedIndex].m_Info.m_aLength[0]<<24)&0xFF000000) | ((m_lDemos[m_DemolistSelectedIndex].m_Info.m_aLength[1]<<16)&0xFF0000) |
					((m_lDemos[m_DemolistSelectedIndex].m_Info.m_aLength[2]<<8)&0xFF00) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aLength[3]&0xFF);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d:%02d", Length/60, Length%60);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Version:"), 14.0f, -1);
		str_format(aBuf, sizeof(aBuf), "%d", m_lDemos[m_DemolistSelectedIndex].m_Info.m_Version);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);

		// right side
		Labels = MainView;
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Map:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapName, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(20.0f, 0, &Left);
		Left.VSplitLeft(130.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Size:"), 14.0f, -1);
		unsigned Size = (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[0]<<24) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[1]<<16) |
					(m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[2]<<8) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapSize[3]);
		str_format(aBuf, sizeof(aBuf), Localize("%d Bytes"), Size);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(20.0f, 0, &Left);
		Left.VSplitLeft(130.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Crc:"), 14.0f, -1);
		unsigned Crc = (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapCrc[0]<<24) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapCrc[1]<<16) |
					(m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapCrc[2]<<8) | (m_lDemos[m_DemolistSelectedIndex].m_Info.m_aMapCrc[3]);
		str_format(aBuf, sizeof(aBuf), "%08x", Crc);
		UI()->DoLabelScaled(&Right, aBuf, 14.0f, -1);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabelScaled(&Left, Localize("Netversion:"), 14.0f, -1);
		UI()->DoLabelScaled(&Right, m_lDemos[m_DemolistSelectedIndex].m_Info.m_aNetversion, 14.0f, -1);
	}

	static int s_DemoListId = 0;
	static float s_ScrollValue = 0;
	UiDoListboxStart(&s_DemoListId, &ListBox, 17.0f, Localize("Demos"), aFooterLabel, m_lDemos.size(), 1, m_DemolistSelectedIndex, s_ScrollValue);
	for(sorted_array<CDemoItem>::range r = m_lDemos.all(); !r.empty(); r.pop_front())
	{
		CListboxItem Item = UiDoListboxNextItem((void*)(&r.front()));
		if(Item.m_Visible)
		{
			Item.m_Rect.VSplitLeft(Item.m_Rect.h, &FileIcon, &Item.m_Rect);
			Item.m_Rect.VSplitLeft(5.0f, 0, &Item.m_Rect);
			DoButton_Icon(IMAGE_FILEICONS, r.front().m_IsDir?SPRITE_FILE_FOLDER:SPRITE_FILE_DEMO1, &FileIcon);
			UI()->DoLabel(&Item.m_Rect, r.front().m_aName, Item.m_Rect.h*ms_FontmodHeight, -1);
		}
	}
	bool Activated = false;
	m_DemolistSelectedIndex = UiDoListboxEnd(&s_ScrollValue, &Activated);
	DemolistOnUpdate(false);

	static int s_RefreshButton = 0;
	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshRect))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}

	static int s_PlayButton = 0;
	if(DoButton_Menu(&s_PlayButton, m_DemolistSelectedIsDir?Localize("Open"):Localize("Play"), 0, &PlayRect) || Activated)
	{
		if(m_DemolistSelectedIndex >= 0)
		{
			if(m_DemolistSelectedIsDir)	// folder
			{
				if(str_comp(m_lDemos[m_DemolistSelectedIndex].m_aFilename, "..") == 0)	// parent folder
					fs_parent_dir(m_aCurrentDemoFolder);
				else	// sub folder
				{
					char aTemp[256];
					str_copy(aTemp, m_aCurrentDemoFolder, sizeof(aTemp));
					str_format(m_aCurrentDemoFolder, sizeof(m_aCurrentDemoFolder), "%s/%s", aTemp, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					m_DemolistStorageType = m_lDemos[m_DemolistSelectedIndex].m_StorageType;
				}
				DemolistPopulate();
				DemolistOnUpdate(true);
			}
			else // file
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Play(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType);
				if(pError)
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
				else
				{
					UI()->SetActiveItem(0);
					return;
				}
			}
		}
	}

	if(!m_DemolistSelectedIsDir)
	{
		static int s_DeleteButton = 0;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &DeleteRect) || m_DeletePressed)
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_DELETE_DEMO;
				return;
			}
		}

		static int s_RenameButton = 0;
		if(DoButton_Menu(&s_RenameButton, Localize("Rename"), 0, &RenameRect))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_RENAME_DEMO;
				str_copy(m_aCurrentDemoFile, m_lDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aCurrentDemoFile));
				return;
			}
		}

		static int s_RenderButton = 0;
		if(DoButton_Menu(&s_RenderButton, Localize("Render"), 0, &RenderRect))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(0);
				m_Popup = POPUP_RENDER_DEMO;
				str_format(m_aDemoRenderSource, sizeof(m_aDemoRenderSource), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
				m_DemoRenderStorageType = m_lDemos[m_DemolistSelectedIndex].m_StorageType;

				// default output name = demo basename without extension
				str_copy(m_aVideoOutputName, m_lDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aVideoOutputName));
				int Len = str_length(m_aVideoOutputName);
				if(Len > 5 && str_comp_nocase(m_aVideoOutputName + Len - 5, ".demo") == 0)
					m_aVideoOutputName[Len - 5] = 0;
				return;
			}
		}
	}
}
