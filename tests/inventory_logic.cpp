#include <game/client/components/inventory_logic.h>

#include <cstdlib>

static void Check(bool Condition)
{
	if(!Condition)
		std::abort();
}

int main()
{
	using namespace InventoryLogic;
	const CLayout Small = SidebarLayout(800.0f, 600.0f, 1.0f, 1.0f);
	Check(Small.m_W >= 154.0f && Small.m_W <= 190.0f && Small.m_X + Small.m_W <= 800.0f);
	const CLayout Scaled = SidebarLayout(1280.0f, 720.0f, 1.5f, 0.0f);
	Check(Scaled.m_W == 190.0f && Scaled.m_X > Small.m_X);
	const CLayout ForgeLayout = SidebarLayout(800.0f, 600.0f, 1.0f, 1.0f, true);
	Check(ForgeLayout.m_W == 190.0f);
	Check(ForgeLayout.m_X + ForgeLayout.m_W == Small.m_X + Small.m_W);
	Check(NavigateGrid(0, 12, 4, -1, 0) == 3);
	Check(NavigateGrid(3, 12, 4, 1, 0) == 0);
	Check(NavigateGrid(10, 12, 4, 0, 1) == 2);
	Check(NavigateGrid(8, 9, 3, 1, 0) == 6);
	Check(NextAvailableTab(TAB_INVENTORY, 1, false, false) == TAB_INVENTORY);
	Check(NextAvailableTab(TAB_INVENTORY, 1, true, false) == TAB_FORGE);
	Check(NextAvailableTab(TAB_INVENTORY, -1, true, true) == TAB_SHOP);
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
