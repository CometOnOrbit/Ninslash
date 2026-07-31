#include <engine/shared/mapchunk.h>
#include <engine/shared/mappath.h>
#include "layers.h"
#include <game/gamecore.h> // MapGen

CLayers::CLayers()
{
	m_GroupsNum = 0;
	m_GroupsStart = 0;
	m_LayersNum = 0;
	m_LayersStart = 0;
	m_ModularInfoNum = 0;
	m_ModularInfoStart = 0;
	m_RulesNum = 0;
	m_RulesStart = 0;
	m_PathInfoNum = 0;
	m_PathInfoStart = 0;
	m_PathPlacementNum = 0;
	m_PathPlacementStart = 0;
	m_pGameGroup = 0;
	m_pGameLayer = 0;
	m_pMap = 0;

	m_apChunkRule = 0x0;
	m_pMapChunk = 0x0;
	m_pMapPath = 0x0;

	m_pTiles = 0;
	// m_pGenerator = new CGenerator();
	m_pBackgrounLayer = 0x0;
	m_pDoodadsLayer = 0x0;
	m_pForegroundLayer = 0x0;
	m_pBase1Layer = 0x0;
	m_pBase2Layer = 0x0;
	m_GameGroupIndex = -1;
	m_GameLayerIndex = -1;
	m_BackgrounLayerIndex = -1;
	m_ForegroundLayerIndex = -1;
	m_Base1LayerIndex = -1;
	m_Base2LayerIndex = -1;
}

void CLayers::ClearModular()
{
	CMapChunk::DestroyChain(m_pMapChunk);
	m_pMapChunk = 0;

	delete[] m_apChunkRule;
	m_apChunkRule = 0;

	delete m_pMapPath;
	m_pMapPath = 0;
}

CLayers::~CLayers()
{
	ClearModular();
}

void CLayers::PruneMapChunks(int LowTileX, int HighTileX)
{
	if(m_pMapChunk)
		m_pMapChunk = m_pMapChunk->FreeOutside(LowTileX, HighTileX);
}

void CLayers::Init(class IKernel *pKernel)
{
	m_pMap = pKernel->RequestInterface<IMap>();
	m_pMap->GetType(MAPITEMTYPE_GROUP, &m_GroupsStart, &m_GroupsNum);
	m_pMap->GetType(MAPITEMTYPE_LAYER, &m_LayersStart, &m_LayersNum);
	m_pMap->GetType(MAPITEMTYPE_MODULARINFO, &m_ModularInfoStart, &m_ModularInfoNum);
	m_pMap->GetType(MAPITEMTYPE_RULE, &m_RulesStart, &m_RulesNum);
	m_pMap->GetType(MAPITEMTYPE_PATHINFO, &m_PathInfoStart, &m_PathInfoNum);
	m_pMap->GetType(MAPITEMTYPE_PATHPLACEMENT, &m_PathPlacementStart, &m_PathPlacementNum);

	ClearModular();

	CMapPathInfo *pPathInfo = GetPathInfo(0);
	if(pPathInfo && pPathInfo->m_Version >= 1 && pPathInfo->m_PlacementCount > 0 &&
	   pPathInfo->m_PlacementCount <= m_PathPlacementNum)
	{
		CMapPathInfoData Info;
		Info.m_Version = pPathInfo->m_Version;
		Info.m_ChunkWidth = pPathInfo->m_ChunkWidth;
		Info.m_ChunkHeight = pPathInfo->m_ChunkHeight;
		Info.m_AtlasColumns = pPathInfo->m_AtlasColumns;
		Info.m_TemplateCount = pPathInfo->m_TemplateCount;
		Info.m_PlacementCount = pPathInfo->m_PlacementCount;

		CMapPathPlacementData aPlacements[MAPPATH_MAX_PLACEMENTS];
		for(int i = 0; i < pPathInfo->m_PlacementCount; i++)
		{
			CMapPathPlacement *pPlacement = GetPathPlacement(i);
			if(!pPlacement)
				continue;
			aPlacements[i].m_GridX = pPlacement->m_GridX;
			aPlacements[i].m_GridY = pPlacement->m_GridY;
			aPlacements[i].m_TemplateIndex = pPlacement->m_TemplateIndex;
			aPlacements[i].m_CourseIndex = pPlacement->m_CourseIndex;
			aPlacements[i].m_EntryDir = pPlacement->m_EntryDir;
			aPlacements[i].m_ExitDir = pPlacement->m_ExitDir;
		}

		m_pMapPath = new CMapPath();
		if(!m_pMapPath->Init(&Info, aPlacements, pPathInfo->m_PlacementCount))
		{
			delete m_pMapPath;
			m_pMapPath = 0;
		}
	}

	CMapModularInfo *pModularInfo = GetModularInfo(0);

	if(!m_pMapPath && pModularInfo && pModularInfo->m_IsModular && pModularInfo->m_RuleCount > 0)
	{
		m_apChunkRule = new int[pModularInfo->m_RuleCount * 4];
		for(int i = 0; i < pModularInfo->m_RuleCount * 4; i++)
			m_apChunkRule[i] = 0;

		for(int i = 0; i < pModularInfo->m_RuleCount; i++)
		{
			CMapRule *pRule = GetRule(i);

			if(pRule)
			{
				m_apChunkRule[i * 4 + 0] = pRule->m_Rule1;
				m_apChunkRule[i * 4 + 1] = pRule->m_Rule2;
				m_apChunkRule[i * 4 + 2] = pRule->m_Rule3;
				m_apChunkRule[i * 4 + 3] = pRule->m_Rule4;
			}
		}

		m_pMapChunk = new CMapChunk(0, pModularInfo->m_ChunkSize, pModularInfo->m_RuleCount, m_apChunkRule, 0);
	}

	// BackgroundLayer & ForegroundLayer can exists or not... need reset to null
	m_pBackgrounLayer = 0x0;
	m_BackgrounLayerIndex = -1;
	m_pDoodadsLayer = 0x0;
	m_DoodadsLayerIndex = -1;
	m_pForegroundLayer = 0x0;
	m_ForegroundLayerIndex = -1;
	m_pBase1Layer = 0x0;
	m_pBase2Layer = 0x0;
	m_Base1LayerIndex = -1;
	m_Base2LayerIndex = -1;

	for(int g = 0; g < NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GetGroup(g);
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer + l);

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);

				// MapGen: Layers access info (Doesn't need group info because is supposed are in Game Layer Group)
				char layerName[64] = {0};
				IntsToStr(pTilemap->m_aName, sizeof(pTilemap->m_aName) / sizeof(int), layerName);
				if(!m_pBackgrounLayer && str_comp_nocase(layerName, "background") == 0)
				{
					m_pBackgrounLayer = pTilemap;
					m_BackgrounLayerIndex = l;
				}
				else if(!m_pDoodadsLayer && str_comp_nocase(layerName, "doodads") == 0)
				{
					m_pDoodadsLayer = pTilemap;
					m_DoodadsLayerIndex = l;
				}
				else if(!m_pBase1Layer && str_comp_nocase(layerName, "base1") == 0)
				{
					m_pBase1Layer = pTilemap;
					m_Base1LayerIndex = l;
				}
				else if(!m_pBase2Layer && str_comp_nocase(layerName, "base2") == 0)
				{
					m_pBase2Layer = pTilemap;
					m_Base2LayerIndex = l;
					break; // Finish here... in not generated maps increase the CPU usage :/
				}
				else if(!m_pForegroundLayer && str_comp_nocase(layerName, "foreground") == 0)
				{
					m_pForegroundLayer = pTilemap;
					m_ForegroundLayerIndex = l;
				}

				if(pTilemap->m_Flags & TILESLAYERFLAG_GAME)
				{
					m_pGameLayer = pTilemap;
					m_pGameGroup = pGroup;

					// make sure the game group has standard settings
					m_pGameGroup->m_OffsetX = 0;
					m_pGameGroup->m_OffsetY = 0;
					m_pGameGroup->m_ParallaxX = 100;
					m_pGameGroup->m_ParallaxY = 100;

					if(m_pGameGroup->m_Version >= 2)
					{
						m_pGameGroup->m_UseClipping = 0;
						m_pGameGroup->m_ClipX = 0;
						m_pGameGroup->m_ClipY = 0;
						m_pGameGroup->m_ClipW = 0;
						m_pGameGroup->m_ClipH = 0;
					}

					// MapGen: Game layer access info
					m_GameGroupIndex = g;
					m_GameLayerIndex = l;
				}
			}
		}
	}
}

void CLayers::GenerateLayers()
{
	for(int g = 0; g < NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GetGroup(g);
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer + l);

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);

				if(!(pTilemap->m_Flags & TILESLAYERFLAG_GAME))
				{
					// do something
					m_pTiles = static_cast<CTile *>(Map()->GetData(pTilemap->m_Data));
				}
			}
		}
	}
}

CMapItemGroup *CLayers::GetGroup(int Index) const
{
	return static_cast<CMapItemGroup *>(m_pMap->GetItem(m_GroupsStart + Index, 0, 0));
}

CMapItemLayer *CLayers::GetLayer(int Index) const
{
	return static_cast<CMapItemLayer *>(m_pMap->GetItem(m_LayersStart + Index, 0, 0));
}

CMapModularInfo *CLayers::GetModularInfo(int Index) const
{
	if(!m_ModularInfoNum)
		return 0;

	return static_cast<CMapModularInfo *>(m_pMap->GetItem(m_ModularInfoStart + Index, 0, 0));
}

CMapRule *CLayers::GetRule(int Index) const
{
	return static_cast<CMapRule *>(m_pMap->GetItem(m_RulesStart + Index, 0, 0));
}

CMapPathInfo *CLayers::GetPathInfo(int Index) const
{
	if(!m_PathInfoNum)
		return 0;
	return static_cast<CMapPathInfo *>(m_pMap->GetItem(m_PathInfoStart + Index, 0, 0));
}

CMapPathPlacement *CLayers::GetPathPlacement(int Index) const
{
	if(!m_PathPlacementNum)
		return 0;
	return static_cast<CMapPathPlacement *>(m_pMap->GetItem(m_PathPlacementStart + Index, 0, 0));
}
