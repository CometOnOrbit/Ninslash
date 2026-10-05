#ifndef GAME_SERVER_ENTITIES_DROID_STORMSERAPH_H
#define GAME_SERVER_ENTITIES_DROID_STORMSERAPH_H

#include <game/storm_seraph.h>
#include "droid_bossv5.h"

class CStormSeraph : public CBossV5
{
  public:
	CStormSeraph(CGameWorld *pGameWorld, vec2 Pos);

  protected:
	const CBossV5Act &ActInfo(int Act) override { return SeraphAct(Act); }
	void Think() override;
	void TickAct(int Elapsed) override;
	void MoveBody() override;
	void TickShot(int i) override;
	int IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg) override;
	void OnActStart(int Act) override;
	void OnActFinished(int Act) override;
	void TickTimers() override;
	bool Exposed() override;
	int StatusFlags() override;
	void SnapShot(const CShot &s, CNetObj_BossShot *pShot) override;
	bool ShotHittable(const CShot &s) override;
	void OnShotDestroyed(int i) override;
	void OnDeathStart() override;
	int PhaseAct() override { return SERAPH_ACT_ROAR; }
	int StaggerAct() override { return SERAPH_ACT_STAGGER; }
	void ExtraDebug(char *pBuffer, int Size) override;

  private:
	bool Shielded();
	int AliveNodes();
	vec2 NodeHome(int Node);
	vec2 HoverPoint();
	void SpawnNodes();
	void FireOrb(int Shot);
	void Impact();
	void SetNodeState(int State);
	int NearestNode(vec2 To);

	int m_aNodeShot[SERAPH_NODES];
	vec2 m_aNodeTarget[SERAPH_NODES];
	int m_NodeState;
	int m_Side;
	int m_SideTimer;
	int m_DiveCooldown;
	int m_LatticeCooldown;
	int m_OrbCooldown;
	int m_PlungeCooldown;
	int m_ZapCooldown;
	int m_DiveIndex;
	int m_DiveStart;
	bool m_Dashing;
	vec2 m_DashDir;
	int m_PlungeStage; // 0 rise, 1 hold, 2 fall
	int m_StageTick;
	vec2 m_FlyTo;
	bool m_FlyFast;
	bool m_Grounded;
	int m_BlindTicks;
};

#endif
