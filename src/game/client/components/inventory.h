#ifndef GAME_CLIENT_COMPONENTS_INVENTORY_H
#define GAME_CLIENT_COMPONENTS_INVENTORY_H
#include <base/math.h>
#include <base/vmath.h>
#include <game/client/component.h>
#include "inventory_logic.h"

class CInventory : public CComponent
{
	void RenderMouse();
	void DrawSidebar(const struct CNetObj_Shop *pShop);
	const struct CNetObj_Shop *NearbyShop();
	int ForgeMode();
	bool ForgeScreenNear();
	void ClearForgeSelection();
	void SubmitForge();
	void ActivateSelection();
	void RequestDrop();
	void SetTab(int Tab);
	int TabItemCount() const;
	void ResetInteractionState();
	void Close();
	
	bool m_WasActive;
	bool m_Active;
	bool m_Render;
	float m_AppearAmount;
	int64 m_LastAnimationTime;
	float AppearanceScale() const;

	vec2 m_SelectorMouse;
	vec2 m_WorldMouse;

	int m_Tab;
	int m_SelectedSlot;
	int m_KeyboardFocus;
	int64 m_LastClickTime;
	int m_LastClickSlot;
	int m_DropConfirmSlot;
	int64 m_DropConfirmDeadline;
	int m_ShopConfirmSlot;
	bool m_DebugVisible;
	int m_DebugTab;
	
	static void ConKeyInventory(IConsole::IResult *pResult, void *pUserData);
	static void ConInventoryRoll(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugInventory(IConsole::IResult *pResult, void *pUserData);
	
	bool m_Mouse1;
	bool m_MouseTrigger;
	bool m_Mouse1Loaded;
	
	vec2 m_MoveStartPos;
	bool m_Moved;
	bool m_MoveTrigger;
	
	int m_WantedTab;
	
	int m_DragItem;
	void Drop(int Slot);
	void Swap(int Item1, int Item2);

	int m_ForgeTargetSlot;
	int m_ForgeMaterialSlot;
	bool m_ForgePending;
	int m_ForgeLastResult;
	int m_ForgeResultEndTick;

	void InventoryRoll(bool All);
	
	bool m_StupidLock;
	
	bool m_Minimized;
	
public:
	CInventory();

	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnMouseMove(float x, float y);
	virtual bool OnInput(IInput::CEvent Event);
	
	bool IsVisible() const { return m_Render || m_Active; }
};

#endif
