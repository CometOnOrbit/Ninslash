#ifndef GAME_SERVER_ENTITIES_DROID_INDUSTRIAL_H
#define GAME_SERVER_ENTITIES_DROID_INDUSTRIAL_H
#include "droid.h"
#include <game/industrial_boss.h>
class CIndustrialBoss : public CDroid
{
	enum
	{
		MAX_SHOTS = 12
	};
	struct CShot
	{
		int m_ID, m_Kind, m_Age, m_Delay, m_Health;
		vec2 m_Pos, m_Vel;
		bool m_aHit[MAX_CHARACTERS];
	};
	CShot m_aShots[MAX_SHOTS];
	int m_Kind, m_Act, m_ActTick, m_Phase, m_Events, m_NextAttack, m_Sequence, m_JumpCooldown, m_PendingDir;
	int m_aParts[4], m_aPartMax[4];
	bool m_aHit[MAX_CHARACTERS], m_Released;
	vec2 m_Aim;
	void StartAct(int Act);
	bool Target();
	void Think();
	void MoveBody();
	void TickAttack();
	void TickShots();
	void ClearShots();
	void RemoveShot(int Index);
	void AddShot(vec2 Pos, vec2 Vel, int Kind, int Delay = 0);
	void MarkPattern();
	void HurtSegment(vec2 From, vec2 To, float Radius, int Damage, bool Once);
	void FinishDeath();
	vec2 Socket(int Which, int Tick = -1);
	bool Exposed();

  public:
#if defined(CONF_DEBUG)
	void DebugAct(int Act)
	{
		if(Act >= 0 && Act < NUM_IB_ACTS && Act != IB_DEATH && m_Health > 0)
		{
			Target();
			if(Act == IB_TURN)
				m_PendingDir = -m_Dir;
			StartAct(Act);
		}
	}
	void DebugState(char *pBuffer, int Size);
#endif
	CIndustrialBoss(CGameWorld *pWorld, vec2 Pos, int Kind);
	~CIndustrialBoss() override;
	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int Client) override;
	bool HitSegment(vec2 From, vec2 To, float Radius, vec2 *pAt) override;
	void TakeDamage(vec2 Force, int Damage, const CAttackSource &Source, vec2 Pos) override;
};
#endif

#if defined(CONF_DEBUG)
void RegisterIndustrialBossDebug(class CGameContext *pGame);
#endif
