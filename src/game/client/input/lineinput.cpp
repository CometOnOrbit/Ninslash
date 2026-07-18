/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <algorithm>

#include <base/system.h>
#include <base/math.h>

#include <engine/keys.h>
#include <engine/input.h>
#include <engine/textrender.h>
#include <engine/graphics.h>
#include <engine/client.h>

#include <game/client/input/lineinput.h>

IInput *CLineInput::s_pInput = 0;
ITextRender *CLineInput::s_pTextRender = 0;
IGraphics *CLineInput::s_pGraphics = 0;
IClient *CLineInput::s_pClient = 0;

CLineInput *CLineInput::s_pActiveInput = 0;
EInputPriority CLineInput::s_ActiveInputPriority = NONE;

vec2 CLineInput::s_CompositionWindowPosition = vec2(0, 0);
float CLineInput::s_CompositionLineHeight = 0.0f;

char CLineInput::s_aStars[128] = { '\0' };

void CLineInput::SetBuffer(char *pStr, int MaxSize, int MaxChars)
{
	if(m_pStr && m_pStr == pStr)
		return;
	const char *pLastStr = m_pStr;
	m_pStr = pStr;
	m_MaxSize = MaxSize;
	m_MaxChars = MaxChars;
	m_WasChanged = m_pStr && pLastStr && m_WasChanged;
	if(!pLastStr)
	{
		m_CursorPos = m_SelectionStart = m_SelectionEnd = 0;
		m_ScrollOffset = m_ScrollOffsetChange = 0.0f;
		m_CaretPosition = vec2(0, 0);
		m_Hidden = false;
		m_WasRendered = false;
		m_FontSize = 10.0f;
		m_OriginX = m_OriginY = 0.0f;
	}
	if(m_pStr && m_pStr != pLastStr)
		UpdateStrData();
}

void CLineInput::Clear()
{
	mem_zero(m_pStr, m_MaxSize);
	UpdateStrData();
}

void CLineInput::Set(const char *pString)
{
	str_copy(m_pStr, pString, m_MaxSize);
	UpdateStrData();
	SetCursorOffset(m_Len);
}

void CLineInput::SetRange(const char *pString, int Begin, int End)
{
	if(Begin > End)
		std::swap(Begin, End);
	Begin = clamp(Begin, 0, m_Len);
	End = clamp(End, 0, m_Len);

	int RemovedCharSize, RemovedCharCount;
	str_utf8_stats(m_pStr + Begin, End - Begin + 1, m_MaxChars, &RemovedCharSize, &RemovedCharCount);

	int AddedCharSize, AddedCharCount;
	str_utf8_stats(pString, m_MaxSize - m_Len + RemovedCharSize, m_MaxChars - m_NumChars + RemovedCharCount, &AddedCharSize, &AddedCharCount);

	if(RemovedCharSize || AddedCharSize)
	{
		if(AddedCharSize < RemovedCharSize)
		{
			if(AddedCharSize)
				mem_copy(m_pStr + Begin, pString, AddedCharSize);
			mem_move(m_pStr + Begin + AddedCharSize, m_pStr + Begin + RemovedCharSize, m_Len - Begin - AddedCharSize);
		}
		else if(AddedCharSize > RemovedCharSize)
			mem_move(m_pStr + End + AddedCharSize - RemovedCharSize, m_pStr + End, m_Len - End);

		if(AddedCharSize >= RemovedCharSize)
			mem_copy(m_pStr + Begin, pString, AddedCharSize);

		m_CursorPos = End - RemovedCharSize + AddedCharSize;
		m_Len += AddedCharSize - RemovedCharSize;
		m_NumChars += AddedCharCount - RemovedCharCount;
		m_WasChanged = true;
		m_pStr[m_Len] = '\0';
		m_SelectionStart = m_SelectionEnd = m_CursorPos;
	}
}

void CLineInput::Insert(const char *pString, int Begin)
{
	SetRange(pString, Begin, Begin);
}

void CLineInput::Append(const char *pString)
{
	Insert(pString, m_Len);
}

void CLineInput::UpdateStrData()
{
	str_utf8_stats(m_pStr, m_MaxSize, m_MaxChars, &m_Len, &m_NumChars);
	if(m_CursorPos < 0 || m_CursorPos > m_Len)
		SetCursorOffset(m_CursorPos);
}

const char *CLineInput::GetDisplayedString()
{
	if(!IsHidden())
		return m_pStr;

	unsigned NumStars = GetNumChars();
	if(NumStars >= sizeof(s_aStars))
		NumStars = sizeof(s_aStars)-1;
	for(unsigned int i = 0; i < NumStars; ++i)
		s_aStars[i] = '*';
	s_aStars[NumStars] = '\0';
	return s_aStars;
}

void CLineInput::MoveCursor(EMoveDirection Direction, bool MoveWord, const char *pStr, int MaxSize, int *pCursorPos)
{
	int PeekCursorPos = Direction == FORWARD ? *pCursorPos : str_utf8_rewind(pStr, *pCursorPos);
	const char *pTemp = pStr + PeekCursorPos;
	bool AnySpace = str_utf8_is_whitespace(str_utf8_decode(&pTemp));
	bool AnyWord = !AnySpace;
	while(true)
	{
		if(Direction == FORWARD)
			*pCursorPos = str_utf8_forward(pStr, *pCursorPos);
		else
			*pCursorPos = str_utf8_rewind(pStr, *pCursorPos);
		if(!MoveWord || *pCursorPos <= 0 || *pCursorPos >= MaxSize)
			break;
		PeekCursorPos = Direction == FORWARD ? *pCursorPos : str_utf8_rewind(pStr, *pCursorPos);
		pTemp = pStr + PeekCursorPos;
		const bool CurrentSpace = str_utf8_is_whitespace(str_utf8_decode(&pTemp));
		const bool CurrentWord = !CurrentSpace;
		if(Direction == FORWARD && AnySpace && !CurrentSpace)
			break;
		else if(Direction == REWIND && AnyWord && !CurrentWord)
			break;
		AnySpace |= CurrentSpace;
		AnyWord |= CurrentWord;
	}
}

void CLineInput::SetCursorOffset(int Offset)
{
	m_SelectionStart = m_SelectionEnd = m_CursorPos = clamp(Offset, 0, m_Len);
}

void CLineInput::SetSelection(int Start, int End)
{
	if(Start > End)
		std::swap(Start, End);
	m_SelectionStart = clamp(Start, 0, m_Len);
	m_SelectionEnd = clamp(End, 0, m_Len);
}

int CLineInput::OffsetFromActualToDisplay(int ActualOffset) const
{
	if(!IsHidden())
		return ActualOffset;
	int DisplayOffset = 0;
	int CurrentOffset = 0;
	while(CurrentOffset < ActualOffset)
	{
		const int PrevOffset = CurrentOffset;
		CurrentOffset = str_utf8_forward(m_pStr, CurrentOffset);
		if(CurrentOffset == PrevOffset)
			break;
		DisplayOffset++;
	}
	return DisplayOffset;
}

int CLineInput::OffsetFromDisplayToActual(int DisplayOffset) const
{
	if(!IsHidden())
		return DisplayOffset;
	int ActualOffset = 0;
	for(int i = 0; i < DisplayOffset; i++)
	{
		const int PrevOffset = ActualOffset;
		ActualOffset = str_utf8_forward(m_pStr, ActualOffset);
		if(ActualOffset == PrevOffset)
			break;
	}
	return ActualOffset;
}

bool CLineInput::ProcessInput(const IInput::CEvent &Event)
{
	UpdateStrData();

	const int OldCursorPos = m_CursorPos;
	const bool Selecting = s_pInput->KeyPressed(KEY_LSHIFT) || s_pInput->KeyPressed(KEY_RSHIFT);
	const int SelectionLength = GetSelectionLength();

	if(Event.m_Flags&IInput::FLAG_TEXT && !(KEY_LCTRL <= Event.m_Key && Event.m_Key <= KEY_RGUI))
		SetRange(Event.m_aText, m_SelectionStart, m_SelectionEnd);

	if(Event.m_Flags&IInput::FLAG_PRESS)
	{
		const bool CtrlPressed = s_pInput->KeyPressed(KEY_LCTRL) || s_pInput->KeyPressed(KEY_RCTRL);
		const bool AltPressed = s_pInput->KeyPressed(KEY_LALT) || s_pInput->KeyPressed(KEY_RALT);

#ifdef CONF_PLATFORM_MACOSX
		const bool MoveWord = AltPressed && !CtrlPressed;
#else
		const bool MoveWord = CtrlPressed && !AltPressed;
#endif

		if(Event.m_Key == KEY_BACKSPACE)
		{
			if(SelectionLength)
			{
				SetRange("", m_SelectionStart, m_SelectionEnd);
			}
			else
			{
				if(m_CursorPos > 0)
				{
					int NewCursorPos = m_CursorPos;
					MoveCursor(REWIND, MoveWord, m_pStr, m_Len, &NewCursorPos);
					SetRange("", NewCursorPos, m_CursorPos);
				}
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
		}
		else if(Event.m_Key == KEY_DELETE)
		{
			if(SelectionLength)
			{
				SetRange("", m_SelectionStart, m_SelectionEnd);
			}
			else
			{
				if(m_CursorPos < m_Len)
				{
					int EndCursorPos = m_CursorPos;
					MoveCursor(FORWARD, MoveWord, m_pStr, m_Len, &EndCursorPos);
					SetRange("", m_CursorPos, EndCursorPos);
				}
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
			}
		}
		else if(Event.m_Key == KEY_LEFT)
		{
			if(SelectionLength && !Selecting)
			{
				m_CursorPos = m_SelectionStart;
			}
			else if(m_CursorPos > 0)
			{
				MoveCursor(REWIND, MoveWord, m_pStr, m_Len, &m_CursorPos);
				if(Selecting)
				{
					if(m_SelectionStart == OldCursorPos)
						m_SelectionStart = m_CursorPos;
					else if(m_SelectionEnd == OldCursorPos)
						m_SelectionEnd = m_CursorPos;
				}
			}

			if(!Selecting)
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
		}
		else if(Event.m_Key == KEY_RIGHT)
		{
			if(SelectionLength && !Selecting)
			{
				m_CursorPos = m_SelectionEnd;
			}
			else if(m_CursorPos < m_Len)
			{
				MoveCursor(FORWARD, MoveWord, m_pStr, m_Len, &m_CursorPos);
				if(Selecting)
				{
					if(m_SelectionEnd == OldCursorPos)
						m_SelectionEnd = m_CursorPos;
					else if(m_SelectionStart == OldCursorPos)
						m_SelectionStart = m_CursorPos;
				}
			}

			if(!Selecting)
				m_SelectionStart = m_SelectionEnd = m_CursorPos;
		}
		else if(Event.m_Key == KEY_HOME)
		{
			if(Selecting)
			{
				if(SelectionLength && m_CursorPos == m_SelectionEnd)
					m_SelectionEnd = m_SelectionStart;
			}
			else
				m_SelectionEnd = 0;
			m_CursorPos = 0;
			m_SelectionStart = 0;
		}
		else if(Event.m_Key == KEY_END)
		{
			if(Selecting)
			{
				if(SelectionLength && m_CursorPos == m_SelectionStart)
					m_SelectionStart = m_SelectionEnd;
			}
			else
				m_SelectionStart = m_Len;
			m_CursorPos = m_Len;
			m_SelectionEnd = m_Len;
		}
		else if(CtrlPressed && !AltPressed && Event.m_Key == KEY_V)
		{
			const char *pClipboardText = s_pInput->GetClipboardText();
			if(pClipboardText)
				SetRange(pClipboardText, m_SelectionStart, m_SelectionEnd);
		}
		else if(CtrlPressed && !AltPressed && (Event.m_Key == KEY_C || Event.m_Key == KEY_X) && SelectionLength)
		{
			char *pSelection = m_pStr + m_SelectionStart;
			char TempChar = pSelection[SelectionLength];
			pSelection[SelectionLength] = '\0';
			s_pInput->SetClipboardText(pSelection);
			pSelection[SelectionLength] = TempChar;
			if(Event.m_Key == KEY_X)
				SetRange("", m_SelectionStart, m_SelectionEnd);
		}
		else if(CtrlPressed && !AltPressed && Event.m_Key == KEY_A)
		{
			m_SelectionStart = 0;
			m_SelectionEnd = m_CursorPos = m_Len;
		}
	}

	m_WasChanged |= OldCursorPos != m_CursorPos;
	m_WasChanged |= SelectionLength != GetSelectionLength();
	return m_WasChanged;
}

void CLineInput::DrawSelection(float HeightWeight, int Start, int End, vec4 Color, float FontSize, float OriginX, float OriginY)
{
	const char *pDisplayStr = GetDisplayedString();
	const float StartX = OriginX + s_pTextRender->TextWidth(0, FontSize, pDisplayStr, Start);
	const float EndX = OriginX + s_pTextRender->TextWidth(0, FontSize, pDisplayStr, End);
	const float H = FontSize * HeightWeight;
	const float Y = OriginY + FontSize * (1.0f - HeightWeight) * 0.5f;

	s_pGraphics->TextureClear();
	s_pGraphics->QuadsBegin();
	s_pGraphics->SetColor(Color.r, Color.g, Color.b, Color.a);
	IGraphics::CQuadItem Quad(StartX, Y, EndX - StartX, H);
	s_pGraphics->QuadsDrawTL(&Quad, 1);
	s_pGraphics->QuadsEnd();
}

void CLineInput::Render(bool Changed)
{
	m_WasRendered = true;

	if(!m_pStr)
		return;

	const char *pDisplayStr = GetDisplayedString();
	const float FontSize = m_FontSize > 0.0f ? m_FontSize : 10.0f;
	const float OriginX = m_OriginX;
	const float OriginY = m_OriginY;

	CTextCursor Cursor;
	s_pTextRender->SetCursor(&Cursor, OriginX, OriginY, FontSize, TEXTFLAG_RENDER);

	if(IsActive())
	{
		const int CursorOffset = GetCursorOffset();
		const int DisplayCursorOffset = OffsetFromActualToDisplay(CursorOffset);
		const bool HasComposition = s_pInput->HasComposition();

		if(HasComposition)
		{
			s_pTextRender->TextEx(&Cursor, pDisplayStr, DisplayCursorOffset);
			s_pTextRender->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
			s_pTextRender->TextEx(&Cursor, s_pInput->GetComposition(), -1);
			s_pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			s_pTextRender->TextEx(&Cursor, pDisplayStr + DisplayCursorOffset, -1);

			const float CompStartX = OriginX + s_pTextRender->TextWidth(0, FontSize, pDisplayStr, DisplayCursorOffset);
			const float CompW = s_pTextRender->TextWidth(0, FontSize, s_pInput->GetComposition(), -1);
			DrawSelection(0.1f, DisplayCursorOffset, DisplayCursorOffset + s_pInput->GetCompositionLength(), vec4(0.7f, 0.7f, 0.7f, 0.7f), FontSize, OriginX, OriginY);
			(void)CompStartX;
			(void)CompW;
		}
		else
		{
			s_pTextRender->TextEx(&Cursor, pDisplayStr, -1);
		}

		if(GetSelectionLength() || HasComposition)
		{
			const int DisplayCompositionStart = OffsetFromActualToDisplay(CursorOffset + s_pInput->GetCompositionCursor());
			const int Start = HasComposition ? DisplayCompositionStart : OffsetFromActualToDisplay(GetSelectionStart());
			const int End = HasComposition ? (DisplayCompositionStart + s_pInput->GetCompositionSelectedLength()) : OffsetFromActualToDisplay(GetSelectionEnd());
			DrawSelection(1.0f, Start, End, vec4(0.3f, 0.3f, 0.3f, 0.3f), FontSize, OriginX, OriginY);
		}

		const float CaretX = OriginX + s_pTextRender->TextWidth(0, FontSize, pDisplayStr, HasComposition ? OffsetFromActualToDisplay(CursorOffset + s_pInput->GetCompositionCursor()) : DisplayCursorOffset);
		m_CaretPosition = vec2(CaretX, OriginY);

		{
			const float LocalTime = s_pClient->LocalTime();
			static float s_LastChanged = 0.0f;
			if(Changed)
				s_LastChanged = LocalTime;
			if(fmod(LocalTime - s_LastChanged, 1.0f) < 0.5f)
			{
				CTextCursor Marker;
				s_pTextRender->SetCursor(&Marker, CaretX - FontSize * 0.15f, OriginY, FontSize, TEXTFLAG_RENDER);
				s_pTextRender->TextEx(&Marker, "|", -1);
			}
		}

		SetCompositionWindowPosition(vec2(CaretX, OriginY + FontSize), FontSize);
	}
	else
	{
		s_pTextRender->TextEx(&Cursor, pDisplayStr, -1);
		m_CaretPosition = vec2(OriginX + s_pTextRender->TextWidth(0, FontSize, pDisplayStr, -1), OriginY);
	}
}

void CLineInput::RenderCandidates()
{
	// Drop UI inputs that were not drawn this frame (closed menus/popups).
	// Chat/console draw manually — they call MarkRendered() instead of Render().
	CLineInput *pActiveInput = GetActiveInput();
	if(pActiveInput != nullptr)
	{
		if(pActiveInput->m_WasRendered)
			pActiveInput->m_WasRendered = false;
		else if(s_ActiveInputPriority == UI)
		{
			pActiveInput->Deactivate();
			return;
		}
	}

	if(!s_pInput->HasComposition() || !s_pInput->GetCandidateCount())
		return;

	const float FontSize = 7.0f;
	const float HMargin = 8.0f;
	const float VMargin = 4.0f;
	const float Height = 300;
	const float Width = Height * s_pGraphics->ScreenAspect();
	const int ScreenWidth = s_pGraphics->ScreenWidth();
	const int ScreenHeight = s_pGraphics->ScreenHeight();

	s_pGraphics->MapScreen(0, 0, Width, Height);

	float MaxW = 0.0f;
	for(int i = 0; i < s_pInput->GetCandidateCount(); ++i)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d. %s", (i+1)%10, s_pInput->GetCandidate(i));
		MaxW = max(MaxW, s_pTextRender->TextWidth(0, FontSize, aBuf, -1));
	}

	const float BoxW = MaxW + HMargin;
	const float BoxH = s_pInput->GetCandidateCount() * FontSize * 1.35f + VMargin;

	vec2 Position = s_CompositionWindowPosition / vec2(ScreenWidth, ScreenHeight) * vec2(Width, Height);
	float BoxX = Position.x;
	float BoxY = Position.y;
	if(BoxY + FontSize * 13.5f > Height)
		BoxY -= BoxH + s_CompositionLineHeight / ScreenHeight * Height;
	if(BoxX + BoxW + HMargin > Width)
		BoxX -= BoxX + BoxW + HMargin - Width;

	s_pGraphics->TextureClear();
	s_pGraphics->QuadsBegin();
	s_pGraphics->BlendNormal();
	s_pGraphics->SetColor(0.0f, 0.0f, 0.0f, 0.8f);
	IGraphics::CQuadItem Shadow(BoxX+0.75f, BoxY+0.75f, BoxW, BoxH);
	s_pGraphics->QuadsDrawTL(&Shadow, 1);
	s_pGraphics->SetColor(0.15f, 0.15f, 0.15f, 1.0f);
	IGraphics::CQuadItem Bg(BoxX, BoxY, BoxW, BoxH);
	s_pGraphics->QuadsDrawTL(&Bg, 1);

	const int Selected = s_pInput->GetCandidateSelectedIndex();
	if(Selected >= 0 && Selected < s_pInput->GetCandidateCount())
	{
		s_pGraphics->SetColor(0.1f, 0.4f, 0.8f, 1.0f);
		IGraphics::CQuadItem Hi(BoxX+HMargin/4, BoxY+VMargin/2+Selected*FontSize*1.35f, BoxW-HMargin/2, FontSize*1.35f);
		s_pGraphics->QuadsDrawTL(&Hi, 1);
	}
	s_pGraphics->QuadsEnd();

	for(int i = 0; i < s_pInput->GetCandidateCount(); ++i)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d. %s", (i+1)%10, s_pInput->GetCandidate(i));
		CTextCursor Cand;
		s_pTextRender->SetCursor(&Cand, BoxX+HMargin/2, BoxY+VMargin/2+i*FontSize*1.35f, FontSize, TEXTFLAG_RENDER);
		s_pTextRender->TextEx(&Cand, aBuf, -1);
	}
}

void CLineInput::SetCompositionWindowPosition(vec2 Anchor, float LineHeight)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	const int ScreenWidth = s_pGraphics->ScreenWidth();
	const int ScreenHeight = s_pGraphics->ScreenHeight();
	s_pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	vec2 ScreenScale = vec2(ScreenWidth / (ScreenX1 - ScreenX0), ScreenHeight / (ScreenY1 - ScreenY0));
	s_CompositionWindowPosition = Anchor * ScreenScale;
	s_CompositionLineHeight = LineHeight * ScreenScale.y;
	s_pInput->SetCompositionWindowPosition(s_CompositionWindowPosition.x, s_CompositionWindowPosition.y - s_CompositionLineHeight, s_CompositionLineHeight);
}

void CLineInput::Activate(EInputPriority Priority)
{
	if(IsActive())
		return;
	if(s_ActiveInputPriority != NONE && Priority < s_ActiveInputPriority)
		return;
	if(s_pActiveInput)
		s_pActiveInput->OnDeactivate();
	s_pActiveInput = this;
	s_pActiveInput->OnActivate();
	s_ActiveInputPriority = Priority;
}

void CLineInput::Deactivate()
{
	if(!IsActive())
		return;
	s_pActiveInput->OnDeactivate();
	s_pActiveInput = 0x0;
	s_ActiveInputPriority = NONE;
}

void CLineInput::OnActivate()
{
	s_pInput->StartTextInput();
}

void CLineInput::OnDeactivate()
{
	s_pInput->StopTextInput();
}
