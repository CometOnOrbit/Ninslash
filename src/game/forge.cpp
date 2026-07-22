#include "forge.h"

#include <cstdint>

namespace
{
bool IsRanged(const CWeaponDefinition &Definition)
{
	// TODO： This is a temporary hack to allow the forge to work with ranged weapons. It should be replaced with a proper check for ranged weapon types.
	return Definition.m_Kind == EWeaponDefinitionKind::Modular && Definition.m_Part1 >= PART1_BASE1 && Definition.m_Part1 <= PART1_BASE6;
}

bool IsMelee(const CWeaponDefinition &Definition)
{
	return Definition.m_Kind == EWeaponDefinitionKind::Modular && Definition.m_Part1 >= PART1_MELEE && Definition.m_Part1 <= PART1_SPIN;
}

bool SameCategory(const CWeaponDefinition &Target, const CWeaponDefinition &Material)
{
	return (IsRanged(Target) && IsRanged(Material)) || (IsMelee(Target) && IsMelee(Material));
}

bool UpgradeProduct(const CWeaponSpec &Target, const CWeaponDefinition &Definition, bool Supercharge, CWeaponSpec *pProduct)
{
	int Level = Target.m_Level;
	if(Supercharge)
	{
		if(Definition.m_Kind == EWeaponDefinitionKind::Static && Definition.m_StaticType == SW_UPGRADE)
			return false;
		if(Definition.m_MaxLevel <= 0 || Level <= Definition.m_MaxLevel)
			return false;
		if(Definition.m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
		{
			if(Level > Definition.m_MaxLevel + WEAPON_HIGH_TIER_SUPERCHARGE_BONUS)
				return false;
			Level += WEAPON_HIGH_TIER_SUPERCHARGE_STEP;
		}
		else
		{
			if(Level > Definition.m_MaxLevel + WEAPON_LOW_TIER_SUPERCHARGE_BONUS)
				return false;
			Level += WEAPON_LOW_TIER_SUPERCHARGE_STEP;
		}
	}
	else if(Definition.m_Kind == EWeaponDefinitionKind::Static && Definition.m_StaticType == SW_UPGRADE)
	{
		if(Level >= WEAPON_UPGRADE_SUPERCHARGE_LEVEL)
			return false;
		Level = WEAPON_UPGRADE_SUPERCHARGE_LEVEL;
	}
	else
	{
		if(Definition.m_MaxLevel <= 0 || Level > Definition.m_MaxLevel)
			return false;
		if(Definition.m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL && Level == Definition.m_MaxLevel)
			++Level;
		++Level;
	}

	if(Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return false;
	*pProduct = {Target.m_DefinitionId, static_cast<uint8_t>(Level)};
	return CWeaponCatalog::IsValidSpec(*pProduct);
}
}

int CForge::Cost(int BaseCost, int LevelCost, int TargetLevel, int MaterialLevel)
{
	const int64_t LevelSum = static_cast<int64_t>(TargetLevel) + static_cast<int64_t>(MaterialLevel);
	const int64_t Value = static_cast<int64_t>(BaseCost) + static_cast<int64_t>(LevelCost) * LevelSum;
	if(Value < 0)
		return 0;
	if(Value > FORGE_MAX_COST)
		return FORGE_MAX_COST;
	return static_cast<int>(Value);
}

int CForge::MapAmmo(int TargetAmmo, int TargetMaxAmmo, int ProductMaxAmmo)
{
	if(TargetMaxAmmo <= 0 || ProductMaxAmmo <= 0)
		return 0;
	const int ClampedAmmo = TargetAmmo < 0 ? 0 : (TargetAmmo > TargetMaxAmmo ? TargetMaxAmmo : TargetAmmo);
	return static_cast<int>(static_cast<int64_t>(ClampedAmmo) * ProductMaxAmmo / TargetMaxAmmo);
}

CForgeRecipe CForge::Resolve(const CWeaponSpec &Target, const CWeaponSpec &Material,
	int TargetAmmo, int BaseCost, int LevelCost)
{
	CForgeRecipe Recipe;
	CResolvedWeaponProfile TargetProfile;
	CResolvedWeaponProfile MaterialProfile;
	if(!CWeaponCatalog::TryResolve(Target, &TargetProfile) || !CWeaponCatalog::TryResolve(Material, &MaterialProfile))
		return Recipe;

	Recipe.m_Cost = Cost(BaseCost, LevelCost, Target.m_Level, Material.m_Level);
	const CWeaponDefinition &TargetDefinition = TargetProfile.m_Definition;
	const CWeaponDefinition &MaterialDefinition = MaterialProfile.m_Definition;

	if(MaterialDefinition.m_Kind == EWeaponDefinitionKind::Static && MaterialDefinition.m_StaticType == SW_UPGRADE)
	{
		Recipe.m_Operation = FORGEOP_UPGRADE;
		if((Material.m_Level != 0 && Material.m_Level != WEAPON_UPGRADE_SUPERCHARGE_LEVEL) ||
			!UpgradeProduct(Target, TargetDefinition, Material.m_Level == WEAPON_UPGRADE_SUPERCHARGE_LEVEL, &Recipe.m_Product))
			return Recipe;
	}
	else if(TargetDefinition.m_Kind == EWeaponDefinitionKind::Modular && MaterialDefinition.m_Kind == EWeaponDefinitionKind::Modular &&
		TargetDefinition.m_Part1 == PART1_MELEE && MaterialDefinition.m_Part1 == PART1_MELEE &&
		TargetDefinition.m_Part2 == MaterialDefinition.m_Part2)
	{
		Recipe.m_Operation = FORGEOP_SPIN;
		Recipe.m_Product = CWeaponCatalog::Modular(PART1_SPIN, TargetDefinition.m_Part2, (Target.m_Level + Material.m_Level) / 2);
	}
	else if(SameCategory(TargetDefinition, MaterialDefinition))
	{
		Recipe.m_Operation = FORGEOP_REPLACE_PART2;
		Recipe.m_Product = CWeaponCatalog::Modular(TargetDefinition.m_Part1, MaterialDefinition.m_Part2,
			(Target.m_Level + Material.m_Level) / 2);
	}
	else
		return Recipe;

	if(!Recipe.m_Product.IsValid())
		return Recipe;
	CResolvedWeaponProfile ProductProfile;
	if(!CWeaponCatalog::TryResolve(Recipe.m_Product, &ProductProfile))
	{
		Recipe.m_Product = {};
		return Recipe;
	}
	Recipe.m_ProductMaxAmmo = ProductProfile.m_Combat.m_MaxAmmo;
	Recipe.m_ProductAmmo = MapAmmo(TargetAmmo, TargetProfile.m_Combat.m_MaxAmmo, Recipe.m_ProductMaxAmmo);
	if(Recipe.m_Product == Target)
	{
		Recipe.m_Result = FORGERESULT_NO_CHANGE;
		return Recipe;
	}
	Recipe.m_Result = FORGERESULT_SUCCESS;
	return Recipe;
}

// TODO: rewrite it
bool CForge::Validate()
{
	const CWeaponSpec Ranged = CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 3);
	const CWeaponSpec RangedOdd = CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2, 4);
	const CWeaponSpec Melee = CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1, 2);
	if(Resolve({}, RangedOdd, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(Ranged, {}, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(Ranged, Melee, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(CWeaponCatalog::Static(SW_BAZOOKA), RangedOdd, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(Ranged, Ranged, 0, 5, 2).m_Result != FORGERESULT_NO_CHANGE ||
		Cost(5, 2, 3, 4) != 19 || Cost(-20, 0, 0, 0) != 0 || Cost(900, 100, 1, 1) != FORGE_MAX_COST ||
		MapAmmo(3, 7, 10) != 4 || MapAmmo(3, 0, 10) != 0 || MapAmmo(3, 7, 0) != 0)
		return false;
	//TODO
	for(int TargetPart1 = PART1_BASE1; TargetPart1 <= PART1_BASE6; ++TargetPart1)
		for(int TargetPart2 = PART2_BARREL1; TargetPart2 <= PART2_RAIL; ++TargetPart2)
			for(int MaterialPart1 = PART1_BASE1; MaterialPart1 <= PART1_BASE6; ++MaterialPart1)
				for(int MaterialPart2 = PART2_BARREL1; MaterialPart2 <= PART2_RAIL; ++MaterialPart2)
				{
					const CWeaponSpec Target = CWeaponCatalog::Modular(TargetPart1, TargetPart2, 3);
					const CWeaponSpec Material = CWeaponCatalog::Modular(MaterialPart1, MaterialPart2, 4);
					const CForgeRecipe Part2 = Resolve(Target, Material, 0, 5, 2);
					CWeaponDefinition Product;
					const int ExpectedPart2Result = MaterialPart2 == TargetPart2 ? FORGERESULT_NO_CHANGE : FORGERESULT_SUCCESS;
					if(Part2.m_Result != ExpectedPart2Result || Part2.m_Operation != FORGEOP_REPLACE_PART2 ||
						!CWeaponCatalog::TryGetDefinition(Part2.m_Product.m_DefinitionId, &Product) ||
						Product.m_Part1 != TargetPart1 || Product.m_Part2 != MaterialPart2 || Part2.m_Product.m_Level != 3)
						return false;
				}

	for(int TargetPart1 = PART1_MELEE; TargetPart1 <= PART1_SPIN; ++TargetPart1)
		for(int TargetPart2 = PART2_MELEE1; TargetPart2 <= PART2_MELEE6; ++TargetPart2)
			for(int MaterialPart1 = PART1_MELEE; MaterialPart1 <= PART1_SPIN; ++MaterialPart1)
				for(int MaterialPart2 = PART2_MELEE1; MaterialPart2 <= PART2_MELEE6; ++MaterialPart2)
				{
					const CWeaponSpec Target = CWeaponCatalog::Modular(TargetPart1, TargetPart2, 1);
					const CWeaponSpec Material = CWeaponCatalog::Modular(MaterialPart1, MaterialPart2, 4);
					const CForgeRecipe Recipe = Resolve(Target, Material, 0, 5, 2);
					CWeaponDefinition Product;
					const bool IsSpin = TargetPart1 == PART1_MELEE && MaterialPart1 == PART1_MELEE && TargetPart2 == MaterialPart2;
					if(Recipe.m_Result != FORGERESULT_SUCCESS ||
						Recipe.m_Operation != (IsSpin ? FORGEOP_SPIN : FORGEOP_REPLACE_PART2) ||
						!CWeaponCatalog::TryGetDefinition(Recipe.m_Product.m_DefinitionId, &Product) ||
						Product.m_Part1 != (IsSpin ? PART1_SPIN : TargetPart1) ||
						Product.m_Part2 != MaterialPart2 || Recipe.m_Product.m_Level != 2)
						return false;
				}

	const CWeaponSpec SpinMaterial = CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1, 5);
	const CForgeRecipe Spin = Resolve(Melee, SpinMaterial, 0, 5, 2);
	const CForgeRecipe MeleePart2 = Resolve(Melee, CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE2), 0, 5, 2);
	CWeaponDefinition SpinDefinition;
	CWeaponDefinition MeleePart2Definition;
	if(Spin.m_Result != FORGERESULT_SUCCESS || Spin.m_Operation != FORGEOP_SPIN || Spin.m_Product.m_Level != 3 ||
		!CWeaponCatalog::TryGetDefinition(Spin.m_Product.m_DefinitionId, &SpinDefinition) || SpinDefinition.m_Part1 != PART1_SPIN ||
		MeleePart2.m_Result != FORGERESULT_SUCCESS || MeleePart2.m_Operation != FORGEOP_REPLACE_PART2 ||
		!CWeaponCatalog::TryGetDefinition(MeleePart2.m_Product.m_DefinitionId, &MeleePart2Definition) ||
		MeleePart2Definition.m_Part1 != PART1_MELEE || MeleePart2Definition.m_Part2 != PART2_MELEE2)
		return false;

	const CWeaponSpec Upgrade = CWeaponCatalog::Static(SW_UPGRADE, 0);
	const CWeaponSpec SuperUpgrade = CWeaponCatalog::Static(SW_UPGRADE, WEAPON_UPGRADE_SUPERCHARGE_LEVEL);
	const CWeaponSpec UpgradeTarget = CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 0);
	const CForgeRecipe Overcharged = Resolve(UpgradeTarget, Upgrade, 0, 5, 2);
	const CForgeRecipe AtCap = Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 4), Upgrade, 0, 5, 2);
	const CForgeRecipe Supercharged = Resolve(AtCap.m_Product, SuperUpgrade, 0, 5, 2);
	const CForgeRecipe HighBoundary = Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 8), SuperUpgrade, 0, 5, 2);
	const CForgeRecipe LowAtCap = Resolve(CWeaponCatalog::Static(SW_BAZOOKA, 2), Upgrade, 0, 5, 2);
	const CForgeRecipe LowSuper1 = Resolve(LowAtCap.m_Product, SuperUpgrade, 0, 5, 2);
	const CForgeRecipe LowSuper2 = Resolve(LowSuper1.m_Product, SuperUpgrade, 0, 5, 2);
	if(Overcharged.m_Result != FORGERESULT_SUCCESS || Overcharged.m_Operation != FORGEOP_UPGRADE || Overcharged.m_Product.m_Level != 1 ||
		AtCap.m_Result != FORGERESULT_SUCCESS || AtCap.m_Product.m_Level != 6 ||
		Supercharged.m_Result != FORGERESULT_SUCCESS || Supercharged.m_Product.m_Level != 8 ||
		HighBoundary.m_Result != FORGERESULT_SUCCESS || HighBoundary.m_Product.m_Level != 10 ||
		Resolve(HighBoundary.m_Product, SuperUpgrade, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		LowAtCap.m_Result != FORGERESULT_SUCCESS || LowAtCap.m_Product.m_Level != 3 ||
		LowSuper1.m_Result != FORGERESULT_SUCCESS || LowSuper1.m_Product.m_Level != 4 ||
		LowSuper2.m_Result != FORGERESULT_SUCCESS || LowSuper2.m_Product.m_Level != 5 ||
		Resolve(LowSuper2.m_Product, SuperUpgrade, 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(UpgradeTarget, CWeaponCatalog::Static(SW_UPGRADE, 5), 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE ||
		Resolve(UpgradeTarget, CWeaponCatalog::Static(SW_TOOL), 0, 5, 2).m_Result != FORGERESULT_INVALID_RECIPE)
		return false;

	return true;
}
