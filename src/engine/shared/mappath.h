#ifndef ENGINE_SHARED_MAPPATH_H
#define ENGINE_SHARED_MAPPATH_H

enum
{
	MAPPATH_DIR_UP = 0,
	MAPPATH_DIR_RIGHT = 1,
	MAPPATH_DIR_DOWN = 2,
	MAPPATH_DIR_LEFT = 3,
	MAPPATH_MAX_PLACEMENTS = 64,
};

struct CMapPathInfoData
{
	int m_Version;
	int m_ChunkWidth;
	int m_ChunkHeight;
	int m_AtlasColumns;
	int m_TemplateCount;
	int m_PlacementCount;
};

struct CMapPathPlacementData
{
	int m_GridX;
	int m_GridY;
	int m_TemplateIndex;
	int m_CourseIndex;
	int m_EntryDir;
	int m_ExitDir;
};

class CMapPath
{
	CMapPathInfoData m_Info;
	CMapPathPlacementData m_aPlacements[MAPPATH_MAX_PLACEMENTS];
	int m_aGridKeys[MAPPATH_MAX_PLACEMENTS];
	int m_aGridValues[MAPPATH_MAX_PLACEMENTS];
	int m_GridCount;
	bool m_Valid;

	int FindGrid(int GridX, int GridY) const;

  public:
	CMapPath();

	void Clear();
	bool Init(const CMapPathInfoData *pInfo, const CMapPathPlacementData *pPlacements, int Count);
	bool Valid() const { return m_Valid; }

	const CMapPathInfoData &Info() const { return m_Info; }
	int PlacementCount() const { return m_Valid ? m_Info.m_PlacementCount : 0; }
	const CMapPathPlacementData *Placement(int Index) const;
	const CMapPathPlacementData *PlacementAtGrid(int GridX, int GridY) const;
	const CMapPathPlacementData *PlacementAtWorldTile(int WorldTileX, int WorldTileY) const;

	bool ResolveTile(int WorldTileX, int WorldTileY, int *pAtlasX, int *pAtlasY, int *pCourseIndex = 0) const;
	// Rendering-only lookup. Empty world cells resolve to the visual swatches
	// stored directly after the logical template atlas; collision and entity
	// code must continue to use ResolveTile.
	bool ResolveVisualTile(int WorldTileX, int WorldTileY, int *pAtlasX, int *pAtlasY, int *pCourseIndex = 0) const;
	bool HasPlacementAtWorldTile(int WorldTileX, int WorldTileY) const;

	static int FloorDiv(int Value, int Size);
	static int ModPositive(int Value, int Size);
	static int OppositeDir(int Dir);
	static void DirOffset(int Dir, int *pDX, int *pDY);
};

#endif
