#ifndef GAME_SERVER_TUTORIAL_DIRECTOR_H
#define GAME_SERVER_TUTORIAL_DIRECTOR_H

#include <game/tutorial.h>

class CGameContext;
struct CNetObj_PlayerInput;

class CTutorialDirector
{
	CGameContext *m_pGameServer;
	CTutorialStateMachine m_Machine;
	int m_InputMask;
	int m_LastFire;
	int m_LastWantedWeapon;
	bool m_CombatRespawnReady;
	void SendState(int ClientID = -1) const;

public:
	explicit CTutorialDirector(CGameContext *pGameServer);
	const CTutorialState &State() const { return m_Machine.State(); }
	void OnClientEnter(int ClientID);
	void OnInput(int ClientID, const CNetObj_PlayerInput *pInput);
	void OnAction(int ClientID, int Action, int Nonce, int Value);
	void OnGameplayProgress(int ClientID, int Event, int Amount = 1);
	void OnDeath(int ClientID);
};

#endif
