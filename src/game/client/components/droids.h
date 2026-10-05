#ifndef GAME_CLIENT_COMPONENTS_DROIDS_H
#define GAME_CLIENT_COMPONENTS_DROIDS_H
#include <base/system.h>
#include <game/client/component.h>

class CDroids : public CComponent
{
	void RenderWalker(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderStar(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderCrawler(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderWarden(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderAngler(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderMatriarch(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderMatriarchShot(const CNetObj_BossShot *pShot, vec2 Pos, int ItemID);
	void RenderBossV5(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID);
	void RenderBossV5Shot(const CNetObj_BossShot *pShot, vec2 Pos, int ItemID);
	void BeginBossV5Frame();
	void EndBossV5Frame();
	vec2 MixPos(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent);

  public:
	virtual void OnReset();
	virtual void OnRender();
};

#endif
