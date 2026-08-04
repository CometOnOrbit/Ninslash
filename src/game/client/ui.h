
#ifndef GAME_CLIENT_UI_H
#define GAME_CLIENT_UI_H

#include <base/vmath.h>
#include <engine/input.h>

class CUIRect
{
	float Scale() const;
	static class IGraphics *s_pGraphics;
	static class CRenderTools *s_pRenderTools;

  public:
	static void Init(class IGraphics *pGraphics, class CRenderTools *pRenderTools);

	float x, y, w, h;

	void HSplitMid(CUIRect *pTop, CUIRect *pBottom) const;
	void HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	void HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const;
	void VSplitMid(CUIRect *pLeft, CUIRect *pRight) const;
	void VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const;
	void VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const;

	void Margin(float Cut, CUIRect *pOtherRect) const;
	void VMargin(float Cut, CUIRect *pOtherRect) const;
	void HMargin(float Cut, CUIRect *pOtherRect) const;

	void Draw(const vec4 &Color, float Rounding = 5.0f, int Corners = 0xF) const;
};

class CUI;

class CUIElementBase
{
	static CUI *s_pUI;

  public:
	static void Init(CUI *pUI) { s_pUI = pUI; }
	CUI *UI() const { return s_pUI; }
	class IClient *Client() const;
	class IGraphics *Graphics() const;
	class IInput *Input() const;
	class ITextRender *TextRender() const;
};

class CButtonContainer : public CUIElementBase
{
	float m_FadeStartTime;
	bool m_CleanBackground;

  public:
	CButtonContainer(bool CleanBackground = false) : m_FadeStartTime(0.0f), m_CleanBackground(CleanBackground) {}
	float GetFade(bool Checked = false, float Seconds = 0.6f);
	bool IsCleanBackground() const { return m_CleanBackground; }
};

class CUI
{
	const void *m_pHotItem;
	const void *m_pActiveItem;
	const void *m_pLastActiveItem;
	const void *m_pBecommingHotItem;
	bool m_ActiveItemValid;
	float m_MouseX, m_MouseY;
	float m_MouseWorldX, m_MouseWorldY;
	unsigned m_MouseButtons;
	unsigned m_LastMouseButtons;

	CUIRect m_Screen;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class IClient *m_pClient;
	class IInput *m_pInput;
	class CRenderTools *m_pRenderTools;

	// Nested clip support: DoEditBox and similar enable/disable their own
	// scissor rect. Without a stack the inner ClipDisable drops the enclosing
	// scroll-region clip, leaking text outside the scroll area.
	enum
	{
		MAX_CLIP_DEPTH = 8,
	};
	CUIRect m_aClipStack[MAX_CLIP_DEPTH];
	int m_ClipDepth;

  public:
	void SetGraphics(class IGraphics *pGraphics, class ITextRender *pTextRender);
	void SetClient(class IClient *pClient) { m_pClient = pClient; }
	void SetInput(class IInput *pInput) { m_pInput = pInput; }
	void SetRenderTools(class CRenderTools *pRenderTools) { m_pRenderTools = pRenderTools; }

	class IGraphics *Graphics() { return m_pGraphics; }
	class ITextRender *TextRender() { return m_pTextRender; }
	class IClient *Client() const { return m_pClient; }
	class IInput *Input() const { return m_pInput; }
	class CRenderTools *RenderTools() const { return m_pRenderTools; }

	CUI();

	enum
	{
		CORNER_TL = 1,
		CORNER_TR = 2,
		CORNER_BL = 4,
		CORNER_BR = 8,

		CORNER_T = CORNER_TL | CORNER_TR,
		CORNER_B = CORNER_BL | CORNER_BR,
		CORNER_R = CORNER_TR | CORNER_BR,
		CORNER_L = CORNER_TL | CORNER_BL,

		CORNER_ALL = CORNER_T | CORNER_B
	};

	int Update(float mx, float my, float Mwx, float Mwy, int m_Buttons);

	float MouseX() const { return m_MouseX; }
	float MouseY() const { return m_MouseY; }
	float MouseWorldX() const { return m_MouseWorldX; }
	float MouseWorldY() const { return m_MouseWorldY; }
	int MouseButton(int Index) const { return (m_MouseButtons >> Index) & 1; }
	int MouseButtonClicked(int Index) { return MouseButton(Index) && !((m_LastMouseButtons >> Index) & 1); }

	void SetHotItem(const void *pID) { m_pBecommingHotItem = pID; }
	void SetActiveItem(const void *pID)
	{
		m_pActiveItem = pID;
		if(pID)
			m_pLastActiveItem = pID;
	}
	void ClearLastActiveItem() { m_pLastActiveItem = 0; }
	const void *HotItem() const { return m_pHotItem; }
	const void *NextHotItem() const { return m_pBecommingHotItem; }
	const void *ActiveItem() const { return m_pActiveItem; }
	const void *LastActiveItem() const { return m_pLastActiveItem; }
	bool CheckActiveItem(const void *pID)
	{
		if(m_pActiveItem == pID)
		{
			m_ActiveItemValid = true;
			return true;
		}
		return false;
	}

	int MouseInside(const CUIRect *pRect);
	bool MouseHovered(const CUIRect *pRect) const;
	bool KeyPress(int Key) const;
	void ConvertMouseMove(float *x, float *y);

	CUIRect *Screen();
	float PixelSize();
	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();

	void SetScale(float s);
	float Scale();

	int DoButtonLogic(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButtonLogic(const void *pID, const CUIRect *pRect) { return DoButtonLogic(pID, "", 0, pRect); }

	void DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, int MaxWidth = -1);
	void DoLabelScaled(const CUIRect *pRect, const char *pText, float Size, int Align, int MaxWidth = -1);

	bool OnInput(const IInput::CEvent &e);
	bool DoEditBox(class CLineInput *pLineInput,
				   const CUIRect *pRect,
				   float FontSize,
				   int Corners = CORNER_ALL,
				   bool *pChanged = 0);
};

#endif
