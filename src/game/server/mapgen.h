#ifndef GAME_MAPGEN_H
#define GAME_MAPGEN_H

#include <engine/storage.h>
#include <game/layers.h>
#include <game/collision.h>

#include <base/tl/array.h>

class CMapGen
{
	IStorage *m_pStorage;
	IStorage *Storage() const { return m_pStorage; }

	class CLayers *m_pLayers;
	CCollision *m_pCollision;

	void GenerateLevel();
	void GeneratePVPLevel();
	void GenerateRoamLevel();
	void ExpandEscapeTowerCanvas();
	void ExpandExtractMazeCanvas();
	void FitTutorialCanvas();

	void WriteLayers(class CGenLayer *pTiles);
	void WriteBackground(class CGenLayer *pTiles);
	void WriteBase(class CGenLayer *pTiles, int BaseNum, ivec2 Pos, float Size);

	void Mirror(class CGenLayer *pTiles);

	void GenerateEnd(class CGenLayer *pTiles);

	void GenerateShop(class CGenLayer *pTiles);
	void GenerateScreen(class CGenLayer *pTiles);
	void GeneratePowerupper(class CGenLayer *pTiles);
	bool GenerateSwitch(class CGenLayer *pTiles);
	bool GenerateReactor(class CGenLayer *pTiles);
	void GenerateTurretStand(class CGenLayer *pTiles);
	void GenerateTurret(class CGenLayer *pTiles);
	void GenerateTeslacoil(class CGenLayer *pTiles);
	void GenerateBarrel(class CGenLayer *pTiles);
	void GenerateLightningWall(class CGenLayer *pTiles);
	void GenerateMine(class CGenLayer *pTiles);
	void GenerateWalker(class CGenLayer *pTiles);
	void GenerateStarDroid(class CGenLayer *pTiles);
	void GenerateCrawlerDroid(class CGenLayer *pTiles);
	void GenerateBossCrawlerDroid(class CGenLayer *pTiles);
	void GenerateEnemySpawn(class CGenLayer *pTiles);
	void GenerateBossEnemySpawn(class CGenLayer *pTiles);
	void GenerateExtractZone(class CGenLayer *pTiles);
	void GenerateHearts(class CGenLayer *pTiles);
	void GenerateAmmo(class CGenLayer *pTiles);
	void GenerateArmor(class CGenLayer *pTiles);
	void GenerateAcid(class CGenLayer *pTiles);

	void GenerateConveyorBelt(class CGenLayer *pTiles);
	void GenerateHangables(class CGenLayer *pTiles);

	void GenerateSawblade(class CGenLayer *pTiles);
	void GenerateFiretrap(class CGenLayer *pTiles);
	void GenerateDeathray(class CGenLayer *pTiles);

	void GenerateWeapon(class CGenLayer *pTiles, int Weapon);

	void ModifTile(ivec2 Pos, int Layer, int Tile, int Flags = 0);

	// auto mapper
	struct CPosRule
	{
		int m_X;
		int m_Y;
		int m_Value;
		bool m_IndexValue;
		int m_Offset;

		enum
		{
			EMPTY = 0,
			FULL
		};
	};

	struct CIndexRule
	{
		int m_ID;
		array<CPosRule> m_aRules;
		int m_Flag;
		int m_RandomValue;
		int m_YDivisor;
		int m_YRemainder;
		bool m_BaseTile;
	};

	struct CConfiguration
	{
		array<CIndexRule> m_aIndexRules;
		char m_aName[128] = {};
	};

	array<CConfiguration> m_lConfigs;
	bool m_FileLoaded;
	bool m_HasModularInfo;
	CMapModularInfo m_ModularInfo;
	int m_aModularRules[6 * 4];
	bool m_HasPathInfo;
	CMapPathInfo m_PathInfo;
	CMapPathPlacement m_aPathPlacements[64];

	void Load(const char *pTileName);
	void Proceed(class CGenLayer *pTiles, int ConfigID);
	void FitRoamAtlasCanvas();

	int ConfigNamesNum() { return m_lConfigs.size(); }
	const char *GetConfigName(int Index);

	const bool IsLoaded() { return m_FileLoaded; }

  public:
	CMapGen();
	~CMapGen();

	void FillMap();
	void Init(CLayers *pLayers, CCollision *pCollision, IStorage *pStorage);
	const CMapModularInfo *GetModularInfo() const { return m_HasModularInfo ? &m_ModularInfo : 0; }
	const int *GetModularRules() const { return m_HasModularInfo ? m_aModularRules : 0; }
	const CMapPathInfo *GetPathInfo() const { return m_HasPathInfo ? &m_PathInfo : 0; }
	const CMapPathPlacement *GetPathPlacements() const { return m_HasPathInfo ? m_aPathPlacements : 0; }
};

#endif
