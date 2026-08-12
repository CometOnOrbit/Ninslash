#include <game/client/components/inventory_logic.h>
#include <game/client/components/hud_layout.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

static void Check(bool Condition)
{
	if(!Condition)
		std::abort();
}

int main()
{
	using namespace InventoryLogic;
	std::ifstream Variables("src/game/variables.h");
	const std::string VariableText((std::istreambuf_iterator<char>(Variables)), std::istreambuf_iterator<char>());
	const size_t ForgeModeMacro = VariableText.find("MACRO_CONFIG_INT(SvForgeMode");
	Check(ForgeModeMacro != std::string::npos);
	const size_t ForgeModeName = VariableText.find("sv_forge_mode", ForgeModeMacro);
	Check(ForgeModeName != std::string::npos);
	const size_t ForgeModeDefault = VariableText.find(',', ForgeModeName);
	Check(ForgeModeDefault != std::string::npos);
	Check(std::strtol(VariableText.c_str() + ForgeModeDefault + 1, nullptr, 10) == 1);

	const CHammerLayout Hammer = HammerLayout(400.0f, 300.0f, 1.0f, true);
	Check(Hammer.m_Bag.m_W == HudLayout::CombatBarWidth + BagPad * 2.0f);
	Check(Hammer.m_Bag.m_X == HudLayout::CombatBarLeft(400.0f) - BagPad);
	Check(Hammer.m_Forge.m_W == Hammer.m_Detail.m_W);
	Check(Hammer.m_Status.m_W == Hammer.m_Detail.m_W);
	Check(Hammer.m_Resource.m_W == Hammer.m_Detail.m_W);
	Check(Hammer.m_Detail.m_W > Hammer.m_Bag.m_W);
	Check(Hammer.m_Forge.m_Y + Hammer.m_Forge.m_H + HammerPartGap <= Hammer.m_Status.m_Y + 0.01f);
	Check(Hammer.m_Status.m_Y + Hammer.m_Status.m_H + HammerPartGap <= Hammer.m_Resource.m_Y + 0.01f);
	Check(Hammer.m_Resource.m_Y + Hammer.m_Resource.m_H + HammerPartGap <= Hammer.m_Detail.m_Y + 0.01f);
	Check(Hammer.m_Detail.m_Y + Hammer.m_Detail.m_H + HammerPartGap <= Hammer.m_Bag.m_Y + 0.01f);
	Check(Hammer.m_Bag.m_Y + Hammer.m_Bag.m_H + TrayGapAboveCombat <= HudLayout::CombatBarTop(300.0f) + 0.01f);
	// Grid stays combat-aligned inside padded bag.
	Check(Hammer.m_Bag.m_X + BagPad == HudLayout::CombatBarLeft(400.0f));
	Check(Hammer.m_Bag.m_X + BagPad + HudLayout::CombatBarWidth <= Hammer.m_Bag.m_X + Hammer.m_Bag.m_W + 0.01f);

	const CHammerLayout NoForge = HammerLayout(400.0f, 300.0f, 1.0f, false);
	Check(NoForge.m_Forge.m_W == 0.0f);
	Check(NoForge.m_Status.m_W == 0.0f);
	Check(NoForge.m_Resource.m_W == NoForge.m_Detail.m_W);
	Check(NoForge.m_Detail.m_W >= NoForge.m_Bag.m_W);
	Check(NoForge.m_Resource.m_Y + NoForge.m_Resource.m_H + HammerPartGap <= NoForge.m_Detail.m_Y + 0.01f);
	Check(ResolveOpenTab(false, 2, true) == TAB_INVENTORY);
	Check(ForgeUsable(1, false));
	return 0;
}
