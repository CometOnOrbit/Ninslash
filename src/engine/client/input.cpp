#include <SDL3/SDL.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/gamepad.h>
#include <engine/input_processing.h>
#include <engine/console.h>
#include <engine/storage.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/platform_services.h>

#include "input.h"

// print >>f, "int inp_key_code(const char *key_name) { int i; if (!strcmp(key_name, \"-?-\")) return -1; else for (i =
// 0; i < 512; i++) if (!strcmp(key_strings[i], key_name)) return i; return -1; }"

// this header is protected so you don't include it from anywere
#define KEYS_INCLUDE
#include "keynames.h"
#undef KEYS_INCLUDE

void CInput::AddEvent(const char *pText, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		if(!pText)
			m_aInputEvents[m_NumEvents].m_aText[0] = 0;
		else
			str_copy(m_aInputEvents[m_NumEvents].m_aText, pText, sizeof(m_aInputEvents[m_NumEvents].m_aText));
		m_NumEvents++;
	}
}

CInput::CInput()
{
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	mem_zero(m_aInputState, sizeof(m_aInputState));

	m_MouseModes = 0;

	m_InputCurrent = 0;
	m_InputDispatched = false;

	m_IgnoreNextRelative = false;
	m_PendingRelX = 0;
	m_PendingRelY = 0;
	m_WheelAccumY = 0;
	m_LastMousePosX = 0;
	m_LastMousePosY = 0;

	m_pCursorSurface = 0;
	m_pCursor = 0;
	m_pConsole = 0;

	m_LastRelease = 0;
	m_ReleaseDelta = -1;
	m_WheelDebugWasEnabled = false;
	m_WheelDebugHeartbeat = 0;

	m_MouseLeft = false;
	m_MouseEntered = false;

	m_GamepadMove = 0;
	m_GamepadDown = false;
	m_GamepadJump = false;
	m_GamepadShoot = false;
	m_GamepadSelect = false;

	m_UsingGamepad = false;
	m_GamepadAimX = 0;
	m_GamepadAimY = 0;
	m_GamepadOldAimX = 0;
	m_GamepadOldAimY = 0;
	m_LastGamepadRelativeTime = 0;
	mem_zero(m_aSteamInputPrevious, sizeof(m_aSteamInputPrevious));
	m_GamepadActionSet = PLATFORM_INPUT_MENU;
	m_TextInputActive = false;

	m_NumEvents = 0;

	m_pClipboardText = 0;

	m_CompositionLength = COMP_LENGTH_INACTIVE;
	m_CompositionCursor = 0;
	m_CompositionSelectedLength = 0;
	m_CandidateCount = 0;
	m_CandidateSelectedIndex = -1;
	m_aComposition[0] = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pGamepad = Kernel()->RequestInterface<IEngineGamepad>();
	m_pPlatformServices = Kernel()->RequestInterface<IPlatformServices>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	StopTextInput();
	ShowCursor(true);
}

void CInput::StartTextInput()
{
	m_TextInputActive = true;
	SDL_StartTextInput(Window());
}

void CInput::StopTextInput()
{
	m_TextInputActive = false;
	SDL_StopTextInput(Window());
	m_CompositionLength = COMP_LENGTH_INACTIVE;
	m_CompositionCursor = 0;
	m_aComposition[0] = 0;
	m_CompositionSelectedLength = 0;
	m_CandidateCount = 0;
	m_CandidateSelectedIndex = -1;
}

void CInput::SetGamepadActionSet(int ActionSet)
{
	if(ActionSet >= PLATFORM_INPUT_GAME && ActionSet < NUM_PLATFORM_INPUT_ACTION_SETS)
		m_GamepadActionSet = ActionSet;
}

void CInput::SetSteamVirtualKey(int Key, bool Down, bool *pPrevious)
{
	if(!pPrevious || *pPrevious == Down || Key < 0 || Key >= 1024)
		return;
	*pPrevious = Down;
	const int Flags = Down ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
	if(Down)
		m_aInputCount[m_InputCurrent][Key].m_Presses++;
	else
		m_aInputCount[m_InputCurrent][Key].m_Releases++;
	m_aInputState[m_InputCurrent][Key] = Down ? 1 : 0;
	AddEvent(0, Key, Flags);
}

void CInput::UpdateSteamInput()
{
	if(!m_pPlatformServices || !m_pPlatformServices->SteamInputActive())
		return;
	m_pPlatformServices->SetInputActionSet(m_TextInputActive ? PLATFORM_INPUT_CHAT
															 : (EPlatformInputActionSet)m_GamepadActionSet);
	CPlatformInputState State;
	if(!m_pPlatformServices->ReadInputState(&State) || !State.m_Connected)
		return;
	static const int s_aKeys[NUM_PLATFORM_INPUT_ACTIONS] = {
		KEY_GAMEPAD_BUTTON_A,		   KEY_GAMEPAD_BUTTON_B,		 KEY_GAMEPAD_TRIGGER_RIGHT,
		KEY_GAMEPAD_TRIGGER_LEFT,	   KEY_GAMEPAD_BUTTON_BACK,		 KEY_GAMEPAD_BUTTON_Y,
		KEY_GAMEPAD_BUTTON_B,		   KEY_GAMEPAD_BUTTON_A,		 KEY_GAMEPAD_BUTTON_LEFTSTICK,
		KEY_GAMEPAD_BUTTON_RIGHTSTICK, KEY_GAMEPAD_SHOULDER_LEFT,	 KEY_GAMEPAD_SHOULDER_RIGHT,
		KEY_GAMEPAD_BUTTON_DPAD_UP,	   KEY_GAMEPAD_BUTTON_DPAD_DOWN, KEY_GAMEPAD_BUTTON_DPAD_LEFT,
		KEY_GAMEPAD_BUTTON_DPAD_RIGHT, KEY_GAMEPAD_BUTTON_A,		 KEY_GAMEPAD_BUTTON_RIGHTSTICK,
		KEY_GAMEPAD_TRIGGER_LEFT,	   KEY_GAMEPAD_BUTTON_X,		 KEY_GAMEPAD_BUTTON_Y,
		KEY_GAMEPAD_BUTTON_LEFTSTICK,  KEY_GAMEPAD_BUTTON_DPAD_LEFT, KEY_GAMEPAD_BUTTON_DPAD_UP,
		KEY_GAMEPAD_BUTTON_DPAD_RIGHT, KEY_GAMEPAD_BUTTON_DPAD_DOWN, KEY_GAMEPAD_BUTTON_START,
		KEY_GAMEPAD_BUTTON_A,		   KEY_GAMEPAD_BUTTON_B,		 KEY_GAMEPAD_BUTTON_X,
		KEY_GAMEPAD_BUTTON_START,	   KEY_GAMEPAD_BUTTON_A,		 KEY_GAMEPAD_SHOULDER_LEFT,
		KEY_GAMEPAD_SHOULDER_RIGHT,	   KEY_GAMEPAD_TRIGGER_RIGHT,	 KEY_GAMEPAD_TRIGGER_LEFT};
	const float MovePress = g_Config.m_ClGamepadMoveDeadzone / 100.0f;
	const float MoveRelease = max(0.05f, MovePress - 0.10f);
	m_GamepadMove = ProcessDigitalAxis(State.m_MoveX, m_GamepadMove, MovePress, MoveRelease);
	const int Vertical = ProcessDigitalAxis(State.m_MoveY, m_GamepadDown ? 1 : (m_GamepadJump ? -1 : 0), MovePress, MoveRelease);
	const bool Left = m_GamepadMove < 0;
	const bool Right = m_GamepadMove > 0;
	const bool Up = Vertical < 0;
	const bool Down = Vertical > 0;
	bool AnyAction = false;
	for(int i = 0; i < NUM_PLATFORM_INPUT_ACTIONS; i++)
	{
		bool ActionDown = State.m_aActions[i];
		if(i == PLATFORM_ACTION_LEFT)
			ActionDown = ActionDown || Left;
		else if(i == PLATFORM_ACTION_RIGHT)
			ActionDown = ActionDown || Right;
		else if(i == PLATFORM_ACTION_UP)
			ActionDown = ActionDown || Up;
		else if(i == PLATFORM_ACTION_DOWN)
			ActionDown = ActionDown || Down;
		AnyAction = AnyAction || ActionDown;
		SetSteamVirtualKey(s_aKeys[i], ActionDown, &m_aSteamInputPrevious[i]);
	}
	m_GamepadJump = Up;
	m_GamepadDown = Down;
	float AimX = State.m_AimX, AimY = State.m_AimY;
	if(g_Config.m_ClSteamGyro)
	{
		const float Scale = g_Config.m_ClSteamGyroSensitivity / 200000.0f;
		AimX += State.m_GyroX * Scale;
		AimY += State.m_GyroY * Scale * (g_Config.m_ClSteamGyroInvert ? -1.0f : 1.0f);
	}
	m_GamepadAimX = (int)(clamp(AimX, -1.0f, 1.0f) * 32767.0f);
	m_GamepadAimY = (int)(clamp(AimY, -1.0f, 1.0f) * 32767.0f);
	if(AnyAction || length(vec2(AimX, AimY)) > g_Config.m_ClGamepadAimDeadzone / 100.0f)
		m_UsingGamepad = true;
}

void CInput::GetGamepadAim(float *pX, float *pY)
{
	const vec2 Raw(m_GamepadAimX / 32767.0f, m_GamepadAimY / 32767.0f);
	const CProcessedStick Processed = ProcessRadialStick(Raw, g_Config.m_ClGamepadAimDeadzone / 100.0f, 0.95f, g_Config.m_ClGamepadAimCurve / 100.0f);
	*pX = Processed.m_Value.x;
	*pY = Processed.m_Value.y * (g_Config.m_ClGamepadInvertY ? -1.0f : 1.0f);
}

void CInput::SetCompositionWindowPosition(float X, float Y, float H)
{
	const float Scale = g_Config.m_GfxScreenWidth > 0 ? m_pGraphics->ScreenWidth() / (float)g_Config.m_GfxScreenWidth : 1.0f;
	SDL_Rect Rect;
	Rect.x = (int)(X / Scale);
	Rect.y = (int)(Y / Scale);
	Rect.w = 0;
	Rect.h = (int)(H / Scale);
	SDL_SetTextInputArea(Window(), &Rect, 0);
}

void CInput::LoadHardwareCursor()
{
	if(m_pCursor != 0)
		return;

	CImageInfo CursorImg;
	if(!m_pGraphics->LoadPNG(&CursorImg, "gui_cursor_small.png", IStorage::TYPE_ALL))
		return;

	m_pCursorSurface =
		SDL_CreateSurfaceFrom(CursorImg.m_Width,
							  CursorImg.m_Height,
							  SDL_GetPixelFormatForMasks(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000),
							  CursorImg.m_pData,
							  4 * CursorImg.m_Width);

	if(!m_pCursorSurface)
		return;

	m_pCursor = SDL_CreateColorCursor(m_pCursorSurface, 0, 0);
}

int CInput::ShowCursor(bool show)
{
	if(g_Config.m_InpHWCursor)
	{
		LoadHardwareCursor();
		SDL_SetCursor(m_pCursor);
	}
	return show ? SDL_ShowCursor() : SDL_HideCursor();
}

void CInput::UpdateMouseGrab(bool WindowFocused)
{
	// Relative mode already grabs via SDL_UpdateWindowGrab → X11_SetWindowMouseGrab.
	// Calling SDL_SetWindowMouseGrab again (our old GrabWindow path) re-runs
	// XUngrabPointer + XGrabPointer(owner_events=False), which races XI2 and
	// drops mousewheel notches under grab — same class of bug as
	// libsdl-org/sdl2-compat#596 / SDL#15553 (DDNet wheel-on-X11).

	SDL_Window *pWindow = Window();
	if(!pWindow)
		return;

	const bool WantRelative = IInput::ShouldGrabMouse(m_MouseModes, WindowFocused);
	const bool IsRelative = SDL_GetWindowRelativeMouseMode(pWindow);
	if(WantRelative && !IsRelative)
	{
		// Clear any leftover explicit grab flag from older builds; relative owns grab.
		if(SDL_GetWindowMouseGrab(pWindow))
			SDL_SetWindowMouseGrab(pWindow, false);

		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, g_Config.m_InpGrab ? "1" : "0");
		SDL_SetWindowRelativeMouseMode(pWindow, true);
		SDL_GetRelativeMouseState(NULL, NULL);
		m_IgnoreNextRelative = true;
		m_PendingRelX = 0;
		m_PendingRelY = 0;
	}
	else if(!WantRelative && IsRelative)
	{
		SDL_SetWindowRelativeMouseMode(pWindow, false);
	}
}

void CInput::SetMouseModes(int modes)
{
	if(m_MouseModes == modes)
		return;

	const bool WindowFocused = Graphics()->WindowActive();
	const bool WasRelative = IInput::ShouldGrabMouse(m_MouseModes, WindowFocused);
	const bool WantRelative = IInput::ShouldGrabMouse(modes, WindowFocused);

	if(!WasRelative && WantRelative)
	{
		float nx = 0, ny = 0;
		SDL_GetMouseState(&nx, &ny);
		m_LastMousePosX = (int)nx;
		m_LastMousePosY = (int)ny;
	}

	m_MouseModes = modes;
	UpdateMouseGrab(WindowFocused);

	if(WasRelative && !WantRelative)
		m_pGraphics->WarpMouse(m_LastMousePosX, m_LastMousePosY);
}

int CInput::GetMouseModes()
{
	return m_MouseModes;
}

void CInput::GetMousePosition(float *x, float *y)
{
	if(GetMouseModes() & MOUSE_MODE_NO_MOUSE)
		return;

	float Sens = g_Config.m_InpMousesens / 100.0f;
	float nx = 0, ny = 0;
	SDL_GetMouseState(&nx, &ny);

	*x = nx * Sens;
	*y = ny * Sens;
}

void CInput::GetRelativePosition(float *x, float *y)
{
	if(m_UsingGamepad)
	{
		float AimX = 0.0f, AimY = 0.0f;
		GetGamepadAim(&AimX, &AimY);
		const int64 Now = time_get();
		const float DeltaSeconds = m_LastGamepadRelativeTime ? (Now - m_LastGamepadRelativeTime) / (float)time_freq() : 1.0f / 60.0f;
		m_LastGamepadRelativeTime = Now;
		const vec2 Delta = IntegrateAimStick(vec2(AimX, AimY), 900.0f, g_Config.m_ClGamepadAimSensitivity / 100.0f, DeltaSeconds);
		*x = Delta.x;
		*y = Delta.y;
		return;
	}
	m_LastGamepadRelativeTime = 0;

	// Raw window-space deltas from MouseMoved (teeworlds MouseRelative).
	// In-game sensitivity is applied by the component (InpMousesens/100).
	*x = m_PendingRelX;
	*y = m_PendingRelY;
	m_PendingRelX = 0;
	m_PendingRelY = 0;
}

bool CInput::MouseMoved()
{
	float x = 0, y = 0;
	SDL_GetRelativeMouseState(&x, &y);
	if(m_IgnoreNextRelative)
	{
		m_IgnoreNextRelative = false;
		x = 0;
		y = 0;
	}
	m_PendingRelX = x;
	m_PendingRelY = y;
	const bool Moved = round_to_int(x) != 0 || round_to_int(y) != 0;
	if(Moved)
		m_UsingGamepad = false;
	return Moved;
}

bool CInput::GamepadMoved()
{
	if(!m_UsingGamepad)
		return false;

	float AimX = 0.0f, AimY = 0.0f;
	GetGamepadAim(&AimX, &AimY);
	if(m_GamepadOldAimX != m_GamepadAimX || m_GamepadOldAimY != m_GamepadAimY || length(vec2(AimX, AimY)) > 0.0f)
	{
		m_GamepadOldAimX = m_GamepadAimX;
		m_GamepadOldAimY = m_GamepadAimY;
		return true;
	}
	return false;
}

int CInput::MouseDoubleClick()
{
	if(m_ReleaseDelta >= 0 && m_ReleaseDelta < (time_freq() >> 2))
	{
		m_LastRelease = 0;
		m_ReleaseDelta = -1;
		return 1;
	}
	return 0;
}

const char *CInput::GetClipboardText()
{
	if(m_pClipboardText)
		SDL_free(m_pClipboardText);
	m_pClipboardText = SDL_GetClipboardText();
	if(m_pClipboardText)
		str_sanitize_cc(m_pClipboardText);
	return m_pClipboardText;
}

void CInput::SetClipboardText(const char *Text)
{
	SDL_SetClipboardText(Text);
}

bool CInput::MouseLeft()
{
	return m_MouseLeft;
}

bool CInput::MouseEntered()
{
	return m_MouseEntered;
}

void CInput::ClearKeyStates()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
}

int CInput::KeyState(int Key)
{
	return m_aInputState[m_InputCurrent][Key];
}

void CInput::ResetGamepad()
{
	m_GamepadMove = 0;
	m_GamepadDown = false;
	m_GamepadJump = false;
	m_GamepadShoot = false;
	m_GamepadSelect = false;

	m_UsingGamepad = false;
	m_GamepadAimX = 0;
	m_GamepadAimY = 0;
	m_GamepadOldAimX = 0;
	m_GamepadOldAimY = 0;
	m_LastGamepadRelativeTime = 0;
}

int CInput::Update()
{
	if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
	{
		const int64 Now = time_get();
		if(!m_WheelDebugWasEnabled || Now >= m_WheelDebugHeartbeat)
		{
			const SDL_WindowFlags Flags = SDL_GetWindowFlags(Window());
			char aBuf[256];
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s video=%s focus_mouse=%d focus_input=%d relative=%d mouse_mode=%d events=%d",
					   m_WheelDebugWasEnabled ? "heartbeat" : "diagnostics-enabled",
					   SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown",
					   (Flags & SDL_WINDOW_MOUSE_FOCUS) != 0,
					   (Flags & SDL_WINDOW_INPUT_FOCUS) != 0,
					   SDL_GetWindowRelativeMouseMode(Window()) ? 1 : 0,
					   m_MouseModes,
					   m_NumEvents);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
			m_WheelDebugHeartbeat = Now + time_freq();
		}
		m_WheelDebugWasEnabled = true;
	}
	else
		m_WheelDebugWasEnabled = false;

	//	if(m_InputGrabbed && !Graphics()->WindowActive())
	//		MouseModeAbsolute();

	/*if(!input_grabbed && Graphics()->WindowActive())
		Input()->MouseModeRelative();*/

	if(m_InputDispatched)
	{
		// clear and begin count on the other one
		m_InputCurrent ^= 1;
		mem_zero(&m_aInputCount[m_InputCurrent], sizeof(m_aInputCount[m_InputCurrent]));
		mem_zero(&m_aInputState[m_InputCurrent], sizeof(m_aInputState[m_InputCurrent]));
		m_InputDispatched = false;
	}

	{
		int i;
		const bool *pState = SDL_GetKeyboardState(&i);
		if(i >= KEY_LAST)
			i = KEY_LAST - 1;
		mem_copy(m_aInputState[m_InputCurrent], pState, i);
	}

	m_MouseLeft = false;
	m_MouseEntered = false;
	UpdateSteamInput();

	// these states must always be updated manually because they are not in the GetKeyState from SDL
	int i = SDL_GetMouseState(0, 0);
	if(i & SDL_BUTTON_MASK(1))
		m_aInputState[m_InputCurrent][KEY_MOUSE_1] = 1; // 1 is left
	if(i & SDL_BUTTON_MASK(3))
		m_aInputState[m_InputCurrent][KEY_MOUSE_2] = 1; // 3 is right
	if(i & SDL_BUTTON_MASK(2))
		m_aInputState[m_InputCurrent][KEY_MOUSE_3] = 1; // 2 is middle
	if(i & SDL_BUTTON_MASK(4))
		m_aInputState[m_InputCurrent][KEY_MOUSE_4] = 1;
	if(i & SDL_BUTTON_MASK(5))
		m_aInputState[m_InputCurrent][KEY_MOUSE_5] = 1;
	if(i & SDL_BUTTON_MASK(6))
		m_aInputState[m_InputCurrent][KEY_MOUSE_6] = 1;
	if(i & SDL_BUTTON_MASK(7))
		m_aInputState[m_InputCurrent][KEY_MOUSE_7] = 1;
	if(i & SDL_BUTTON_MASK(8))
		m_aInputState[m_InputCurrent][KEY_MOUSE_8] = 1;

	{
		SDL_Event Event;

		while(SDL_PollEvent(&Event))
		{
			int Key = -1;
			int Action = IInput::FLAG_PRESS;

			int LastGamepadMove = m_GamepadMove;
			bool LastGamepadDown = m_GamepadDown;
			bool LastGamepadJump = m_GamepadJump;
			bool LastGamepadShoot = m_GamepadShoot;
			bool LastGamepadSelect = m_GamepadSelect;

			switch(Event.type)
			{
				case SDL_EVENT_TEXT_EDITING:
				{
					m_CompositionLength = str_length(Event.edit.text);
					if(m_CompositionLength)
					{
						str_copy(m_aComposition, Event.edit.text, sizeof(m_aComposition));
						m_CompositionCursor = 0;
						for(int i = 0; i < Event.edit.start; i++)
							m_CompositionCursor = str_utf8_forward(m_aComposition, m_CompositionCursor);
						int CompositionEnd = m_CompositionCursor;
						for(int i = 0; i < Event.edit.length; i++)
							CompositionEnd = str_utf8_forward(m_aComposition, CompositionEnd);
						m_CompositionSelectedLength = CompositionEnd - m_CompositionCursor;
						AddEvent(0, 0, IInput::FLAG_TEXT);
					}
					else
					{
						m_aComposition[0] = '\0';
						m_CompositionLength = 0;
						m_CompositionCursor = 0;
						m_CompositionSelectedLength = 0;
					}
					break;
				}
				case SDL_EVENT_TEXT_EDITING_CANDIDATES:
				{
					m_CandidateCount = 0;
					m_CandidateSelectedIndex = Event.edit_candidates.selected_candidate;
					const int Count = min(Event.edit_candidates.num_candidates, (Sint32)MAX_CANDIDATES);
					for(int i = 0; i < Count; i++)
					{
						if(!Event.edit_candidates.candidates || !Event.edit_candidates.candidates[i])
							continue;
						str_copy(m_aaCandidates[m_CandidateCount],
								 Event.edit_candidates.candidates[i],
								 sizeof(m_aaCandidates[m_CandidateCount]));
						m_CandidateCount++;
					}
					break;
				}
				case SDL_EVENT_TEXT_INPUT:
					m_aComposition[0] = 0;
					m_CompositionLength = COMP_LENGTH_INACTIVE;
					m_CompositionCursor = 0;
					m_CompositionSelectedLength = 0;
					AddEvent(Event.text.text, 0, IInput::FLAG_TEXT);
					break;
				// handle keys
				case SDL_EVENT_KEY_DOWN:
					m_UsingGamepad = false;
					Key = SDL_GetScancodeFromName(SDL_GetKeyName(Event.key.key));
					if(Event.key.repeat)
						Action = IInput::FLAG_REPEAT;
					break;
				case SDL_EVENT_KEY_UP:
					m_UsingGamepad = false;
					Action = IInput::FLAG_RELEASE;
					Key = SDL_GetScancodeFromName(SDL_GetKeyName(Event.key.key));
					break;

				// handle gamepad
				case SDL_EVENT_GAMEPAD_ADDED:
					ResetGamepad();
					Gamepad()->ConnectGamepad();
					Gamepad()->Rumble(1.0f, 2000);
					break;

				case SDL_EVENT_GAMEPAD_REMOVED:
					ResetGamepad();
					Gamepad()->DisconnectGamepad(0);
					break;

				case SDL_EVENT_GAMEPAD_BUTTON_UP:
					Action = IInput::FLAG_RELEASE;

				// fall through
				case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
					m_UsingGamepad = true;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
						Key = KEY_GAMEPAD_SHOULDER_LEFT;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
						Key = KEY_GAMEPAD_SHOULDER_RIGHT;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
						Key = KEY_GAMEPAD_BUTTON_BACK;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK)
						Key = KEY_GAMEPAD_BUTTON_LEFTSTICK;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
						Key = KEY_GAMEPAD_BUTTON_RIGHTSTICK;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
						Key = KEY_GAMEPAD_BUTTON_A;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)
						Key = KEY_GAMEPAD_BUTTON_B;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_WEST)
						Key = KEY_GAMEPAD_BUTTON_X;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH)
						Key = KEY_GAMEPAD_BUTTON_Y;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
						Key = KEY_GAMEPAD_BUTTON_DPAD_UP;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
						Key = KEY_GAMEPAD_BUTTON_DPAD_DOWN;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
						Key = KEY_GAMEPAD_BUTTON_DPAD_LEFT;
					if(Event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
						Key = KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
					break;

				case SDL_EVENT_GAMEPAD_AXIS_MOTION:
				{
					const bool AimAxis = Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX || Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY;
					const bool TriggerAxis = Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER || Event.jaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
					const int ActivationThreshold = (TriggerAxis ? 12 : (AimAxis ? g_Config.m_ClGamepadAimDeadzone : g_Config.m_ClGamepadMoveDeadzone)) * 32767 / 100;
					if(abs(Event.jaxis.value) > ActivationThreshold)
						m_UsingGamepad = true;

					// attack
					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
					{
						m_GamepadShoot = ProcessAnalogButton(Event.jaxis.value / 32767.0f, m_GamepadShoot, 0.12f, 0.08f);
					}

					// select
					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
					{
						m_GamepadSelect = ProcessAnalogButton(Event.jaxis.value / 32767.0f, m_GamepadSelect, 0.12f, 0.08f);
					}

					// jump
					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
					{
						const float Value = Event.jaxis.value / 32767.0f;
						const float Press = g_Config.m_ClGamepadMoveDeadzone / 100.0f;
						const int Vertical = ProcessDigitalAxis(Value, m_GamepadDown ? 1 : (m_GamepadJump ? -1 : 0), Press, max(0.05f, Press - 0.10f));
						m_GamepadJump = Vertical < 0;
						m_GamepadDown = Vertical > 0;
					}

					// move
					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
					{
						const float Press = g_Config.m_ClGamepadMoveDeadzone / 100.0f;
						m_GamepadMove = ProcessDigitalAxis(Event.jaxis.value / 32767.0f, m_GamepadMove, Press, max(0.05f, Press - 0.10f));
					}

					// aim
					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX)
						m_GamepadAimX = Event.jaxis.value;

					if(Event.jaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY)
						m_GamepadAimY = Event.jaxis.value;
					break;
				}

				// handle mouse buttons
				case SDL_EVENT_MOUSE_BUTTON_UP:
					Action = IInput::FLAG_RELEASE;

					if(Event.button.button == 1) // ignore_convention
					{
						m_ReleaseDelta = time_get() - m_LastRelease;
						m_LastRelease = time_get();
					}

				// fall through
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					m_UsingGamepad = false;
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
					{
						char aBuf[224];
						str_format(aBuf,
								   sizeof(aBuf),
								   "raw-button button=%u edge=%s clicks=%u x=%.1f y=%.1f events=%d/%d",
								   (unsigned)Event.button.button,
								   Action == IInput::FLAG_RELEASE ? "release" : "press",
								   (unsigned)Event.button.clicks,
								   Event.button.x,
								   Event.button.y,
								   m_NumEvents,
								   INPUT_BUFFER_SIZE);
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
					}

					if(Event.button.button == SDL_BUTTON_LEFT)
						Key = KEY_MOUSE_1; // ignore_convention
					if(Event.button.button == SDL_BUTTON_RIGHT)
						Key = KEY_MOUSE_2; // ignore_convention
					if(Event.button.button == SDL_BUTTON_MIDDLE)
						Key = KEY_MOUSE_3; // ignore_convention
					if(Event.button.button == 6)
						Key = KEY_MOUSE_6; // ignore_convention
					if(Event.button.button == 7)
						Key = KEY_MOUSE_7; // ignore_convention
					if(Event.button.button == 8)
						Key = KEY_MOUSE_8; // ignore_convention
					break;

				case SDL_EVENT_MOUSE_WHEEL:
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
					{
						char aBuf[256];
						str_format(aBuf,
								   sizeof(aBuf),
								   "raw time_ns=%llu mouse=%u x=%.3f y=%.3f integer_x=%d integer_y=%d direction=%u accum=%.3f events=%d/%d",
								   (unsigned long long)Event.wheel.timestamp,
								   (unsigned)Event.wheel.which,
								   Event.wheel.x,
								   Event.wheel.y,
								   Event.wheel.integer_x,
								   Event.wheel.integer_y,
								   (unsigned)Event.wheel.direction,
								   m_WheelAccumY,
								   m_NumEvents,
								   INPUT_BUFFER_SIZE);
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
					}
					{
						float Wy = Event.wheel.y;
						int TickY = Event.wheel.integer_y;
						if(Event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
						{
							Wy = -Wy;
							TickY = -TickY;
						}
						// See DDNet #12381.
						int Steps = TickY;
						if(Steps == 0 && Wy != 0.0f)
						{
							if((m_WheelAccumY > 0.0f && Wy < 0.0f) || (m_WheelAccumY < 0.0f && Wy > 0.0f))
								m_WheelAccumY = 0.0f;
							m_WheelAccumY += Wy;
							Steps = (int)m_WheelAccumY;
							m_WheelAccumY -= (float)Steps;
						}
						else if(Steps != 0)
							m_WheelAccumY = 0.0f;

						if(Steps == 0)
						{
							if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", "raw-drop no-integer-step");
							break;
						}

						Key = Steps > 0 ? KEY_MOUSE_WHEEL_UP : KEY_MOUSE_WHEEL_DOWN; // ignore_convention
						Action = IInput::FLAG_PRESS | IInput::FLAG_RELEASE;
						if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
						{
							char aBuf[192];
							str_format(aBuf,
									   sizeof(aBuf),
									   "raw-map key=%d name=%s steps=%d accum=%.3f queued_before=%d",
									   Key,
									   KeyName(Key),
									   Steps,
									   m_WheelAccumY,
									   m_NumEvents);
							m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
						}
						for(int Extra = 1; Extra < absolute(Steps); Extra++)
						{
							if(Action & IInput::FLAG_PRESS)
								m_aInputCount[m_InputCurrent][Key].m_Presses++;
							if(Action & IInput::FLAG_RELEASE)
								m_aInputCount[m_InputCurrent][Key].m_Releases++;
							AddEvent(0, Key, Action);
						}
					}
					break;

				case SDL_EVENT_WINDOW_MOUSE_ENTER:
					m_MouseEntered = true;
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", "window mouse-enter");
					break;
				case SDL_EVENT_WINDOW_MOUSE_LEAVE:
					m_MouseLeft = true;
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", "window mouse-leave");
					break;
				case SDL_EVENT_WINDOW_FOCUS_GAINED:
					UpdateMouseGrab(true);
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", "window focus-gained");
					break;
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					UpdateMouseGrab(false);
					if(g_Config.m_ClDebugWeaponWheel && m_pConsole)
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", "window focus-lost");
					break;
				// other messages
				case SDL_EVENT_QUIT:
					return 1;
			}

			//
			if(Key != -1 && !HasComposition())
			{
				if(Action & IInput::FLAG_PRESS)
				{
					m_aInputCount[m_InputCurrent][Key].m_Presses++;
					m_aInputState[m_InputCurrent][Key] = 1;
				}
				if(Action & IInput::FLAG_RELEASE)
				{
					m_aInputCount[m_InputCurrent][Key].m_Releases++;
					m_aInputState[m_InputCurrent][Key] = 0;
				}
				AddEvent(0, Key, Action);
			}

			// send gamepad shoot
			if(LastGamepadShoot != m_GamepadShoot)
			{
				Action = m_GamepadShoot ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
				if(Action == IInput::FLAG_PRESS)
				{
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_TRIGGER_RIGHT].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_TRIGGER_RIGHT] = 1;
				}
				else
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_TRIGGER_RIGHT].m_Releases++;
				AddEvent(0, KEY_GAMEPAD_TRIGGER_RIGHT, Action);
			}

			// send gamepad select
			if(LastGamepadSelect != m_GamepadSelect)
			{
				Action = m_GamepadSelect ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
				if(Action == IInput::FLAG_PRESS)
				{
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_TRIGGER_LEFT].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_TRIGGER_LEFT] = 1;
				}
				else
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_TRIGGER_LEFT].m_Releases++;
				AddEvent(0, KEY_GAMEPAD_TRIGGER_LEFT, Action);
			}

			// send gamepad jump
			if(LastGamepadJump != m_GamepadJump)
			{
				Action = m_GamepadJump ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
				if(Action == IInput::FLAG_PRESS)
				{
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_UP].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_UP] = 1;
				}
				else
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_UP].m_Releases++;
				AddEvent(0, KEY_GAMEPAD_AXIS_UP, Action);
			}

			// send gamepad down
			if(LastGamepadDown != m_GamepadDown)
			{
				Action = m_GamepadDown ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
				if(Action == IInput::FLAG_PRESS)
				{
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_DOWN].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_DOWN] = 1;
				}
				else
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_DOWN].m_Releases++;
				AddEvent(0, KEY_GAMEPAD_AXIS_DOWN, Action);
			}

			// send gamepad axis x
			if(LastGamepadMove != m_GamepadMove)
			{
				if(m_GamepadMove == -1)
				{
					// press left
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT] = 1;
					AddEvent(0, KEY_GAMEPAD_AXIS_LEFT, IInput::FLAG_PRESS);

					// release right
					if(LastGamepadMove == 1)
					{
						m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT].m_Releases++;
						m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT] = 0;
						AddEvent(0, KEY_GAMEPAD_AXIS_RIGHT, IInput::FLAG_RELEASE);
					}
				}
				else if(m_GamepadMove == 1)
				{
					// press right
					m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT].m_Presses++;
					m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT] = 1;
					AddEvent(0, KEY_GAMEPAD_AXIS_RIGHT, IInput::FLAG_PRESS);

					// release left
					if(LastGamepadMove == -1)
					{
						m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT].m_Releases++;
						m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT] = 0;
						AddEvent(0, KEY_GAMEPAD_AXIS_LEFT, IInput::FLAG_RELEASE);
					}
				}
				else if(m_GamepadMove == 0)
				{
					// release left
					if(LastGamepadMove == -1)
					{
						m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT].m_Releases++;
						m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT] = 0;
						AddEvent(0, KEY_GAMEPAD_AXIS_LEFT, IInput::FLAG_RELEASE);
					}

					// release right
					if(LastGamepadMove == 1)
					{
						m_aInputCount[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT].m_Releases++;
						m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT] = 0;
						AddEvent(0, KEY_GAMEPAD_AXIS_RIGHT, IInput::FLAG_RELEASE);
					}
				}
			}
		}
	}

	if(m_CompositionLength == 0)
		m_CompositionLength = COMP_LENGTH_INACTIVE;

	// SDL does not expose gamepad buttons in its keyboard state array. Preserve
	// virtual held states across input frames so overlays and bind UIs can query
	// the actual physical state without waiting for another axis event.
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_LEFT] = m_GamepadMove < 0;
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_RIGHT] = m_GamepadMove > 0;
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_UP] = m_GamepadJump;
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_AXIS_DOWN] = m_GamepadDown;
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_TRIGGER_RIGHT] = m_GamepadShoot;
	m_aInputState[m_InputCurrent][KEY_GAMEPAD_TRIGGER_LEFT] = m_GamepadSelect;

	return 0;
}

IEngineInput *CreateEngineInput()
{
	return new CInput;
}
