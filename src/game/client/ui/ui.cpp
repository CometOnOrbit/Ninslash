
#include <base/system.h>
#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/client.h>
#include <engine/input.h>
#include <game/client/render/render.h>
#include <game/client/ui/ui.h>
#include <game/client/input/lineinput.h>


/********************************************************
 UI
*********************************************************/

CUI *CUIElementBase::s_pUI = 0;
IGraphics *CUIRect::s_pGraphics = 0;
CRenderTools *CUIRect::s_pRenderTools = 0;

IClient *CUIElementBase::Client() const { return s_pUI->Client(); }
IGraphics *CUIElementBase::Graphics() const { return s_pUI->Graphics(); }
IInput *CUIElementBase::Input() const { return s_pUI->Input(); }
ITextRender *CUIElementBase::TextRender() const { return s_pUI->TextRender(); }

float CButtonContainer::GetFade(bool Checked, float Seconds)
{
	if(Checked || (UI()->HotItem() == this))
	{
		m_FadeStartTime = Client()->LocalTime();
		return 1.0f;
	}
	return clamp(1.0f - (float)(Client()->LocalTime() - m_FadeStartTime) / Seconds, 0.0f, 1.0f);
}

void CUIRect::Init(IGraphics *pGraphics, CRenderTools *pRenderTools)
{
	s_pGraphics = pGraphics;
	s_pRenderTools = pRenderTools;
}

void CUIRect::Draw(const vec4 &Color, float Rounding, int Corners) const
{
	if(!s_pRenderTools)
		return;
	CUIRect Rect = *this;
	s_pRenderTools->DrawUIRect(&Rect, Color, Corners, Rounding);
}

CUI::CUI()
{
	m_pHotItem = 0;
	m_pActiveItem = 0;
	m_pLastActiveItem = 0;
	m_pBecommingHotItem = 0;
	m_ActiveItemValid = false;
	m_pClient = 0;
	m_pInput = 0;
	m_pRenderTools = 0;
	m_pGraphics = 0;
	m_pTextRender = 0;

	m_MouseX = 0;
	m_MouseY = 0;
	m_MouseWorldX = 0;
	m_MouseWorldY = 0;
	m_MouseButtons = 0;
	m_LastMouseButtons = 0;

	m_Screen.x = 0;
	m_Screen.y = 0;
	m_Screen.w = 848.0f;
	m_Screen.h = 480.0f;

	CUIElementBase::Init(this);
}

void CUI::SetGraphics(IGraphics *pGraphics, ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	CUIRect::Init(pGraphics, m_pRenderTools);
}

int CUI::Update(float Mx, float My, float Mwx, float Mwy, int Buttons)
{
	m_MouseX = Mx;
	m_MouseY = My;
	m_MouseWorldX = Mwx;
	m_MouseWorldY = Mwy;
	m_LastMouseButtons = m_MouseButtons;
	m_MouseButtons = Buttons;
	m_pHotItem = m_pBecommingHotItem;
	if(m_pActiveItem)
		m_pHotItem = m_pActiveItem;
	m_pBecommingHotItem = 0;
	return 0;
}

int CUI::MouseInside(const CUIRect *r)
{
	if(m_MouseX >= r->x && m_MouseX <= r->x+r->w && m_MouseY >= r->y && m_MouseY <= r->y+r->h)
		return 1;
	return 0;
}

bool CUI::MouseHovered(const CUIRect *pRect) const
{
	return m_MouseX >= pRect->x && m_MouseX < pRect->x+pRect->w
		&& m_MouseY >= pRect->y && m_MouseY < pRect->y+pRect->h;
}

bool CUI::KeyPress(int Key) const
{
	if(!m_pInput)
		return false;
	return m_pInput->KeyPresses(Key) != 0;
}

void CUI::ConvertMouseMove(float *x, float *y)
{
	float Fac = (float)(g_Config.m_UiMousesens)/g_Config.m_InpMousesens;
	*x = *x*Fac;
	*y = *y*Fac;
}

CUIRect *CUI::Screen()
{
	float Aspect = Graphics()->ScreenAspect();

	m_Screen.h = 600.0f;
	m_Screen.w = Aspect * m_Screen.h;

	return &m_Screen;
}

float CUI::PixelSize()
{
	return Screen()->w/Graphics()->ScreenWidth();
}

void CUI::SetScale(float s)
{
	g_Config.m_UiScale = (int)(s*100.0f);
}

float CUI::Scale()
{
	return g_Config.m_UiScale/100.0f;
}

float CUIRect::Scale() const
{
	return g_Config.m_UiScale/100.0f;
}

void CUI::ClipEnable(const CUIRect *r)
{
	float XScale = Graphics()->ScreenWidth()/Screen()->w;
	float YScale = Graphics()->ScreenHeight()/Screen()->h;
	Graphics()->ClipEnable((int)(r->x*XScale), (int)(r->y*YScale), (int)(r->w*XScale), (int)(r->h*YScale));
}

void CUI::ClipDisable()
{
	Graphics()->ClipDisable();
}

void CUIRect::HSplitMid(CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	float Cut = r.h/2;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut;
	}
}

void CUIRect::HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if (pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut;
	}

	if (pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut;
	}
}

void CUIRect::HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if (pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = r.h - Cut;
	}

	if (pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + r.h - Cut;
		pBottom->w = r.w;
		pBottom->h = Cut;
	}
}


void CUIRect::VSplitMid(CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	float Cut = r.w/2;
//	Cut *= Scale();

	if (pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut;
		pLeft->h = r.h;
	}

	if (pRight)
	{
		pRight->x = r.x + Cut;
		pRight->y = r.y;
		pRight->w = r.w - Cut;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if (pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut;
		pLeft->h = r.h;
	}

	if (pRight)
	{
		pRight->x = r.x + Cut;
		pRight->y = r.y;
		pRight->w = r.w - Cut;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if (pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = r.w - Cut;
		pLeft->h = r.h;
	}

	if (pRight)
	{
		pRight->x = r.x + r.w - Cut;
		pRight->y = r.y;
		pRight->w = Cut;
		pRight->h = r.h;
	}
}

void CUIRect::Margin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x + Cut;
	pOtherRect->y = r.y + Cut;
	pOtherRect->w = r.w - 2*Cut;
	pOtherRect->h = r.h - 2*Cut;
}

void CUIRect::VMargin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x + Cut;
	pOtherRect->y = r.y;
	pOtherRect->w = r.w - 2*Cut;
	pOtherRect->h = r.h;
}

void CUIRect::HMargin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x;
	pOtherRect->y = r.y + Cut;
	pOtherRect->w = r.w;
	pOtherRect->h = r.h - 2*Cut;
}

int CUI::DoButtonLogic(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	// logic
	int ReturnValue = 0;
	int Inside = MouseInside(pRect);
	static int ButtonUsed = 0;

	if(ActiveItem() == pID)
	{
		if(!MouseButton(ButtonUsed))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1+ButtonUsed;
			SetActiveItem(0);
		}
	}
	else if(HotItem() == pID)
	{
		if(MouseButton(0))
		{
			SetActiveItem(pID);
			ButtonUsed = 0;
		}

		if(MouseButton(1))
		{
			SetActiveItem(pID);
			ButtonUsed = 1;
		}
	}

	if(Inside)
		SetHotItem(pID);

	return ReturnValue;
}
/*
int CUI::DoButton(const void *id, const char *text, int checked, const CUIRect *r, ui_draw_button_func draw_func, const void *extra)
{
	// logic
	int ret = 0;
	int inside = ui_MouseInside(r);
	static int button_used = 0;

	if(ui_ActiveItem() == id)
	{
		if(!ui_MouseButton(button_used))
		{
			if(inside && checked >= 0)
				ret = 1+button_used;
			ui_SetActiveItem(0);
		}
	}
	else if(ui_HotItem() == id)
	{
		if(ui_MouseButton(0))
		{
			ui_SetActiveItem(id);
			button_used = 0;
		}

		if(ui_MouseButton(1))
		{
			ui_SetActiveItem(id);
			button_used = 1;
		}
	}

	if(inside)
		ui_SetHotItem(id);

	if(draw_func)
		draw_func(id, text, checked, r, extra);
	return ret;
}*/

void CUI::DoLabel(const CUIRect *r, const char *pText, float Size, int Align, int MaxWidth)
{
	// Size is absolute UI units. Height-relative callers (e.g. button labels)
	// already track layout; use DoLabelScaled for fixed-size labels that should
	// follow ui_scale.
	if(Align == 0)
	{
		float tw = TextRender()->TextWidth(0, Size, pText, MaxWidth);
		TextRender()->Text(0, r->x + r->w/2-tw/2, r->y - Size/10, Size, pText, MaxWidth);
	}
	else if(Align < 0)
		TextRender()->Text(0, r->x, r->y - Size/10, Size, pText, MaxWidth);
	else if(Align > 0)
	{
		float tw = TextRender()->TextWidth(0, Size, pText, MaxWidth);
		TextRender()->Text(0, r->x + r->w-tw, r->y - Size/10, Size, pText, MaxWidth);
	}
}

void CUI::DoLabelScaled(const CUIRect *r, const char *pText, float Size, int Align, int MaxWidth)
{
	DoLabel(r, pText, Size*Scale(), Align, MaxWidth);
}

bool CUI::OnInput(const IInput::CEvent &e)
{
	CLineInput *pActiveInput = CLineInput::GetActiveInput();
	if(pActiveInput && pActiveInput->ProcessInput(e))
		return true;
	return false;
}

bool CUI::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, bool *pChanged)
{
	const bool Inside = MouseHovered(pRect);
	const bool Active = LastActiveItem() == pLineInput;
	const bool Changed = pLineInput->WasChanged();
	const char *pDisplayStr = pLineInput->GetDisplayedString();

	bool UpdateOffset = false;
	float ScrollOffset = pLineInput->GetScrollOffset();

	static bool s_DoScroll = false;
	static int s_SelectionStartOffset = -1;

	FontSize *= Scale();

	CUIRect Textbox = *pRect;
	Textbox.VMargin(2.0f, &Textbox);
	Textbox.HMargin(2.0f, &Textbox);

	if(Active)
	{
		int CursorOffset = pLineInput->GetCursorOffset();

		if(Inside && MouseButton(0) && !Changed)
		{
			s_DoScroll = true;
			const float MxRel = MouseX() - Textbox.x + ScrollOffset;
			float TotalTextWidth = 0.0f;
			for(int i = 1, Offset = 0; i <= pLineInput->GetNumChars(); i++)
			{
				const int PrevOffset = Offset;
				Offset = str_utf8_forward(pDisplayStr, Offset);
				const float AddedTextWidth = TextRender()->TextWidth(0, FontSize, pDisplayStr + PrevOffset, Offset - PrevOffset);
				if(TotalTextWidth + AddedTextWidth/2.0f > MxRel)
				{
					CursorOffset = PrevOffset;
					if(s_SelectionStartOffset < 0)
						s_SelectionStartOffset = CursorOffset;
					UpdateOffset = true;
					break;
				}
				TotalTextWidth += AddedTextWidth;

				if(i == pLineInput->GetNumChars())
				{
					CursorOffset = pLineInput->GetLength();
					if(s_SelectionStartOffset < 0)
						s_SelectionStartOffset = CursorOffset;
					UpdateOffset = true;
				}
			}
		}
		else if(!MouseButton(0) || Changed)
		{
			s_DoScroll = false;
			s_SelectionStartOffset = -1;
		}
		else if(s_DoScroll)
		{
			if(MouseX() < Textbox.x)
			{
				CursorOffset = str_utf8_rewind(pDisplayStr, CursorOffset);
				UpdateOffset = true;
			}
			else if(MouseX() > Textbox.x + Textbox.w)
			{
				CursorOffset = str_utf8_forward(pDisplayStr, CursorOffset);
				UpdateOffset = true;
			}
		}

		if(s_SelectionStartOffset >= 0)
		{
			const int ActualCursorOffset = pLineInput->OffsetFromDisplayToActual(CursorOffset);
			pLineInput->SetCursorOffset(ActualCursorOffset);
			pLineInput->SetSelection(pLineInput->OffsetFromDisplayToActual(s_SelectionStartOffset), ActualCursorOffset);
		}
	}

	bool JustGotActive = false;

	if(CheckActiveItem(pLineInput))
	{
		if(MouseButton(0))
		{
			if(pLineInput->IsActive() && (Input()->HasComposition() || Input()->GetCandidateCount()))
			{
				Input()->StopTextInput();
				Input()->StartTextInput();
			}
		}
		else
		{
			s_DoScroll = false;
			s_SelectionStartOffset = -1;
			SetActiveItem(0);
		}
	}
	else if(HotItem() == pLineInput)
	{
		if(MouseButton(0))
		{
			if(!Active)
				JustGotActive = true;
			SetActiveItem(pLineInput);
		}
	}

	if(Inside)
		SetHotItem(pLineInput);

	if(Active)
		pLineInput->Activate(UI);
	else
		pLineInput->Deactivate();

	ClipEnable(pRect);
	pLineInput->SetRenderOrigin(Textbox.x - ScrollOffset, Textbox.y, FontSize);
	pLineInput->Render(UpdateOffset || Changed);
	ClipDisable();

	if(Active && !JustGotActive && (UpdateOffset || Changed))
	{
		const float CaretX = pLineInput->GetCaretPosition().x - Textbox.x;
		if(CaretX > Textbox.w)
			ScrollOffset += CaretX - Textbox.w;
		else if(CaretX < 0.0f)
			ScrollOffset += CaretX;
		if(ScrollOffset < 0.0f)
			ScrollOffset = 0.0f;
	}

	pLineInput->SetScrollOffset(ScrollOffset);

	if(pChanged)
		*pChanged = Changed;
	return Changed;
}
