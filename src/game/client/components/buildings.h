#ifndef GAME_CLIENT_COMPONENTS_BUILDINGS_H
#define GAME_CLIENT_COMPONENTS_BUILDINGS_H
#include <base/system.h>
#include <game/client/component.h>

class CBuildings : public CComponent
{
	bool m_NodesPass;

	void RenderTurret(const struct CNetObj_Turret *pCurrent, const struct CNetObj_Turret *pPrev);
	void RenderPowerupper(const struct CNetObj_Powerupper *pCurrent);
	void RenderSawblade(const struct CNetObj_Building *pCurrent);
	void RenderMine(const struct CNetObj_Building *pCurrent);
	void RenderElectromine(const struct CNetObj_Building *pCurrent);
	void
	RenderBarrel(const struct CNetObj_Building *pCurrent, const struct CNetObj_Building *pPrev, int SpriteVariation);
	void RenderPowerBarrel(const struct CNetObj_Building *pCurrent,
						   const struct CNetObj_Building *pPrev,
						   int SpriteVariation);
	void RenderLazer(const struct CNetObj_Building *pCurrent);
	void RenderBase(const struct CNetObj_Building *pCurrent);
	void RenderStand(const struct CNetObj_Building *pCurrent, const struct CNetObj_Building *pPrev);
	void RenderReactor(const struct CNetObj_Building *pCurrent);
	void RenderTeslacoil(const struct CNetObj_Building *pCurrent, const struct CNetObj_Building *pPrev);
	void RenderDestroyedReactor(const struct CNetObj_Building *pCurrent);
	void RenderFlametrap(const struct CNetObj_Building *pCurrent, const struct CNetObj_Building *pPrev);
	void RenderSwitch(const struct CNetObj_Building *pCurrent);
	void RenderDoor1(const struct CNetObj_Building *pCurrent);
	void RenderGenerator(const struct CNetObj_Building *pCurrent, const struct CNetObj_Building *pPrev);
	void RenderOverseerShieldNode(const struct CNetObj_Building *pCurrent);
	void RenderScreen(const struct CNetObj_Building *pCurrent);
	void RenderShop(const struct CNetObj_Shop *pCurrent);
	void RenderNodesPowerRange(vec2 Center, float Radius, vec4 Color);
	void RenderNodesNoPower(vec2 Center, float Top, int Tick);
	void RenderNodesShieldEffect(vec2 Center, float Radius, bool Powered, int Tick);
	void RenderNodesBuilding(const struct CNetObj_Building *pCurrent);

  public:
	explicit CBuildings(bool NodesPass = false) : m_NodesPass(NodesPass) {}
	virtual void OnReset();
	virtual void OnRender();
};

#endif
