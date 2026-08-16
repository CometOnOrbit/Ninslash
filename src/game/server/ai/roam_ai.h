#ifndef GAME_SERVER_AI_ROAM_AI_H
#define GAME_SERVER_AI_ROAM_AI_H
#include <game/server/ai.h>
#include <game/server/gamecontext.h>

class CAIroam : public CAI
{
  public:
	CAIroam(CGameContext *pGameServer, CCharacter *pCharacter, int Level);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);
	void ReceiveDamage(int CID, int Dmg);

  private:
	bool FindAccelerationHookTarget(class CCharacter *pCharacter, vec2 Travel, vec2 *pTargetPos) const;
	void ReleaseAccelerationHook(int Now, int CooldownTicks);
	void UpdateAccelerationHook(class CCharacter *pCharacter, vec2 Travel, bool Suppress);

	int m_ShockTimer;
	int m_Level;
	int m_TargetCourseOrdinal;
	int m_LastProgressTick;
	int m_LastMistakeTick;
	int m_HesitateUntil;
	int m_RecoveryDirection;
	vec2 m_LastProgressPos;
	bool m_AccelerationHookActive;
	int m_AccelerationHookStartTick;
	int m_AccelerationHookCooldownTick;
	vec2 m_AccelerationHookTarget;
};

#endif
