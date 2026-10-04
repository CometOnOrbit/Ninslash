#ifndef GAME_SERVER_ENTITIES_DROID_ABYSSANGLER_H
#define GAME_SERVER_ENTITIES_DROID_ABYSSANGLER_H

#include <game/abyss_angler.h>
#include "droid.h"

class CAbyssAngler : public CDroid
{
  public:
	CAbyssAngler(CGameWorld *pGameWorld, vec2 Pos);
	virtual ~CAbyssAngler();

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	bool HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt) override;

  private:
	void StartAct(int Act);
	void Think();
	void TickAct();
	void DiveImpact();
	void FinishAct();
	void TickDeath();
	void MoveBody();
	bool AcquireTarget(bool NeedSight);
	bool PartAlive(int Part) const { return Part == ANGLER_PART_CORE || m_aPartHealth[Part] > 0; }
	void BreakPart(int Part);
	vec2 Mouth() const { return m_Pos + vec2(ANGLER_MOUTH.x * m_Dir, ANGLER_MOUTH.y); }
	vec2 TargetPos();
	bool Dark();
	bool GillsOpen();
	bool Suction();
	void HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, bool Front, float Slow = 0.0f);
	void FireBubble(vec2 Dir);
	void TickBubbles();
	void PopBubble(int i, bool Shot);
	void ClearBubbles();

	int m_Act;
	int m_ActTick;
	int m_Events;
	int m_Phase;
	int m_Stagger;
	int m_ActsDone;
	int m_LureCooldown;
	int m_SpitCooldown;
	int m_DiveCooldown;
	int m_aPartHealth[NUM_ANGLER_PARTS];
	int m_aPartMax[NUM_ANGLER_PARTS];
	vec2 m_DiveAim;

	int m_aBubbleID[ANGLER_MAX_BUBBLES];
	vec2 m_aBubblePos[ANGLER_MAX_BUBBLES];
	vec2 m_aBubbleVel[ANGLER_MAX_BUBBLES];
	int m_aBubbleLife[ANGLER_MAX_BUBBLES];
	int m_aBubbleTarget[ANGLER_MAX_BUBBLES];
};

#endif
