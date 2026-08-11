#ifndef GAME_SERVER_ENTITIES_DROID_CYCLONECRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_CYCLONECRAWLER_H

#include "droid_crawler.h"

const int CycloneCrawlerPhysSize = 48;

class CCycloneCrawler : public CCrawler
{
  public:
	CCycloneCrawler(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

  private:
	enum
	{
		CYCLONE_READY,
		CYCLONE_ASCENDING,
		CYCLONE_HOVERING,
		CYCLONE_DESCENDING,
	};

	bool FindTarget() override;
	bool UpdateTarget();
	bool IsGrounded();
	void FireBarrageRocket();
	void CreateLandingImpact();

	int m_CycloneState;
	int m_CycloneCooldownTick;
	int m_HoverStartTick;
	int m_RocketsFired;
	float m_AngleTimer;
	bool m_WasAirborne;
};

#endif
