#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/keys.h>
#include <engine/input_processing.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h> // get_angle
#include <game/weapons.h>
#include <game/weapon_catalog.h>
#include <game/forge.h>
#include <game/buildables.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include <game/client/customstuff.h>
#include <game/client/components/controls.h>
#include <game/client/components/camera.h>
#include <game/client/components/effects.h>
#include <game/client/components/binds.h>
#include <game/client/components/build_placement.h>
#include <game/client/components/pve_roguelite.h>
#include <game/client/components/sounds.h>
#include "inventory.h"
#include "menus.h"
#include <game/client/weapon_rank_icon.h>

static CWeaponSpec ShopWeapon(const CNetObj_Shop *pShop, int Slot)
{
	CWeaponSpec Spec;
	switch(Slot)
	{
		case 0:
			CWeaponCatalog::TryFromProtocol(pShop->m_Item1DefinitionId, pShop->m_Item1Level, &Spec);
			break;
		case 1:
			CWeaponCatalog::TryFromProtocol(pShop->m_Item2DefinitionId, pShop->m_Item2Level, &Spec);
			break;
		case 2:
			CWeaponCatalog::TryFromProtocol(pShop->m_Item3DefinitionId, pShop->m_Item3Level, &Spec);
			break;
		case 3:
			CWeaponCatalog::TryFromProtocol(pShop->m_Item4DefinitionId, pShop->m_Item4Level, &Spec);
			break;
		case 4:
			CWeaponCatalog::TryFromProtocol(pShop->m_Item5DefinitionId, pShop->m_Item5Level, &Spec);
			break;
		default:
			break;
	}
	return Spec;
}

static bool WeaponDefinition(const CWeaponSpec &Spec, CWeaponDefinition *pDefinition)
{
	return Spec.IsValid() && CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, pDefinition);
}

static int ShopWeaponCost(const CNetObj_Shop *pShop, int Slot)
{
	CResolvedWeaponProfile Profile;
	return CWeaponCatalog::TryResolve(ShopWeapon(pShop, Slot), &Profile) ? Profile.m_Combat.m_Cost : 0;
}

static const char *s_TipText[NUM_STATIC_WEAPONS] = {"Repair tool",
													"Pistol",
													"Burst pistol",
													"Grenade",
													"Electric grenade",
													"Supply grenade",
													"Rocket launcher",
													"Ricochet gun",
													"Chainsaw",
													"Flamethrower",
													"Weapon upgrade",
													"Energy shield",
													"Respawn device",
													"Mask of regeneration",
													"Mask of speed",
													"Mask of protection",
													"Mask of plenty",
													"Mask of melee",
													"Invisibility device",
													"Electrowall",
													"Area Shield",
													"The Cure",
													"Cluster grenade",
													"Shuriken",
													"Zombie claw",
													"Bomb (for destroying reactors)",
													"Ball",
													"Flash grenade",
													"Blind grenade"};

static const char *WeaponDisplayName(const CWeaponSpec &Spec, char *pBuffer, int BufferSize)
{
	CWeaponDefinition Definition{};
	if(!WeaponDefinition(Spec, &Definition))
		return Localize("Unknown item");
	if(Definition.m_Custom)
		return Localize(Definition.m_aNameKey[0] ? Definition.m_aNameKey : Definition.m_aStableId);
	if(Definition.m_Kind == EWeaponDefinitionKind::Static)
		return Localize(s_TipText[Definition.m_StaticType]);
	str_format(pBuffer,
			   BufferSize,
			   "%s · %s",
			   Localize(CWeaponCatalog::Part1NameKey(Definition.m_Part1)),
			   Localize(CWeaponCatalog::Part2NameKey(Definition.m_Part2)));
	return pBuffer;
}

static const char *FiringModeName(int Type, bool FullAuto)
{
	if(FullAuto)
		return Localize("Automatic");
	switch(Type)
	{
		case WFT_MELEE:
			return Localize("Melee");
		case WFT_CHARGE:
			return Localize("Charge");
		case WFT_HOLD:
			return Localize("Automatic");
		case WFT_THROW:
			return Localize("Throw");
		case WFT_ACTIVATE:
			return Localize("Activate");
		default:
			return Localize("Semi-auto");
	}
}

CInventory::CInventory()
{
	m_DebugVisible = false;
	m_DebugTab = 0;
	OnReset();
	m_WantedTab = -1;
	m_StupidLock = false;
}

float CInventory::AppearanceScale() const
{
	const float Eased = 1.0f - (1.0f - m_AppearAmount) * (1.0f - m_AppearAmount) * (1.0f - m_AppearAmount);
	return 0.96f + 0.04f * Eased;
}

int CInventory::ForgeMode()
{
	return m_pClient->m_Snap.m_pGameInfoObj ? m_pClient->m_Snap.m_pGameInfoObj->m_ForgeMode : 0;
}

bool CInventory::ForgeScreenNear()
{
	if(!CustomStuff()->m_LocalAlive || !Client()->IsGameWorldActive())
		return false;
	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; ++i)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
		if(Item.m_Type != NETOBJTYPE_BUILDING)
			continue;
		const CNetObj_Building *pBuilding = (const CNetObj_Building *)pData;
		if(pBuilding->m_Type == BUILDING_SCREEN &&
		   distance(CustomStuff()->m_LocalPos, vec2(pBuilding->m_X, pBuilding->m_Y)) <= FORGE_SCREEN_RANGE)
			return true;
	}
	return false;
}

void CInventory::ClearForgeSelection()
{
	m_ForgeTargetSlot = -1;
	m_ForgeMaterialSlot = -1;
}

const CNetObj_Shop *CInventory::NearbyShop()
{
	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; ++i)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
		if(Item.m_Type != NETOBJTYPE_SHOP)
			continue;
		const CNetObj_Shop *pShop = (const CNetObj_Shop *)pData;
		if(abs(CustomStuff()->m_LocalPos.x - pShop->m_X) <= 100 && abs(CustomStuff()->m_LocalPos.y - pShop->m_Y) <= 100)
			return pShop;
	}
	return 0;
}

void CInventory::Close()
{
	m_Active = false;
	m_WasActive = false;
	m_Render = false;
	m_DragItem = -1;
	m_DropConfirmSlot = -1;
	CustomStuff()->m_Inventory = false;
}

void CInventory::SetTab(int Tab)
{
	if(Tab < 0 || Tab >= InventoryLogic::NUM_TABS)
		return;
	if(Tab == InventoryLogic::TAB_FORGE)
		Tab = InventoryLogic::TAB_INVENTORY;
	if(Tab == InventoryLogic::TAB_SHOP && !NearbyShop())
		return;
	m_Tab = Tab;
	m_ManualSelection = Tab == InventoryLogic::TAB_SHOP;
	m_SelectedSlot = Tab == InventoryLogic::TAB_SHOP ? 0 : clamp(CustomStuff()->m_WeaponSlot, 0, 3);
	m_KeyboardFocus = 0;
	m_DragItem = -1;
	m_DropConfirmSlot = -1;
}

void CInventory::SyncHeldSelection()
{
	if(m_ManualSelection || m_Tab == InventoryLogic::TAB_SHOP)
		return;
	m_SelectedSlot = clamp(CustomStuff()->m_WeaponSlot, 0, 3);
}

int CInventory::TabItemCount() const
{
	if(m_Tab == InventoryLogic::TAB_SHOP)
		return InventoryLogic::NUM_SHOP_SLOTS;
	return InventoryLogic::NUM_SLOTS;
}

void CInventory::PurchaseShopSlot(int Slot)
{
	const CNetObj_Shop *pShop = NearbyShop();
	if(!pShop || Slot < 0 || Slot >= InventoryLogic::NUM_SHOP_SLOTS || !ShopWeapon(pShop, Slot).IsValid())
	{
		SetActionFeedback(Localize("Empty slot"), true);
		return;
	}
	const int Price = m_pClient->m_pPveRoguelite->ShopCost(ShopWeaponCost(pShop, Slot));
	if(CustomStuff()->m_Gold < Price)
	{
		SetActionFeedback(Localize("Not enough gold"), true);
		return;
	}
	CNetMsg_Cl_InventoryAction Msg;
	Msg.m_Type = INVENTORYACTION_SHOP;
	Msg.m_Slot = Slot;
	Msg.m_Item1 = 0;
	Msg.m_Item2 = 0;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_INV4, 0);
	SetActionFeedback(Localize("Purchase sent"), false);
}

void CInventory::SetActionFeedback(const char *pText, bool Danger)
{
	str_copy(m_aActionFeedback, pText ? pText : "", sizeof(m_aActionFeedback));
	m_ActionFeedbackUntil = time_get() + time_freq() * 2;
	m_ActionFeedbackDanger = Danger;
}

void CInventory::ActivateSelection()
{
	if(m_Tab == InventoryLogic::TAB_SHOP)
	{
		PurchaseShopSlot(m_SelectedSlot);
		return;
	}
	if(m_SelectedSlot < 0 || m_SelectedSlot >= 12 || !CustomStuff()->m_aItem[m_SelectedSlot].IsValid())
		return;
	const int Target = InventoryLogic::EquipTarget(m_SelectedSlot, CustomStuff()->m_WeaponSlot);
	if(m_SelectedSlot < 4)
		m_pClient->m_pControls->QueueWeaponSlot(Target + 2);
	else if(Target >= 0)
		Swap(m_SelectedSlot, Target);
	m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_INV4, 0);
}

void CInventory::RequestDrop()
{
	if(m_Tab != InventoryLogic::TAB_INVENTORY || m_SelectedSlot < 0 || m_SelectedSlot >= 12 ||
	   !CustomStuff()->m_aItem[m_SelectedSlot].IsValid())
		return;
	const int64 Now = time_get();
	if(InventoryLogic::DropConfirmationActive(m_DropConfirmSlot, m_SelectedSlot, Now, m_DropConfirmDeadline))
	{
		Drop(m_SelectedSlot);
		m_DropConfirmSlot = -1;
	}
	else
	{
		m_DropConfirmSlot = m_SelectedSlot;
		m_DropConfirmDeadline = Now + time_freq() * 3;
	}
}

void CInventory::ConKeyInventory(IConsole::IResult *pResult, void *pUserData)
{
	CInventory *pSelf = (CInventory *)pUserData;
	if(pResult->GetInteger(0) && pSelf->m_pClient->m_pBuildPlacement->Active())
		return;

	if(!pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		const int OpenTab = InventoryLogic::ResolveOpenTab(
			pSelf->NearbyShop() != 0, pSelf->ForgeMode(), pSelf->ForgeScreenNear());
		if(!pSelf->m_Render || pSelf->m_Tab == OpenTab)
		{
			pSelf->m_Active = pResult->GetInteger(0) != 0;
			pSelf->m_Tab = OpenTab;
			pSelf->m_Minimized = false;
		}
		else if(pSelf->m_Render)
		{
			pSelf->m_WantedTab = OpenTab;
		}
	}
}

void CInventory::ConInventoryRoll(IConsole::IResult *pResult, void *pUserData)
{
	((CInventory *)pUserData)->InventoryRoll(false);
}

void CInventory::ConDebugInventory(IConsole::IResult *pResult, void *pUserData)
{
	CInventory *pSelf = static_cast<CInventory *>(pUserData);
	pSelf->m_DebugTab = clamp(pResult->GetInteger(0), 0, 2);
	pSelf->m_DebugVisible = pSelf->m_DebugTab != 0;
	// 1 = inventory, 2 = inventory with forge slots prefilled
	if(pSelf->m_DebugTab != 0)
		pSelf->m_Tab = InventoryLogic::TAB_INVENTORY;
	if(!pSelf->m_DebugVisible)
	{
		pSelf->ClearForgeSelection();
		pSelf->Close();
	}
}

void CInventory::InventoryRoll(bool All)
{
	if(m_StupidLock)
	{
		m_StupidLock = false;
		return;
	}
	else
		m_StupidLock = true;

	// Not using -1 for "all" because of if future need do something here
	if(All)
	{
		for(int i = 0; i < 4; i++) // Num inventory slot in a row.
		{
			CNetMsg_Cl_InventoryAction Msg;
			Msg.m_Type = INVENTORYACTION_ROLL;
			Msg.m_Slot = i;
			Msg.m_Item1 = 0;
			Msg.m_Item2 = 0;
			Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
		}
	}
	else
	{
		CNetMsg_Cl_InventoryAction Msg;
		Msg.m_Type = INVENTORYACTION_ROLL;
		Msg.m_Slot = -1; // -1 means cureent Weapon Slot
		Msg.m_Item1 = 0;
		Msg.m_Item2 = 0;
		Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	}
}

void CInventory::OnConsoleInit()
{
	// Console()->Register("+gamepaditempicker", "", CFGFLAG_CLIENT, ConKeyItemPicker, this, "Open item selector");
	Console()->Register("+inventory", "", CFGFLAG_CLIENT, ConKeyInventory, this, "Open inventory");
	Console()->Register("+inventoryroll", "", CFGFLAG_CLIENT, ConInventoryRoll, this, "Roll inventory");
	Console()->Register("dbg_inventory",
						"i",
						CFGFLAG_CLIENT,
						ConDebugInventory,
						this,
						"Force inventory preview: 0 off, 1 inventory, 2 forge, 3 inventory drag forge");
}

void CInventory::ResetInteractionState()
{
	m_WasActive = false;
	m_Active = false;
	m_Render = false;
	m_Mouse1 = false;
	m_MouseTrigger = false;
	m_DragItem = -1;
	m_MoveStartPos = vec2(0, 0);
	m_Moved = false;
	m_MoveTrigger = false;
	m_Minimized = false;
	ClearForgeSelection();
	m_ForgePending = false;
}

void CInventory::OnReset()
{
	ResetInteractionState();
	m_Mouse1Loaded = false;
	m_AppearAmount = 0.0f;
	m_LastAnimationTime = time_get();
	m_Tab = InventoryLogic::TAB_INVENTORY;
	m_SelectedSlot = 0;
	m_ManualSelection = false;
	m_KeyboardFocus = 0;
	m_LastClickTime = 0;
	m_LastClickSlot = -1;
	m_DropConfirmSlot = -1;
	m_DropConfirmDeadline = 0;
	m_aActionFeedback[0] = 0;
	m_ActionFeedbackUntil = 0;
	m_ActionFeedbackDanger = false;
	m_SelectorMouse = vec2(0, 0);
	m_WorldMouse = vec2(0, 0);
	m_LastGamepadCursorTime = 0;
	m_WantedTab = -1;
	m_ForgeLastResult = -1;
	m_ForgeResultEndTick = 0;
}

void CInventory::OnRelease()
{
	ResetInteractionState();
}

void CInventory::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType != NETMSGTYPE_SV_FORGERESULT)
		return;
	const CNetMsg_Sv_ForgeResult *pMsg = (const CNetMsg_Sv_ForgeResult *)pRawMsg;
	m_ForgePending = false;
	m_ForgeLastResult = pMsg->m_Result;
	m_ForgeResultEndTick = Client()->GameTick() + Client()->GameTickSpeed() * 3;
	if(pMsg->m_Result == FORGERESULT_SUCCESS)
	{
		ClearForgeSelection();
		m_pClient->m_pEffects->Repair(CustomStuff()->m_LocalPos);
		SetActionFeedback(Localize("Forge complete"), false);
	}
	else
	{
		static const char *s_apResultText[NUM_FORGERESULTS] = {"Forge complete",
															   "Forge disabled",
															   "Stand next to a Screen to forge",
															   "Not enough gold",
															   "Weapon is busy",
															   "Invalid slots",
															   "Invalid recipe",
															   "Result would not change"};
		SetActionFeedback(Localize(s_apResultText[clamp(pMsg->m_Result, 0, NUM_FORGERESULTS - 1)]), true);
	}
}

bool CInventory::OnMouseMove(float x, float y)
{
	if(!m_Render)
		return false;

	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);

	if(Input()->UsingGamepad())
	{
		float AimX = 0.0f, AimY = 0.0f;
		Input()->GetGamepadAim(&AimX, &AimY);
		const int64 Now = time_get();
		const float Delta = m_LastGamepadCursorTime ? (Now - m_LastGamepadCursorTime) / (float)time_freq() : 1.0f / 60.0f;
		m_LastGamepadCursorTime = Now;
		m_WorldMouse += IntegrateAimStick(vec2(AimX, AimY), 700.0f, 1.0f, Delta);
		m_SelectorMouse += IntegrateAimStick(vec2(AimX, AimY), 240.0f, 1.0f, Delta);
	}
	else
	{
		m_LastGamepadCursorTime = 0;
		Input()->GetRelativePosition(&x, &y);
		const float AimScale = g_Config.m_InpMousesens / 100.0f;
		m_WorldMouse += vec2(x, y) * AimScale;
		const float HudScale = 300.0f / max(1, Graphics()->ScreenHeight());
		m_SelectorMouse += vec2(x, y) * HudScale;
	}

	if(!m_Mouse1)
	{
		m_MoveStartPos = m_SelectorMouse;
		m_Moved = false;
		m_MoveTrigger = false;
	}
	else if(!m_Moved)
	{
		if(abs(length(m_SelectorMouse - m_MoveStartPos)) > 1.0f)
		{
			m_Moved = true;
			m_MoveTrigger = true;
		}
	}

	// The left stick is still consumed by CControls for locomotion. Inventory
	// keeps the aim delta for its cursor while combat buttons remain suppressed.
	return !Input()->UsingGamepad();
}

bool CInventory::OnInput(IInput::CEvent Event)
{
	if(!m_Render)
		return false;
	const bool Press = (Event.m_Flags & IInput::FLAG_PRESS) != 0;

	if(Event.m_Key == KEY_MOUSE_1)
	{
		bool M = m_Mouse1;
		m_Mouse1 = Event.m_Flags & IInput::FLAG_PRESS;
		if(M != m_Mouse1)
			m_MouseTrigger = true;
		return true;
	}
	else if(Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_GAMEPAD_BUTTON_B)
	{
		if(Press && !m_pClient->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK)
			Close();
		return true;
	}
	const char *pBinding = m_pClient->m_pBinds->Get(Event.m_Key);
	if(str_comp(pBinding, "+inventory") == 0 || str_comp(pBinding, "+buildmenu") == 0)
		return false;
	static const char *s_apMovementBindings[] = {
		"+left",
		"+right",
		"+down",
		"+jump",
		"+gamepadleft",
		"+gamepadright",
		"+gamepaddown",
		"+gamepadjump",
	};
	for(const char *pMovementBinding : s_apMovementBindings)
		if(str_comp(pBinding, pMovementBinding) == 0)
			return false;
	if(!Press)
		return true;

	if(Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
	{
		if(m_Tab == InventoryLogic::TAB_SHOP)
			SetTab(InventoryLogic::TAB_INVENTORY);
		return true;
	}
	const bool Left = Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
	const bool Right = Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
	const bool Up = Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP;
	const bool Down = Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN;
	if(Left || Right || Up || Down)
	{
		const int Columns = m_Tab == InventoryLogic::TAB_SHOP ? 1 : 4;
		m_SelectedSlot = InventoryLogic::NavigateGrid(m_SelectedSlot, TabItemCount(), Columns, Right - Left, Down - Up);
		m_ManualSelection = true;
		m_KeyboardFocus = 1;
		m_DropConfirmSlot = -1;
		return true;
	}
	if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A)
	{
		ActivateSelection();
		return true;
	}
	if(Event.m_Key == KEY_DELETE || Event.m_Key == KEY_GAMEPAD_BUTTON_X)
	{
		RequestDrop();
		return true;
	}

	// Let the inventory/build-menu toggle binding receive its own press and
	// release so the same key can close the overlay. Every other event belongs
	// to this focused overlay and must not reach gameplay bindings underneath.
	return true;
}

void CInventory::Drop(int Slot)
{
	if(Slot < 0 || Slot >= 12)
		return;

	// CustomStuff()->m_aItem[Slot] = 0;
	vec2 Pos = m_WorldMouse + m_pClient->m_pCamera->m_Center;

	CNetMsg_Cl_InventoryAction Msg;
	Msg.m_Type = INVENTORYACTION_DROP;
	Msg.m_Slot = Slot;
	Msg.m_Item1 = int(Pos.x);
	Msg.m_Item2 = int(Pos.y);
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CInventory::Swap(int Item1, int Item2)
{
	if(Item1 < 0 || Item2 < 0 || Item1 >= 12 || Item2 >= 12)
		return;

	CWeaponSpec i1 = CustomStuff()->m_aItem[Item1];
	CustomStuff()->m_aItem[Item1] = CustomStuff()->m_aItem[Item2];
	CustomStuff()->m_aItem[Item2] = i1;
	const int Ammo1 = CustomStuff()->m_aItemAmmo[Item1];
	CustomStuff()->m_aItemAmmo[Item1] = CustomStuff()->m_aItemAmmo[Item2];
	CustomStuff()->m_aItemAmmo[Item2] = Ammo1;

	CNetMsg_Cl_InventoryAction Msg;
	Msg.m_Type = INVENTORYACTION_SWAP;
	Msg.m_Slot = 0;
	Msg.m_Item1 = Item1;
	Msg.m_Item2 = Item2;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);

	m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_INV1, 0);
}

bool CInventory::SubmitForgeSlots(int TargetSlot, int MaterialSlot)
{
	if(m_ForgePending || TargetSlot < 0 || TargetSlot >= 12 || MaterialSlot < 0 || MaterialSlot >= 12 ||
	   TargetSlot == MaterialSlot)
		return false;

	CResolvedWeaponProfile MaterialProfile{};
	const bool MaterialIsUpgrade =
		CWeaponCatalog::TryResolve(CustomStuff()->m_aItem[MaterialSlot], &MaterialProfile) &&
		WeaponHasBehavior(MaterialProfile.m_Definition, WEAPON_BEHAVIOR_UPGRADE);
	// Mode 3: Upgrade drag works anywhere; other forge still needs a screen.
	if(!InventoryLogic::ForgeUsable(ForgeMode(), ForgeScreenNear()) &&
	   !(ForgeMode() == 3 && MaterialIsUpgrade))
		return false;

	const CNetObj_GameInfo *pGameInfo = m_pClient->m_Snap.m_pGameInfoObj;
	if(!pGameInfo)
		return false;
	const CWeaponSpec Target = CustomStuff()->m_aItem[TargetSlot];
	const CWeaponSpec Material = CustomStuff()->m_aItem[MaterialSlot];
	const CForgeRecipe Recipe = CForge::Resolve(Target,
		Material,
		CustomStuff()->m_aItemAmmo[TargetSlot],
		pGameInfo->m_ForgeBaseCost,
		pGameInfo->m_ForgeLevelCost,
		CustomStuff()->m_aItemAmmo[MaterialSlot]);
	const int Cost = CForge::EffectiveCost(Recipe, ForgeMode());
	if(Recipe.m_Result != FORGERESULT_SUCCESS || CustomStuff()->m_Gold < Cost)
		return false;

	CNetMsg_Cl_InventoryAction Msg;
	Msg.m_Type = INVENTORYACTION_COMBINE;
	Msg.m_Slot = FORGEOP_AUTO;
	Msg.m_Item1 = TargetSlot;
	Msg.m_Item2 = MaterialSlot;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
	m_ForgePending = true;
	m_ForgeLastResult = -1;
	m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_INV2, 0);
	return true;
}

bool CInventory::IsUpgradeSlot(int Slot) const
{
	if(Slot < 0 || Slot >= InventoryLogic::NUM_SLOTS)
		return false;
	CResolvedWeaponProfile Profile{};
	return CWeaponCatalog::TryResolve(CustomStuff()->m_aItem[Slot], &Profile) &&
		   WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_UPGRADE);
}

bool CInventory::TryUpgradeCombine(int SlotA, int SlotB)
{
	if(SubmitUpgradeDrag(SlotA, SlotB) || SubmitUpgradeDrag(SlotB, SlotA))
	{
		ClearForgeSelection();
		return true;
	}
	return false;
}

void CInventory::NotifyUpgradeFailed(int TargetSlot, int MaterialSlot)
{
	if(ForgeMode() < 1)
	{
		SetActionFeedback(Localize("Forge disabled"), true);
		return;
	}
	if(ForgeMode() == 2 && !ForgeScreenNear())
	{
		SetActionFeedback(Localize("Move closer to a screen"), true);
		return;
	}
	if(TargetSlot < 0 || MaterialSlot < 0 || TargetSlot == MaterialSlot || IsUpgradeSlot(TargetSlot) ||
	   !IsUpgradeSlot(MaterialSlot))
	{
		SetActionFeedback(Localize("Select a target weapon"), true);
		return;
	}
	const CNetObj_GameInfo *pGameInfo = m_pClient->m_Snap.m_pGameInfoObj;
	if(!pGameInfo)
	{
		SetActionFeedback(Localize("Invalid recipe"), true);
		return;
	}
	const CForgeRecipe Recipe = CForge::Resolve(CustomStuff()->m_aItem[TargetSlot],
												CustomStuff()->m_aItem[MaterialSlot],
												CustomStuff()->m_aItemAmmo[TargetSlot],
												pGameInfo->m_ForgeBaseCost,
												pGameInfo->m_ForgeLevelCost,
												CustomStuff()->m_aItemAmmo[MaterialSlot]);
	if(Recipe.m_Result == FORGERESULT_SUCCESS && CustomStuff()->m_Gold < CForge::EffectiveCost(Recipe, ForgeMode()))
		SetActionFeedback(Localize("Not enough gold"), true);
	else if(Recipe.m_Result == FORGERESULT_NO_CHANGE)
		SetActionFeedback(Localize("Result would not change"), true);
	else
		SetActionFeedback(Localize("Invalid recipe"), true);
}

bool CInventory::SubmitUpgradeDrag(int TargetSlot, int MaterialSlot)
{
	if(ForgeMode() < 1 || TargetSlot < 0 || TargetSlot >= 12 || MaterialSlot < 0 ||
	   MaterialSlot >= 12 || TargetSlot == MaterialSlot)
		return false;

	// Mode 2 needs a forge screen; mode 1/3 allow upgrade drag without a screen.
	if(ForgeMode() == 2 && !ForgeScreenNear())
		return false;

	CResolvedWeaponProfile MaterialProfile{};
	if(!CWeaponCatalog::TryResolve(CustomStuff()->m_aItem[MaterialSlot], &MaterialProfile) ||
	   !WeaponHasBehavior(MaterialProfile.m_Definition, WEAPON_BEHAVIOR_UPGRADE))
		return false;

	return SubmitForgeSlots(TargetSlot, MaterialSlot);
}

void CInventory::SubmitForge()
{
	SubmitForgeSlots(m_ForgeTargetSlot, m_ForgeMaterialSlot);
}

void CInventory::RenderMouse()
{
	if(m_Moved && m_DragItem >= 0 && m_DragItem < 12)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		const CWeaponSpec w = CustomStuff()->m_aItem[m_DragItem];
		if(w.IsValid())
		{
			RenderTools()->SetShadersForWeapon(w);
			RenderTools()->RenderWeapon(
				w, m_SelectorMouse, vec2(1, 0), 7.5f, true, 0, 1.0f, false, false, false, m_AppearAmount);
			Graphics()->ShaderEnd();
		}
	}

	// cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x, m_SelectorMouse.y, 8, 8);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CInventory::DrawSidebar(const CNetObj_Shop *pShop)
{
	const float ScreenW = 300.0f * Graphics()->ScreenAspect();
	const float ScreenH = 300.0f;
	const bool ForgeAvailable = m_DebugTab == 2 || InventoryLogic::ForgeTabVisible(ForgeMode());
	const int CurrentForgeMode = ForgeMode();
	const bool NeedScreen =
		ForgeAvailable && CurrentForgeMode >= 2 && m_DebugTab != 2 && !ForgeScreenNear();
	const float Alpha = m_AppearAmount;

	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	vec4 Accent = CMenus::ThemeAccent();
	Accent.a = 1.0f;
	vec4 AccentDim = CMenus::ThemeAccentDim();
	AccentDim.a = 1.0f;
	vec4 Danger = CMenus::ThemeDanger();
	Danger.a = 1.0f;
	const vec4 Amber(0.95f, 0.63f, 0.16f, 1.0f);
	const vec4 Text = CMenus::ThemeText();
	const vec4 Muted = CMenus::ThemeTextMuted();
	const vec4 OnAccent(0.05f, 0.08f, 0.10f, 1.0f);

	const bool Click = m_MouseTrigger && !m_Mouse1 && !m_Moved;
	const bool Press = m_MouseTrigger && m_Mouse1;
	auto Inside = [this](const CUIRect &Rect)
	{
		return m_SelectorMouse.x >= Rect.x && m_SelectorMouse.x <= Rect.x + Rect.w && m_SelectorMouse.y >= Rect.y &&
			   m_SelectorMouse.y <= Rect.y + Rect.h;
	};
	auto TechShape = [&](const CUIRect &Rect, vec4 Color, float Cut)
	{
		if(Rect.w <= 0.0f || Rect.h <= 0.0f)
			return;
		Cut = clamp(Cut, 0.0f, min(Rect.w, Rect.h) * 0.45f);
		IGraphics::CFreeformItem aParts[3] = {
			IGraphics::CFreeformItem(Rect.x, Rect.y + Cut, Rect.x + Cut, Rect.y, Rect.x, Rect.y + Rect.h - Cut, Rect.x + Cut, Rect.y + Rect.h),
			IGraphics::CFreeformItem(Rect.x + Cut, Rect.y, Rect.x + Rect.w - Cut, Rect.y, Rect.x + Cut, Rect.y + Rect.h, Rect.x + Rect.w - Cut, Rect.y + Rect.h),
			IGraphics::CFreeformItem(Rect.x + Rect.w - Cut, Rect.y, Rect.x + Rect.w, Rect.y + Cut, Rect.x + Rect.w - Cut, Rect.y + Rect.h, Rect.x + Rect.w, Rect.y + Rect.h - Cut)};
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a * Alpha);
		Graphics()->QuadsDrawFreeform(aParts, 3);
		Graphics()->QuadsEnd();
	};
	auto SmokedGlass = [&](const CUIRect &Rect, vec4 EdgeColor, bool Active, float Cut)
	{
		CUIRect Shadow = {Rect.x + 0.7f, Rect.y + 1.1f, Rect.w, Rect.h};
		TechShape(Shadow, vec4(0.0f, 0.008f, 0.014f, 0.32f), Cut);
		EdgeColor.a = (Active ? 0.48f : 0.24f);
		TechShape(Rect, EdgeColor, Cut);
		CUIRect Inner = {Rect.x + 0.8f, Rect.y + 0.8f, Rect.w - 1.6f, Rect.h - 1.6f};
		const vec4 SmokedFill(Deep.r * 0.58f + Panel.r * 0.42f,
			Deep.g * 0.58f + Panel.g * 0.42f,
			Deep.b * 0.58f + Panel.b * 0.42f,
			Active ? 0.62f : 0.50f);
		TechShape(Inner, SmokedFill, max(0.0f, Cut - 0.8f));
	};
	auto Label = [this, Alpha](const CUIRect &Rect, const char *pText, float Size, int Align, vec4 Color)
	{
		const float Width = TextRender()->TextWidth(0, Size, pText, -1);
		float X = Rect.x;
		if(Align == 0)
			X = Rect.x + (Rect.w - Width) * 0.5f;
		else if(Align > 0)
			X = Rect.x + Rect.w - Width;
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a * Alpha);
		TextRender()->Text(0, X, Rect.y + (Rect.h - Size) * 0.5f - 0.5f, Size, pText, Rect.w);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	};
	auto DrawWeapon = [&](const CWeaponSpec &Spec, vec2 Pos, float Size)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		RenderTools()->SetShadersForWeapon(Spec);
		RenderTools()->RenderWeapon(Spec, Pos, vec2(1, 0), Size, true, 0, 1.0f, false, false, false, Alpha);
		Graphics()->ShaderEnd();
	};
	auto ToRect = [](const InventoryLogic::CLayout &L) -> CUIRect {
		return {L.m_X, L.m_Y, L.m_W, L.m_H};
	};

	char aBuf[256];
	char aName[128];

	if(m_Tab == InventoryLogic::TAB_SHOP && pShop)
	{
		const InventoryLogic::CLayout Layout =
			InventoryLogic::PanelLayout(ScreenW, ScreenH, UI()->Scale(), m_AppearAmount, InventoryLogic::PANEL_SHOP);
		CUIRect ShopPanel = ToRect(Layout);
		SmokedGlass(ShopPanel, AccentDim, true, 4.0f);
		const float InnerX = ShopPanel.x + 5.0f;
		const float InnerW = ShopPanel.w - 10.0f;
		Label({InnerX, ShopPanel.y + 3.0f, InnerW * 0.5f, 10.0f}, Localize("Shop"), 5.6f, -1, Accent);
		str_format(aBuf, sizeof(aBuf), "%s %d", Localize("Gold"), CustomStuff()->m_Gold);
		Label({InnerX + InnerW * 0.45f, ShopPanel.y + 3.0f, InnerW * 0.55f, 10.0f}, aBuf, 4.8f, 1, Amber);
		const float RowH = 16.0f;
		const float BuyW = 36.0f;
		const float ListY = ShopPanel.y + 14.0f;
		const float DetailH = InventoryLogic::DetailH;
		CUIRect Detail = {InnerX, ShopPanel.y + ShopPanel.h - DetailH - 4.0f, InnerW, DetailH};
		for(int Slot = 0; Slot < InventoryLogic::NUM_SHOP_SLOTS; ++Slot)
		{
			const CWeaponSpec Spec = ShopWeapon(pShop, Slot);
			const bool Valid = Spec.IsValid();
			const int Price = m_pClient->m_pPveRoguelite->ShopCost(ShopWeaponCost(pShop, Slot));
			const bool CanAfford = Valid && CustomStuff()->m_Gold >= Price;
			const bool Selected = Slot == m_SelectedSlot;
			CUIRect Row = {InnerX, ListY + Slot * (RowH + 2.0f), InnerW, RowH};
			CUIRect Buy = {Row.x + Row.w - BuyW, Row.y + 1.0f, BuyW - 1.0f, RowH - 2.0f};
			SmokedGlass(Row, Selected ? Accent : AccentDim, Selected, 2.0f);
			if(Valid)
			{
				const vec2 WeaponPos(Row.x + 14.0f, Row.y + Row.h * 0.62f);
				const float WeaponSize = 3.4f;
				DrawWeaponRankOverWeapon(Graphics(), RenderTools(), Spec, WeaponPos, WeaponSize, Alpha);
				DrawWeapon(Spec, WeaponPos, WeaponSize);
				Label({Row.x + 26.0f, Row.y + 1.0f, Row.w - BuyW - 30.0f, Row.h - 2.0f},
					  WeaponDisplayName(Spec, aName, sizeof(aName)),
					  4.4f,
					  -1,
					  Text);
			}
			else
				Label({Row.x + 6.0f, Row.y, Row.w - BuyW - 8.0f, Row.h}, Localize("Empty"), 4.4f, -1, Muted);
			SmokedGlass(Buy, CanAfford ? Accent : AccentDim, CanAfford, 2.0f);
			Label(Buy, Localize("Buy"), 4.2f, 0, CanAfford ? OnAccent : Muted);
			if(Click && Inside(Buy) && CanAfford)
			{
				m_SelectedSlot = Slot;
				m_ManualSelection = true;
				PurchaseShopSlot(Slot);
			}
			else if(Click && Inside(Row) && !Inside(Buy))
			{
				const int64 Now = time_get();
				const bool DoubleClick = Slot == m_LastClickSlot && Now - m_LastClickTime <= time_freq() * 350 / 1000;
				m_SelectedSlot = Slot;
				m_ManualSelection = true;
				m_LastClickSlot = Slot;
				m_LastClickTime = Now;
				if(DoubleClick && CanAfford)
					PurchaseShopSlot(Slot);
			}
		}

		SmokedGlass(Detail, AccentDim, true, 2.5f);
		{
			const CWeaponSpec SelectedSpec =
				m_SelectedSlot >= 0 && m_SelectedSlot < InventoryLogic::NUM_SHOP_SLOTS ? ShopWeapon(pShop, m_SelectedSlot)
																					  : CWeaponSpec{};
			CResolvedWeaponProfile Profile{};
			const bool HasProfile = CWeaponCatalog::TryResolve(SelectedSpec, &Profile);
			Label({Detail.x + 5.0f, Detail.y + 2.0f, Detail.w - 10.0f, 9.0f},
				  SelectedSpec.IsValid() ? WeaponDisplayName(SelectedSpec, aName, sizeof(aName)) : Localize("Empty slot"),
				  5.4f,
				  -1,
				  Text);
			if(HasProfile)
			{
				const float Damage = max(Profile.m_Combat.m_ProjectileDamage,
										 max(Profile.m_Combat.m_ExplosionDamage, Profile.m_Combat.m_WeaponKnockback));
				char aMaxAmmo[32];
				if(Profile.m_Combat.m_UsesAmmo)
					str_format(aMaxAmmo, sizeof(aMaxAmmo), "%d", Profile.m_Combat.m_MaxAmmo);
				else
					str_copy(aMaxAmmo, Localize("Infinite"), sizeof(aMaxAmmo));
				const int Price = m_pClient->m_pPveRoguelite->ShopCost(ShopWeaponCost(pShop, m_SelectedSlot));
				str_format(aBuf,
						   sizeof(aBuf),
						   "%s %d   %s %s   %s %d",
						   Localize("Weapon level"),
						   SelectedSpec.m_Level,
						   Localize("Ammo"),
						   aMaxAmmo,
						   Localize("Cost"),
						   Price);
				Label({Detail.x + 5.0f, Detail.y + 12.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.4f, -1, Accent);
				str_format(aBuf,
						   sizeof(aBuf),
						   "%s %.0f   %s",
						   Localize("Damage"),
						   Damage,
						   FiringModeName(Profile.m_Combat.m_FiringType, Profile.m_Combat.m_FullAuto));
				Label({Detail.x + 5.0f, Detail.y + 21.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.4f, -1, Text);
				const char *pTrait1 = Profile.m_Combat.m_ExplosiveProjectile
										  ? Localize("Explosive")
										  : (Profile.m_Combat.m_LaserWeapon ? Localize("Laser") : 0);
				const char *pTrait2 = Profile.m_Combat.m_ProjectileBounces > 0
										  ? Localize("Ricochet")
										  : (Profile.m_Combat.m_FiringType == WFT_MELEE ? Localize("Melee") : 0);
				str_format(aBuf,
						   sizeof(aBuf),
						   "%s%s%s",
						   pTrait1 ? pTrait1 : "",
						   pTrait1 && pTrait2 ? " · " : "",
						   pTrait2 ? pTrait2 : "");
				if(aBuf[0])
					Label({Detail.x + 5.0f, Detail.y + 29.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.2f, -1, Amber);
			}
		}
		m_MouseTrigger = false;
		return;
	}

	const InventoryLogic::CHammerLayout Hammer =
		InventoryLogic::HammerLayout(ScreenW, ScreenH, m_AppearAmount, ForgeAvailable);
	CUIRect BagPanel = ToRect(Hammer.m_Bag);
	CUIRect Resource = ToRect(Hammer.m_Resource);
	CUIRect Detail = ToRect(Hammer.m_Detail);
	SmokedGlass(Resource, AccentDim, true, 2.0f);
	SmokedGlass(Detail, AccentDim, true, 2.5f);
	SmokedGlass(BagPanel, AccentDim, true, 3.0f);
	Label({BagPanel.x + 2.0f, BagPanel.y + 1.0f, BagPanel.w - 4.0f, InventoryLogic::SectionTitleH},
		  Localize("Inventory"),
		  4.4f,
		  0,
		  Accent);

	str_format(aBuf, sizeof(aBuf), "%s  %d", Localize("Gold"), CustomStuff()->m_Gold);
	Label({Resource.x + 5.0f, Resource.y, Resource.w * 0.5f - 5.0f, Resource.h}, aBuf, 4.8f, -1, Amber);
	str_format(aBuf, sizeof(aBuf), "%s  %d", Localize("Kits"), CustomStuff()->m_LocalKits);
	Label({Resource.x + Resource.w * 0.5f, Resource.y, Resource.w * 0.5f - 5.0f, Resource.h}, aBuf, 4.8f, 1, Accent);

	CWeaponSpec SelectedSpec =
		m_SelectedSlot >= 0 && m_SelectedSlot < 12 ? CustomStuff()->m_aItem[m_SelectedSlot] : CWeaponSpec{};
	CResolvedWeaponProfile Profile{};
	const bool HasProfile = CWeaponCatalog::TryResolve(SelectedSpec, &Profile);
	Label({Detail.x + 5.0f, Detail.y + 2.0f, Detail.w - 10.0f, 9.0f},
		  SelectedSpec.IsValid() ? WeaponDisplayName(SelectedSpec, aName, sizeof(aName)) : Localize("Empty slot"),
		  5.4f,
		  -1,
		  Text);
	if(HasProfile)
	{
		const float Damage = max(Profile.m_Combat.m_ProjectileDamage,
								 max(Profile.m_Combat.m_ExplosionDamage, Profile.m_Combat.m_WeaponKnockback));
		char aMaxAmmo[32];
		if(Profile.m_Combat.m_UsesAmmo)
			str_format(aMaxAmmo, sizeof(aMaxAmmo), "%d", Profile.m_Combat.m_MaxAmmo);
		else
			str_copy(aMaxAmmo, Localize("Infinite"), sizeof(aMaxAmmo));
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s %d   %s %d/%s",
				   Localize("Weapon level"),
				   SelectedSpec.m_Level,
				   Localize("Ammo"),
				   max(0, CustomStuff()->m_aItemAmmo[m_SelectedSlot]),
				   aMaxAmmo);
		Label({Detail.x + 5.0f, Detail.y + 12.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.4f, -1, Accent);
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s %.0f   %s",
				   Localize("Damage"),
				   Damage,
				   FiringModeName(Profile.m_Combat.m_FiringType, Profile.m_Combat.m_FullAuto));
		Label({Detail.x + 5.0f, Detail.y + 21.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.4f, -1, Text);
		const char *pTrait1 = Profile.m_Combat.m_ExplosiveProjectile
								  ? Localize("Explosive")
								  : (Profile.m_Combat.m_LaserWeapon ? Localize("Laser") : 0);
		const char *pTrait2 = Profile.m_Combat.m_ProjectileBounces > 0
								  ? Localize("Ricochet")
								  : (Profile.m_Combat.m_FiringType == WFT_MELEE ? Localize("Melee") : 0);
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s%s%s",
				   pTrait1 ? pTrait1 : "",
				   pTrait1 && pTrait2 ? " · " : "",
				   pTrait2 ? pTrait2 : "");
		if(aBuf[0])
			Label({Detail.x + 5.0f, Detail.y + 29.0f, Detail.w - 10.0f, 8.0f}, aBuf, 4.2f, -1, Amber);
	}

	CUIRect FrameSlot = {};
	CUIRect BarrelSlot = {};
	CUIRect PreviewSlot = {};
	if(ForgeAvailable)
	{
		CUIRect ForgePanel = ToRect(Hammer.m_Forge);
		SmokedGlass(ForgePanel, Accent, true, 3.5f);
		Label({ForgePanel.x + 2.0f, ForgePanel.y + 1.0f, ForgePanel.w - 4.0f, InventoryLogic::SectionTitleH},
			  Localize("Forge"),
			  4.6f,
			  0,
			  Accent);
		const float Gap = 4.0f;
		const float Pad = 3.0f;
		const float TitlePad = InventoryLogic::SectionTitleH + 1.0f;
		const float InnerW = ForgePanel.w - Pad * 2.0f - Gap * 2.0f;
		const float SideW = InnerW * 0.28f;
		const float CenterW = InnerW - SideW * 2.0f;
		const float FullH = ForgePanel.h - Pad - TitlePad;
		const float SideH = max(22.0f, FullH - InventoryLogic::ForgeSideLift);
		const float SideY = ForgePanel.y + TitlePad + InventoryLogic::ForgeSideLift;
		// Result box sits higher (top-aligned); material slots sit lower beside it.
		FrameSlot = {ForgePanel.x + Pad, SideY, SideW, SideH};
		PreviewSlot = {FrameSlot.x + SideW + Gap, ForgePanel.y + TitlePad, CenterW, FullH};
		BarrelSlot = {PreviewSlot.x + CenterW + Gap, SideY, SideW, SideH};

		const CNetObj_GameInfo *pGameInfo = m_pClient->m_Snap.m_pGameInfoObj;
		const CWeaponSpec TargetSpec = m_ForgeTargetSlot >= 0 && m_ForgeTargetSlot < 12
										   ? CustomStuff()->m_aItem[m_ForgeTargetSlot]
										   : CWeaponSpec{};
		const CWeaponSpec MaterialSpec = m_ForgeMaterialSlot >= 0 && m_ForgeMaterialSlot < 12
											 ? CustomStuff()->m_aItem[m_ForgeMaterialSlot]
											 : CWeaponSpec{};
		CForgeRecipe Recipe;
		if(pGameInfo && TargetSpec.IsValid() && MaterialSpec.IsValid())
			Recipe = CForge::Resolve(TargetSpec,
									 MaterialSpec,
									 CustomStuff()->m_aItemAmmo[m_ForgeTargetSlot],
									 pGameInfo->m_ForgeBaseCost,
									 pGameInfo->m_ForgeLevelCost,
									 CustomStuff()->m_aItemAmmo[m_ForgeMaterialSlot]);
		const int RecipeCost = CForge::EffectiveCost(Recipe, CurrentForgeMode);
		const bool CanAfford = CustomStuff()->m_Gold >= RecipeCost;
		const bool ForgeUsable = m_DebugTab == 2 || InventoryLogic::ForgeUsable(CurrentForgeMode, ForgeScreenNear());
		const bool CanForge = Recipe.m_Result == FORGERESULT_SUCCESS && CanAfford && ForgeUsable && !m_ForgePending;
		const bool HasTarget = TargetSpec.IsValid();
		const bool HasMaterial = MaterialSpec.IsValid();

		CUIRect Status = ToRect(Hammer.m_Status);
		const bool HasFeedback = m_aActionFeedback[0] && time_get() <= m_ActionFeedbackUntil;
		const char *pOperation = 0;
		if(Recipe.m_Operation == FORGEOP_REPLACE_PART2)
			pOperation = Localize("Part 2 transplant");
		else if(Recipe.m_Operation == FORGEOP_SPIN)
			pOperation = Localize("Spin");
		else if(Recipe.m_Operation == FORGEOP_UPGRADE)
			pOperation = Localize("Upgrade");
		else if(Recipe.m_Operation == FORGEOP_MOD_RECIPE && Recipe.m_aRecipeName[0])
			pOperation = Localize(Recipe.m_aRecipeName);
		const char *pStatus = Localize("Select target and material");
		vec4 StatusColor = Muted;
		bool StatusAlert = false;
		if(HasFeedback)
		{
			pStatus = m_aActionFeedback;
			StatusColor = m_ActionFeedbackDanger ? Danger : Accent;
			StatusAlert = m_ActionFeedbackDanger;
			pOperation = 0;
		}
		else if(!ForgeUsable)
		{
			pStatus = Localize("Move closer to a screen");
			StatusColor = Danger;
			StatusAlert = true;
		}
		else if(m_ForgePending)
		{
			pStatus = Localize("Waiting for server");
			StatusColor = Accent;
		}
		else if(m_ForgeLastResult >= 0 && Client()->GameTick() <= m_ForgeResultEndTick)
		{
			static const char *s_apResultText[NUM_FORGERESULTS] = {"Forge complete",
																   "Forge disabled",
																   "Move closer to a screen",
																   "Not enough gold",
																   "Weapon is busy",
																   "Invalid slots",
																   "Invalid recipe",
																   "Result would not change"};
			pStatus = Localize(s_apResultText[clamp(m_ForgeLastResult, 0, NUM_FORGERESULTS - 1)]);
			StatusColor = m_ForgeLastResult == FORGERESULT_SUCCESS ? Accent : Danger;
			StatusAlert = m_ForgeLastResult != FORGERESULT_SUCCESS;
		}
		else if(HasTarget && !HasMaterial)
			pStatus = Localize("Select a material weapon");
		else if(!HasTarget && HasMaterial)
			pStatus = Localize("Select a target weapon");
		else if(HasTarget && HasMaterial && Recipe.m_Result == FORGERESULT_NO_CHANGE)
		{
			pStatus = Localize("Result would not change");
			StatusColor = Danger;
			StatusAlert = true;
		}
		else if(HasTarget && HasMaterial && Recipe.m_Result != FORGERESULT_SUCCESS)
		{
			pStatus = Localize("Invalid recipe");
			StatusColor = Danger;
			StatusAlert = true;
		}
		else if(Recipe.m_Result == FORGERESULT_SUCCESS && !CanAfford)
		{
			pStatus = Localize("Not enough gold");
			StatusColor = Danger;
			StatusAlert = true;
		}
		else if(Recipe.m_Result == FORGERESULT_SUCCESS)
		{
			pStatus = Localize("Ready to forge");
			StatusColor = Accent;
		}
		str_format(aBuf, sizeof(aBuf), "%s%s%s", pOperation ? pOperation : "", pOperation ? "  ·  " : "", pStatus);
		SmokedGlass(Status, StatusAlert ? Danger : AccentDim, StatusAlert || HasFeedback, 2.0f);
		Label({Status.x + 4.0f, Status.y, Status.w - 8.0f, Status.h}, aBuf, 4.4f, 0, StatusColor);

		auto DrawMat = [&](const CUIRect &Cell, const char *pEmpty, const CWeaponSpec &Spec, vec4 Edge)
		{
			SmokedGlass(Cell, Edge, Spec.IsValid() || Inside(Cell), 2.2f);
			if(Spec.IsValid())
			{
				char aItemName[128];
				Label({Cell.x + 1.0f, Cell.y + 1.0f, Cell.w - 2.0f, 7.0f},
					  WeaponDisplayName(Spec, aItemName, sizeof(aItemName)),
					  3.6f,
					  0,
					  Text);
				const vec2 WeaponPos(Cell.x + Cell.w * 0.5f, Cell.y + Cell.h * 0.62f);
				const float WeaponSize = 4.6f;
				DrawWeaponRankOverWeapon(Graphics(), RenderTools(), Spec, WeaponPos, WeaponSize, Alpha);
				DrawWeapon(Spec, WeaponPos, WeaponSize);
			}
			else
			{
				Label({Cell.x + 1.0f, Cell.y + 2.0f, Cell.w - 2.0f, 7.0f},
					  Localize("Put a weapon here"),
					  3.2f,
					  0,
					  Muted);
				Label({Cell.x + 1.0f, Cell.y + Cell.h * 0.42f, Cell.w - 2.0f, 12.0f}, pEmpty, 5.0f, 0, Accent);
			}
		};
		DrawMat(FrameSlot, Localize("Part 1"), TargetSpec, Accent);
		DrawMat(BarrelSlot, Localize("Part 2"), MaterialSpec, Amber);

		SmokedGlass(PreviewSlot, CanForge ? Accent : AccentDim, Recipe.m_Product.IsValid() || CanForge, 2.5f);
		if(Recipe.m_Product.IsValid())
		{
			char aProductName[128];
			Label({PreviewSlot.x + 2.0f, PreviewSlot.y + 1.0f, PreviewSlot.w - 4.0f, 8.0f},
				  WeaponDisplayName(Recipe.m_Product, aProductName, sizeof(aProductName)),
				  4.0f,
				  0,
				  Text);
			DrawWeaponRankOverWeapon(Graphics(),
									 RenderTools(),
									 Recipe.m_Product,
									 vec2(PreviewSlot.x + PreviewSlot.w * 0.5f, PreviewSlot.y + PreviewSlot.h * 0.52f),
									 5.4f,
									 Alpha);
			DrawWeapon(Recipe.m_Product, vec2(PreviewSlot.x + PreviewSlot.w * 0.5f, PreviewSlot.y + PreviewSlot.h * 0.52f), 5.4f);
			str_format(aBuf, sizeof(aBuf), "%s  %d %s", Localize("Cost"), RecipeCost, Localize("Gold"));
			Label({PreviewSlot.x + 2.0f, PreviewSlot.y + PreviewSlot.h - 9.0f, PreviewSlot.w - 4.0f, 8.0f},
				  aBuf,
				  3.8f,
				  0,
				  CanAfford ? Amber : Danger);
			if(CanForge)
				TechShape({PreviewSlot.x + 4.0f, PreviewSlot.y + PreviewSlot.h - 2.2f, PreviewSlot.w - 8.0f, 1.6f}, Accent, 0.6f);
			else if(NeedScreen)
				Label({PreviewSlot.x + 2.0f, PreviewSlot.y + PreviewSlot.h - 17.0f, PreviewSlot.w - 4.0f, 8.0f},
					  Localize("Need Screen"),
					  3.6f,
					  0,
					  Danger);
		}
		else
			Label({PreviewSlot.x + 2.0f, PreviewSlot.y + PreviewSlot.h * 0.28f, PreviewSlot.w - 4.0f, 18.0f},
				  HasTarget ? (HasMaterial ? Localize("No valid result") : Localize("Choose material below"))
							: "?",
				  HasTarget ? 4.2f : 7.0f,
				  0,
				  Muted);

		if(Click && Inside(FrameSlot) && SelectedSpec.IsValid() && m_SelectedSlot != m_ForgeMaterialSlot)
		{
			const InventoryLogic::CForgeSlots Spots =
				InventoryLogic::AssignForgeSlot({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_SelectedSlot, true);
			m_ForgeTargetSlot = Spots.m_Target;
			m_ForgeMaterialSlot = Spots.m_Material;
			m_ForgeLastResult = -1;
		}
		if(Click && Inside(BarrelSlot) && SelectedSpec.IsValid() && m_SelectedSlot != m_ForgeTargetSlot)
		{
			const InventoryLogic::CForgeSlots Spots =
				InventoryLogic::AssignForgeSlot({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_SelectedSlot, false);
			m_ForgeTargetSlot = Spots.m_Target;
			m_ForgeMaterialSlot = Spots.m_Material;
			m_ForgeLastResult = -1;
		}
		if(Click && Inside(PreviewSlot))
		{
			if(CanForge)
				SubmitForge();
			else if(NeedScreen)
				SetActionFeedback(Localize("Stand next to a Screen to forge"), true);
			else if(Recipe.m_Product.IsValid())
			{
				if(!CanAfford)
					SetActionFeedback(Localize("Not enough gold"), true);
				else if(m_ForgePending)
					SetActionFeedback(Localize("Waiting for server"), true);
			}
			else
				SetActionFeedback(Localize("Select materials"), true);
		}
	}

	const float Cell = InventoryLogic::BagCellSize();
	const float GridGap = InventoryLogic::BagRowGap;
	// Grid matches combat-bar slots; BagPad keeps cell borders inside the Inventory frame.
	const float GridX = Hammer.m_Bag.m_X + InventoryLogic::BagPad;
	const float GridY = Hammer.m_Bag.m_Y + InventoryLogic::BagPad + InventoryLogic::SectionTitleH;
	const float Pitch = HudLayout::CombatBarSlotWidth() + HudLayout::CombatBarSlotGap;
	int HoveredBag = -1;
	for(int Bag = 0; Bag < InventoryLogic::NUM_BAG_SLOTS; ++Bag)
	{
		const int Slot = Bag + InventoryLogic::NUM_EQUIPMENT_SLOTS;
		const int Row = Bag / 4;
		const int Column = Bag % 4;
		CUIRect CellRect = {GridX + Column * Pitch, GridY + Row * (Cell + GridGap), Cell, Cell};
		const bool Selected = Slot == m_SelectedSlot;
		const bool Hover = Inside(CellRect);
		SmokedGlass(CellRect, Selected ? Accent : AccentDim, Selected || Hover, 2.2f);
		if(Slot == m_ForgeTargetSlot || Slot == m_ForgeMaterialSlot)
			TechShape({CellRect.x + 2.0f, CellRect.y + CellRect.h - 2.0f, CellRect.w - 4.0f, 1.5f},
					  Slot == m_ForgeTargetSlot ? Accent : Amber,
					  0.5f);
		const CWeaponSpec Spec = CustomStuff()->m_aItem[Slot];
		if(Spec.IsValid() && Slot != m_DragItem)
		{
			const vec2 WeaponPos(CellRect.x + CellRect.w * 0.5f, CellRect.y + CellRect.h * 0.55f);
			const float WeaponSize = 4.8f;
			DrawWeaponRankOverWeapon(Graphics(), RenderTools(), Spec, WeaponPos, WeaponSize, Alpha);
			DrawWeapon(Spec, WeaponPos, WeaponSize);
		}
		if(Hover)
			HoveredBag = Slot;
	}

	const int HoveredCombat = InventoryLogic::HitCombatSlot(
		ScreenW, ScreenH, m_SelectorMouse.x, m_SelectorMouse.y, CustomStuff()->m_WeaponSlot);
	int Hovered = HoveredBag >= 0 ? HoveredBag : HoveredCombat;

	if(m_DragItem >= 0 || HoveredCombat >= 0)
	{
		for(int Slot = 0; Slot < InventoryLogic::NUM_EQUIPMENT_SLOTS; ++Slot)
		{
			if(Slot != HoveredCombat)
				continue;
			const InventoryLogic::CLayout Cell =
				InventoryLogic::CombatSlotLayout(ScreenW, ScreenH, Slot, Slot == CustomStuff()->m_WeaponSlot);
			TechShape({Cell.m_X - 1.0f, Cell.m_Y - 1.0f, Cell.m_W + 2.0f, Cell.m_H + 2.0f},
					  vec4(Accent.r, Accent.g, Accent.b, 0.35f),
					  2.5f);
		}
	}

	if(Press && Hovered >= 0 && CustomStuff()->m_aItem[Hovered].IsValid())
	{
		m_SelectedSlot = Hovered;
		m_ManualSelection = true;
		m_DragItem = Hovered;
	}

	bool ReleasedClick = false;
	if(m_MouseTrigger && !m_Mouse1 && m_DragItem >= 0)
	{
		if(m_Moved)
		{
			InventoryLogic::EForgeDropTarget DropTarget = InventoryLogic::FORGE_DROP_NONE;
			if(ForgeAvailable)
			{
				if(Inside(FrameSlot))
					DropTarget = InventoryLogic::FORGE_DROP_TARGET;
				else if(Inside(BarrelSlot))
					DropTarget = InventoryLogic::FORGE_DROP_MATERIAL;
			}
			if(DropTarget != InventoryLogic::FORGE_DROP_NONE)
			{
				// Dragging an upgrade card onto forge slots should upgrade, not replace the weapon.
				if(IsUpgradeSlot(m_DragItem))
				{
					int TargetSlot = m_ForgeTargetSlot;
					if(DropTarget == InventoryLogic::FORGE_DROP_TARGET && TargetSlot >= 0 &&
					   TargetSlot != m_DragItem && !IsUpgradeSlot(TargetSlot))
					{
						if(!TryUpgradeCombine(TargetSlot, m_DragItem))
							NotifyUpgradeFailed(TargetSlot, m_DragItem);
					}
					else
					{
						m_ForgeMaterialSlot = m_DragItem;
						if(m_ForgeTargetSlot >= 0 && m_ForgeTargetSlot != m_DragItem &&
						   !IsUpgradeSlot(m_ForgeTargetSlot))
						{
							if(!TryUpgradeCombine(m_ForgeTargetSlot, m_DragItem))
								NotifyUpgradeFailed(m_ForgeTargetSlot, m_DragItem);
						}
						else
							m_ForgeLastResult = -1;
					}
				}
				else
				{
					const InventoryLogic::CForgeSlots Slots =
						InventoryLogic::DropForgeItem({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_DragItem, DropTarget);
					m_ForgeTargetSlot = Slots.m_Target;
					m_ForgeMaterialSlot = Slots.m_Material;
					m_ForgeLastResult = -1;
					if(m_ForgeTargetSlot >= 0 && m_ForgeMaterialSlot >= 0 && IsUpgradeSlot(m_ForgeMaterialSlot))
					{
						if(!TryUpgradeCombine(m_ForgeTargetSlot, m_ForgeMaterialSlot))
							NotifyUpgradeFailed(m_ForgeTargetSlot, m_ForgeMaterialSlot);
					}
				}
			}
			else if(HoveredCombat >= 0 || HoveredBag >= 0)
			{
				const int DropSlot = HoveredCombat >= 0 ? HoveredCombat : HoveredBag;
				if(m_DragItem != DropSlot)
				{
					// Upgrade cards only combine onto a real weapon; empty slots just take the move/swap.
					if(IsUpgradeSlot(m_DragItem) || IsUpgradeSlot(DropSlot))
					{
						const int TargetSlot = IsUpgradeSlot(m_DragItem) ? DropSlot : m_DragItem;
						const int MaterialSlot = IsUpgradeSlot(m_DragItem) ? m_DragItem : DropSlot;
						const bool TargetOk = TargetSlot >= 0 && TargetSlot < InventoryLogic::NUM_SLOTS &&
											  CustomStuff()->m_aItem[TargetSlot].IsValid() &&
											  !IsUpgradeSlot(TargetSlot);
						if(TargetOk)
						{
							if(!TryUpgradeCombine(TargetSlot, MaterialSlot))
								NotifyUpgradeFailed(TargetSlot, MaterialSlot);
						}
						else
							Swap(m_DragItem, DropSlot);
					}
					else
						Swap(m_DragItem, DropSlot);
				}
			}
			else if(InventoryLogic::ShouldDropOutsideInventory(
						m_DragItem,
						CustomStuff()->m_aItem[m_DragItem].IsValid(),
						m_Moved,
						Hovered,
						InventoryLogic::PointInLayout(Hammer.m_Bounds, m_SelectorMouse.x, m_SelectorMouse.y) ||
							HoveredCombat >= 0))
				Drop(m_DragItem);
		}
		ReleasedClick = !m_Moved && Hovered >= 0;
		m_DragItem = -1;
		m_Moved = false;
		m_MoveTrigger = false;
	}
	if((Click || ReleasedClick) && Hovered >= 0)
	{
		const int64 Now = time_get();
		const bool DoubleClick = Hovered == m_LastClickSlot && Now - m_LastClickTime <= time_freq() * 350 / 1000;
		m_SelectedSlot = Hovered;
		m_ManualSelection = true;
		m_LastClickSlot = Hovered;
		m_LastClickTime = Now;
		m_DropConfirmSlot = -1;
		if(DoubleClick)
			ActivateSelection();
	}

	m_MouseTrigger = false;
}

void CInventory::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;
	if(m_DebugVisible)
	{
		m_Render = true;
		m_AppearAmount = 1.0f;
		m_Tab = InventoryLogic::TAB_INVENTORY;
		if(m_DebugTab == 2 && (m_ForgeTargetSlot < 0 || m_ForgeMaterialSlot < 0))
		{
			ClearForgeSelection();
			for(int Slot = 0; Slot < InventoryLogic::NUM_SLOTS; ++Slot)
			{
				if(!CustomStuff()->m_aItem[Slot].IsValid())
					continue;
				if(m_ForgeTargetSlot < 0)
					m_ForgeTargetSlot = Slot;
				else
				{
					m_ForgeMaterialSlot = Slot;
					break;
				}
			}
		}
	}
	const int ActiveSlot = CustomStuff()->m_WeaponSlot;
	if(m_pClient->m_Snap.m_pLocalCharacter && ActiveSlot >= 0 && ActiveSlot < 4)
	{
		CWeaponSpec ActiveWeapon;
		if(CWeaponCatalog::TryFromProtocol(m_pClient->m_Snap.m_pLocalCharacter->m_WeaponDefinitionId,
										   m_pClient->m_Snap.m_pLocalCharacter->m_WeaponLevel,
										   &ActiveWeapon) &&
		   ActiveWeapon == CustomStuff()->m_aItem[ActiveSlot])
			CustomStuff()->m_aItemAmmo[ActiveSlot] = max(0, m_pClient->m_Snap.m_pLocalCharacter->m_AmmoCount);
	}

	CustomStuff()->m_Inventory = false;

	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		m_Active = false;
		m_WasActive = false;
		m_Render = false;
		return;
	}
	if(m_Tab == InventoryLogic::TAB_FORGE)
		m_Tab = InventoryLogic::TAB_INVENTORY;
	if(m_WantedTab == InventoryLogic::TAB_FORGE)
		m_WantedTab = InventoryLogic::TAB_INVENTORY;
	if(m_DebugTab != 2 && !InventoryLogic::ForgeTabVisible(ForgeMode()))
		ClearForgeSelection();

	if(m_WantedTab < 0)
	{
		if(m_Active && !m_WasActive)
		{
			m_Render = !m_Render;
			m_WasActive = m_Active;
			if(!m_Render)
			{
				m_DragItem = -1;
				m_Moved = false;
				m_MoveTrigger = false;
				ClearForgeSelection();
			}
			if(m_Render)
			{
				m_ManualSelection = m_Tab == InventoryLogic::TAB_SHOP;
				SyncHeldSelection();
				const float HudW = 300.0f * Graphics()->ScreenAspect();
				const InventoryLogic::EPanelKind Kind = m_Tab == InventoryLogic::TAB_SHOP
															? InventoryLogic::PANEL_SHOP
															: InventoryLogic::PANEL_INVENTORY;
				const InventoryLogic::CLayout Dock =
					InventoryLogic::PanelLayout(HudW, 300.0f, 1.0f, 1.0f, Kind);
				m_SelectorMouse = vec2(Dock.m_X + Dock.m_W * 0.5f, Dock.m_Y + 48.0f);
				m_pClient->m_pControls->CancelQueuedWeaponSlot();
			}
		}

		if(!m_Active && m_WasActive)
			m_WasActive = m_Active;
	}

	if(m_WantedTab >= 0)
	{
		const int WantedTab = m_WantedTab;
		m_WantedTab = -1;
		m_Minimized = false;
		SetTab(WantedTab);
	}

	const int64 Now = time_get();
	const float Dt = clamp((float)((Now - m_LastAnimationTime) / (double)time_freq()), 0.0f, 0.05f);
	const float Target = m_Render ? 1.0f : 0.0f;
	m_AppearAmount += (Target - m_AppearAmount) * (1.0f - expf(-12.0f * Dt));
	m_AppearAmount = clamp(m_AppearAmount, 0.0f, 1.0f);
	m_LastAnimationTime = Now;

	CustomStuff()->m_Inventory = m_Render;
	if(m_Render)
		SyncHeldSelection();
	if(!m_Render && m_AppearAmount < 0.01f)
		return;

	const CNetObj_Shop *pShop = NearbyShop();
	if(m_Tab == InventoryLogic::TAB_SHOP && !pShop)
	{
		Close();
		return;
	}
	Graphics()->BlendNormal();
	const float HudW = 300.0f * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, HudW, 300.0f);
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 0.0f, HudW - 8.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 0.0f, 300.0f - 8.0f);
	if(!m_Mouse1)
		m_Mouse1Loaded = true;
	DrawSidebar(pShop);
	if(m_Mouse1)
		m_Mouse1Loaded = false;
	if(m_Render)
		RenderMouse();
}
