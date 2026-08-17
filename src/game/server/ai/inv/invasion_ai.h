#ifndef GAME_SERVER_AI_INVASION_AI_H
#define GAME_SERVER_AI_INVASION_AI_H

#include <game/server/ai.h>

#include "invasion_profile.h"

class CCharacter;

class CInvasionAI : public CAI
{
  protected:
	EInvasionSkinId m_ProfileId;
	const CInvasionSkinProfile *m_pProfile;
	int m_Level;
	vec2 m_StartPos;
	int m_ShockTimer;
	int m_NextRepositionTick;
	int m_StrafeSide;

	const CInvasionSkinProfile &Profile() const { return *m_pProfile; }
	int Level() const { return m_Level; }

	bool SelectProfileTarget();
	bool ShootAtProfileTarget();
	void SetProfileTargetPosition(bool HasTarget, bool Shooting);
	void MoveWithProfileTarget();
	virtual void ApplyFamilyTactics(bool HasTarget, bool Shooting);

	void RunProfileBehavior();

  public:
	CInvasionAI(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId);

	void OnCharacterSpawn(CCharacter *pChr) override;
	void ReceiveDamage(int CID, int Dmg) override;

	EInvasionSkinId ProfileId() const { return m_ProfileId; }
};

#endif
