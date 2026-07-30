#ifndef GAME_FORGE_H
#define GAME_FORGE_H

#include <game/weapon_catalog.h>

constexpr float FORGE_SCREEN_RANGE = 128.0f;
constexpr int FORGE_MAX_COST = 999;

struct CForgeRecipe
{
	int m_Result = FORGERESULT_INVALID_RECIPE;
	int m_Operation = FORGEOP_AUTO;
	CWeaponSpec m_Product;
	int m_Cost = 0;
	int m_ProductAmmo = 0;
	int m_ProductMaxAmmo = 0;
	char m_aRecipeName[128] = {};
};

class CForge
{
  public:
	static int Cost(int BaseCost, int LevelCost, int TargetLevel, int MaterialLevel);
	static int MapAmmo(int TargetAmmo, int TargetMaxAmmo, int ProductMaxAmmo);
	static CForgeRecipe Resolve(const CWeaponSpec &Target,
								const CWeaponSpec &Material,
								int TargetAmmo,
								int BaseCost,
								int LevelCost,
								int MaterialAmmo = 0);
	static bool Validate();
};

#endif
