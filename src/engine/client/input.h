
#ifndef ENGINE_CLIENT_INPUT_H
#define ENGINE_CLIENT_INPUT_H

class CInput : public IEngineInput
{
	class IPlatformServices *m_pPlatformServices;
	IEngineGraphics *m_pGraphics;
	IEngineGamepad *m_pGamepad;

	int m_MouseModes;

	int m_LastMousePosX;
	int m_LastMousePosY;
	bool m_FirstWarp;
	char *m_pClipboardText;
	bool m_MouseLeft;
	bool m_MouseEntered;

	int m_GamepadMove;
	bool m_GamepadDown;
	bool m_GamepadJump;
	bool m_GamepadShoot;
	bool m_GamepadSelect;

	bool m_UsingGamepad;
	int m_GamepadAimX;
	int m_GamepadAimY;

	int m_GamepadOldAimX;
	int m_GamepadOldAimY;
	int64 m_LastGamepadRelativeTime;
	bool m_aSteamInputPrevious[64];
	int m_GamepadActionSet;
	bool m_TextInputActive;

	void ResetGamepad();
	void UpdateSteamInput();
	void SetSteamVirtualKey(int Key, bool Down, bool *pPrevious);

	SDL_Surface *m_pCursorSurface;
	SDL_Cursor *m_pCursor;

	int64 m_LastRelease;
	int64 m_ReleaseDelta;

	// ime support
	char m_aComposition[MAX_COMPOSITION_ARRAY_SIZE];
	int m_CompositionCursor;
	int m_CompositionSelectedLength;
	int m_CompositionLength;
	char m_aaCandidates[MAX_CANDIDATES][MAX_CANDIDATE_ARRAY_SIZE];
	int m_CandidateCount;
	int m_CandidateSelectedIndex;

	void AddEvent(const char *pText, int Key, int Flags);

	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineGamepad *Gamepad() { return m_pGamepad; }
	SDL_Window *Window() { return (SDL_Window *)m_pGraphics->GetWindowHandle(); }

  public:
	CInput();

	virtual void Init();

	virtual void SetMouseModes(int modes);
	virtual int GetMouseModes();
	virtual void GetMousePosition(float *x, float *y);
	virtual void GetRelativePosition(float *x, float *y);
	virtual bool MouseMoved();
	virtual bool GamepadMoved();
	virtual bool UsingGamepad() { return m_UsingGamepad; }
	virtual void GetGamepadAim(float *pX, float *pY);
	virtual void SetGamepadActionSet(int ActionSet);
	virtual int MouseDoubleClick();
	virtual const char *GetClipboardText();
	virtual void SetClipboardText(const char *Text);
	virtual bool MouseLeft();
	virtual bool MouseEntered();

	virtual int ShowCursor(bool show);

	void LoadHardwareCursor();

	void ClearKeyStates();
	int KeyState(int Key);

	int ButtonPressed(int Button) { return m_aInputState[m_InputCurrent][Button]; }

	virtual int Update();

	virtual void StartTextInput();
	virtual void StopTextInput();
	virtual const char *GetComposition() const { return m_aComposition; }
	virtual bool HasComposition() const { return m_CompositionLength != COMP_LENGTH_INACTIVE; }
	virtual int GetCompositionCursor() const { return m_CompositionCursor; }
	virtual int GetCompositionSelectedLength() const { return m_CompositionSelectedLength; }
	virtual int GetCompositionLength() const { return m_CompositionLength; }
	virtual const char *GetCandidate(int Index) const { return m_aaCandidates[Index]; }
	virtual int GetCandidateCount() const { return m_CandidateCount; }
	virtual int GetCandidateSelectedIndex() const { return m_CandidateSelectedIndex; }
	virtual void SetCompositionWindowPosition(float X, float Y, float H);
};

#endif
