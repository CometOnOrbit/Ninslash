#include <stdio.h> // sscanf
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include "mapgen.h"
#include <game/server/mapgen/gen_layer.h>
#include <game/server/mapgen/room.h>
#include <game/server/mapgen/maze.h>
#include <game/server/roam_mapgen_layout.h>
#include <game/server/gamecontext.h>
#include <game/layers.h>
#include <game/mapitems.h>
#include <game/pve/questinfo.h>
#include <game/pve/tutorial.h>
#include <game/pve/tutorial_map.h>

static ivec2 FindStandableFallback(CGenLayer *pTiles, bool PreferBottom);

CMapGen::CMapGen()
{
	m_pLayers = 0x0;
	m_pCollision = 0x0;
	m_pStorage = 0x0;
	m_FileLoaded = false;
	m_HasModularInfo = false;
	mem_zero(&m_ModularInfo, sizeof(m_ModularInfo));
	mem_zero(m_aModularRules, sizeof(m_aModularRules));
	m_HasPathInfo = false;
	mem_zero(&m_PathInfo, sizeof(m_PathInfo));
	mem_zero(m_aPathPlacements, sizeof(m_aPathPlacements));
}
CMapGen::~CMapGen()
{
}

void CMapGen::Init(CLayers *pLayers, CCollision *pCollision, IStorage *pStorage)
{
	m_pLayers = pLayers;
	m_pCollision = pCollision;
	m_pStorage = pStorage;

	Load("metal_main");
}

void CMapGen::Load(const char *pTileName)
{
	char aPath[256];
	str_format(aPath, sizeof(aPath), "editor/%s.rules", pTileName);
	IOHANDLE RulesFile = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!RulesFile)
		return;

	CLineReader LineReader;
	LineReader.Init(RulesFile);

	CConfiguration *pCurrentConf = 0;
	CIndexRule *pCurrentIndex = 0;

	// read each line
	while(char *pLine = LineReader.Get())
	{
		// skip blank/empty lines as well as comments
		if(str_length(pLine) > 0 && pLine[0] != '#' && pLine[0] != '\n' && pLine[0] != '\r' && pLine[0] != '\t' &&
		   pLine[0] != '\v' && pLine[0] != ' ')
		{
			if(pLine[0] == '[')
			{
				// new configuration, get the name
				pLine++;

				CConfiguration NewConf;
				int ID = m_lConfigs.add(NewConf);
				pCurrentConf = &m_lConfigs[ID];

				str_copy(pCurrentConf->m_aName, pLine, sizeof(pCurrentConf->m_aName));
				const int NameLength = str_length(pCurrentConf->m_aName);
				if(NameLength > 0 && pCurrentConf->m_aName[NameLength - 1] == ']')
					pCurrentConf->m_aName[NameLength - 1] = 0;
			}
			else
			{
				if(!str_comp_num(pLine, "Index", 5))
				{
					// new index
					int ID = 0;
					char aFlip[128] = "";

					sscanf(pLine, "Index %d %127s", &ID, aFlip);

					CIndexRule NewIndexRule;
					NewIndexRule.m_ID = ID;
					NewIndexRule.m_Flag = 0;
					NewIndexRule.m_RandomValue = 0;
					NewIndexRule.m_YDivisor = 0;
					NewIndexRule.m_YRemainder = 0;
					NewIndexRule.m_BaseTile = false;

					if(str_length(aFlip) > 0)
					{
						if(!str_comp(aFlip, "XFLIP"))
							NewIndexRule.m_Flag = TILEFLAG_VFLIP;
						else if(!str_comp(aFlip, "YFLIP"))
							NewIndexRule.m_Flag = TILEFLAG_HFLIP;
						else if(!str_comp(aFlip, "XYFLIP"))
							NewIndexRule.m_Flag = TILEFLAG_VFLIP + TILEFLAG_HFLIP;
						else if(!str_comp(aFlip, "ROTATE"))
							NewIndexRule.m_Flag = TILEFLAG_ROTATE;
						else if(!str_comp(aFlip, "XFLIP_ROTATE"))
							NewIndexRule.m_Flag = TILEFLAG_ROTATE + TILEFLAG_VFLIP;
						else if(!str_comp(aFlip, "YFLIP_ROTATE"))
							NewIndexRule.m_Flag = TILEFLAG_ROTATE + TILEFLAG_HFLIP;
						else if(!str_comp(aFlip, "XYFLIP_ROTATE"))
							NewIndexRule.m_Flag = TILEFLAG_ROTATE + TILEFLAG_VFLIP + TILEFLAG_HFLIP;
					}

					// add the index rule object and make it current
					int ArrayID = pCurrentConf->m_aIndexRules.add(NewIndexRule);
					pCurrentIndex = &pCurrentConf->m_aIndexRules[ArrayID];
				}
				else if(!str_comp_num(pLine, "BaseTile", 8) && pCurrentIndex)
				{
					pCurrentIndex->m_BaseTile = true;
				}
				else if(!str_comp_num(pLine, "Pos", 3) && pCurrentIndex)
				{
					int x = 0, y = 0;
					char aValue[128];
					int Value = CPosRule::EMPTY;
					bool IndexValue = false;

					sscanf(pLine, "Pos %d %d %127s", &x, &y, aValue);

					if(!str_comp(aValue, "FULL"))
						Value = CPosRule::FULL;
					else if(!str_comp_num(aValue, "INDEX", 5))
					{
						sscanf(pLine, "Pos %*d %*d INDEX %d", &Value);
						IndexValue = true;
					}

					CPosRule NewPosRule = {x, y, Value, IndexValue, 0};
					pCurrentIndex->m_aRules.add(NewPosRule);
				}
				else if(!str_comp_num(pLine, "Random", 6) && pCurrentIndex)
				{
					sscanf(pLine, "Random %d", &pCurrentIndex->m_RandomValue);
				}
				else if(!str_comp_num(pLine, "YRemainder", 10) && pCurrentIndex)
				{
					sscanf(pLine, "YRemainder %d %d", &pCurrentIndex->m_YDivisor, &pCurrentIndex->m_YRemainder);
				}
			}
		}
	}

	io_close(RulesFile);

	m_FileLoaded = true;
}

const char *CMapGen::GetConfigName(int Index)
{
	if(Index < 0 || Index >= m_lConfigs.size())
		return "";

	return m_lConfigs[Index].m_aName;
}

void CMapGen::ExpandEscapeTowerCanvas()
{
	// Invasion escape levels need a tall canvas; templates like generate_city1 are ~400x80.
	if(str_comp(g_Config.m_SvGametype, "coop") != 0)
		return;
	if(InvasionThemeFromLevel(g_Config.m_SvMapGenLevel) != INVASION_THEME_ACID_ESCAPE)
		return;
	if(!m_pLayers || !m_pLayers->GameLayer() || !m_pLayers->Map())
		return;

	CMapItemLayerTilemap *pGame = m_pLayers->GameLayer();
	const int TargetW = 120;
	const int TargetH = 320;

	if(pGame->m_Height >= TargetH && pGame->m_Width >= 100 && pGame->m_Width <= 160)
		return;

	const int NewW = TargetW;
	const int NewH = max(TargetH, pGame->m_Height);
	dbg_msg("mapgen", "expanding escape tower canvas %dx%d -> %dx%d", pGame->m_Width, pGame->m_Height, NewW, NewH);

	IMap *pMap = m_pLayers->Map();
	int LayerStart = 0;
	int LayerNum = 0;
	pMap->GetType(MAPITEMTYPE_LAYER, &LayerStart, &LayerNum);

	for(int i = 0; i < LayerNum; i++)
	{
		CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(pMap->GetItem(LayerStart + i, 0, 0));
		if(!pLayer || pLayer->m_Type != LAYERTYPE_TILES)
			continue;

		CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
		pTilemap->m_Width = NewW;
		pTilemap->m_Height = NewH;
		if(!pMap->ReplaceData(pTilemap->m_Data, NewW * NewH * (int)sizeof(CTile)))
			dbg_msg("mapgen", "failed to resize tile layer data index=%d", pTilemap->m_Data);
	}

	m_pCollision->RefreshMapgenDimensions();
}

void CMapGen::ExpandExtractMazeCanvas()
{
	if(str_comp(g_Config.m_SvGametype, "extract") != 0)
		return;
	if(!m_pLayers || !m_pLayers->GameLayer() || !m_pLayers->Map())
		return;

	CMapItemLayerTilemap *pGame = m_pLayers->GameLayer();
	const int TargetW = 200;
	const int TargetH = 140;
	if(pGame->m_Width >= TargetW && pGame->m_Height >= TargetH)
		return;

	const int NewW = max(TargetW, pGame->m_Width);
	const int NewH = max(TargetH, pGame->m_Height);
	dbg_msg("mapgen", "expanding extract maze canvas %dx%d -> %dx%d", pGame->m_Width, pGame->m_Height, NewW, NewH);

	IMap *pMap = m_pLayers->Map();
	int LayerStart = 0;
	int LayerNum = 0;
	pMap->GetType(MAPITEMTYPE_LAYER, &LayerStart, &LayerNum);

	for(int i = 0; i < LayerNum; i++)
	{
		CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(pMap->GetItem(LayerStart + i, 0, 0));
		if(!pLayer || pLayer->m_Type != LAYERTYPE_TILES)
			continue;

		CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
		pTilemap->m_Width = NewW;
		pTilemap->m_Height = NewH;
		if(!pMap->ReplaceData(pTilemap->m_Data, NewW * NewH * (int)sizeof(CTile)))
			dbg_msg("mapgen", "failed to resize tile layer data index=%d", pTilemap->m_Data);
	}

	m_pCollision->RefreshMapgenDimensions();
}

void CMapGen::FitTutorialCanvas()
{
	if(!IsTutorialGametype(g_Config.m_SvGametype) || !m_pLayers || !m_pLayers->GameLayer() || !m_pLayers->Map())
		return;
	CMapItemLayerTilemap *pGame = m_pLayers->GameLayer();
	const int NewW = TUTORIAL_MAP_W;
	const int NewH = TUTORIAL_MAP_H;
	if(pGame->m_Width == NewW && pGame->m_Height == NewH)
		return;
	dbg_msg("mapgen", "fitting tutorial canvas %dx%d -> %dx%d", pGame->m_Width, pGame->m_Height, NewW, NewH);
	IMap *pMap = m_pLayers->Map();
	int LayerStart = 0;
	int LayerNum = 0;
	pMap->GetType(MAPITEMTYPE_LAYER, &LayerStart, &LayerNum);
	for(int i = 0; i < LayerNum; i++)
	{
		CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(pMap->GetItem(LayerStart + i, 0, 0));
		if(!pLayer || pLayer->m_Type != LAYERTYPE_TILES)
			continue;
		CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
		pTilemap->m_Width = NewW;
		pTilemap->m_Height = NewH;
		if(!pMap->ReplaceData(pTilemap->m_Data, NewW * NewH * (int)sizeof(CTile)))
			dbg_msg("mapgen", "failed to resize tutorial tile layer data index=%d", pTilemap->m_Data);
	}
	m_pCollision->RefreshMapgenDimensions();
}

void CMapGen::FillMap()
{
	dbg_msg("mapgen", "started map generation");
	m_HasModularInfo = false;
	m_HasPathInfo = false;

	if(g_Config.m_SvMapGenRandSeed)
	{
		g_Config.m_SvMapGenSeed = (int)((unsigned long long)time_get() % 0x7FFFFFFFull);
		if(g_Config.m_SvMapGenSeed <= 0)
			g_Config.m_SvMapGenSeed = 1;
		g_Config.m_SvMapGenRandSeed = 0;
	}
	seed_random(DeterministicSeed((unsigned long long)(unsigned)g_Config.m_SvMapGenSeed, "mapgen") +
				(unsigned long long)(unsigned)g_Config.m_SvMapGenLevel);

	FitTutorialCanvas();
	FitRoamAtlasCanvas();
	ExpandEscapeTowerCanvas();
	ExpandExtractMazeCanvas();

	int64 ProcessTime = 0;
	int64 TotalTime = time_get();

	// clear map, but keep background, envelopes etc
	ProcessTime = time_get();
	const int Group = m_pLayers->GetGameGroupIndex();
	const bool BatchCleared = m_pCollision->ClearTileLayer(Group, m_pLayers->GetGameLayerIndex()) &&
							  m_pCollision->ClearTileLayer(Group, m_pLayers->GetBackgroundLayerIndex()) &&
							  m_pCollision->ClearTileLayer(Group, m_pLayers->GetDoodadsLayerIndex()) &&
							  m_pCollision->ClearTileLayer(Group, m_pLayers->GetForegroundLayerIndex());
	if(!BatchCleared)
	{
		const int Width = m_pLayers->GameLayer()->m_Width;
		const int LayerSize = Width * m_pLayers->GameLayer()->m_Height;
		for(int i = 0; i < LayerSize; i++)
		{
			const int x = i % Width;
			const ivec2 TilePos(x, (i - x) / Width);

			// clear the different layers
			ModifTile(TilePos, m_pLayers->GetGameLayerIndex(), TILE_AIR);
			ModifTile(TilePos, m_pLayers->GetBackgroundLayerIndex(), TILE_AIR);
			ModifTile(TilePos, m_pLayers->GetDoodadsLayerIndex(), TILE_AIR);
			ModifTile(TilePos, m_pLayers->GetForegroundLayerIndex(), TILE_AIR);
		}
	}
	dbg_msg("mapgen", "map normalized in %.5fs", (float)(time_get() - ProcessTime) / time_freq());

	ProcessTime = time_get();

	if(str_comp(g_Config.m_SvGametype, "roam") == 0)
		GenerateRoamLevel();
	else if(IsTutorialGametype(g_Config.m_SvGametype))
		GenerateTutorialLevel();
	else if(IsCoopMapGenGametype(g_Config.m_SvGametype))
		GenerateLevel();
	else
		GeneratePVPLevel();

	dbg_msg("mapgen", "map successfully generated in %.5fs", (float)(time_get() - TotalTime) / time_freq());
}

void CMapGen::GenerateEnd(CGenLayer *pTiles)
{
	int w = pTiles->Width();
	int h = pTiles->Height();

	// find a platform
	if(str_comp(g_Config.m_SvGametype, "coop") == 0 &&
	   InvasionThemeFromLevel(g_Config.m_SvMapGenLevel) == INVASION_THEME_ACID_ESCAPE)
	{
		for(int y = 3; y < h - 3; y++)
			for(int x = w - 3; x > 3; x--)
			{
				if(!pTiles->Get(x - 2, y) && !pTiles->Get(x - 1, y) && !pTiles->Get(x, y) && !pTiles->Get(x + 1, y) &&
				   !pTiles->Get(x + 2, y) && !pTiles->Get(x + 3, y) && pTiles->Get(x - 3, y + 1) &&
				   pTiles->Get(x - 2, y + 1) && pTiles->Get(x - 1, y + 1) && pTiles->Get(x, y + 1) &&
				   pTiles->Get(x + 1, y + 1) && pTiles->Get(x + 2, y + 1) && pTiles->Get(x + 3, y + 1) &&
				   !pTiles->Get(x, y - 2) && !pTiles->Get(x, y - 3) && !pTiles->Get(x, y - 4) && !pTiles->Get(x, y - 5))
				{
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DOOR1);

					pTiles->m_EndPos = ivec2(x, y);

					pTiles->Set(-1, x - 2, y);
					pTiles->Set(-1, x - 1, y);
					pTiles->Set(-1, x, y);
					pTiles->Set(-1, x + 1, y);
					pTiles->Set(-1, x + 2, y);

					// clear
					for(int xx = -2; xx < 3; xx++)
						for(int yy = -4; yy < 0; yy++)
							pTiles->Set(-1, x + xx, y + yy);

					// background
					for(int xx = -5; xx < 6; xx++)
						for(int yy = -7; yy < 500; yy++)
							pTiles->Set(1, x + xx, y + yy, 0, CGenLayer::BACKGROUND);

					return;
				}
			}
	}
	else
	{
		for(int x = w - 3; x > 3; x--)
			for(int y = 3; y < h - 3; y++)
			{
				if(!pTiles->Get(x - 2, y) && !pTiles->Get(x - 1, y) && !pTiles->Get(x, y) && !pTiles->Get(x + 1, y) &&
				   !pTiles->Get(x + 2, y) && !pTiles->Get(x + 3, y) && pTiles->Get(x - 3, y + 1) &&
				   pTiles->Get(x - 2, y + 1) && pTiles->Get(x - 1, y + 1) && pTiles->Get(x, y + 1) &&
				   pTiles->Get(x + 1, y + 1) && pTiles->Get(x + 2, y + 1) && pTiles->Get(x + 3, y + 1) &&
				   !pTiles->Get(x, y - 2) && !pTiles->Get(x, y - 3) && !pTiles->Get(x, y - 4) && !pTiles->Get(x, y - 5))
				{
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DOOR1);

					pTiles->m_EndPos = ivec2(x, y);

					pTiles->Set(-1, x - 2, y);
					pTiles->Set(-1, x - 1, y);
					pTiles->Set(-1, x, y);
					pTiles->Set(-1, x + 1, y);
					pTiles->Set(-1, x + 2, y);

					// clear
					for(int xx = -2; xx < 3; xx++)
						for(int yy = -4; yy < 0; yy++)
							pTiles->Set(-1, x + xx, y + yy);

					// background
					for(int xx = -5; xx < 6; xx++)
						for(int yy = -7; yy < 500; yy++)
							pTiles->Set(1, x + xx, y + yy, 0, CGenLayer::BACKGROUND);

					return;
				}
			}
	}
}

void CMapGen::GenerateSawblade(CGenLayer *pTiles)
{
	ivec2 p = ivec2(0, 0);

	if(frandom() < 0.4f)
		p = pTiles->GetSharpCorner();
	else if(frandom() < 0.4f)
	{
		p = pTiles->GetCeiling();
		p.y -= 1;
	}
	else if(frandom() < 0.4f)
	{
		p = pTiles->GetWall();

		if(p.x == 0)
			return;

		if(pTiles->Get(p.x - 1, p.y))
			p.x -= 1;
		else
			p.x += 1;
	}
	else
	{
		p = pTiles->GetPlatform();
		p.y += 1;
	}

	if(p.x == 0)
		return;

	for(int x = -2; x < 2; x++)
		for(int y = -2; y < 2; y++)
			pTiles->Use(p.x + x, p.y + y);

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SAWBLADE);
}

void CMapGen::GenerateWeapon(CGenLayer *pTiles, int Weapon)
{
	ivec2 p = ivec2(0, 0);

	p = pTiles->GetTopCorner();

	if(p.x != 0)
	{
		if(pTiles->Get(p.x - 1, p.y))
			p.x += 1;
		else
			p.x -= 1;

		p.y += 1;

		pTiles->Use(p.x, p.y);
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + Weapon);
		return;
	}

	p = pTiles->GetCeiling();
	if(p.x != 0)
	{
		p.y += 1;
		pTiles->Use(p.x, p.y);
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + Weapon);
		return;
	}

	p = pTiles->GetPlatform();
	if(p.x == 0)
		p = pTiles->GetMedPlatform();
	if(p.x == 0)
		p = FindStandableFallback(pTiles, false);
	if(p.x == 0)
	{
		dbg_msg("mapgen", "GenerateWeapon(%d) failed: no standable tile", Weapon);
		return;
	}

	pTiles->Use(p.x, p.y);
	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + Weapon);
}

void CMapGen::GenerateBarrel(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetPlatform();

	bool Dublos = false;

	if(p.x == 0)
	{
		p = pTiles->GetMedPlatform();
		Dublos = true;
	}

	if(p.x == 0)
		return;

	if(Dublos)
	{
		if(frandom() < 0.3f)
			ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_POWERBARREL);
		else
			ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_BARREL);

		ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_POWERBARREL);
	}
	else
	{
		if(frandom() < 0.3f)
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_POWERBARREL);
		else
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_BARREL);
	}

	if(IsCoopMapGenGametype(g_Config.m_SvGametype))
	{
		if(frandom() < 0.3f && g_Config.m_SvMapGenLevel > 5)
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_POWERBARREL);
		else
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_BARREL);
	}
	else
	{
	}
}

void CMapGen::GenerateLightningWall(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetPlatform();

	if(p.x == 0)
		p = pTiles->GetMedPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_LIGHTNINGWALL);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateConveyorBelt(CGenLayer *pTiles)
{
	ivec3 p = pTiles->GetLongPlatform();

	if(p.x == 0)
		return;

	int i = TILE_MOVELEFT;

	if(frandom() < 0.5f)
		i = TILE_MOVERIGHT;

	for(int x = p.x; x <= p.z; x++)
		ModifTile(ivec2(x, p.y), m_pLayers->GetGameLayerIndex(), i);
}

void CMapGen::GenerateHangables(CGenLayer *pTiles)
{
	ivec3 p = pTiles->GetLongCeiling();

	if(p.x == 0)
		return;

	p.y++;

	for(int x = p.x; x <= p.z; x++)
	{
		ModifTile(ivec2(x, p.y), m_pLayers->GetGameLayerIndex(), TILE_HANG);
		if(frandom() < 0.11f)
			ModifTile(ivec2(x, p.y), m_pLayers->GetForegroundLayerIndex(), 91, 0);
		else
			ModifTile(ivec2(x, p.y), m_pLayers->GetForegroundLayerIndex(), 90, 0);
	}

	if(pTiles->Get(p.x - 1, p.y))
		ModifTile(ivec2(p.x, p.y), m_pLayers->GetForegroundLayerIndex(), 89, 0);
	else
	{
		ModifTile(ivec2(p.x, p.y), m_pLayers->GetForegroundLayerIndex(), 92, TILEFLAG_VFLIP);
		ModifTile(ivec2(p.x + 1, p.y), m_pLayers->GetForegroundLayerIndex(), 91, 0);
	}

	if(pTiles->Get(p.z + 1, p.y))
		ModifTile(ivec2(p.z, p.y), m_pLayers->GetForegroundLayerIndex(), 89, TILEFLAG_VFLIP);
	else
	{
		ModifTile(ivec2(p.z, p.y), m_pLayers->GetForegroundLayerIndex(), 92, 0);
		ModifTile(ivec2(p.z - 1, p.y), m_pLayers->GetForegroundLayerIndex(), 91, 0);
	}
}

void CMapGen::GenerateMine(CGenLayer *pTiles)
{
	// Mines were removed from the entity set; place firetraps instead.
	GenerateFiretrap(pTiles);
}

void CMapGen::GenerateWalker(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetMedPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DROID_WALKER);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateStarDroid(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetOpenArea();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DROID_STAR);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateBossCrawlerDroid(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetOpenArea();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DROID_BOSSCRAWLER);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateCrawlerDroid(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetMedPlatform();

	if(p.x == 0)
	{
		p = pTiles->GetPlatform();

		if(p.x == 0)
			return;
	}

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_DROID_CRAWLER);
	pTiles->Use(p.x, p.y);
}

static ivec2 FindStandableFallback(CGenLayer *pTiles, bool PreferBottom)
{
	ivec2 p = ivec2(0, 0);
	const int w = pTiles->Width();
	const int h = pTiles->Height();
	const int yStart = PreferBottom ? h - 4 : 4;
	const int yEnd = PreferBottom ? 3 : h - 4;
	const int yStep = PreferBottom ? -1 : 1;

	for(int y = yStart; (PreferBottom ? y > yEnd : y < yEnd) && p.x == 0; y += yStep)
		for(int x = 3; x < w - 3; x++)
		{
			if(!pTiles->Get(x, y) && !pTiles->Used(x, y) && !pTiles->InPit(x, y) && pTiles->Get(x, y + 1) &&
			   pTiles->Get(x - 1, y + 1) && pTiles->Get(x + 1, y + 1) && !pTiles->Get(x, y - 1) &&
			   !pTiles->Get(x, y - 2))
			{
				p = ivec2(x, y);
				break;
			}
		}
	return p;
}

static bool SwitchSpotOk(CGenLayer *pTiles, ivec2 p)
{
	return p.x != 0 && !pTiles->InPit(p.x, p.y) && !pTiles->InPit(p.x, p.y + 1);
}

bool CMapGen::GenerateSwitch(CGenLayer *pTiles)
{
	ivec2 p = ivec2(0, 0);
	const int Theme = InvasionThemeFromLevel(g_Config.m_SvMapGenLevel);

	for(int Tries = 0; Tries < 8 && !SwitchSpotOk(pTiles, p); Tries++)
	{
		if(Theme == INVASION_THEME_ACID_ESCAPE)
			p = pTiles->GetBotPlatform();
		else
			p = pTiles->GetPlatform();

		if(!SwitchSpotOk(pTiles, p))
			p = pTiles->GetPlatform();
		if(!SwitchSpotOk(pTiles, p))
			p = pTiles->GetLeftPlatform();
		if(!SwitchSpotOk(pTiles, p))
			p = pTiles->GetMedPlatform();
		if(!SwitchSpotOk(pTiles, p))
			p = pTiles->GetBotPlatform();
		if(!SwitchSpotOk(pTiles, p))
			p = FindStandableFallback(pTiles, Theme == INVASION_THEME_ACID_ESCAPE);
	}

	if(!SwitchSpotOk(pTiles, p))
	{
		dbg_msg("mapgen", "GenerateSwitch failed: no platform found");
		return false;
	}

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SWITCH);
	pTiles->Use(p.x, p.y);
	dbg_msg("mapgen", "switch placed at %d,%d", p.x, p.y);
	return true;
}

bool CMapGen::GenerateReactor(CGenLayer *pTiles)
{
	// Reactor-defend wants the objective on the far right so players can
	// approach from one side instead of holding a centered crossfire.
	ivec2 p = pTiles->GetRightPlatform();
	if(p.x == 0)
		p = pTiles->GetMedPlatform();
	if(p.x == 0)
		p = pTiles->GetPlatform();
	if(p.x == 0)
		p = pTiles->GetBotPlatform();
	if(p.x == 0)
		p = FindStandableFallback(pTiles, false);

	if(p.x == 0)
	{
		dbg_msg("mapgen", "GenerateReactor failed: no platform found");
		return false;
	}

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_REACTOR);
	pTiles->Use(p.x, p.y);
	dbg_msg("mapgen", "reactor placed at %d,%d", p.x, p.y);
	return true;
}

void CMapGen::GenerateTurretStand(CGenLayer *pTiles)
{

	if(frandom() < 0.4f)
	{
		ivec2 p = ivec2(0, 0);

		if(frandom() < 0.6f)
			p = pTiles->GetLeftCeiling();
		else
			p = pTiles->GetCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_STAND);
			pTiles->Use(p.x, p.y);
			return;
		}
	}

	ivec2 p = pTiles->GetLeftPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_STAND);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateTurret(CGenLayer *pTiles)
{

	if(frandom() < 0.4f)
	{
		ivec2 p = pTiles->GetRightCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_TURRET);
			pTiles->Use(p.x, p.y);
			return;
		}
	}

	ivec2 p = pTiles->GetRightPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_TURRET);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateTeslacoil(CGenLayer *pTiles)
{

	if(frandom() < 0.4f)
	{
		ivec2 p = pTiles->GetRightCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_TESLACOIL);
			pTiles->Use(p.x, p.y);
			return;
		}
	}

	ivec2 p = pTiles->GetRightPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_TESLACOIL);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GeneratePowerupper(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetPlatform();

	if(p.x == 0)
		p = pTiles->GetMedPlatform();

	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_POWERUPPER);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateEnemySpawn(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetPlatform();

	if(p.x == 0)
		p = pTiles->GetMedPlatform();

	if(p.x == 0)
		p = pTiles->GetOpenArea();

	if(p.x == 0)
		p = pTiles->GetCeiling();

	if(p.x == 0)
		return;

	ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ENEMYSPAWN);
	ModifTile(p + ivec2(+1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ENEMYSPAWN);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateExtractZone(CGenLayer *pTiles)
{
	// Task anchor for Extraction objectives (elite spawn / defend zone /
	// supply point / timed-clear point). Open areas are preferred so runtime
	// effects have room; Get* consume used points, which provides spacing.
	ivec2 p = pTiles->GetOpenArea();
	if(p.x == 0)
		p = pTiles->GetPlatform();
	if(p.x == 0)
		p = pTiles->GetMedPlatform();
	if(p.x == 0)
		return;

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_EXTRACT_ZONE);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateBossEnemySpawn(CGenLayer *pTiles)
{
	// Reserve one generic enemy marker in a dedicated 9x9-tile room. The
	// runtime boss clearance includes a movement lane, so a normal open-area
	// marker is not wide enough.
	ivec2 p = pTiles->GetBossArea();
	if(p.x == 0)
	{
		dbg_msg("mapgen", "boss room: no 9x9 open area available");
		return;
	}

	ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ENEMYSPAWN);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateFiretrap(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetWall();

	if(p.x == 0)
		return;

	if(pTiles->Get(p.x - 1, p.y))
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_FLAMETRAP_RIGHT);
	else
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_FLAMETRAP_LEFT);

	for(int x = -1; x < 1; x++)
		for(int y = -1; y < 1; y++)
			pTiles->Use(p.x + x, p.y + y);
}

void CMapGen::GenerateDeathray(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetCeiling();

	if(p.x == 0)
		return;

	bool Valid = false;

	for(int y = 1; y < 22; y++)
	{
		if(pTiles->Get(p.x, p.y + y))
			Valid = true;
	}

	if(Valid)
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_LAZER);
	else
		ModifTile(p + ivec2(0, -1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SAWBLADE);

	for(int x = -1; x < 1; x++)
		for(int y = -1; y < 1; y++)
			pTiles->Use(p.x + x, p.y + y);
}

void CMapGen::GenerateScreen(CGenLayer *pTiles)
{
	/*
	ivec3 p = pTiles->GetLongPlatform();

	if (p.x == 0)
		return;
	int x = (p.x+p.z)/2;

	for (int y = 1; y < 6; y++)
		if (pTiles->Get(x, p.y-y))
			return;


	if (frandom() < 0.7f)
		ModifTile(ivec2(x, p.y-1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET+ENTITY_SCREEN);
	else
		ModifTile(ivec2(x, p.y-1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET+ENTITY_REACTOR);

	pTiles->Use(x, p.y-1);
	*/

	ivec2 p = pTiles->GetMedPlatform();

	if(p.x == 0)
		return;

	if(frandom() < 0.7f)
		ModifTile(ivec2(p.x, p.y), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SCREEN);
	else
		ModifTile(ivec2(p.x, p.y), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_REACTOR);

	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateShop(CGenLayer *pTiles)
{
	/*
	ivec3 p = pTiles->GetLongPlatform();

	if (p.x == 0)
		return;
	int x = (p.x+p.z)/2;

	for (int y = 1; y < 6; y++)
		if (pTiles->Get(x, p.y-y))
			return;

	ModifTile(ivec2(x, p.y-1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET+ENTITY_SHOP);

	pTiles->Use(x, p.y-1);
	*/

	ivec2 p = pTiles->GetMedPlatform();

	if(p.x == 0)
		return;

	ModifTile(ivec2(p.x, p.y), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SHOP);
	pTiles->Use(p.x, p.y);
}

void CMapGen::GenerateHearts(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetTopCorner();

	if(p.x != 0)
	{
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
		ModifTile(p + ivec2(0, 1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
		ModifTile(p + ivec2(0, 2), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
	}
	else
	{
		p = pTiles->GetCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
			ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
			ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
		}
		else
		{
			p = pTiles->GetWall();

			if(p.x != 0)
			{
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
				ModifTile(p + ivec2(0, -1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
			}
			else
			{
				p = pTiles->GetPlatform();

				if(p.x == 0)
					p = pTiles->GetMedPlatform();

				if(p.x == 0)
					return;

				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
				ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
				ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_HEALTH_1);
			}
		}
	}
}

void CMapGen::GenerateAmmo(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetTopCorner();

	if(p.x != 0)
	{
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
		ModifTile(p + ivec2(0, 1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
		ModifTile(p + ivec2(0, 2), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
	}
	else
	{
		p = pTiles->GetCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
			ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
			ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
		}
		else
		{
			p = pTiles->GetWall();

			if(p.x != 0)
			{
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
				ModifTile(p + ivec2(0, -1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
			}
			else
			{
				p = pTiles->GetPlatform();

				if(p.x == 0)
					p = pTiles->GetMedPlatform();

				if(p.x == 0)
					return;

				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
				ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
				ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_AMMO_1);
			}
		}
	}
}

void CMapGen::GenerateArmor(CGenLayer *pTiles)
{
	ivec2 p = pTiles->GetTopCorner();

	if(p.x != 0)
	{
		ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
		ModifTile(p + ivec2(0, 1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
		ModifTile(p + ivec2(0, 2), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
	}
	else
	{
		p = pTiles->GetCeiling();

		if(p.x != 0)
		{
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
			ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
			ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
		}
		else
		{
			p = pTiles->GetWall();

			if(p.x != 0)
			{
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
				ModifTile(p + ivec2(0, -1), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
			}
			else
			{
				p = pTiles->GetPlatform();

				if(p.x == 0)
					p = pTiles->GetMedPlatform();

				if(p.x == 0)
					return;

				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
				ModifTile(p + ivec2(1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
				ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_ARMOR_1);
			}
		}
	}
}

void CMapGen::GenerateAcid(CGenLayer *pTiles)
{
	ivec4 p = pTiles->GetPit();

	if(p.x == 0)
		return;

	const CTile *pGame = m_pCollision->GetTiles();
	const int W = m_pCollision->GetWidth();
	const int H = m_pCollision->GetHeight();
	for(int x = p.x; x < p.z; x++)
	{
		for(int y = p.y; y < p.w; y++)
		{
			if(x < 0 || y < 0 || x >= W || y >= H)
				continue;
			if(pGame[y * W + x].m_Index >= ENTITY_OFFSET)
				return;
		}
	}

	for(int x = p.x; x < p.z; x++)
		for(int y = p.y; y < p.w; y++)
		{
			ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_DAMAGEFLUID);
			pTiles->Use(x, y);
		}
}

void CMapGen::GenerateTutorialLevel()
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;
	if(w < 10 || h < 10)
		return;

	const int Chapter = clamp(g_Config.m_SvTutorialChapter, 1, (int)NUM_TUTORIAL_CHAPTERS);
	CGenLayer *pTiles = new CGenLayer(w, h);
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++)
			if(TutorialHallAir(x, y, w, h))
				pTiles->Set(0, x, y);

	pTiles->GenerateBackground();
	Proceed(pTiles, 0);
	WriteLayers(pTiles);
	WriteBackground(pTiles);

	CTutorialStamp aStamp[TUTORIAL_MAP_MAX_STAMPS];
	const int N = TutorialMapStamps(Chapter, aStamp, TUTORIAL_MAP_MAX_STAMPS);
	for(int i = 0; i < N; i++)
	{
		const ivec2 Pos(aStamp[i].m_X, aStamp[i].m_Y);
		if(Pos.x <= 1 || Pos.x >= w - 1 || Pos.y <= 1 || Pos.y >= h - 1)
			continue;
		ModifTile(Pos, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + aStamp[i].m_Entity);
		if(aStamp[i].m_Entity == ENTITY_DOOR1)
			pTiles->m_EndPos = Pos;
	}

	dbg_msg("mapgen",
			"tutorial chapter %d stamped %d entities on %dx%d hall",
			Chapter,
			N,
			w,
			h);
	delete pTiles;
}

void CMapGen::GenerateLevel()
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;

	if(w < 10 || h < 10)
		return;

	CGenLayer *pTiles = new CGenLayer(w, h);

	// generate room structure
	CRoomGenerated *pRoom = new CRoomGenerated(3, 3, w - 6, h - 6);
	CMaze *pMaze = new CMaze(w, h);

	int Level = g_Config.m_SvMapGenLevel;

	pMaze->OpenRooms(pRoom);

	pRoom->Generate(pTiles);

	// pTiles->GenerateMoreForeground();

	// check for too tight corridors
	{
		for(int y = 3; y < h - 4; y++)
			for(int x = 3; x < w - 4; x++)
			{
				if(!pTiles->Get(x - 1, y) && pTiles->Get(x, y) && pTiles->Get(x + 1, y) && !pTiles->Get(x + 2, y))
					pRoom->Fill(pTiles, 0, x, y, 2, 1);

				if(!pTiles->Get(x, y - 1) && pTiles->Get(x, y) && pTiles->Get(x, y + 1) && !pTiles->Get(x, y + 2))
					pRoom->Fill(pTiles, 0, x, y, 1, 2);
			}
	}

	pTiles->GenerateSlopes();
	pTiles->RemoveSingles();

	dbg_msg("mapgen", "rooms generated, map size: %d", pTiles->Size());

	int n = pTiles->Size() / 500;

	GenerateEnd(pTiles);
	pTiles->GenerateBackground();
	pTiles->GenerateMoreBackground();

	// Keep escape towers vertical — skip wide air platforms on acid-escape themes.
	// Extraction mazes get extra platforms for vertical complexity.
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
	{
		const int Platforms = max(3, n / 3) + irandom(2);
		pTiles->GenerateAirPlatforms(Platforms);
	}
	else if(InvasionThemeFromLevel(Level) != INVASION_THEME_ACID_ESCAPE)
	{
		if(n > 1)
			pTiles->GenerateAirPlatforms(n / 2 + irandom(n / 2));
		else
			pTiles->GenerateAirPlatforms(n);
	}

	dbg_msg("mapgen", "Proceed tiles");
	Proceed(pTiles, 0);

	pTiles->GenerateBoxes();
	pTiles->GeneratePlatforms();

	pTiles->GenerateFences();

	// write to layers; foreground
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y);

			if(i > 0)
			{
				int f = pTiles->GetFlags(x, y);
				ModifTile(ivec2(x, y), m_pLayers->GetForegroundLayerIndex(), i, f);

				// slopes
				if(i == 20 && f == TILEFLAG_VFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_RAMP_RIGHT);
				else if(i == 20 && f == 0)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_RAMP_LEFT);
				else if(i == 20 && f == TILEFLAG_HFLIP + TILEFLAG_VFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_ROOFSLOPE_RIGHT);
				else if(i == 20 && f == TILEFLAG_HFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_ROOFSLOPE_LEFT);
				else
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), 1);
			}
		}

	// write to layers; FGOBJECTS to foreground
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::FGOBJECTS);

			if(i > 0)
			{
				int f = pTiles->GetFlags(x, y, CGenLayer::FGOBJECTS);
				ModifTile(ivec2(x, y), m_pLayers->GetForegroundLayerIndex(), i, f);

				if(i >= 14 * 16 + 1 && i <= 14 * 16 + 3)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_PLATFORM);
				else
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), 1);
			}
		}

	// background
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::BACKGROUND);

			if(i > 0)
				ModifTile(ivec2(x, y),
						  m_pLayers->GetBackgroundLayerIndex(),
						  i,
						  pTiles->GetFlags(x, y, CGenLayer::BACKGROUND));
		}

	// doodads
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::DOODADS);

			if(i > 0)
				ModifTile(
					ivec2(x, y), m_pLayers->GetDoodadsLayerIndex(), i, pTiles->GetFlags(x, y, CGenLayer::DOODADS));
		}

	// find platforms, corners etc.
	dbg_msg("mapgen", "Scanning level");
	pTiles->Scan();

	// start pos — skip invalid (0,0) so we never stamp ENTITY_SPAWN into solids
	for(int i = 0; i < 4; i++)
	{
		ivec2 p = pTiles->GetPlayerSpawn();
		if(p.x <= 1 || p.y <= 1)
			continue;
		ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN);
		ModifTile(p + ivec2(+1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN);
	}

	// Theme switches / reactors must be placed before other generators consume platforms.
	const int Theme = InvasionThemeFromLevel(Level);
	const int HazardDiv = (Level >= 5 && Level <= 15) ? 2 : 1;
	const bool ExtractMode = str_comp(g_Config.m_SvGametype, "extract") == 0;
	const bool InvasionMode = str_comp(g_Config.m_SvGametype, "coop") == 0;
	if(Theme == INVASION_THEME_ACID_ESCAPE && !ExtractMode)
	{
		if(!GenerateSwitch(pTiles))
			GenerateSwitch(pTiles);
	}
	else if(ExtractMode)
	{
		// 3–5 switches, spread across the maze
		auto PlaceSwitchAt = [&](ivec2 p) -> bool
		{
			if(!SwitchSpotOk(pTiles, p))
				return false;
			ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SWITCH);
			pTiles->Use(p.x, p.y);
			dbg_msg("mapgen", "extract switch at %d,%d", p.x, p.y);
			return true;
		};

		auto FarEnough = [&](ivec2 Cand, const ivec2 *aPlaced, int Count, int MinDist) -> bool
		{
			for(int i = 0; i < Count; i++)
			{
				if(abs(Cand.x - aPlaced[i].x) + abs(Cand.y - aPlaced[i].y) < MinDist)
					return false;
			}
			return true;
		};

		const int Wanted = 3 + irandom(3); // 3–5
		const int MinDist = max(28, min(pTiles->Width(), pTiles->Height()) / 5);
		ivec2 aPlaced[8];
		int Placed = 0;

		// seed corners / extremes first
		ivec2 Seeds[4] = {
			pTiles->GetLeftPlatform(), pTiles->GetRightPlatform(), pTiles->GetBotPlatform(), pTiles->GetMedPlatform()};
		for(int s = 0; s < 4 && Placed < Wanted; s++)
		{
			ivec2 p = Seeds[s];
			if(p.x == 0)
				continue;
			if(FarEnough(p, aPlaced, Placed, MinDist) && PlaceSwitchAt(p))
				aPlaced[Placed++] = p;
		}

		for(int tries = 0; tries < 40 && Placed < Wanted; tries++)
		{
			ivec2 Cand = pTiles->GetPlatform();
			if(Cand.x == 0)
				Cand = pTiles->GetMedPlatform();
			if(Cand.x == 0)
				Cand = FindStandableFallback(pTiles, false);
			if(Cand.x == 0)
				continue;
			if(!FarEnough(Cand, aPlaced, Placed, MinDist))
				continue;
			if(PlaceSwitchAt(Cand))
				aPlaced[Placed++] = Cand;
		}

		while(Placed < 2 && GenerateSwitch(pTiles))
			Placed++;

		dbg_msg("mapgen", "extract placed %d/%d switches (minDist=%d)", Placed, Wanted, MinDist);
	}
	else if(Theme == INVASION_THEME_DUAL_SWITCHES)
	{
		int Placed = 0;
		for(int i = 0; i < 16 && Placed < 2; i++)
		{
			if(GenerateSwitch(pTiles))
				Placed++;
		}
		if(Placed < 2)
			dbg_msg("mapgen", "dual-switch layout: only placed %d/2 switches", Placed);
	}
	else if(Theme == INVASION_THEME_REACTOR_DEFEND)
	{
		bool Placed = false;
		for(int i = 0; i < 8 && !Placed; i++)
			Placed = GenerateReactor(pTiles);
		if(!Placed)
			dbg_msg("mapgen", "reactor-defend layout: failed to place reactor");
	}

	// acid pools (fewer on escape towers so the climb stays readable; skip rising-acid feel for extract/horde)
	int AcidPools = (Theme == INVASION_THEME_ACID_ESCAPE) ? 1 + Level / 20 : 2 + Level / 2;
	AcidPools = (AcidPools + HazardDiv - 1) / HazardDiv;
	if(ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
		AcidPools = min(AcidPools, 2);
	for(int i = 0; i < AcidPools; i++)
		GenerateAcid(pTiles);

	// conveyor belts
	{
		int c = irandom(min(6, 1 + Level / 2));
		c = (c + HazardDiv - 1) / HazardDiv;
		for(int i = 0; i < c; i++)
			GenerateConveyorBelt(pTiles);
	}

	// hangables
	{
		int c = 1 + irandom(min(11, 1 + Level / 4));
		c = (c + HazardDiv - 1) / HazardDiv;
		for(int i = 0; i < c; i++)
			GenerateHangables(pTiles);
	}

	for(int i = 0; i < min(4, 2 + int(Level * 0.5f)); i++)
		GeneratePowerupper(pTiles);

	if(Level > 2)
		GenerateShop(pTiles);

	if(Level > 3)
	{
		if(Level <= 15 && frandom() >= 0.5f)
		{ /* skip half walkers early */
		}
		else
			GenerateWalker(pTiles);
	}

	if(Level > 7)
		GenerateWalker(pTiles);

	if(Level > 12)
		GenerateWalker(pTiles);

	if(Level > 17)
		GenerateWalker(pTiles);

	// Enemy spawn positions. Keep one wide candidate for large runtime bosses.
	GenerateBossEnemySpawn(pTiles);
	for(int i = 0; i < 4; i++)
		GenerateEnemySpawn(pTiles);

	// if (Level > 3 && frandom() < 0.75f)

	for(int i = 0; i < 6; i++)
		GenerateScreen(pTiles);

	if(InvasionMode)
	{
		// Keep the first 20 floors unchanged. After that, crawler density grows
		// at roughly half the old rate so later maps do not become spawn rooms.
		int CrawlerCount = 0;
		if(Theme == INVASION_THEME_BOSS_ASSAULT)
			CrawlerCount = Level <= 20 ? min(12, Level / 3) : min(8, 1 + Level / 5);
		else if(Level > 3)
			CrawlerCount = Level <= 20 ? min(15, 1 + Level / 4) : min(10, 4 + (Level - 20) / 6);
		for(int i = 0; i < CrawlerCount; i++)
			GenerateCrawlerDroid(pTiles);

		// A Boss Crawler is now a milestone encounter rather than a regular
		// post-floor-20 map decoration: one every 20 floors outside boss assault.
		if(Theme != INVASION_THEME_BOSS_ASSAULT && Level % 20 == 0)
			GenerateBossCrawlerDroid(pTiles);
	}
	else
	{
		// Preserve the existing droid density for Horde and other generators.
		if(Theme == INVASION_THEME_BOSS_ASSAULT)
			for(int i = 0; i < min(12, Level / 3); i++)
				GenerateCrawlerDroid(pTiles);
		else if(Level > 3)
			for(int i = 0; i < min(15, 1 + Level / 4); i++)
				GenerateCrawlerDroid(pTiles);

		if(Theme != INVASION_THEME_BOSS_ASSAULT && (Level % 20 == 0))
			GenerateBossCrawlerDroid(pTiles);
		else if(Level > 20)
			for(int i = 0; i < min(3, Level / 5 - 3); i++)
				GenerateBossCrawlerDroid(pTiles);
	}

	// trap theme: sprinkle mines
	if(Theme == INVASION_THEME_TRAP_RUN)
	{
		int Mines = 4 + Level / 5;
		for(int i = 0; i < Mines; i++)
			GenerateMine(pTiles);
	}

	// Extraction mazes get a light trap layer for the dynamic event pool
	// (EVT_TRAP_ZONE picks among these).
	if(ExtractMode)
	{
		for(int i = 0; i < 3; i++)
			GenerateMine(pTiles);
		for(int i = 0; i < 2; i++)
			GenerateFiretrap(pTiles);
	}

	// lightning walls
	if(Level > 1)
	{
		int l = 1 + irandom(min(10, 1 + Level / 2));
		for(int i = 0; i < l; i++)
			GenerateLightningWall(pTiles);
	}

	{
		int TurretStands = 3 + Level / 5;
		if(Theme == INVASION_THEME_REACTOR_DEFEND)
			TurretStands = 6 + Level / 3;
		for(int i = 0; i < TurretStands; i++)
			GenerateTurretStand(pTiles);
	}

	// pickups
	// for (int i = 0; i < (pTiles->Size()-Level*5)/700; i++)

	// w = 2 + irandom()%3 + (Level > 15 ? 1 : 0);

	w = 4 + min(4, Level / 3);
	if(ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
		w = 12 + min(8, Level);

	for(int i = 0; i < w; i++)
		GenerateWeapon(pTiles, ENTITY_RANDOM_WEAPON);

	GenerateWeapon(pTiles, ENTITY_KIT);
	GenerateWeapon(pTiles, ENTITY_KIT);

	if(Level > 3 || ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
		GenerateWeapon(pTiles, ENTITY_KIT);
	if(Level > 8 || ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
		GenerateWeapon(pTiles, ENTITY_KIT);
	if(ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
	{
		GenerateWeapon(pTiles, ENTITY_KIT);
		GenerateWeapon(pTiles, ENTITY_RANDOM_WEAPON);
		GenerateWeapon(pTiles, ENTITY_RANDOM_WEAPON);
	}

	if(Theme == INVASION_THEME_REACTOR_DEFEND || Level % 5 == 4 || Level % 7 == 6 || Level % 11 == 9)
	{
		for(int i = 0; i < 2 + (0.3f + frandom()) * min(10.0f, Level * 0.8f); i++)
			GenerateTurret(pTiles);

		if(Level > 10 && frandom() < 0.7f)
			GenerateTeslacoil(pTiles);
	}
	else
	{
		if(frandom() < 0.5f && Level > 2)
			GenerateTurret(pTiles);

		if(frandom() < 0.5f && Level > 4)
			GenerateTurret(pTiles);
	}

	for(int i = 0; i < (pTiles->Size()) / 900; i++)
		GenerateHearts(pTiles);

	for(int i = 0; i < (pTiles->Size()) / 900; i++)
		GenerateAmmo(pTiles);

	for(int i = 0; i < (pTiles->Size()) / 1100; i++)
		GenerateArmor(pTiles);

	if(ExtractMode || str_comp(g_Config.m_SvGametype, "horde") == 0)
	{
		for(int i = 0; i < 6; i++)
			GenerateAmmo(pTiles);
		for(int i = 0; i < 4; i++)
			GenerateHearts(pTiles);
		for(int i = 0; i < 3; i++)
			GenerateArmor(pTiles);
	}

	// walkers
	/*
	if (Level%3 == 0 || Level%7 == 0 || Level%13 == 0 || Level%17 == 0)
	{
		int w = 1 + irandom(1 + min(Level / 4, 4));

		for (int i = 0; i < w; i++)
			GenerateWalker(pTiles);
	}
	*/

	/*
	if (Level > 3)
		GenerateWalker(pTiles);

	if (Level > 7)
		GenerateWalker(pTiles);

	if (Level > 12)
		GenerateWalker(pTiles);
	*/

	if(Level > 4)
		GenerateStarDroid(pTiles);

	if(Level > 8)
		GenerateStarDroid(pTiles);

	// barrels
	int b = max(4, 15 - Level / 3) + irandom(3);

	for(int i = 0; i < (pTiles->NumPlatforms() + pTiles->NumMedPlatforms()) / b; i++)
		GenerateBarrel(pTiles);

	// star droids
	/*
	if (Level > 5)
		if (Level%4 == 0 || Level%7 == 0 || Level%11 == 0 || Level%17 == 0)
		{
			int w = 1 + irandom(1 + min(Level / 4, 4));

			for (int i = 0; i < w; i++)
				GenerateStarDroid(pTiles);
		}
		*/

	// obstacles
	if(Level % 20 == 0)
		for(int i = 0; i < Level / 20; i++)
			GenerateDeathray(pTiles);

	for(int i = 0; i < Level / 4; i++)
		GenerateFiretrap(pTiles);

	for(int i = 0; i < Level / 6; i++)
		GenerateSawblade(pTiles);

	// more enemy spawn positions
	for(int i = 0; i < min(Level, 10); i++)
		GenerateEnemySpawn(pTiles);

	// Extraction task anchors (zone markers). Fixed count of 3; unused zones
	// stay idle when the runtime task pool picks fewer tasks.
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		for(int i = 0; i < 3; i++)
			GenerateExtractZone(pTiles);

	if(pRoom)
		delete pRoom;

	if(pTiles)
		delete pTiles;

	if(pMaze)
		delete pMaze;

	dbg_msg("mapgen", "Level generated");
}

void CMapGen::Mirror(CGenLayer *pTiles)
{
	int w = pTiles->Width();
	int h = pTiles->Height();

	for(int x = 0; x < w / 2; x++)
		for(int y = 0; y < h; y++)
		{
			pTiles->Set(pTiles->Get(w / 2 - x, y), w / 2 + x, y);
		}
}

void CMapGen::FitRoamAtlasCanvas()
{
	if(str_comp(g_Config.m_SvGametype, "roam") != 0 || !m_pLayers || !m_pLayers->GameLayer() || !m_pLayers->Map())
		return;

	const int NewW = RoamMapGen::AtlasWidth();
	const int NewH = RoamMapGen::AtlasHeight();
	CMapItemLayerTilemap *pGame = m_pLayers->GameLayer();
	if(pGame->m_Width == NewW && pGame->m_Height == NewH)
		return;

	dbg_msg("mapgen", "fitting roam atlas canvas %dx%d -> %dx%d", pGame->m_Width, pGame->m_Height, NewW, NewH);
	IMap *pMap = m_pLayers->Map();
	int LayerStart = 0;
	int LayerNum = 0;
	pMap->GetType(MAPITEMTYPE_LAYER, &LayerStart, &LayerNum);
	for(int i = 0; i < LayerNum; i++)
	{
		CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(pMap->GetItem(LayerStart + i, 0, 0));
		if(!pLayer || pLayer->m_Type != LAYERTYPE_TILES)
			continue;
		CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
		pTilemap->m_Width = NewW;
		pTilemap->m_Height = NewH;
		if(!pMap->ReplaceData(pTilemap->m_Data, NewW * NewH * (int)sizeof(CTile)))
			dbg_msg("mapgen", "failed to resize roam tile layer data index=%d", pTilemap->m_Data);
	}
	m_pCollision->RefreshMapgenDimensions();
}

void CMapGen::GenerateRoamLevel()
{
	const int Width = m_pLayers->GameLayer()->m_Width;
	const int Height = m_pLayers->GameLayer()->m_Height;
	const int NeedW = RoamMapGen::AtlasWidth();
	const int NeedH = RoamMapGen::AtlasHeight();
	if(Width < NeedW || Height < NeedH)
	{
		dbg_msg("mapgen", "roam atlas too small: %dx%d (need %dx%d)", Width, Height, NeedW, NeedH);
		return;
	}

	const int CheckpointCount = RoamMapGen::ClampCheckpointCount(g_Config.m_SvRoamCheckpoints);
	const int CourseLength = CheckpointCount + 1;
	RoamMapGen::CCoursePlacement aCourse[RoamMapGen::MAX_COURSE_LENGTH];
	const int PlacementCount = RoamMapGen::GenerateCourse(g_Config.m_SvMapGenSeed, aCourse, CourseLength);
	if(PlacementCount != CourseLength)
	{
		dbg_msg("mapgen", "roam course generation failed: seed=%d checkpoints=%d", g_Config.m_SvMapGenSeed, CheckpointCount);
		return;
	}

	CGenLayer Tiles(Width, Height);
	int aCanonicalPortTiles[3][2][RoamMapGen::PORT_WIDTH + 2] = {{{0}}};
	int aCanonicalPortFlags[3][2][RoamMapGen::PORT_WIDTH + 2] = {{{0}}};
	bool aCanonicalPortSet[3][2] = {{false, false}, {false, false}, {false, false}};
	for(int y = 0; y < Height; y++)
		for(int x = 0; x < Width; x++)
		{
			Tiles.Set(0, x, y, 0, CGenLayer::FOREGROUND);
			Tiles.Set(0, x, y, 0, CGenLayer::BACKGROUND);
			Tiles.Set(0, x, y, 0, CGenLayer::DOODADS);
			Tiles.Set(0, x, y, 0, CGenLayer::FGOBJECTS);
		}
	for(int Template = 0; Template < RoamMapGen::TEMPLATE_COUNT; Template++)
	{
		const int AtlasX = RoamMapGen::AtlasX(Template);
		const int AtlasY = RoamMapGen::AtlasY(Template);
		RoamMapGen::CTemplateGrid Grid;
		const RoamMapGen::CTemplateSpec Spec = RoamMapGen::TemplateSpec(Template);
		RoamMapGen::GenerateTemplate(Spec, &Grid);
		if(!RoamMapGen::ValidateTemplate(Spec, Grid))
		{
			dbg_msg("mapgen", "invalid roam template %d", Template);
			return;
		}

		// Auto-map each template in isolation. The one-tile halo describes only
		// its declared ports, so atlas neighbours can never influence an edge.
		CGenLayer Local(RoamMapGen::CHUNK_W + 2, RoamMapGen::CHUNK_H + 2);
		for(int y = -1; y <= RoamMapGen::CHUNK_H; y++)
			for(int x = -1; x <= RoamMapGen::CHUNK_W; x++)
			{
				bool Solid = true;
				if(x >= 0 && x < RoamMapGen::CHUNK_W && y >= 0 && y < RoamMapGen::CHUNK_H)
					Solid = Grid.Solid(x, y);
				else
				{
					const bool LeftOpen = x < 0 && (Spec.m_EntryDir == MAPPATH_DIR_LEFT || (!Spec.m_Finish && Spec.m_ExitDir == MAPPATH_DIR_LEFT)) && y >= 10 && y <= 17;
					const bool RightOpen = x >= RoamMapGen::CHUNK_W && (Spec.m_EntryDir == MAPPATH_DIR_RIGHT || (!Spec.m_Finish && Spec.m_ExitDir == MAPPATH_DIR_RIGHT)) && y >= 10 && y <= 17;
					const bool UpOpen = y < 0 && (Spec.m_EntryDir == MAPPATH_DIR_UP || (!Spec.m_Finish && Spec.m_ExitDir == MAPPATH_DIR_UP)) && x >= 17 && x <= 24;
					const bool DownOpen = y >= RoamMapGen::CHUNK_H && (Spec.m_EntryDir == MAPPATH_DIR_DOWN || (!Spec.m_Finish && Spec.m_ExitDir == MAPPATH_DIR_DOWN)) && x >= 17 && x <= 24;
					Solid = !(LeftOpen || RightOpen || UpOpen || DownOpen);
				}
				Local.Set(Solid ? 1 : 0, x + 1, y + 1);
			}
		Local.GenerateBackground();
		Local.GenerateMoreBackground();
		Proceed(&Local, 0);
		for(int Dir = 0; Dir < 4; Dir++)
		{
			const bool HasPort = Dir == Spec.m_EntryDir || (!Spec.m_Finish && Dir == Spec.m_ExitDir);
			if(!HasPort) continue;
			const int Axis = Dir == MAPPATH_DIR_LEFT || Dir == MAPPATH_DIR_RIGHT ? 0 : 1;
			for(int Across = -1; Across <= RoamMapGen::PORT_WIDTH; Across++)
			{
				const int X = Axis == 0 ? (Dir == MAPPATH_DIR_LEFT ? 1 : RoamMapGen::CHUNK_W) : 18 + Across;
				const int Y = Axis == 1 ? (Dir == MAPPATH_DIR_UP ? 1 : RoamMapGen::CHUNK_H) : 11 + Across;
				const int Slot = Across + 1;
				for(int Layer = CGenLayer::FOREGROUND; Layer <= CGenLayer::DOODADS; Layer++)
				{
					const int Tile = Local.Get(X, Y, Layer);
					const int Flags = Local.GetFlags(X, Y, Layer);
					if(aCanonicalPortSet[Layer][Axis])
						Local.Set(aCanonicalPortTiles[Layer][Axis][Slot], X, Y, aCanonicalPortFlags[Layer][Axis][Slot], Layer);
					else
					{
						aCanonicalPortTiles[Layer][Axis][Slot] = Tile;
						aCanonicalPortFlags[Layer][Axis][Slot] = Flags;
					}
				}
			}
			for(int Layer = CGenLayer::FOREGROUND; Layer <= CGenLayer::DOODADS; Layer++)
				aCanonicalPortSet[Layer][Axis] = true;
		}
		for(int y = 0; y < RoamMapGen::CHUNK_H; y++)
			for(int x = 0; x < RoamMapGen::CHUNK_W; x++)
			{
				for(int Layer = CGenLayer::FOREGROUND; Layer <= CGenLayer::FGOBJECTS; Layer++)
					Tiles.Set(Local.Get(x + 1, y + 1, Layer), AtlasX + x, AtlasY + y,
						Local.GetFlags(x + 1, y + 1, Layer), Layer);
			}
	}
	WriteLayers(&Tiles);
	WriteBackground(&Tiles);

	// Four rendering-only columns follow the logical atlas. All four carry a
	// continuous dark background; column 3 additionally seals the exact tile
	// ring around placed chunks on the foreground layer.
	for(int y = 0; y < Height; y++)
	{
		for(int Swatch = 0; Swatch < RoamMapGen::VISUAL_SWATCH_COLUMNS; Swatch++)
			ModifTile(ivec2(RoamMapGen::AtlasLogicalWidth() + Swatch, y), m_pLayers->GetBackgroundLayerIndex(),
				34 + ((y + Swatch * 7) % 13 == 0 ? 17 : 0), TILEFLAG_OPAQUE);
		ModifTile(ivec2(RoamMapGen::AtlasLogicalWidth() + 3, y), m_pLayers->GetForegroundLayerIndex(), 19, TILEFLAG_OPAQUE);
	}

	// Non-colliding tile gates. Middle/finish templates get a checkpoint strip
	// at their entry; finish templates also get a double strip in the arena.
	auto DrawGate = [&](int Template, int Dir, bool Finish)
	{
		const RoamMapGen::CTileAabb B = RoamMapGen::RaceGateLocalAabb(Dir, Finish);
		const bool VerticalStrip = Dir == MAPPATH_DIR_LEFT || Dir == MAPPATH_DIR_RIGHT;
		const int Lines = Finish ? 2 : 1;
		for(int Line = 0; Line < Lines; Line++)
			for(int Across = 0; Across < RoamMapGen::PORT_WIDTH; Across++)
			{
				int X, Y;
				if(VerticalStrip) { X = B.m_MinX + Line; Y = B.m_MinY + Across; }
				else { X = B.m_MinX + Across; Y = B.m_MinY + Line; }
				const int Tile = Across == 0 ? 89 : Across == RoamMapGen::PORT_WIDTH - 1 ? 92 : (Across + Line) % 2 ? 90 : 91;
				int Flags = VerticalStrip ? TILEFLAG_ROTATE : 0;
				if(Dir == MAPPATH_DIR_RIGHT || Dir == MAPPATH_DIR_DOWN) Flags |= TILEFLAG_HFLIP;
				ModifTile(ivec2(RoamMapGen::AtlasX(Template) + X, RoamMapGen::AtlasY(Template) + Y),
					m_pLayers->GetDoodadsLayerIndex(), Tile, Flags);
			}
	};
	for(int Template = RoamMapGen::TEMPL_MIDDLE_FIRST; Template < RoamMapGen::TEMPLATE_COUNT; Template++)
	{
		const RoamMapGen::CTemplateSpec Spec = RoamMapGen::TemplateSpec(Template);
		DrawGate(Template, Spec.m_EntryDir, false);
		if(Spec.m_Finish) DrawGate(Template, Spec.m_EntryDir, true);
	}

	m_HasPathInfo = true;
	m_PathInfo.m_Version = 1;
	m_PathInfo.m_ChunkWidth = RoamMapGen::CHUNK_W;
	m_PathInfo.m_ChunkHeight = RoamMapGen::CHUNK_H;
	m_PathInfo.m_AtlasColumns = RoamMapGen::ATLAS_COLS;
	m_PathInfo.m_TemplateCount = RoamMapGen::TEMPLATE_COUNT;
	m_PathInfo.m_PlacementCount = PlacementCount;
	for(int i = 0; i < PlacementCount; i++)
	{
		m_aPathPlacements[i].m_GridX = aCourse[i].m_GridX;
		m_aPathPlacements[i].m_GridY = aCourse[i].m_GridY;
		m_aPathPlacements[i].m_TemplateIndex = aCourse[i].m_TemplateIndex;
		m_aPathPlacements[i].m_CourseIndex = aCourse[i].m_CourseIndex;
		m_aPathPlacements[i].m_EntryDir = aCourse[i].m_EntryDir;
		m_aPathPlacements[i].m_ExitDir = aCourse[i].m_ExitDir;
	}

	dbg_msg("mapgen", "roam path-v2 generated: seed=%d checkpoints=%d placements=%d chunk=%dx%d",
		g_Config.m_SvMapGenSeed, CheckpointCount, PlacementCount, RoamMapGen::CHUNK_W, RoamMapGen::CHUNK_H);
}

void CMapGen::GeneratePVPLevel()
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;

	if(w < 10 || h < 10)
		return;

	CGenLayer *pTiles = new CGenLayer(w, h);

	// generate room structure
	CRoomGenerated *pRoom = new CRoomGenerated(3, 3, w - 6, h - 6);
	CMaze *pMaze = new CMaze(w, h);

	pMaze->OpenRooms(pRoom);

	pRoom->Generate(pTiles);

	// pTiles->GenerateMoreForeground();

	// check for too tight corridors
	{
		for(int y = 3; y < h - 4; y++)
			for(int x = 3; x < w - 4; x++)
			{
				if(!pTiles->Get(x - 1, y) && pTiles->Get(x, y) && pTiles->Get(x + 1, y) && !pTiles->Get(x + 2, y))
					pRoom->Fill(pTiles, 0, x, y, 2, 1);

				if(!pTiles->Get(x, y - 1) && pTiles->Get(x, y) && pTiles->Get(x, y + 1) && !pTiles->Get(x, y + 2))
					pRoom->Fill(pTiles, 0, x, y, 1, 2);
			}
	}

	pTiles->GenerateSlopes();

	bool BR = false;

	if(str_comp(g_Config.m_SvGametype, "dm") == 0)
	{
		if(g_Config.m_SvSurvivalMode)
			BR = true; // battle royale
	}
	else
		Mirror(pTiles);

	pTiles->RemoveSingles();

	dbg_msg("mapgen", "rooms generated, map size: %d", pTiles->Size());

	int n = pTiles->Size() / 500;

	pTiles->GenerateBackground();
	pTiles->GenerateMoreBackground();

	if(n > 1)
		pTiles->GenerateAirPlatforms(n / 2 + irandom(n / 2));
	else
		pTiles->GenerateAirPlatforms(n);

	dbg_msg("mapgen", "Proceed tiles");
	Proceed(pTiles, 0);

	pTiles->GenerateBoxes();
	pTiles->GeneratePlatforms();

	pTiles->GenerateFences();

	WriteLayers(pTiles);
	WriteBackground(pTiles);

	// find platforms, corners etc.
	dbg_msg("mapgen", "Scanning level");
	pTiles->Scan();

	// flags to ctf
	if(str_comp(g_Config.m_SvGametype, "ctf") == 0)
	{
		// left & rightmost tiles as spawns

		// red team spawn pos
		/*
		for (int i = 0; i < 2; i++)
		{
			ivec2 p = pTiles->GetLeftPlatform();

			if (p.x != 0)
			{
				pTiles->Use(p.x, p.y);
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET+ENTITY_SPAWN_RED);
			}
		}

		// blue team spawn pos
		for (int i = 0; i < 2; i++)
		{
			ivec2 p = pTiles->GetRightPlatform();

			if (p.x != 0)
			{
				pTiles->Use(p.x, p.y);
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET+ENTITY_SPAWN_BLUE);
			}
		}
		*/

		{
			ivec2 p = pTiles->GetLeftPlatform();

			if(p.x != 0)
			{
				pTiles->Use(p.x, p.y);
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_FLAGSTAND_RED);
				WriteBase(pTiles, 0, p, 6);
			}
			else
				dbg_msg("mapgen", "Can't set red flag");
		}
		{
			ivec2 p = pTiles->GetRightPlatform();

			if(p.x != 0)
			{
				pTiles->Use(p.x, p.y);
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_FLAGSTAND_BLUE);
				WriteBase(pTiles, 1, p, 6);
			}
			else
				dbg_msg("mapgen", "Can't set blue flag");
		}
	}

	// dm spawn pos
	if(str_comp(g_Config.m_SvGametype, "dm") == 0)
	{
		for(int i = 0; i < 16; i++)
		{
			ivec2 p = pTiles->GetPlatform();

			if(p.x == 0)
				p = pTiles->GetWall();
			if(p.x == 0)
				p = pTiles->GetCeiling();

			if(p.x != 0)
				ModifTile(p, m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN);
		}
	}
	else
	// tdm & ctf
	{
		for(int i = 0; i < 6; i++)
		{
			// red team spawn pos
			{
				ivec2 p = pTiles->GetLeftPlatform();

				if(p.x != 0)
				{
					pTiles->Use(p.x, p.y);
					ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN_RED);
					ModifTile(p + ivec2(+1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN_RED);
				}
			}

			// blue team spawn pos
			{
				ivec2 p = pTiles->GetRightPlatform();

				if(p.x != 0)
				{
					pTiles->Use(p.x, p.y);
					ModifTile(p + ivec2(-1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN_BLUE);
					ModifTile(p + ivec2(+1, 0), m_pLayers->GetGameLayerIndex(), ENTITY_OFFSET + ENTITY_SPAWN_BLUE);
				}
			}
		}
	}

	// acid pools
	for(int i = 0; i < 40; i++)
		GenerateAcid(pTiles);

	// conveyor belts
	{
		int c = 2 + irandom(8);
		for(int i = 0; i < c; i++)
			GenerateConveyorBelt(pTiles);
	}

	// hangables
	int c = 2 + irandom(4);
	for(int i = 0; i < c; i++)
		GenerateHangables(pTiles);

	for(int i = 0; i < 6; i++)
		GenerateScreen(pTiles);

	int Obs = 3; // 1 + pTiles->NumPlatforms() / 4.0f;

	GeneratePowerupper(pTiles);
	GeneratePowerupper(pTiles);

	if(BR)
		GeneratePowerupper(pTiles);

	// barrels
	int b = 5 + irandom(3);

	for(int i = 0; i < (pTiles->NumPlatforms() + pTiles->NumMedPlatforms()) / b; i++)
		GenerateBarrel(pTiles);

	w = 2 + (pTiles->NumPlatforms() + pTiles->NumMedPlatforms()) / 4.0f;

	for(int i = 0; i < w; i++)
		GenerateWeapon(pTiles, ENTITY_RANDOM_WEAPON);

	for(int i = 0; i < 5; i++)
		GenerateWeapon(pTiles, ENTITY_KIT);

	for(int i = 0; i < (pTiles->Size()) / 750; i++)
		GenerateHearts(pTiles);

	for(int i = 0; i < (pTiles->Size()) / 750; i++)
		GenerateAmmo(pTiles);

	for(int i = 0; i < (pTiles->Size()) / 1000; i++)
		GenerateArmor(pTiles);

	// obstacles

	while(Obs-- > 0)
	{
		switch(irandom(6))
		{
			case 0:
			case 1:
			case 2:
				GenerateSawblade(pTiles);
				break;
			case 3:
			case 4:
				GenerateFiretrap(pTiles);
				break;
			case 5:
				GenerateDeathray(pTiles);
				break;
		}
	}

	if(pRoom)
		delete pRoom;

	if(pTiles)
		delete pTiles;

	if(pMaze)
		delete pMaze;
}

void CMapGen::WriteBase(class CGenLayer *pTiles, int BaseNum, ivec2 Pos, float Size)
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;

	CGenLayer *pBaseTiles = new CGenLayer(w, h);
	pBaseTiles->CleanTiles();

	// copy tiles & check distance to base pos
	for(int x = 1; x < w - 1; x++)
		for(int y = 1; y < h - 1; y++)
		{
			int i = pTiles->Get(x, y);

			if(i > 0 && distance(vec2(Pos.x, Pos.y), vec2(x, y)) < Size)
				pBaseTiles->Set(1, x, y);
		}

	// auto map
	pBaseTiles->RemoveSingles();
	pBaseTiles->BaseCleanup();
	Proceed(pBaseTiles, 0);

	// write to layer
	int LayerIndex = 0;

	if(BaseNum == 0)
		LayerIndex = m_pLayers->GetBase1LayerIndex();
	else if(BaseNum == 1)
		LayerIndex = m_pLayers->GetBase2LayerIndex();

	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pBaseTiles->Get(x, y);

			if(i > 0)
			{
				int f = pBaseTiles->GetFlags(x, y);
				ModifTile(ivec2(x, y), LayerIndex, i, f);
			}
		}

	delete pBaseTiles;
}

void CMapGen::WriteLayers(CGenLayer *pTiles)
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;

	// write to layers; foreground
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y);

			if(i > 0)
			{
				int f = pTiles->GetFlags(x, y);
				ModifTile(ivec2(x, y), m_pLayers->GetForegroundLayerIndex(), i, f);

				// slopes
				if(i == 20 && f == TILEFLAG_VFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_RAMP_RIGHT);
				else if(i == 20 && f == 0)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_RAMP_LEFT);
				else if(i == 20 && f == TILEFLAG_HFLIP + TILEFLAG_VFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_ROOFSLOPE_RIGHT);
				else if(i == 20 && f == TILEFLAG_HFLIP)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_ROOFSLOPE_LEFT);
				else
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), 1);
			}
		}

	// write to layers; FGOBJECTS to foreground
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::FGOBJECTS);

			if(i > 0)
			{
				int f = pTiles->GetFlags(x, y, CGenLayer::FGOBJECTS);
				ModifTile(ivec2(x, y), m_pLayers->GetForegroundLayerIndex(), i, f);

				if(i >= 14 * 16 + 1 && i <= 14 * 16 + 3)
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), TILE_PLATFORM);
				else
					ModifTile(ivec2(x, y), m_pLayers->GetGameLayerIndex(), 1);
			}
		}

	/*
	// background
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::BACKGROUND);

			if (i > 0)
				ModifTile(ivec2(x, y), m_pLayers->GetBackgroundLayerIndex(), i, pTiles->GetFlags(x, y,
	CGenLayer::BACKGROUND));
		}
	*/

	// doodads
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::DOODADS);

			if(i > 0)
				ModifTile(
					ivec2(x, y), m_pLayers->GetDoodadsLayerIndex(), i, pTiles->GetFlags(x, y, CGenLayer::DOODADS));
		}
}

void CMapGen::WriteBackground(CGenLayer *pTiles)
{
	int w = m_pLayers->GameLayer()->m_Width;
	int h = m_pLayers->GameLayer()->m_Height;

	// background
	for(int x = 0; x < w; x++)
		for(int y = 0; y < h; y++)
		{
			int i = pTiles->Get(x, y, CGenLayer::BACKGROUND);

			if(i > 0)
				ModifTile(ivec2(x, y),
						  m_pLayers->GetBackgroundLayerIndex(),
						  i,
						  pTiles->GetFlags(x, y, CGenLayer::BACKGROUND));
		}
}

inline void CMapGen::ModifTile(ivec2 Pos, int Layer, int Tile, int Flags)
{
	m_pCollision->ModifTile(Pos, m_pLayers->GetGameGroupIndex(), Layer, Tile, Flags, 0);
}

void CMapGen::Proceed(CGenLayer *pTiles, int ConfigID)
{
	if(!m_FileLoaded || ConfigID < 0 || ConfigID >= m_lConfigs.size())
		return;

	CConfiguration *pConf = &m_lConfigs[ConfigID];

	if(!pConf->m_aIndexRules.size())
		return;

	int BaseTile = 1;
	bool BaseTileFound = false;
	array<CIndexRule *> aRules;

	// Find the base tile and compact the rules used for every occupied tile.
	for(int i = 0; i < pConf->m_aIndexRules.size(); ++i)
	{
		CIndexRule *pIndexRule = &pConf->m_aIndexRules[i];
		if(pIndexRule->m_BaseTile)
		{
			if(!BaseTileFound)
			{
				BaseTile = pIndexRule->m_ID;
				BaseTileFound = true;
			}
			continue;
		}
		aRules.add(pIndexRule);
	}

	const int Width = pTiles->Width();
	const int Height = pTiles->Height();
	for(int i = 0; i < aRules.size(); i++)
		for(int j = 0; j < aRules[i]->m_aRules.size(); j++)
			aRules[i]->m_aRules[j].m_Offset = aRules[i]->m_aRules[j].m_Y * Width + aRules[i]->m_aRules[j].m_X;

	array<unsigned char> aRuleYMatches;
	for(int i = 0; i < aRules.size(); i++)
		aRuleYMatches.add(1);

	int *apLayerTiles[3] = {pTiles->m_pTiles, pTiles->m_pBGTiles, pTiles->m_pDoodadsTiles};
	int *apLayerFlags[3] = {pTiles->m_pFlags, pTiles->m_pBGFlags, pTiles->m_pDoodadsFlags};

	// auto map !
	for(int l = 0; l < 3; l++)
	{
		int *pLayerTiles = apLayerTiles[l];
		int *pLayerFlags = apLayerFlags[l];
		for(int y = 0; y < Height; y++)
		{
			for(int i = 0; i < aRules.size(); i++)
				aRuleYMatches[i] = aRules[i]->m_YDivisor < 2 || y % aRules[i]->m_YDivisor == aRules[i]->m_YRemainder;

			const int RowStart = y * Width;
			for(int x = 0; x < Width; x++)
			{
				const int TileIndex = RowStart + x;
				if(pLayerTiles[TileIndex] <= 0)
					continue;

				pLayerTiles[TileIndex] = BaseTile;
				pLayerFlags[TileIndex] = 0;

				if(y == 0 || y == Height - 1 || x == 0 || x == Width - 1)
					continue;

				for(int i = 0; i < aRules.size(); ++i)
				{
					CIndexRule *pIndexRule = aRules[i];
					bool RespectRules = true;
					for(int j = 0; j < pIndexRule->m_aRules.size() && RespectRules; ++j)
					{
						CPosRule *pRule = &pIndexRule->m_aRules[j];
						const int CheckX = x + pRule->m_X;
						const int CheckY = y + pRule->m_Y;
						if(CheckX < 0 || CheckX >= Width || CheckY < 0 || CheckY >= Height)
							RespectRules = false;
						else
						{
							const int CheckIndex = CheckY * Width + CheckX;
							const int RawTileValue = pLayerTiles[CheckIndex];
							const int TileValue = RawTileValue < 0 ? 0 : RawTileValue;
							if(pRule->m_IndexValue)
							{
								if(TileValue != pRule->m_Value)
									RespectRules = false;
							}
							else if((TileValue > 0 && pRule->m_Value == CPosRule::EMPTY) ||
									(TileValue == 0 && pRule->m_Value == CPosRule::FULL))
								RespectRules = false;
						}
					}

					if(RespectRules && aRuleYMatches[i] &&
					   (pIndexRule->m_RandomValue <= 1 ||
						irandom(pIndexRule->m_RandomValue) == 1))
					{
						pLayerTiles[TileIndex] = pIndexRule->m_ID;
						pLayerFlags[TileIndex] = pIndexRule->m_Flag;
					}
				}
			}
		}
	}
}
