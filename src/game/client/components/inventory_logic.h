#ifndef GAME_CLIENT_COMPONENTS_INVENTORY_LOGIC_H
#define GAME_CLIENT_COMPONENTS_INVENTORY_LOGIC_H

#include <base/math.h>

namespace InventoryLogic
{
constexpr int NUM_SLOTS = 12;
constexpr int NUM_EQUIPMENT_SLOTS = 4;
constexpr int GRID_COLUMNS = 4;

enum ETab
{
	TAB_INVENTORY,
	TAB_FORGE,
	TAB_SHOP,
	NUM_TABS,
};

inline bool ForgeTabVisible(int ForgeMode)
{
	return ForgeMode != 0;
}

inline bool ForgeUsable(int ForgeMode, bool ScreenNear)
{
	return ForgeMode == 1 || (ForgeMode == 2 && ScreenNear);
}

struct CLayout
{
	float m_X;
	float m_Y;
	float m_W;
	float m_H;
};

inline CLayout
SidebarLayout(float ScreenWidth, float ScreenHeight, float UiScale, float AppearAmount, bool Wide = false)
{
	const float Scale = clamp(UiScale, 1.0f, 1.5f);
	const float BaseWidth = Wide ? 190.0f : 154.0f;
	const float Width = clamp(BaseWidth * Scale, BaseWidth, min(190.0f, ScreenWidth - 24.0f));
	const float Margin = 8.0f;
	const float Slide = 18.0f * (1.0f - clamp(AppearAmount, 0.0f, 1.0f));
	return {ScreenWidth - Width - Margin + Slide, Margin, Width, ScreenHeight - Margin * 2.0f};
}

inline CLayout
BottomOverlayLayout(float ScreenWidth, float ScreenHeight, float UiScale, float AppearAmount, bool Workbench)
{
	const float Scale = clamp(UiScale, 1.0f, 1.5f);
	const float BaseWidth = Workbench ? 320.0f : 260.0f;
	const float Width = min(BaseWidth * Scale, min(BaseWidth, ScreenWidth - 24.0f));
	const float Height = min(Workbench ? 284.0f : 235.0f, ScreenHeight - 16.0f);
	const float Margin = 8.0f;
	const float Slide = 18.0f * (1.0f - clamp(AppearAmount, 0.0f, 1.0f));
	return {(ScreenWidth - Width) * 0.5f, ScreenHeight - Height - Margin + Slide, Width, Height};
}

inline int NavigateGrid(int Current, int Count, int Columns, int DeltaX, int DeltaY)
{
	if(Count <= 0 || Columns <= 0)
		return -1;
	Current = clamp(Current, 0, Count - 1);
	const int Row = Current / Columns;
	const int Column = Current % Columns;
	const int Rows = (Count + Columns - 1) / Columns;
	int NewRow = (Row + DeltaY + Rows) % Rows;
	int NewColumn = (Column + DeltaX + Columns) % Columns;
	int Result = NewRow * Columns + NewColumn;
	while(Result >= Count)
	{
		NewColumn = (NewColumn + Columns - 1) % Columns;
		Result = NewRow * Columns + NewColumn;
	}
	return Result;
}

inline int NextAvailableTab(int Current, int Direction, bool ForgeAvailable, bool ShopAvailable)
{
	for(int Step = 1; Step <= NUM_TABS; ++Step)
	{
		const int Candidate = (Current + Direction * Step + NUM_TABS * 2) % NUM_TABS;
		if((Candidate != TAB_FORGE || ForgeAvailable) && (Candidate != TAB_SHOP || ShopAvailable))
			return Candidate;
	}
	return TAB_INVENTORY;
}

inline int EquipTarget(int SelectedSlot, int CurrentWeaponSlot)
{
	if(SelectedSlot < 0 || SelectedSlot >= NUM_SLOTS)
		return -1;
	if(SelectedSlot < NUM_EQUIPMENT_SLOTS)
		return SelectedSlot;
	return CurrentWeaponSlot >= 0 && CurrentWeaponSlot < NUM_EQUIPMENT_SLOTS ? CurrentWeaponSlot : 0;
}

inline bool DropConfirmationActive(int ConfirmSlot, int SelectedSlot, long long Now, long long Deadline)
{
	return ConfirmSlot == SelectedSlot && SelectedSlot >= 0 && Now <= Deadline;
}

inline bool ShouldDropOutsideInventory(
	int DraggedSlot, bool DraggedItemValid, bool Moved, int HoveredSlot, bool CursorInsidePanel)
{
	return DraggedSlot >= 0 && DraggedSlot < NUM_SLOTS && DraggedItemValid && Moved && HoveredSlot < 0 &&
		   !CursorInsidePanel;
}

struct CForgeSlots
{
	int m_Target;
	int m_Material;
};

enum EForgeDropTarget
{
	FORGE_DROP_NONE,
	FORGE_DROP_TARGET,
	FORGE_DROP_MATERIAL,
};

inline CForgeSlots AssignForgeSlot(CForgeSlots Slots, int SelectedSlot, bool AsTarget)
{
	if(SelectedSlot < 0 || SelectedSlot >= NUM_SLOTS)
		return Slots;
	if(AsTarget)
	{
		if(SelectedSlot == Slots.m_Material)
			return Slots;
		Slots.m_Target = Slots.m_Target == SelectedSlot ? -1 : SelectedSlot;
	}
	else
	{
		if(SelectedSlot == Slots.m_Target)
			return Slots;
		Slots.m_Material = Slots.m_Material == SelectedSlot ? -1 : SelectedSlot;
	}
	return Slots;
}

inline CForgeSlots DropForgeItem(CForgeSlots Slots, int DraggedSlot, EForgeDropTarget DropTarget)
{
	if(DropTarget == FORGE_DROP_TARGET)
		return AssignForgeSlot(Slots, DraggedSlot, true);
	if(DropTarget == FORGE_DROP_MATERIAL)
		return AssignForgeSlot(Slots, DraggedSlot, false);
	return Slots;
}
} // namespace InventoryLogic

#endif
