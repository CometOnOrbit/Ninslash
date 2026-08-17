

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/input_processing.h>

#include <engine/keys.h>

#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/components/inventory.h>
#include <game/client/components/build_placement.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>

#include <game/client/customstuff.h>

#include "controls.h"

CControls::CControls()
{
	mem_zero(&m_LastData, sizeof(m_LastData));
	m_Ready = false;
	m_LastGamepadAimTime = 0;
	m_WasGameplayCaptured = false;
	m_AimAssistTargetType = 0;
	m_AimAssistTargetID = -1;
	m_WeaponSelectionPulse.Reset();
	m_WheelDebugSequence = 0;
	m_WheelDebugLastSlot = -1;
}

void CControls::OnReset()
{
	m_LastData.m_Direction = 0;
	m_LastData.m_Hook = 0;
	m_LastData.m_Down = 0;
	m_LastData.m_Charge = 0;
	// simulate releasing the fire button
	if((m_LastData.m_Fire & 1) != 0)
		m_LastData.m_Fire++;
	m_LastData.m_Fire &= INPUT_STATE_MASK;
	m_LastData.m_Jump = 0;
	m_InputData = m_LastData;
	m_LastData.m_WantedWeapon = m_InputData.m_WantedWeapon = 0;

	m_InputDirectionLeft = 0;
	m_InputDirectionRight = 0;

	m_PickedWeapon = -1;
	m_SignalWeapon = -1;
	m_LastWeapon = 1;
	m_Ready = false;
	m_LastGamepadAimTime = 0;
	m_AimAssistTargetType = 0;
	m_AimAssistTargetID = -1;
	m_WeaponSelectionPulse.Reset();
}

bool CControls::FindAimAssistTarget(vec2 AimDirection, vec2 *pTargetDirection, float *pAngle)
{
	if(g_Config.m_ClGamepadAimAssist <= 0 || !m_pClient->m_Snap.m_pLocalCharacter || length(AimDirection) < 0.01f)
		return false;
	const vec2 Origin = m_pClient->m_LocalCharacterPos;
	const bool Coop = m_pClient->m_Snap.m_pGameInfoObj && (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP);
	const float AcquireAngle = 6.0f * pi / 180.0f;
	const float RetainAngle = 9.0f * pi / 180.0f;
	float BestAngle = 1000.0f;
	int BestType = 0, BestID = -1;
	vec2 BestDirection(0, 0);
	auto Consider = [&](int Type, int ID, vec2 Pos) {
		const vec2 Delta = Pos - Origin;
		const float Dist = length(Delta);
		if(Dist < 1.0f || Dist > 900.0f || Collision()->IntersectLine(Origin, Pos, 0, 0))
			return;
		const vec2 Direction = Delta / Dist;
		const float Angle = acosf(clamp(dot(AimDirection, Direction), -1.0f, 1.0f));
		const bool Retained = Type == m_AimAssistTargetType && ID == m_AimAssistTargetID;
		if(Angle > (Retained ? RetainAngle : AcquireAngle) || Angle >= BestAngle)
			return;
		BestAngle = Angle;
		BestType = Type;
		BestID = ID;
		BestDirection = Direction;
	};

	if(Coop)
	{
		for(int i = 0; i < Client()->SnapNumItems(IClient::SNAP_CURRENT); i++)
		{
			IClient::CSnapItem Item;
			const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
			if(Item.m_Type == NETOBJTYPE_DROID)
			{
				const CNetObj_Droid *pDroid = static_cast<const CNetObj_Droid *>(pData);
				Consider(1, Item.m_ID, vec2(pDroid->m_X, pDroid->m_Y));
			}
		}
	}
	else
	{
		const int LocalID = m_pClient->m_Snap.m_LocalClientID;
		const int LocalTeam = m_pClient->m_Snap.m_pLocalInfo ? m_pClient->m_Snap.m_pLocalInfo->m_Team : TEAM_SPECTATORS;
		for(int ClientID = 0; ClientID < MAX_CHARACTERS; ClientID++)
		{
			if(ClientID == LocalID || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !m_pClient->m_Snap.m_paPlayerInfos[ClientID])
				continue;
			const int Team = m_pClient->m_Snap.m_paPlayerInfos[ClientID]->m_Team;
			if(LocalTeam != TEAM_SPECTATORS && LocalTeam == Team && m_pClient->m_Snap.m_pGameInfoObj && (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS))
				continue;
			Consider(2, ClientID, m_pClient->m_Snap.m_aCharacters[ClientID].m_Position);
		}
	}
	if(BestID < 0)
	{
		m_AimAssistTargetType = 0;
		m_AimAssistTargetID = -1;
		return false;
	}
	m_AimAssistTargetType = BestType;
	m_AimAssistTargetID = BestID;
	*pTargetDirection = BestDirection;
	*pAngle = BestAngle;
	return true;
}

void CControls::RestoreHeldMovement()
{
	auto CommandHeld = [this](const char *pCommand) {
		int Held = 0;
		for(int Key = 1; Key < KEY_LAST; Key++)
			if(str_comp(m_pClient->m_pBinds->Get(Key), pCommand) == 0 && Input()->KeyPressed(Key))
				Held++;
		return Held;
	};
	m_InputDirectionLeft = CommandHeld("+left") + CommandHeld("+gamepadleft");
	m_InputDirectionRight = CommandHeld("+right") + CommandHeld("+gamepadright");
	m_InputData.m_Down = CommandHeld("+down") + CommandHeld("+gamepaddown");
}

void CControls::OnRelease()
{
	OnReset();
}

void CControls::OnPlayerDeath()
{
	m_LastData.m_WantedWeapon = m_InputData.m_WantedWeapon = 0;
	m_WeaponSelectionPulse.Reset();
}

void CControls::QueueWeaponSlot(int ProtocolSlot)
{
	if(ProtocolSlot > 0)
		m_WeaponSelectionPulse.Queue(ProtocolSlot);
}

void CControls::CancelQueuedWeaponSlot()
{
	m_WeaponSelectionPulse.CancelQueued();
	if(!m_WeaponSelectionPulse.NeedsRelease())
		m_InputData.m_WantedWeapon = 0;
}

void CControls::DebugWeaponWheelEvent(int *pCounter, bool Pressed, int Before, int After)
{
	if(!g_Config.m_ClDebugWeaponWheel)
		return;
	if(Pressed)
		m_WheelDebugSequence++;
	const char *pDirection = pCounter == &m_InputData.m_NextWeapon ? "next" : "prev";
	char aBuf[256];
	str_format(aBuf,
			   sizeof(aBuf),
			   "event seq=%d dir=%s edge=%s slot=%d counter=%d->%d last=%d next=%d prev=%d wanted=%d fire=%d",
			   m_WheelDebugSequence,
			   pDirection,
			   Pressed ? "press" : "release",
			   CustomStuff()->m_WeaponSlot,
			   Before,
			   After,
			   pCounter == &m_InputData.m_NextWeapon ? m_LastData.m_NextWeapon : m_LastData.m_PrevWeapon,
			   m_InputData.m_NextWeapon,
			   m_InputData.m_PrevWeapon,
			   m_InputData.m_WantedWeapon,
			   m_InputData.m_Fire);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
}

static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	int *pState = (int *)pUserData;
	*pState = clamp(*pState + (pResult->GetInteger(0) ? 1 : -1), 0, 16);
}

static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	int *v = (int *)pUserData;
	if(((*v) & 1) != pResult->GetInteger(0))
		(*v)++;
	*v &= INPUT_STATE_MASK;
}

static void ReleaseInputCounter(int *pValue)
{
	if((*pValue & 1) != 0)
		(*pValue)++;
	*pValue &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_pVariable;
	int m_Value;
};

static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	if(pResult->GetInteger(0))
		pSet->m_pControls->QueueWeaponSlot(pSet->m_Value);
}

static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	const int Before = *pSet->m_pVariable;
	if(pResult->GetInteger(0))
		*pSet->m_pVariable = (*pSet->m_pVariable + 2) & INPUT_STATE_MASK;
	pSet->m_pControls->DebugWeaponWheelEvent(
		pSet->m_pVariable, pResult->GetInteger(0) != 0, Before, *pSet->m_pVariable);
	pSet->m_pControls->CancelQueuedWeaponSlot();
}

void CControls::ConZoomPlus(IConsole::IResult *pResult, void *pUserData)
{
	CControls *pControls = (CControls *)pUserData;
	if(pControls->m_pClient->m_pChat->IsActive())
		return;
	if(pControls->m_pClient->m_pMenus->IsActive())
		return;
	if(pControls->Client()->State() != IClient::STATE_ONLINE &&
	   pControls->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!pControls->m_pClient->LocalZoomAllowed())
		return;
	if(g_Config.m_ClZoom < 30)
		g_Config.m_ClZoom++;
	if(g_Config.m_ClZoom < 1)
		g_Config.m_ClZoom = 1;
}

void CControls::ConZoomMinus(IConsole::IResult *pResult, void *pUserData)
{
	CControls *pControls = (CControls *)pUserData;
	if(pControls->m_pClient->m_pChat->IsActive())
		return;
	if(pControls->m_pClient->m_pMenus->IsActive())
		return;
	if(pControls->Client()->State() != IClient::STATE_ONLINE &&
	   pControls->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!pControls->m_pClient->LocalZoomAllowed())
		return;
	if(g_Config.m_ClZoom > 1)
		g_Config.m_ClZoom--;
	if(g_Config.m_ClZoom > 30)
		g_Config.m_ClZoom = 30;
}

void CControls::OnConsoleInit()
{
	// game commands
	Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionLeft, "Move left");
	Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionRight, "Move right");
	Console()->Register("+down", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Down, "Slide / down");
	Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Jump, "Jump");
	Console()->Register("+turbo", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Hook, "Turbo");
	Console()->Register("+charge", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Charge, "Charge");
	Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, &m_InputData.m_Fire, "Fire");

	// gamepad
	Console()->Register("+gamepadleft", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionLeft, "Move left");
	Console()->Register("+gamepadright", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputDirectionRight, "Move right");
	Console()->Register("+gamepaddown", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Down, "Slide");
	Console()->Register("+gamepadjump", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Jump, "Jump");
	Console()->Register("+gamepadturbo", "", CFGFLAG_CLIENT, ConKeyInputState, &m_InputData.m_Hook, "Turbo");
	Console()->Register("+gamepadfire", "", CFGFLAG_CLIENT, ConKeyInputCounter, &m_InputData.m_Fire, "Fire");

	{
		static CInputSet s_Set = {this, &m_InputData.m_NextWeapon, 0};
		Console()->Register("+gamepadnextweapon",
							"",
							CFGFLAG_CLIENT,
							ConKeyInputNextPrevWeapon,
							(void *)&s_Set,
							"Switch to next weapon");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_PrevWeapon, 0};
		Console()->Register("+gamepadprevweapon",
							"",
							CFGFLAG_CLIENT,
							ConKeyInputNextPrevWeapon,
							(void *)&s_Set,
							"Switch to previous weapon");
	}

	// can't pick tool except with build key
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 2};
		Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon2");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 3};
		Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon3");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 4};
		Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon4");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 5};
		Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon5");
	}
	/*
	{ static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 6}; Console()->Register("+weapon6", "",
	CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon6"); } { static CInputSet s_Set = {this,
	&m_InputData.m_WantedWeapon, 7}; Console()->Register("+weapon7", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set,
	"Switch to weapon7"); } { static CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 8};
	Console()->Register("+weapon8", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to weapon8"); } { static
	CInputSet s_Set = {this, &m_InputData.m_WantedWeapon, 9}; Console()->Register("+weapon9", "", CFGFLAG_CLIENT,
	ConKeyInputSet, (void *)&s_Set, "Switch to weapon9"); } { static CInputSet s_Set = {this,
	&m_InputData.m_WantedWeapon, 10}; Console()->Register("+weapon10", "", CFGFLAG_CLIENT, ConKeyInputSet, (void
	*)&s_Set, "Switch to weapon10"); }
	*/

	{
		static CInputSet s_Set = {this, &m_InputData.m_NextWeapon, 0};
		Console()->Register(
			"+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to next weapon");
	}
	{
		static CInputSet s_Set = {this, &m_InputData.m_PrevWeapon, 0};
		Console()->Register(
			"+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to previous weapon");
	}

	Console()->Register("zoom+", "", CFGFLAG_CLIENT, ConZoomPlus, this, "Zoom in");
	Console()->Register("zoom-", "", CFGFLAG_CLIENT, ConZoomMinus, this, "Zoom out");
}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
}

int CControls::SnapInput(int *pData)
{
	static int64 LastSendTime = 0;
	static int PrevWeapon = 0;
	bool Send = false;
	if(g_Config.m_ClDebugWeaponWheel && CustomStuff()->m_WeaponSlot != m_WheelDebugLastSlot)
	{
		char aBuf[192];
		str_format(aBuf,
				   sizeof(aBuf),
				   "slot seq=%d confirmed=%d previous=%d next=%d prev=%d wanted=%d",
				   m_WheelDebugSequence,
				   CustomStuff()->m_WeaponSlot,
				   m_WheelDebugLastSlot,
				   m_InputData.m_NextWeapon,
				   m_InputData.m_PrevWeapon,
				   m_InputData.m_WantedWeapon);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
		m_WheelDebugLastSlot = CustomStuff()->m_WeaponSlot;
	}

	// update player state
	if(m_pClient->m_pChat->IsActive())
		m_InputData.m_PlayerFlags = PLAYERFLAG_CHATTING;
	else if(m_pClient->GameplayInputFullyCaptured())
		m_InputData.m_PlayerFlags = PLAYERFLAG_IN_MENU;
	else
		m_InputData.m_PlayerFlags = PLAYERFLAG_PLAYING;

	if(m_pClient->m_pScoreboard->Active())
		m_InputData.m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(m_Ready)
		m_InputData.m_PlayerFlags |= PLAYERFLAG_READY;

	if(m_LastData.m_PlayerFlags != m_InputData.m_PlayerFlags)
		Send = true;

	m_LastData.m_PlayerFlags = m_InputData.m_PlayerFlags;

	// Focused overlays own the controls. Reset held movement/fire as well as
	// blocking new events so opening an overlay cannot leave an old action stuck.
	const bool GameplayCaptured = !(m_InputData.m_PlayerFlags & PLAYERFLAG_PLAYING);
	if(GameplayCaptured)
	{
		OnReset();

		mem_copy(pData, &m_InputData, sizeof(m_InputData));

		// send once a second just to be sure
		if(time_get() > LastSendTime + time_freq())
			Send = true;
	}
	else
	{
		if(m_WasGameplayCaptured)
			RestoreHeldMovement();

		// Direct weapon selection is an event, not a held state. Always put a
		// zero packet between requests so pressing the same number twice is
		// visible to the server as two distinct actions.
		const int WeaponPulse = m_WeaponSelectionPulse.Prepare();
		if(WeaponPulse >= 0)
			m_InputData.m_WantedWeapon = WeaponPulse;

		m_InputData.m_TargetX = (int)m_MousePos.x;
		m_InputData.m_TargetY = (int)m_MousePos.y;
		if(!m_InputData.m_TargetX && !m_InputData.m_TargetY)
		{
			m_InputData.m_TargetX = 1;
			m_MousePos.x = 1;
		}

		// Inventory and build placement keep locomotion live, while combat
		// controls stay released so UI clicks cannot fire, hook or switch weapons.
		if(m_pClient->m_pInventory->IsVisible() || m_pClient->m_pBuildPlacement->Active())
		{
			m_InputData.m_Hook = 0;
			m_InputData.m_Charge = 0;
			ReleaseInputCounter(&m_InputData.m_Fire);
			ReleaseInputCounter(&m_InputData.m_NextWeapon);
			ReleaseInputCounter(&m_InputData.m_PrevWeapon);
			m_PickedWeapon = -1;
		}

		// set direction
		m_InputData.m_Direction = 0;
		if(m_InputDirectionLeft && !m_InputDirectionRight)
			m_InputData.m_Direction = -1;
		if(!m_InputDirectionLeft && m_InputDirectionRight)
			m_InputData.m_Direction = 1;

		// stress testing
		if(g_Config.m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_InputData, sizeof(m_InputData));

			m_InputData.m_Direction = ((int)t / 2) & 1;
			m_InputData.m_Jump = ((int)t);
			m_InputData.m_Fire = ((int)(t * 10));
			m_InputData.m_Hook = ((int)(t * 2)) & 1;
			m_InputData.m_Down = ((int)(t * 3)) & 1;
			m_InputData.m_Charge = ((int)(t * 4)) & 1;
			m_InputData.m_WantedWeapon = ((int)t) % 4;
			m_InputData.m_TargetX = (int)(sinf(t * 3) * 100.0f);
			m_InputData.m_TargetY = (int)(cosf(t * 3) * 100.0f);
		}

		m_PickedWeapon = -1;

		if(m_InputData.m_WantedWeapon != PrevWeapon)
		{
			PrevWeapon = m_InputData.m_WantedWeapon;
		}

		// check if we need to send input
		if(m_InputData.m_Direction != m_LastData.m_Direction)
			Send = true;
		else if(m_InputData.m_Jump != m_LastData.m_Jump)
			Send = true;
		else if(m_InputData.m_Fire != m_LastData.m_Fire)
			Send = true;
		else if(m_InputData.m_Hook != m_LastData.m_Hook)
			Send = true;
		else if(m_InputData.m_Down != m_LastData.m_Down)
			Send = true;
		else if(m_InputData.m_Charge != m_LastData.m_Charge)
			Send = true;
		else if(m_InputData.m_WantedWeapon != m_LastData.m_WantedWeapon)
			Send = true;
		else if(m_InputData.m_NextWeapon != m_LastData.m_NextWeapon)
			Send = true;
		else if(m_InputData.m_PrevWeapon != m_LastData.m_PrevWeapon)
			Send = true;

		// send at at least 10hz
		if(time_get() > LastSendTime + time_freq() / 25)
			Send = true;
	}
	m_WasGameplayCaptured = GameplayCaptured;

	if(!Send)
	{
		m_LastData = m_InputData;
		return 0;
	}

	LastSendTime = time_get();
	mem_copy(pData, &m_InputData, sizeof(m_InputData));
	if(g_Config.m_ClDebugWeaponWheel &&
	   (m_InputData.m_NextWeapon != m_LastData.m_NextWeapon ||
		m_InputData.m_PrevWeapon != m_LastData.m_PrevWeapon))
	{
		char aBuf[224];
		str_format(aBuf,
				   sizeof(aBuf),
				   "packet seq=%d slot=%d next=%d(prev=%d) prev=%d(prev=%d) wanted=%d fire=%d",
				   m_WheelDebugSequence,
				   CustomStuff()->m_WeaponSlot,
				   m_InputData.m_NextWeapon,
				   m_LastData.m_NextWeapon,
				   m_InputData.m_PrevWeapon,
				   m_LastData.m_PrevWeapon,
				   m_InputData.m_WantedWeapon,
				   m_InputData.m_Fire);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
	}
	m_LastData = m_InputData;
	if(m_InputData.m_WantedWeapon > 0)
		m_InputData.m_WantedWeapon = 0;
	m_WeaponSelectionPulse.OnSent(m_LastData.m_WantedWeapon);
	return sizeof(m_InputData);
}

void CControls::OnRender()
{
	// update target pos
	if(m_pClient->m_Snap.m_pGameInfoObj && !m_pClient->m_Snap.m_SpecInfo.m_Active)
		m_TargetPos = m_pClient->m_LocalCharacterPos + m_MousePos;
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
		m_TargetPos = m_pClient->m_Snap.m_SpecInfo.m_Position + m_MousePos;
	else
		m_TargetPos = m_MousePos;
}

bool CControls::OnMouseMove(float x, float y)
{
	if((m_pClient->m_Snap.m_pGameInfoObj &&
		m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED) ||
	   (m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_pChat->IsActive()))
		return false;

	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	Input()->ShowCursor(false);

	if(Input()->UsingGamepad())
	{
		float AimX = 0.0f, AimY = 0.0f;
		Input()->GetGamepadAim(&AimX, &AimY);
		const int64 Now = time_get();
		const float DeltaSeconds = m_LastGamepadAimTime ? (Now - m_LastGamepadAimTime) / (float)time_freq() : 1.0f / 60.0f;
		m_LastGamepadAimTime = Now;
		const float Speed = m_pClient->m_Snap.m_SpecInfo.m_Active ? 140.0f : 1400.0f;
		vec2 Delta = IntegrateAimStick(vec2(AimX, AimY), Speed, g_Config.m_ClGamepadAimSensitivity / 100.0f, DeltaSeconds);
		vec2 TargetDirection;
		float TargetAngle = 0.0f;
		const vec2 AimDirection = length(m_MousePos) > 0.01f ? normalize(m_MousePos) : vec2(1, 0);
		if(FindAimAssistTarget(AimDirection, &TargetDirection, &TargetAngle))
		{
			const float Strength = g_Config.m_ClGamepadAimAssist / 100.0f;
			const float Blend = 1.0f - clamp(TargetAngle / (6.0f * pi / 180.0f), 0.0f, 1.0f);
			Delta *= 1.0f - Strength * Blend;
			const bool Coop = m_pClient->m_Snap.m_pGameInfoObj && (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP);
			if(Coop && length(vec2(AimX, AimY)) >= 0.20f && length(m_MousePos) > 0.01f)
			{
				const float MaxTurn = (12.0f * pi / 180.0f) * (g_Config.m_ClGamepadAimAssist / 35.0f) * clamp(DeltaSeconds, 0.0f, 0.05f);
				const float SignedAngle = atan2f(AimDirection.x * TargetDirection.y - AimDirection.y * TargetDirection.x, dot(AimDirection, TargetDirection));
				const float Turn = clamp(SignedAngle, -MaxTurn, MaxTurn);
				const float C = cosf(Turn), S = sinf(Turn);
				const vec2 Turned(AimDirection.x * C - AimDirection.y * S, AimDirection.x * S + AimDirection.y * C);
				m_MousePos = Turned * length(m_MousePos);
			}
		}
		m_MousePos += Delta;
	}
	else
	{
		Input()->GetRelativePosition(&x, &y);
		m_LastGamepadAimTime = 0;
		// teeworlds: raw relative delta * (inp_mousesens / 100)
		m_MousePos += vec2(x, y) * (g_Config.m_InpMousesens / 100.0f);
	}
	ClampMousePos();

	return true;
}

void CControls::ClampMousePos()
{
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		// TODO
		if(Collision()->IsMapModular())
		{
			m_MousePos.x = clamp(m_MousePos.x, -10000000.0f, 10000000.0f);
			m_MousePos.y = clamp(m_MousePos.y, -10000000.0f, 10000000.0f);
		}
		else
		{
			m_MousePos.x = clamp(m_MousePos.x, 200.0f, Collision()->GetWidth() * 32 - 200.0f);
			m_MousePos.y = clamp(m_MousePos.y, 200.0f, Collision()->GetHeight() * 32 - 200.0f);
		}
	}
	else
	{
		float CameraMaxDistance = 200.0f;
		float FollowFactor = g_Config.m_ClMouseFollowfactor / 100.0f;
		float MouseMax =
			min(CameraMaxDistance / FollowFactor + g_Config.m_ClMouseDeadzone, (float)g_Config.m_ClMouseMaxDistance);

		if(length(m_MousePos) > MouseMax)
			m_MousePos = normalize(m_MousePos) * MouseMax;
	}
}
