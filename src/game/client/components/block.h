#ifndef GAME_CLIENT_COMPONENTS_BLOCK_H
#define GAME_CLIENT_COMPONENTS_BLOCK_H
#include <base/vmath.h>
#include <game/client/component.h>

#include <map>
#include <utility>

class CBlocks : public CComponent
{
	friend class CGameClient;

	int *m_pBlocks;
	int *m_pBlockSyncTick;
	int m_Width;
	int m_Height;
	struct CModularBlock
	{
		int m_Type;
		int m_SyncTick;
	};
	std::map<std::pair<int, int>, CModularBlock> m_ModularBlocks;

	void SetBlock(ivec2 Pos, int Block);
	void RenderModularBlocks();

  public:
	CBlocks();
	~CBlocks();

	void ResetBlocks();
	void RenderBlocks();

	virtual void OnInit();
	virtual void OnReset();
	virtual void OnRender();
	virtual void OnMapLoad();
};
#endif
