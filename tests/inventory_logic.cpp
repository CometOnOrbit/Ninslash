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
	std::ifstream DefaultConfig("cfg/default.cfg");
	const std::string DefaultConfigText(
		(std::istreambuf_iterator<char>(DefaultConfig)), std::istreambuf_iterator<char>());
	Check(DefaultConfigText.find("sv_forge_mode 1") != std::string::npos);
	const CLayout Small = SidebarLayout(800.0f, 600.0f, 1.0f, 1.0f);
	Check(Small.m_W >= 154.0f && Small.m_W <= 190.0f && Small.m_X + Small.m_W <= 800.0f);
	const CLayout Scaled = SidebarLayout(1280.0f, 720.0f, 1.5f, 0.0f);
	Check(Scaled.m_W == 190.0f && Scaled.m_X > Small.m_X);
	const CLayout ForgeLayout = SidebarLayout(800.0f, 600.0f, 1.0f, 1.0f, true);
	Check(ForgeLayout.m_W == 190.0f);
	Check(ForgeLayout.m_X + ForgeLayout.m_W == Small.m_X + Small.m_W);
	const CLayout InventoryOverlay = BottomOverlayLayout(400.0f, 300.0f, 1.0f, 1.0f, false);
	Check(InventoryOverlay.m_W == 260.0f && InventoryOverlay.m_H == 235.0f);
	Check(InventoryOverlay.m_X == 70.0f && InventoryOverlay.m_Y + InventoryOverlay.m_H == 292.0f);
	const CLayout WorkbenchOverlay = BottomOverlayLayout(400.0f, 300.0f, 1.0f, 1.0f, true);
	Check(WorkbenchOverlay.m_W == 320.0f && WorkbenchOverlay.m_H == 284.0f);
	Check(WorkbenchOverlay.m_X == 40.0f && WorkbenchOverlay.m_Y == 8.0f);
	Check(NavigateGrid(0, 12, 4, -1, 0) == 3);
	Check(NavigateGrid(3, 12, 4, 1, 0) == 0);
	Check(NavigateGrid(10, 12, 4, 0, 1) == 2);
	Check(NavigateGrid(8, 9, 3, 1, 0) == 6);
	Check(NextAvailableTab(TAB_INVENTORY, 1, false, false) == TAB_INVENTORY);
	Check(NextAvailableTab(TAB_INVENTORY, 1, true, false) == TAB_FORGE);
	Check(NextAvailableTab(TAB_INVENTORY, -1, true, true) == TAB_SHOP);
	Check(!ForgeTabVisible(0));
	Check(ForgeTabVisible(1) && ForgeTabVisible(2));
	Check(ForgeUsable(1, false));
	Check(!ForgeUsable(2, false) && ForgeUsable(2, true));
	Check(HudLayout::BottomStatusTop(300.0f) == 262.0f);
	Check(HudLayout::BottomStatusBottom(300.0f) == 296.0f);
	Check(HudLayout::VitalCoreHeight == 50.0f);
	Check(HudLayout::VitalCoreTop(300.0f) == 246.0f);
	Check(HudLayout::CombatBarTop(300.0f) == 270.0f);
	Check(HudLayout::ObjectiveTop == 82.0f);
	Check(HudLayout::BuildEffectsLeft == 10.0f);
	Check(HudLayout::BuildEffectsRows(1) == 1);
	Check(HudLayout::BuildEffectsRows(4) == 2);
	Check(HudLayout::BuildEffectsHeight(4) == 36.0f);
	Check(HudLayout::LowHealthThreshold == 60);
	Check(HudLayout::LowHealthAmount(60) == 0.0f);
	Check(HudLayout::LowHealthAmount(59) > 0.0f);
	Check(HudLayout::LowHealthAmount(40) > HudLayout::LowHealthAmount(59));
	Check(HudLayout::LowHealthAmount(20) > HudLayout::LowHealthAmount(40));
	Check(HudLayout::LowHealthAmount(0) == 1.0f);
	Check(HudLayout::CriticalHealthThreshold == 20);
	Check(HudLayout::CriticalHealthAmount(20) == 0.0f);
	Check(HudLayout::CriticalHealthAmount(10) == 0.5f);
	Check(HudLayout::CriticalHealthAmount(0) == 1.0f);
	Check(HudLayout::VitalCombinedAmount(0.5f, 0.2f) > 0.69f &&
		HudLayout::VitalCombinedAmount(0.5f, 0.2f) < 0.71f);
	Check(HudLayout::VitalOverlapAmount(0.5f, 0.2f) == 0.0f);
	Check(HudLayout::VitalHealthOnlyEnd(0.5f, 0.2f) == 0.5f);
	Check(HudLayout::VitalOverlapAmount(0.8f, 0.3f) > 0.09f &&
		HudLayout::VitalOverlapAmount(0.8f, 0.3f) < 0.11f);
	Check(HudLayout::VitalHealthOnlyEnd(0.8f, 0.3f) > 0.69f &&
		HudLayout::VitalHealthOnlyEnd(0.8f, 0.3f) < 0.71f);
	Check(HudLayout::ChatAvoidsBottomStatus(300.0f));
	Check(HudLayout::ChatInputTop(300.0f) + HudLayout::ChatInputHeight + HudLayout::ChatStatusGap <=
		  HudLayout::VitalCoreTop(300.0f));
	Check(EquipTarget(2, 1) == 2);
	Check(EquipTarget(7, 3) == 3);
	Check(EquipTarget(7, -1) == 0);
	Check(EquipTarget(12, 0) == -1);
	Check(DropConfirmationActive(5, 5, 99, 100));
	Check(!DropConfirmationActive(5, 4, 99, 100));
	Check(!DropConfirmationActive(5, 5, 101, 100));
	Check(ShouldDropOutsideInventory(5, true, true, -1, false));
	Check(!ShouldDropOutsideInventory(5, true, false, -1, false));
	Check(!ShouldDropOutsideInventory(5, true, true, 6, false));
	Check(!ShouldDropOutsideInventory(5, true, true, -1, true));
	Check(!ShouldDropOutsideInventory(5, false, true, -1, false));
	Check(!ShouldDropOutsideInventory(12, true, true, -1, false));
	CForgeSlots Forge{-1, -1};
	Forge = AssignForgeSlot(Forge, 3, true);
	Check(Forge.m_Target == 3 && Forge.m_Material == -1);
	Forge = AssignForgeSlot(Forge, 7, false);
	Check(Forge.m_Target == 3 && Forge.m_Material == 7);
	Forge = AssignForgeSlot(Forge, 3, false);
	Check(Forge.m_Target == 3 && Forge.m_Material == 7);
	Forge = AssignForgeSlot(Forge, 7, false);
	Check(Forge.m_Target == 3 && Forge.m_Material == -1);
	Forge = DropForgeItem(Forge, 5, FORGE_DROP_TARGET);
	Check(Forge.m_Target == 5 && Forge.m_Material == -1);
	Forge = DropForgeItem(Forge, 8, FORGE_DROP_MATERIAL);
	Check(Forge.m_Target == 5 && Forge.m_Material == 8);
	Forge = DropForgeItem(Forge, 8, FORGE_DROP_TARGET);
	Check(Forge.m_Target == 5 && Forge.m_Material == 8);
	Forge = DropForgeItem(Forge, 2, FORGE_DROP_NONE);
	Check(Forge.m_Target == 5 && Forge.m_Material == 8);
	return 0;
}
