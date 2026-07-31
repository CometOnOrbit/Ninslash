#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>
#include "pathfinding.h"

#include <set>
#include <utility>

class CCharacterCore;

class CCollision
{
	friend class CCharacterCore;
	friend class CBallCore;
	class CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	bool *m_pBlocks;
	std::set<std::pair<int, int>> m_ModularBlocks;
	int *m_pLightRays;
	enum
	{
		MODIF_TILE_CACHE_SIZE = 8
	};
	int m_aModifTileGroup[MODIF_TILE_CACHE_SIZE];
	int m_aModifTileLayer[MODIF_TILE_CACHE_SIZE];
	class CMapItemLayerTilemap *m_apModifTilemap[MODIF_TILE_CACHE_SIZE];
	class CTile *m_apModifTiles[MODIF_TILE_CACHE_SIZE];

	class CMapChunk *m_pMapChunkRoot;
	class CMapChunk *m_pMapChunk;

	int SolidState(int x, int y, bool IncludeDeath = false, bool Down = false, bool IncludeBlocks = true);
	int ForceState(int x, int y);

	int GetTile(int x, int y, bool Down = true, bool IncludeBlocks = true);
	int GetTileRay(int x, int y, bool Down = true);
	bool GetBlock(int x, int y);

	int m_WaypointCount;
	int m_ConnectionCount;
	int m_LastWaypointChunk;

	void ClearWaypoints();

	void RemoveClosedAreas();
	void AddWaypoint(vec2 Position, bool InnerCorner = false);
	CWaypoint *GetWaypointAt(int x, int y);
	bool WaypointLineBlocked(vec2 Pos0, vec2 Pos1, const unsigned char *pTileCanCollide);
	void ConnectWaypoints(signed char *pVisibilityCache, const unsigned char *pTileCanCollide);
	CWaypoint *GetClosestWaypoint(vec2 Pos);

	CWaypoint *m_apWaypoint[MAX_WAYPOINTS];
	CWaypoint *m_pCenterWaypoint;

	CWaypointPath *m_pPath;

	int m_LowestPoint;
	void ClearModifTileCache();

  public:
	bool IsMapModular() { return m_pMapChunkRoot ? true : false; }
	int GetModularPos(int x);
	int GetChunkSize();
	void PruneMapChunks(int LowTileX, int HighTileX);

	enum
	{
		COLFLAG_SOLID = 1,
		COLFLAG_DEATH = 2,
		COLFLAG_INSTADEATH = 4,

		COLFLAG_RAMP_LEFT = 8,
		COLFLAG_RAMP_RIGHT = 16,
		COLFLAG_ROOFSLOPE_LEFT = 32,
		COLFLAG_ROOFSLOPE_RIGHT = 64,
		COLFLAG_DAMAGEFLUID = 128,

		// 256 = out of range for unsigned char, ugly! spurdo :D
		COLFLAG_MOVELEFT = 129,
		COLFLAG_MOVERIGHT = 130,
		COLFLAG_HANG = 131,
		COLFLAG_PLATFORM = 132,
	};

	enum
	{
		SS_NOCOL = 0,
		SS_COL = 1,
		SS_COL_RL = 2,
		SS_COL_RR = 3,
		SS_COL_HL = 4,
		SS_COL_HR = 5,
	};

	vec2 GetClosestWaypointPos(vec2 Pos);

	bool IsTileSolid(int x, int y, bool IncludeDeath = false);
	int GetRayPoint(int x, int y);

	void GenerateWaypoints();
	void GenerateWaypointsAround(int WorldTileX);
	bool GenerateSomeMoreWaypoints();
	int WaypointCount() { return m_WaypointCount; }
	int ConnectionCount() { return m_ConnectionCount; }

	void InitLightRays();
	int GetLightRay(ivec2 Pos);

	void SetBlock(ivec2 Pos, bool Block);

	void SetWaypointCenter(vec2 Position);
	void AddWeight(vec2 Pos, int Weight);

	vec2 GetRandomWaypointPos();

	int m_Time;
	bool m_GlobalAcid;

	int GetLowestPoint();
	float GetGlobalAcidLevel();

	// CWaypointPath *AStar(vec2 Start, vec2 End);
	bool AStar(vec2 Start, vec2 End);

	CWaypointPath *GetPath() { return m_pPath; }
	void ForgetAboutThePath() { m_pPath = 0; }

	// for testing
	vec2 m_aPath[99];
	int m_PathLen;

	CCollision();
	~CCollision();
	void Init(class CLayers *pLayers);
	void RefreshMapgenDimensions(); // after mapgen resizes tile layers
	// bool CheckPoint(float x, float y, bool IncludeDeath = false) { return IsTileSolid(round_to_int(x),
	// round_to_int(y), IncludeDeath); }

	// -1=left, 0=nope, 1=right
	int IsForceTile(float x, float y) { return ForceState(round_to_int(x), round_to_int(y)); }
	int IsForceTile(float LeftX, float RightX, float y)
	{
		const int RoundedLeftX = round_to_int(LeftX);
		const int RoundedRightX = round_to_int(RightX);
		const int RoundedY = round_to_int(y);
		const int LeftForce = ForceState(RoundedLeftX, RoundedY);
		if(LeftForce != 0 || RoundedLeftX / 32 == RoundedRightX / 32)
			return LeftForce;
		return ForceState(RoundedRightX, RoundedY);
	}
	int IsForceTile(vec2 Pos) { return IsForceTile(Pos.x, Pos.y); }

	bool IsSawblade(float x, float y);
	bool IsSawblade(vec2 Pos) { return IsSawblade(Pos.x, Pos.y); }

	bool IsHangTile(float x, float y);
	bool IsHangTile(vec2 Pos) { return IsHangTile(Pos.x, Pos.y); }

	bool IsPlatform(float x, float y);

	int CheckPoint(float x, float y, bool IncludeDeath = false, bool Down = true, bool IncludeBlocks = true)
	{
		return SolidState(round_to_int(x), round_to_int(y), IncludeDeath, Down, IncludeBlocks);
	}
	bool CheckPoint(vec2 Pos, bool Down = true, bool IncludeBlocks = true)
	{
		return CheckPoint(Pos.x, Pos.y, false, Down, IncludeBlocks);
	}

	bool CheckBlocks(float x, float y) { return GetBlock(round_to_int(x), round_to_int(y)); }
	bool CheckBlocks(vec2 Pos) { return CheckBlocks(Pos.x, Pos.y); }

	int GetCollisionAt(float x, float y, bool Down = true, bool IncludeBlocks = true)
	{
		return GetTile(round_to_int(x), round_to_int(y), Down, IncludeBlocks);
	}

	int IsInFluid(float x, float y);

	bool CanBuildBlock(int x, int y);
	bool CanBuildBlock(vec2 Pos) { return CanBuildBlock(round_to_int(Pos.x), round_to_int(Pos.y)); }

	int GetWidth() { return m_Width; };
	int GetHeight() { return m_Height; };
	int FastIntersectLine(vec2 Pos0, vec2 Pos1);
	int IntersectLine(vec2 Pos0,
					  vec2 Pos1,
					  vec2 *pOutCollision,
					  vec2 *pOutBeforeCollision,
					  bool IncludeDeath = false,
					  bool IncludePlatforms = false,
					  bool IncludeBlocks = true);
	bool IntersectBlocks(vec2 Pos0, vec2 Pos1);

	vec2 Reflect(vec2 v, vec2 n);
	vec2 WallReflect(vec2 Pos, vec2 Direction, int Collision);

	// return bounced (true) or not (false)
	bool MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces, bool IgnoreCollision = false);
	void
	MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool check_speed = true, bool Down = false);
	int TestBox(vec2 Pos, vec2 Size, bool Down = false);

	// MapGen
	bool ClearTileLayer(int group, int layer);
	bool ModifTile(ivec2 pos, int group, int layer, int tile, int flags, int reserved);
};

#endif
