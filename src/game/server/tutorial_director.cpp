#include "tutorial_director.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

CTutorialDirector::CTutorialDirector(CGameContext *pGameServer) :
	m_pGameServer(pGameServer), m_InputMask(0), m_LastFire(0), m_LastWantedWeapon(0), m_CombatRespawnReady(false)
{
	m_Machine.Start(g_Config.m_SvTutorialChapter, g_Config.m_SvTutorialStep, g_Config.m_SvTutorialCompletedMask);
}

void CTutorialDirector::SendState(int ClientID) const
{
	const CTutorialState &State = m_Machine.State();
	CNetMsg_Sv_TutorialState Msg;
	Msg.m_Chapter = State.m_Chapter;
	Msg.m_Step = State.m_Step;
	Msg.m_Progress = State.m_Progress;
	Msg.m_Target = State.m_Target;
	Msg.m_Nonce = State.m_Nonce;
	Msg.m_CompletedMask = State.m_CompletedMask;
	Msg.m_Flags = State.m_Active ? 1 : 2;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CTutorialDirector::OnClientEnter(int ClientID)
{
	SendState(ClientID);
	if(m_Machine.State().m_Chapter == TUTORIAL_CHAPTER_BUILD && m_Machine.State().m_Step == 0 &&
		m_pGameServer->m_pPveDirector && !m_pGameServer->m_pPveDirector->InIntermission())
		m_pGameServer->m_pPveDirector->StartIntermission(false, true);
}

void CTutorialDirector::OnInput(int ClientID, const CNetObj_PlayerInput *pInput)
{
	if(!pInput || m_Machine.State().m_Chapter != TUTORIAL_CHAPTER_DEPLOYMENT)
		return;
	const int Step = m_Machine.State().m_Step;
	bool Progress = false;
	if(Step == 0)
	{
		if(pInput->m_Direction != 0)
			m_InputMask |= 1;
		if(pInput->m_Jump & 1)
			m_InputMask |= 2;
		Progress = m_InputMask == 3;
	}
	else if(Step == 1)
		Progress = pInput->m_Fire != m_LastFire;
	m_LastFire = pInput->m_Fire;
	m_LastWantedWeapon = pInput->m_WantedWeapon;
	if(Progress)
	{
		m_InputMask = 0;
		m_Machine.AddProgress();
		SendState();
	}
	(void)ClientID;
}

void CTutorialDirector::OnAction(int ClientID, int Action, int Nonce, int Value)
{
	(void)Value;
	if(m_Machine.OnAction(Action, Nonce))
		SendState();
	else
		SendState(ClientID);
}

void CTutorialDirector::OnGameplayProgress(int ClientID, int Event, int Amount)
{
	(void)ClientID;
	const CTutorialState &State = m_Machine.State();
	bool Expected = false;
	if(State.m_Chapter == TUTORIAL_CHAPTER_DEPLOYMENT && State.m_Step == 2)
	{
		const int EventBit = Event == TUTORIAL_EVENT_WEAPON_SWITCH ? 4 : Event == TUTORIAL_EVENT_TARGET_HIT ? 8 : 0;
		if(EventBit && !(m_InputMask & EventBit))
		{
			m_InputMask |= EventBit;
			m_Machine.AddProgress();
			SendState();
		}
		return;
	}
	else
		Expected = TutorialGameplayEventMatches(State.m_Chapter, State.m_Step, Event, m_CombatRespawnReady);
	if(!Expected)
		return;
	const int PreviousStep = State.m_Step;
	m_Machine.AddProgress(Amount);
	if(State.m_Chapter == TUTORIAL_CHAPTER_COMBAT && PreviousStep == 0 && m_Machine.State().m_Step == 1 &&
		ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->TakeDamage(CAttackSource::World(WEAPON_WORLD), 20, vec2(0, 0), pChr->m_Pos);
	}
	if(State.m_Chapter == TUTORIAL_CHAPTER_COMBAT && PreviousStep == 1 && m_Machine.State().m_Step == 2)
		m_CombatRespawnReady = false;
	SendState();
}

void CTutorialDirector::OnDeath(int ClientID)
{
	m_InputMask = 0;
	m_Machine.RetryCurrentStep();
	if(m_Machine.State().m_Chapter == TUTORIAL_CHAPTER_COMBAT && m_Machine.State().m_Step == 2)
		m_CombatRespawnReady = true;
	SendState(ClientID);
}
