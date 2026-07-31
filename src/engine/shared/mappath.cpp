#include "mappath.h"

#include <base/system.h>

CMapPath::CMapPath()
{
	Clear();
}

void CMapPath::Clear()
{
	mem_zero(&m_Info, sizeof(m_Info));
	mem_zero(m_aPlacements, sizeof(m_aPlacements));
	mem_zero(m_aGridKeys, sizeof(m_aGridKeys));
	mem_zero(m_aGridValues, sizeof(m_aGridValues));
	m_GridCount = 0;
	m_Valid = false;
}

int CMapPath::FloorDiv(int Value, int Size)
{
	if(Size <= 0)
		return 0;
	if(Value >= 0)
		return Value / Size;
	return -((-Value + Size - 1) / Size);
}

int CMapPath::ModPositive(int Value, int Size)
{
	if(Size <= 0)
		return 0;
	int R = Value % Size;
	return R < 0 ? R + Size : R;
}

int CMapPath::OppositeDir(int Dir)
{
	return (Dir + 2) & 3;
}

void CMapPath::DirOffset(int Dir, int *pDX, int *pDY)
{
	static const int aDX[4] = {0, 1, 0, -1};
	static const int aDY[4] = {-1, 0, 1, 0};
	*pDX = aDX[Dir & 3];
	*pDY = aDY[Dir & 3];
}

int CMapPath::FindGrid(int GridX, int GridY) const
{
	const int Key = (GridX + 512) * 1024 + (GridY + 512);
	for(int i = 0; i < m_GridCount; i++)
		if(m_aGridKeys[i] == Key)
			return m_aGridValues[i];
	return -1;
}

bool CMapPath::Init(const CMapPathInfoData *pInfo, const CMapPathPlacementData *pPlacements, int Count)
{
	Clear();
	if(!pInfo || !pPlacements || pInfo->m_Version < 1 || Count <= 0 || Count > MAPPATH_MAX_PLACEMENTS)
		return false;
	if(pInfo->m_ChunkWidth <= 0 || pInfo->m_ChunkHeight <= 0 || pInfo->m_AtlasColumns <= 0 || pInfo->m_TemplateCount <= 0)
		return false;
	if(pInfo->m_PlacementCount != Count)
		return false;

	m_Info = *pInfo;
	for(int i = 0; i < Count; i++)
	{
		if(pPlacements[i].m_TemplateIndex < 0 || pPlacements[i].m_TemplateIndex >= pInfo->m_TemplateCount ||
			pPlacements[i].m_EntryDir < MAPPATH_DIR_UP || pPlacements[i].m_EntryDir > MAPPATH_DIR_LEFT ||
			pPlacements[i].m_ExitDir < MAPPATH_DIR_UP || pPlacements[i].m_ExitDir > MAPPATH_DIR_LEFT)
			return false;
		// A world cell may have one and only one atlas template.  Rejecting
		// duplicate cells here prevents ambiguous collision/render resolution.
		if(FindGrid(pPlacements[i].m_GridX, pPlacements[i].m_GridY) >= 0)
			return false;
		m_aPlacements[i] = pPlacements[i];
		const int Key = (pPlacements[i].m_GridX + 512) * 1024 + (pPlacements[i].m_GridY + 512);
		m_aGridKeys[m_GridCount] = Key;
		m_aGridValues[m_GridCount] = i;
		m_GridCount++;
	}
	m_Valid = true;
	return true;
}

const CMapPathPlacementData *CMapPath::Placement(int Index) const
{
	if(!m_Valid || Index < 0 || Index >= m_Info.m_PlacementCount)
		return 0;
	return &m_aPlacements[Index];
}

const CMapPathPlacementData *CMapPath::PlacementAtGrid(int GridX, int GridY) const
{
	const int Index = FindGrid(GridX, GridY);
	return Index >= 0 ? &m_aPlacements[Index] : 0;
}

const CMapPathPlacementData *CMapPath::PlacementAtWorldTile(int WorldTileX, int WorldTileY) const
{
	if(!m_Valid)
		return 0;
	return PlacementAtGrid(FloorDiv(WorldTileX, m_Info.m_ChunkWidth), FloorDiv(WorldTileY, m_Info.m_ChunkHeight));
}

bool CMapPath::ResolveTile(int WorldTileX, int WorldTileY, int *pAtlasX, int *pAtlasY, int *pCourseIndex) const
{
	const CMapPathPlacementData *pPlacement = PlacementAtWorldTile(WorldTileX, WorldTileY);
	if(!pPlacement)
		return false;

	const int LocalX = ModPositive(WorldTileX, m_Info.m_ChunkWidth);
	const int LocalY = ModPositive(WorldTileY, m_Info.m_ChunkHeight);
	const int AtlasCol = pPlacement->m_TemplateIndex % m_Info.m_AtlasColumns;
	const int AtlasRow = pPlacement->m_TemplateIndex / m_Info.m_AtlasColumns;
	if(pAtlasX)
		*pAtlasX = AtlasCol * m_Info.m_ChunkWidth + LocalX;
	if(pAtlasY)
		*pAtlasY = AtlasRow * m_Info.m_ChunkHeight + LocalY;
	if(pCourseIndex)
		*pCourseIndex = pPlacement->m_CourseIndex;
	return true;
}

bool CMapPath::ResolveVisualTile(int WorldTileX, int WorldTileY, int *pAtlasX, int *pAtlasY, int *pCourseIndex) const
{
	if(ResolveTile(WorldTileX, WorldTileY, pAtlasX, pAtlasY, pCourseIndex))
		return true;
	if(!m_Valid)
		return false;

	// Swatch 3 is the foreground seal. The other three columns contain only
	// the looping background. A world tile is sealed only when it directly
	// touches a placed cell, so the visual wall is exactly one tile thick.
	bool Adjacent = false;
	static const int s_aDX[4] = {0, 1, 0, -1};
	static const int s_aDY[4] = {-1, 0, 1, 0};
	for(int Dir = 0; Dir < 4 && !Adjacent; Dir++)
		Adjacent = PlacementAtWorldTile(WorldTileX + s_aDX[Dir], WorldTileY + s_aDY[Dir]) != 0;

	const int LogicalWidth = m_Info.m_AtlasColumns * m_Info.m_ChunkWidth;
	const int Variant = Adjacent ? 3 : ModPositive(WorldTileX + WorldTileY * 3, 3);
	if(pAtlasX)
		*pAtlasX = LogicalWidth + Variant;
	if(pAtlasY)
		*pAtlasY = ModPositive(WorldTileY, ((m_Info.m_TemplateCount + m_Info.m_AtlasColumns - 1) / m_Info.m_AtlasColumns) * m_Info.m_ChunkHeight);
	if(pCourseIndex)
		*pCourseIndex = -1;
	return true;
}

bool CMapPath::HasPlacementAtWorldTile(int WorldTileX, int WorldTileY) const
{
	return PlacementAtWorldTile(WorldTileX, WorldTileY) != 0;
}
