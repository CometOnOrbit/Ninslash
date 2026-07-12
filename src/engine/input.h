
#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include "kernel.h"

#include <base/system.h>

extern const char g_aaKeyStrings[512][20];


class IInput : public IInterface
{
	MACRO_INTERFACE("input", 0)
public:
	class CEvent
	{
	public:
		int m_Flags;
		int m_Key;
		char m_aText[32*UTF8_BYTE_LENGTH+1];
	};

protected:
	enum
	{
		INPUT_BUFFER_SIZE=32
	};

	// quick access to events
	int m_NumEvents;
	IInput::CEvent m_aInputEvents[INPUT_BUFFER_SIZE];

	//quick access to input
	struct
	{
		unsigned char m_Presses;
		unsigned char m_Releases;
	} m_aInputCount[2][1024];

	unsigned char m_aInputState[2][1024];
	int m_InputCurrent;
	bool m_InputDispatched;

	int KeyWasPressed(int Key) { return m_aInputState[m_InputCurrent^1][Key]; }

public:
	enum
	{
		FLAG_PRESS=1,
		FLAG_RELEASE=2,
		FLAG_REPEAT=4,
		FLAG_TEXT=8,

		MAX_CANDIDATES = 16,
		MAX_CANDIDATE_LENGTH = 16,
		MAX_CANDIDATE_ARRAY_SIZE=MAX_CANDIDATE_LENGTH*UTF8_BYTE_LENGTH+1,
		MAX_COMPOSITION_ARRAY_SIZE = 32,

		COMP_LENGTH_INACTIVE = -1
	};

	// events
	int NumEvents() const { return m_NumEvents; }
	void ClearEvents()
	{
		m_NumEvents = 0;
		m_InputDispatched = true;
	}
	CEvent GetEvent(int Index) const
	{
		if(Index < 0 || Index >= m_NumEvents)
		{
			IInput::CEvent e = {0,0};
			e.m_aText[0] = 0;
			return e;
		}
		return m_aInputEvents[Index];
	}

	// keys
	int KeyPressed(int Key) { return m_aInputState[m_InputCurrent][Key]; }
	int KeyReleases(int Key) { return m_aInputCount[m_InputCurrent][Key].m_Releases; }
	int KeyPresses(int Key) { return m_aInputCount[m_InputCurrent][Key].m_Presses; }
	int KeyDown(int Key) { return KeyPressed(Key)&&!KeyWasPressed(Key); }
	const char *KeyName(int Key) { return (Key >= 0 && Key < 512) ? g_aaKeyStrings[Key] : g_aaKeyStrings[0]; }

	enum MouseMode {
		MOUSE_MODE_NONE,
		MOUSE_MODE_WARP_CENTER = 1 << 0,
		MOUSE_MODE_NO_MOUSE = 1 << 1
	};

	//
	virtual void SetMouseModes(int modes) = 0;
	virtual int GetMouseModes() = 0;
	virtual void GetMousePosition(float *x, float *y) = 0;
	virtual void GetRelativePosition(float *x, float *y) = 0;
	virtual bool MouseMoved() = 0;
	virtual bool GamepadMoved() = 0;
	virtual bool UsingGamepad() = 0;
	virtual int MouseDoubleClick() = 0;
	virtual const char* GetClipboardText() = 0;
	virtual void SetClipboardText(const char *Text) = 0;
	virtual bool MouseLeft() = 0;
	virtual bool MouseEntered() = 0;

	virtual int ShowCursor(bool show) = 0;

	// text editing
	virtual void StartTextInput() = 0;
	virtual void StopTextInput() = 0;
	virtual const char *GetComposition() const = 0;
	virtual bool HasComposition() const = 0;
	virtual int GetCompositionCursor() const = 0;
	virtual int GetCompositionSelectedLength() const = 0;
	virtual int GetCompositionLength() const = 0;
	virtual const char *GetCandidate(int Index) const = 0;
	virtual int GetCandidateCount() const = 0;
	virtual int GetCandidateSelectedIndex() const = 0;
	virtual void SetCompositionWindowPosition(float X, float Y, float H) = 0;
};


class IEngineInput : public IInput
{
	MACRO_INTERFACE("engineinput", 0)
public:
	virtual void Init() = 0;
	virtual int Update() = 0;
};

extern IEngineInput *CreateEngineInput();

#endif
