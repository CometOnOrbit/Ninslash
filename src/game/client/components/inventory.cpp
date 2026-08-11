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

static int WeaponRankSprite(const CWeaponSpec &Spec)
{
	if(!Spec.IsValid() || Spec.m_Level <= 0)
		return -1;
	CResolvedWeaponProfile Profile{};
	if(!CWeaponCatalog::TryResolve(Spec, &Profile) ||
	   WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_UPGRADE))
		return -1;
	const int MaxLevel = max(1, (int)Profile.m_Definition.m_MaxLevel);
	const int Level = min((int)Spec.m_Level, MaxLevel);
	const int Rank = 1 + (Level - 1) * 6 / max(1, MaxLevel - 1);
	return SPRITE_WEAPONRANK1 + clamp(Rank, 1, 7) - 1;
}

static void DrawWeaponRankIcon(IGraphics *pGraphics,
	CRenderTools *pRenderTools,
	const CWeaponSpec &Spec,
	vec2 Pos,
	float Size,
	float Alpha)
{
	const int RankSprite = WeaponRankSprite(Spec);
	if(RankSprite < 0)
		return;
	pGraphics->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	pRenderTools->SelectSprite(RankSprite);
	pRenderTools->DrawSprite(Pos.x, Pos.y, Size);
	pGraphics->QuadsEnd();
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
	m_ShopConfirmSlot = -1;
	CustomStuff()->m_Inventory = false;
}

void CInventory::SetTab(int Tab)
{
	if(Tab < 0 || Tab >= InventoryLogic::NUM_TABS)
		return;
	if(Tab == InventoryLogic::TAB_FORGE && !InventoryLogic::ForgeTabVisible(ForgeMode()))
		return;
	if(Tab == InventoryLogic::TAB_SHOP && !NearbyShop())
		return;
	m_Tab = Tab;
	m_SelectedSlot = 0;
	m_KeyboardFocus = 0;
	m_DragItem = -1;
	m_DropConfirmSlot = -1;
	m_ShopConfirmSlot = -1;
}

int CInventory::TabItemCount() const
{
	if(m_Tab == InventoryLogic::TAB_SHOP)
		return 5;
	return 12;
}

void CInventory::ActivateSelection()
{
	if(m_Tab == InventoryLogic::TAB_INVENTORY)
	{
		if(m_SelectedSlot < 0 || m_SelectedSlot >= 12 || !CustomStuff()->m_aItem[m_SelectedSlot].IsValid())
			return;
		const int Target = InventoryLogic::EquipTarget(m_SelectedSlot, CustomStuff()->m_WeaponSlot);
		if(m_SelectedSlot < 4)
			m_pClient->m_pControls->QueueWeaponSlot(Target + 2);
		else if(Target >= 0)
			Swap(m_SelectedSlot, Target);
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_INV4, 0);
	}
	else if(m_Tab == InventoryLogic::TAB_FORGE)
	{
		if(m_SelectedSlot < 0 || m_SelectedSlot >= 12 || !CustomStuff()->m_aItem[m_SelectedSlot].IsValid())
			return;
		InventoryLogic::CForgeSlots Slots{m_ForgeTargetSlot, m_ForgeMaterialSlot};
		Slots = InventoryLogic::AssignForgeSlot(
			Slots, m_SelectedSlot, m_ForgeTargetSlot < 0 || m_ForgeTargetSlot == m_SelectedSlot);
		m_ForgeTargetSlot = Slots.m_Target;
		m_ForgeMaterialSlot = Slots.m_Material;
		m_ForgeLastResult = -1;
	}
	else if(m_Tab == InventoryLogic::TAB_SHOP)
	{
		const CNetObj_Shop *pShop = NearbyShop();
		if(!pShop || m_SelectedSlot < 0 || m_SelectedSlot >= 5 || !ShopWeapon(pShop, m_SelectedSlot).IsValid())
			return;
		if(m_ShopConfirmSlot != m_SelectedSlot)
		{
			m_ShopConfirmSlot = m_SelectedSlot;
			return;
		}
		CNetMsg_Cl_InventoryAction Msg;
		Msg.m_Type = INVENTORYACTION_SHOP;
		Msg.m_Slot = m_SelectedSlot;
		Msg.m_Item1 = 0;
		Msg.m_Item2 = 0;
		Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
		m_ShopConfirmSlot = -1;
	}
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
		const int OpenTab = pSelf->ForgeMode() == 2 && pSelf->ForgeScreenNear() ? InventoryLogic::TAB_FORGE
																				: InventoryLogic::TAB_INVENTORY;
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
	if(pSelf->m_DebugTab == 1)
		pSelf->m_Tab = InventoryLogic::TAB_INVENTORY;
	else if(pSelf->m_DebugTab == 2)
		pSelf->m_Tab = InventoryLogic::TAB_FORGE;
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
	m_KeyboardFocus = 0;
	m_LastClickTime = 0;
	m_LastClickSlot = -1;
	m_DropConfirmSlot = -1;
	m_DropConfirmDeadline = 0;
	m_ShopConfirmSlot = -1;
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

	const bool ForgeAvailable = m_DebugTab == 2 || InventoryLogic::ForgeTabVisible(ForgeMode());
	const bool ShopAvailable = NearbyShop() != 0;
	if(Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT)
	{
		SetTab(InventoryLogic::NextAvailableTab(
			m_Tab, Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT ? -1 : 1, ForgeAvailable, ShopAvailable));
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
		m_KeyboardFocus = 1;
		m_DropConfirmSlot = -1;
		m_ShopConfirmSlot = -1;
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
	   TargetSlot == MaterialSlot ||
	   !InventoryLogic::ForgeUsable(ForgeMode(), ForgeScreenNear()))
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
	if(Recipe.m_Result != FORGERESULT_SUCCESS || CustomStuff()->m_Gold < Recipe.m_Cost)
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

bool CInventory::SubmitUpgradeDrag(int TargetSlot, int MaterialSlot)
{
	if(ForgeMode() < 1 || TargetSlot < 0 || TargetSlot >= 12 || MaterialSlot < 0 ||
	   MaterialSlot >= 12 || TargetSlot == MaterialSlot)
		return false;
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
				w, m_SelectorMouse, vec2(1, 0), 15.0f, true, 0, 1.0f, false, false, false, m_AppearAmount);
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
	const InventoryLogic::CLayout Layout = InventoryLogic::BottomOverlayLayout(
		ScreenW,
		ScreenH,
		UI()->Scale(),
		m_AppearAmount,
		m_Tab == InventoryLogic::TAB_FORGE || m_Tab == InventoryLogic::TAB_SHOP);
	const float Alpha = m_AppearAmount;
	vec4 Graphite = CMenus::ThemeBgDeep();
	Graphite.a = 0.82f * Alpha;
	vec4 Surface = CMenus::ThemeBgPanel();
	Surface.a = 0.74f * Alpha;
	vec4 Border = CMenus::ThemeAccentDim();
	Border.a = 0.82f * Alpha;
	const vec4 Amber(0.95f, 0.63f, 0.16f, 0.92f * Alpha);
	vec4 Cyan = CMenus::ThemeAccent();
	Cyan.a = 0.92f * Alpha;
	vec4 Danger = CMenus::ThemeDanger();
	Danger.a = 0.92f * Alpha;
	const bool Click = m_MouseTrigger && !m_Mouse1 && !m_Moved;
	const bool Press = m_MouseTrigger && m_Mouse1;
	auto Inside = [this](const CUIRect &Rect)
	{
		return m_SelectorMouse.x >= Rect.x && m_SelectorMouse.x <= Rect.x + Rect.w && m_SelectorMouse.y >= Rect.y &&
			   m_SelectorMouse.y <= Rect.y + Rect.h;
	};
	auto Box = [this](const CUIRect &Rect, vec4 Color, float Radius = 3.0f)
	{
		RenderTools()->DrawUIRect(&Rect, Color, CUI::CORNER_ALL, Radius);
	};
	auto Label = [this, Alpha](const CUIRect &Rect,
							   const char *pText,
							   float Size,
							   int Align = -1,
							   vec4 Color = vec4(0.96f, 0.97f, 0.98f, 1.0f))
	{
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a * Alpha);
		UI()->DoLabel(&Rect, pText, Size, Align);
		TextRender()->TextColor(1, 1, 1, 1);
	};

	CUIRect Dim = {0, 0, ScreenW, ScreenH};
	Box(Dim, vec4(0.0f, 0.0f, 0.0f, 0.22f * Alpha), 0.0f);
	CUIRect Panel = {Layout.m_X, Layout.m_Y, Layout.m_W, Layout.m_H};
	Box(Panel, Graphite, 4.0f);
	CUIRect Edge = {Panel.x, Panel.y, 2.0f, Panel.h};
	Box(Edge, Amber, 0.0f);

	const bool ForgeAvailable = m_DebugTab == 2 || InventoryLogic::ForgeTabVisible(ForgeMode());
	const bool ShopAvailable = pShop != 0;
	const int aCandidateTabs[] = {InventoryLogic::TAB_INVENTORY, InventoryLogic::TAB_FORGE, InventoryLogic::TAB_SHOP};
	const char *s_apCandidateNames[] = {"Inventory", "Forge", "Shop"};
	int aTabs[3];
	const char *apTabNames[3];
	int NumTabs = 0;
	for(int i = 0; i < 3; ++i)
		if((aCandidateTabs[i] != InventoryLogic::TAB_FORGE || ForgeAvailable) &&
		   (aCandidateTabs[i] != InventoryLogic::TAB_SHOP || ShopAvailable))
		{
			aTabs[NumTabs] = aCandidateTabs[i];
			apTabNames[NumTabs++] = s_apCandidateNames[i];
		}
	const float InnerX = Panel.x + 6.0f;
	const float InnerW = Panel.w - 12.0f;
	const float TabGap = 2.0f;
	const float TabW = (InnerW - TabGap * (NumTabs - 1)) / NumTabs;
	for(int i = 0; i < NumTabs; ++i)
	{
		CUIRect TabRect = {InnerX + i * (TabW + TabGap), Panel.y + 6.0f, TabW, 18.0f};
		Box(TabRect, aTabs[i] == m_Tab ? Amber : Surface, 3.0f);
		Label(TabRect,
			  Localize(apTabNames[i]),
			  5.8f,
			  0,
			  aTabs[i] == m_Tab ? vec4(0.09f, 0.08f, 0.05f, 1) : vec4(0.86f, 0.89f, 0.90f, 1));
		if(Click && Inside(TabRect))
			SetTab(aTabs[i]);
	}

	CUIRect ResourceBar = {InnerX, Panel.y + 27.0f, InnerW, 13.0f};
	Box(ResourceBar, vec4(0.07f, 0.085f, 0.092f, 0.98f * Alpha), 3.0f);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s  %d", Localize("Gold"), CustomStuff()->m_Gold);
	Label({ResourceBar.x + 5.0f, ResourceBar.y, ResourceBar.w * 0.5f - 5.0f, ResourceBar.h}, aBuf, 5.8f, -1, Amber);
	str_format(aBuf, sizeof(aBuf), "%s  %d", Localize("Kits"), CustomStuff()->m_LocalKits);
	Label({ResourceBar.x + ResourceBar.w * 0.5f, ResourceBar.y, ResourceBar.w * 0.5f - 5.0f, ResourceBar.h},
		  aBuf,
		  5.8f,
		  1,
		  Cyan);

	const float ContentY = Panel.y + 44.0f;
	if(m_Tab == InventoryLogic::TAB_FORGE)
	{
		const CNetObj_GameInfo *pGameInfo = m_pClient->m_Snap.m_pGameInfoObj;
		const CWeaponSpec SelectedSpec =
			m_SelectedSlot >= 0 && m_SelectedSlot < 12 ? CustomStuff()->m_aItem[m_SelectedSlot] : CWeaponSpec{};
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
		const bool HasTarget = TargetSpec.IsValid();
		const bool HasMaterial = MaterialSpec.IsValid();
		const bool CanAfford = CustomStuff()->m_Gold >= Recipe.m_Cost;
		const bool ForgeUsable = m_DebugTab == 2 || InventoryLogic::ForgeUsable(ForgeMode(), ForgeScreenNear());
		const bool CanForge = Recipe.m_Result == FORGERESULT_SUCCESS && CanAfford && ForgeUsable && !m_ForgePending;

		auto DrawWeaponFrame = [&](const CUIRect &Frame,
								   const char *pTitle,
								   const CWeaponSpec &Spec,
								   vec4 AccentColor,
								   const char *pEmptyText)
		{
			Box(Frame, Border, 4.0f);
			CUIRect Inner = {Frame.x + 1.0f, Frame.y + 1.0f, Frame.w - 2.0f, Frame.h - 2.0f};
			Box(Inner, vec4(Surface.r, Surface.g, Surface.b, 0.96f * Alpha), 3.0f);
			CUIRect Mark = {Frame.x, Frame.y + 7.0f, 2.0f, Frame.h - 14.0f};
			Box(Mark, AccentColor, 1.0f);
			Label({Frame.x + 5.0f, Frame.y + 3.0f, Frame.w - 10.0f, 7.0f}, pTitle, 5.2f, -1, AccentColor);
			if(Spec.IsValid())
			{
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
				RenderTools()->SetShadersForWeapon(Spec);
				RenderTools()->RenderWeapon(Spec,
											vec2(Frame.x + Frame.w * 0.5f, Frame.y + 22.0f),
											vec2(1, 0),
											5.5f,
											true,
											0,
											1.0f,
											false,
											false,
											false,
											Alpha);
				Graphics()->ShaderEnd();
				char aName[128];
				Label({Frame.x + 4.0f, Frame.y + 33.0f, Frame.w - 8.0f, 8.0f},
					  WeaponDisplayName(Spec, aName, sizeof(aName)),
					  4.7f,
					  0);
				str_format(aBuf, sizeof(aBuf), "%s %d", Localize("Weapon level"), Spec.m_Level);
				Label({Frame.x + 4.0f, Frame.y + 42.0f, Frame.w - 8.0f, 7.0f},
					  aBuf,
					  4.0f,
					  0,
					  vec4(0.72f, 0.76f, 0.78f, 1.0f));
			}
			else
				Label({Frame.x + 4.0f, Frame.y + 18.0f, Frame.w - 8.0f, 20.0f},
					  pEmptyText,
					  4.8f,
					  0,
					  vec4(0.68f, 0.72f, 0.74f, 1.0f));
		};

		const float InputGap = 3.0f;
		const float CoreW = 42.0f;
		const float InputW = (InnerW - CoreW - InputGap * 2.0f) * 0.5f;
		CUIRect TargetFrame = {InnerX, ContentY + 2.0f, InputW, 52.0f};
		CUIRect CoreFrame = {InnerX + InputW + InputGap, ContentY + 2.0f, CoreW, 52.0f};
		CUIRect MaterialFrame = {CoreFrame.x + CoreW + InputGap, ContentY + 2.0f, InputW, 52.0f};
		const bool DraggingWeapon = m_Moved && m_DragItem >= 0 && m_DragItem < InventoryLogic::NUM_SLOTS &&
									CustomStuff()->m_aItem[m_DragItem].IsValid();
		const bool TargetDropHovered = DraggingWeapon && Inside(TargetFrame);
		const bool MaterialDropHovered = DraggingWeapon && Inside(MaterialFrame);
		char aTargetTitle[64];
		char aMaterialTitle[64];
		str_format(aTargetTitle, sizeof(aTargetTitle), "1  %s", Localize("Target"));
		str_format(aMaterialTitle, sizeof(aMaterialTitle), "2  %s", Localize("Material"));
		DrawWeaponFrame(TargetFrame, aTargetTitle, TargetSpec, Cyan, Localize("Place target"));
		DrawWeaponFrame(MaterialFrame, aMaterialTitle, MaterialSpec, Amber, Localize("Place material"));
		const vec4 CoreAccent = CanForge ? Cyan : (HasTarget || HasMaterial ? Amber : Border);
		Box(CoreFrame, CoreAccent, 5.0f);
		Box({CoreFrame.x + 1.0f, CoreFrame.y + 1.0f, CoreFrame.w - 2.0f, CoreFrame.h - 2.0f},
			vec4(Graphite.r, Graphite.g, Graphite.b, 0.86f * Alpha),
			4.0f);
		Box({CoreFrame.x + 7.0f, CoreFrame.y + 16.0f, CoreFrame.w - 14.0f, CoreFrame.h - 23.0f},
			vec4(CoreAccent.r, CoreAccent.g, CoreAccent.b, 0.14f * Alpha),
			4.0f);
		const float CoreScanPhase = fmod(time_get() / (double)time_freq() * 0.6f, 1.0f);
		Box({CoreFrame.x + 8.0f,
				 CoreFrame.y + 17.0f + CoreScanPhase * (CoreFrame.h - 25.0f),
				 CoreFrame.w - 16.0f,
				 1.0f},
			vec4(CoreAccent.r, CoreAccent.g, CoreAccent.b, 0.34f * Alpha),
			0.0f);
		Box({CoreFrame.x + CoreFrame.w * 0.5f - 1.0f, CoreFrame.y + 20.0f, 2.0f, 16.0f}, CoreAccent, 1.0f);
		Box({CoreFrame.x + 13.0f, CoreFrame.y + 27.0f, CoreFrame.w - 26.0f, 2.0f}, CoreAccent, 1.0f);
		Label({CoreFrame.x + 4.0f, CoreFrame.y + 3.0f, CoreFrame.w - 8.0f, 8.0f},
			  Localize("Weapon forge"),
			  4.3f,
			  0,
			  CoreAccent);
		Label({CoreFrame.x + 3.0f, CoreFrame.y + 38.0f, CoreFrame.w - 6.0f, 8.0f},
			  CanForge ? Localize("Ready") : (HasTarget || HasMaterial ? Localize("Selected") : Localize("Forge")),
			  4.0f,
			  0,
			  CoreAccent);
		Box({TargetFrame.x + TargetFrame.w, CoreFrame.y + 27.0f, InputGap, 2.0f},
			vec4(Cyan.r, Cyan.g, Cyan.b, (HasTarget ? 0.82f : 0.26f) * Alpha),
			0.0f);
		Box({CoreFrame.x + CoreW, CoreFrame.y + 27.0f, InputGap, 2.0f},
			vec4(Amber.r, Amber.g, Amber.b, (HasMaterial ? 0.82f : 0.26f) * Alpha),
			0.0f);
		if(TargetDropHovered)
			Box(TargetFrame, vec4(Cyan.r, Cyan.g, Cyan.b, 0.30f * Alpha), 4.0f);
		if(MaterialDropHovered)
			Box(MaterialFrame, vec4(Amber.r, Amber.g, Amber.b, 0.30f * Alpha), 4.0f);
		if(Click && Inside(TargetFrame) && SelectedSpec.IsValid() && m_SelectedSlot != m_ForgeMaterialSlot)
		{
			const InventoryLogic::CForgeSlots Slots =
				InventoryLogic::AssignForgeSlot({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_SelectedSlot, true);
			m_ForgeTargetSlot = Slots.m_Target;
			m_ForgeMaterialSlot = Slots.m_Material;
			m_ForgeLastResult = -1;
		}
		if(Click && Inside(MaterialFrame) && SelectedSpec.IsValid() && m_SelectedSlot != m_ForgeTargetSlot)
		{
			const InventoryLogic::CForgeSlots Slots =
				InventoryLogic::AssignForgeSlot({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_SelectedSlot, false);
			m_ForgeTargetSlot = Slots.m_Target;
			m_ForgeMaterialSlot = Slots.m_Material;
			m_ForgeLastResult = -1;
		}

		CUIRect ResultFrame = {InnerX, ContentY + 57.0f, InnerW, 50.0f};
		Box(ResultFrame, Recipe.m_Product.IsValid() ? vec4(Cyan.r, Cyan.g, Cyan.b, 0.55f * Alpha) : Border, 4.0f);
		CUIRect ResultInner = {ResultFrame.x + 1.0f, ResultFrame.y + 1.0f, ResultFrame.w - 2.0f, ResultFrame.h - 2.0f};
		Box(ResultInner, Surface, 3.0f);
		Label({ResultFrame.x + 5.0f, ResultFrame.y + 3.0f, ResultFrame.w - 10.0f, 8.0f},
			  Localize("Result"),
			  5.2f,
			  -1,
			  Cyan);
		if(Recipe.m_Product.IsValid())
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
			RenderTools()->SetShadersForWeapon(Recipe.m_Product);
			RenderTools()->RenderWeapon(Recipe.m_Product,
										vec2(ResultFrame.x + 29.0f, ResultFrame.y + 28.0f),
										vec2(1, 0),
										6.0f,
										true,
										0,
										1.0f,
										false,
										false,
										false,
										Alpha);
			Graphics()->ShaderEnd();
			char aName[128];
			Label({ResultFrame.x + 55.0f, ResultFrame.y + 13.0f, ResultFrame.w - 60.0f, 9.0f},
				  WeaponDisplayName(Recipe.m_Product, aName, sizeof(aName)),
				  5.3f,
				  -1);
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s %d   %s %d/%d",
					   Localize("Weapon level"),
					   Recipe.m_Product.m_Level,
					   Localize("Ammo"),
					   Recipe.m_ProductAmmo,
					   Recipe.m_ProductMaxAmmo);
			Label({ResultFrame.x + 55.0f, ResultFrame.y + 24.0f, ResultFrame.w - 60.0f, 8.0f}, aBuf, 4.6f, -1, Cyan);
			if(Recipe.m_Cost > 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s  %d %s", Localize("Cost"), Recipe.m_Cost, Localize("Gold"));
				Label({ResultFrame.x + 55.0f, ResultFrame.y + 34.0f, ResultFrame.w - 60.0f, 8.0f},
					  aBuf,
					  4.8f,
					  -1,
					  CanAfford ? Amber : Danger);
			}
		}
		else
			Label({ResultFrame.x + 6.0f, ResultFrame.y + 15.0f, ResultFrame.w - 12.0f, 18.0f},
				  HasTarget ? (HasMaterial ? Localize("No valid result") : Localize("Choose material below"))
							: Localize("Choose target below"),
				  5.0f,
				  0,
				  vec4(0.52f, 0.57f, 0.59f, 1.0f));

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
		vec4 StatusColor = vec4(0.65f, 0.69f, 0.71f, 1.0f);
		if(!ForgeUsable)
		{
			pStatus = Localize("Move closer to a screen");
			StatusColor = Danger;
		}
		else if(m_ForgePending)
		{
			pStatus = Localize("Waiting for server");
			StatusColor = Cyan;
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
			StatusColor = m_ForgeLastResult == FORGERESULT_SUCCESS ? Cyan : Danger;
		}
		else if(HasTarget && !HasMaterial)
			pStatus = Localize("Select a material weapon");
		else if(!HasTarget && HasMaterial)
			pStatus = Localize("Select a target weapon");
		else if(HasTarget && HasMaterial && Recipe.m_Result == FORGERESULT_NO_CHANGE)
		{
			pStatus = Localize("Result would not change");
			StatusColor = Danger;
		}
		else if(HasTarget && HasMaterial && Recipe.m_Result != FORGERESULT_SUCCESS)
		{
			pStatus = Localize("Invalid recipe");
			StatusColor = Danger;
		}
		else if(Recipe.m_Result == FORGERESULT_SUCCESS && !CanAfford)
		{
			pStatus = Localize("Not enough gold");
			StatusColor = Danger;
		}
		else if(Recipe.m_Result == FORGERESULT_SUCCESS)
		{
			pStatus = Localize("Ready to forge");
			StatusColor = Cyan;
		}
		str_format(aBuf, sizeof(aBuf), "%s%s%s", pOperation ? pOperation : "", pOperation ? "  ·  " : "", pStatus);
		CUIRect StatusBar = {InnerX, ContentY + 110.0f, InnerW, 14.0f};
		Box(StatusBar, vec4(StatusColor.r, StatusColor.g, StatusColor.b, 0.15f * Alpha), 3.0f);
		const vec4 StatusMark = !HasTarget && !HasMaterial && !m_ForgePending && m_ForgeLastResult < 0
									? vec4(0.38f, 0.42f, 0.44f, 0.75f * Alpha)
									: StatusColor;
		Box({StatusBar.x, StatusBar.y + 3.0f, 2.0f, StatusBar.h - 6.0f}, StatusMark, 1.0f);
		Label({StatusBar.x + 5.0f, StatusBar.y, StatusBar.w - 10.0f, StatusBar.h}, aBuf, 4.9f, 0, StatusColor);

		Label({InnerX, ContentY + 127.0f, InnerW * 0.5f, 8.0f}, Localize("Equipment"), 5.1f, -1, Cyan);
		Label({InnerX + InnerW * 0.5f, ContentY + 127.0f, InnerW * 0.5f, 8.0f}, Localize("Inventory"), 4.8f, 1, vec4(0.70f, 0.75f, 0.77f, 1.0f));
		Box({InnerX + 1.0f, ContentY + 135.0f, InnerW - 2.0f, 1.0f}, vec4(Cyan.r, Cyan.g, Cyan.b, 0.24f * Alpha), 0.0f);
		const float GridGap = 3.0f;
		const float CellW = (InnerW - GridGap * 3.0f) / 4.0f;
		const float CellH = 23.0f;
		int Hovered = -1;
		for(int Slot = 0; Slot < 12; ++Slot)
		{
			const int Row = Slot < 4 ? 2 : (Slot - 4) / 4;
			const int Column = Slot % 4;
			CUIRect CellRect = {
				InnerX + Column * (CellW + GridGap), ContentY + 137.0f + Row * (CellH + GridGap), CellW, CellH};
			const bool Selected = Slot == m_SelectedSlot;
			Box(CellRect, Selected ? vec4(Amber.r, Amber.g, Amber.b, 0.48f * Alpha) : Surface, 3.0f);
			if(Selected)
				Box({CellRect.x + 3.0f, CellRect.y + CellRect.h - 2.0f, CellRect.w - 6.0f, 2.0f}, Amber, 1.0f);
			if(Slot == m_ForgeTargetSlot || Slot == m_ForgeMaterialSlot)
			{
				const vec4 RoleColor = Slot == m_ForgeTargetSlot ? Cyan : Amber;
				CUIRect Mark = {CellRect.x, CellRect.y, CellRect.w, 2.0f};
				Box(Mark, RoleColor, 0.0f);
				Label({CellRect.x + 2.0f, CellRect.y + 1.0f, 7.0f, 7.0f},
					  Slot == m_ForgeTargetSlot ? "1" : "2",
					  4.4f,
					  -1,
					  RoleColor);
			}
			const CWeaponSpec Spec = CustomStuff()->m_aItem[Slot];
			if(Spec.IsValid() && Slot != m_DragItem)
			{
				DrawWeaponRankIcon(Graphics(),
					RenderTools(),
					Spec,
					vec2(CellRect.x + CellRect.w * 0.5f, CellRect.y + 6.5f),
					11.0f,
					Alpha);
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
				RenderTools()->SetShadersForWeapon(Spec);
				RenderTools()->RenderWeapon(Spec,
											vec2(CellRect.x + CellRect.w * 0.5f, CellRect.y + CellRect.h * 0.54f),
											vec2(1, 0),
											5.0f,
											true,
											0,
											1.0f,
											false,
											false,
											false,
											Alpha);
				Graphics()->ShaderEnd();
			}
			if(Inside(CellRect))
				Hovered = Slot;
		}
		Box({InnerX + 1.0f, ContentY + 137.0f + 2.0f * (CellH + GridGap) - 2.0f, InnerW - 2.0f, 1.0f},
			vec4(Amber.r, Amber.g, Amber.b, 0.32f * Alpha),
			0.0f);
		if(Press && Hovered >= 0 && CustomStuff()->m_aItem[Hovered].IsValid())
		{
			m_SelectedSlot = Hovered;
			m_DragItem = Hovered;
		}
		bool ReleasedClick = false;
		if(m_MouseTrigger && !m_Mouse1 && m_DragItem >= 0)
		{
			if(m_Moved)
			{
				const InventoryLogic::EForgeDropTarget DropTarget =
					Inside(TargetFrame)		? InventoryLogic::FORGE_DROP_TARGET
					: Inside(MaterialFrame) ? InventoryLogic::FORGE_DROP_MATERIAL
											: InventoryLogic::FORGE_DROP_NONE;
				const InventoryLogic::CForgeSlots Slots =
					InventoryLogic::DropForgeItem({m_ForgeTargetSlot, m_ForgeMaterialSlot}, m_DragItem, DropTarget);
				if(Slots.m_Target != m_ForgeTargetSlot || Slots.m_Material != m_ForgeMaterialSlot)
				{
					m_ForgeTargetSlot = Slots.m_Target;
					m_ForgeMaterialSlot = Slots.m_Material;
					m_ForgeLastResult = -1;
				}
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
			m_LastClickSlot = Hovered;
			m_LastClickTime = Now;
			if(DoubleClick && CustomStuff()->m_aItem[Hovered].IsValid())
				ActivateSelection();
		}

		CUIRect ForgeButton = {InnerX, Panel.y + Panel.h - 25.0f, InnerW, 18.0f};
		Box(ForgeButton, CanForge ? Amber : vec4(Border.r, Border.g, Border.b, 0.65f * Alpha), 3.0f);
		if(Recipe.m_Result == FORGERESULT_SUCCESS)
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s   %d %s",
					   m_ForgePending ? Localize("Forging...") : Localize("Forge"),
					   Recipe.m_Cost,
					   Localize("Gold"));
		else
			str_copy(aBuf, Localize("Forge"), sizeof(aBuf));
		Label(ForgeButton, aBuf, 6.2f, 0, CanForge ? vec4(0.09f, 0.07f, 0.03f, 1.0f) : vec4(0.68f, 0.71f, 0.72f, 1.0f));
		if(Click && Inside(ForgeButton) && CanForge)
			SubmitForge();
		m_MouseTrigger = false;
		return;
	}
	if(m_Tab == InventoryLogic::TAB_INVENTORY)
	{
		const float Gap = 3.0f;
		const float Cell = (InnerW - Gap * 3.0f) / 4.0f;
		const float CellH = min(34.0f, Cell * 0.82f);
		const float ActionY = ContentY + 46.0f;
		const float GridTop = Panel.y + Panel.h - 3.0f * (CellH + Gap);
		CUIRect Section = {InnerX, GridTop - 9.0f, InnerW, 8.0f};
		Label({Section.x, Section.y, Section.w * 0.5f, Section.h}, Localize("Equipment"), 6.0f, -1, Cyan);
		Label({Section.x + Section.w * 0.5f, Section.y, Section.w * 0.5f, Section.h},
			  Localize("Inventory"),
			  5.2f,
			  1,
			  vec4(0.70f, 0.75f, 0.77f, 1.0f));
		int Hovered = -1;
		for(int Slot = 0; Slot < 12; ++Slot)
		{
			const int Row = Slot < 4 ? 2 : (Slot - 4) / 4;
			const int Column = Slot % 4;
			CUIRect CellRect = {InnerX + Column * (Cell + Gap), GridTop + Row * (CellH + Gap), Cell, CellH};
			const bool Selected = Slot == m_SelectedSlot;
			const bool Active =
				m_Tab == InventoryLogic::TAB_INVENTORY && Slot < 4 && Slot == CustomStuff()->m_WeaponSlot;
			Box(CellRect, Selected ? vec4(Amber.r, Amber.g, Amber.b, 0.48f * Alpha) : Surface, 3.0f);
			if(Selected)
				Box({CellRect.x + 3.0f, CellRect.y + CellRect.h - 2.0f, CellRect.w - 6.0f, 2.0f}, Amber, 1.0f);
			if(Active)
			{
				CUIRect Mark = {CellRect.x, CellRect.y, 2.0f, CellRect.h};
				Box(Mark, Cyan, 0.0f);
			}
			if(Slot < 4)
			{
				str_format(aBuf, sizeof(aBuf), "%d", Slot + 1);
				Label({CellRect.x + 2, CellRect.y + 1, 8, 7}, aBuf, 5.0f, -1, Cyan);
			}
			const CWeaponSpec Spec = CustomStuff()->m_aItem[Slot];
			if(Spec.IsValid() && Slot != m_DragItem)
			{
				DrawWeaponRankIcon(Graphics(),
					RenderTools(),
					Spec,
					vec2(CellRect.x + CellRect.w * 0.5f, CellRect.y + 8.5f),
					12.0f,
					Alpha);
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
				RenderTools()->SetShadersForWeapon(Spec);
				RenderTools()->RenderWeapon(Spec,
											vec2(CellRect.x + CellRect.w * 0.5f, CellRect.y + CellRect.h * 0.55f),
											vec2(1, 0),
											5.0f,
											true,
											0,
											1.0f,
											false,
											false,
											false,
											Alpha);
				Graphics()->ShaderEnd();
				const int Ammo = CustomStuff()->m_aItemAmmo[Slot];
				CResolvedWeaponProfile SlotProfile{};
				const bool InfiniteAmmo =
					CWeaponCatalog::TryResolve(Spec, &SlotProfile) && !SlotProfile.m_Combat.m_UsesAmmo;
				if(Ammo < 0 || InfiniteAmmo)
					str_copy(aBuf, Localize("Infinite"), sizeof(aBuf));
				else
					str_format(aBuf, sizeof(aBuf), "%d", Ammo);
				Label({CellRect.x + CellRect.w - 13, CellRect.y + CellRect.h - 8, 11, 7}, aBuf, 4.8f, 1);
			}
			if(Inside(CellRect))
				Hovered = Slot;
		}
		Box({InnerX + 1.0f, GridTop + 2.0f * (CellH + Gap) - 2.0f, InnerW - 2.0f, 1.0f},
			vec4(Amber.r, Amber.g, Amber.b, 0.32f * Alpha),
			0.0f);
		if(Press && Hovered >= 0 && CustomStuff()->m_aItem[Hovered].IsValid())
		{
			m_SelectedSlot = Hovered;
			m_DragItem = Hovered;
		}
		bool ReleasedClick = false;
		if(m_MouseTrigger && !m_Mouse1 && m_DragItem >= 0)
		{
			if(m_Moved && Hovered >= 0 && Hovered != m_DragItem && m_Tab == InventoryLogic::TAB_INVENTORY)
			{
				if(!SubmitUpgradeDrag(Hovered, m_DragItem))
					Swap(m_DragItem, Hovered);
			}
			else if(InventoryLogic::ShouldDropOutsideInventory(m_DragItem,
														 CustomStuff()->m_aItem[m_DragItem].IsValid(),
														 m_Moved,
														 Hovered,
														 Inside(Panel)))
				Drop(m_DragItem);
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
			m_LastClickSlot = Hovered;
			m_LastClickTime = Now;
			m_DropConfirmSlot = -1;
			if(DoubleClick)
				ActivateSelection();
		}

		CWeaponSpec SelectedSpec;
		if(m_SelectedSlot >= 0 && m_SelectedSlot < 12)
			SelectedSpec = CustomStuff()->m_aItem[m_SelectedSlot];
		CResolvedWeaponProfile Profile{};
		const bool HasProfile = CWeaponCatalog::TryResolve(SelectedSpec, &Profile);
		CUIRect Detail = {InnerX, ContentY, InnerW, max(43.0f, ActionY - ContentY - 3.0f)};
		Box(Detail, Surface, 3.0f);
		char aName[128];
		Label({Detail.x + 5, Detail.y + 3, Detail.w - 10, 9},
			  SelectedSpec.IsValid() ? WeaponDisplayName(SelectedSpec, aName, sizeof(aName)) : Localize("Empty slot"),
			  6.4f,
			  -1);
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
			Label({Detail.x + 5, Detail.y + 14, Detail.w - 10, 8}, aBuf, 5.1f, -1, Cyan);
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s %.0f   %s",
					   Localize("Damage"),
					   Damage,
					   FiringModeName(Profile.m_Combat.m_FiringType, Profile.m_Combat.m_FullAuto));
			Label({Detail.x + 5, Detail.y + 23, Detail.w - 10, 8}, aBuf, 5.1f, -1);
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
			Label({Detail.x + 5, Detail.y + 32, Detail.w - 10, 8}, aBuf, 4.8f, -1, Amber);
		}

		CUIRect Equip = {InnerX, ActionY, (InnerW - 3) * 0.58f, 18};
		CUIRect DropButton = {Equip.x + Equip.w + 3, ActionY, InnerW - Equip.w - 3, 18};
		Box(Equip, Amber, 3.0f);
		const bool ConfirmDrop = InventoryLogic::DropConfirmationActive(
			m_DropConfirmSlot, m_SelectedSlot, time_get(), m_DropConfirmDeadline);
		Box(DropButton, ConfirmDrop ? Danger : vec4(Danger.r, Danger.g, Danger.b, 0.35f * Alpha), 3.0f);
		Label(Equip, Localize("Equip"), 6.2f, 0, vec4(0.08f, 0.07f, 0.04f, 1));
		Label(DropButton, ConfirmDrop ? Localize("Confirm") : Localize("Drop"), 5.8f, 0);
		if(Click && Inside(Equip))
			ActivateSelection();
		if(Click && Inside(DropButton))
			RequestDrop();
	}
	else if(m_Tab == InventoryLogic::TAB_SHOP && pShop)
	{
		const int FeaturedSlot = clamp(m_SelectedSlot, 0, 4);
		const CWeaponSpec FeaturedSpec = ShopWeapon(pShop, FeaturedSlot);
		const bool FeaturedValid = FeaturedSpec.IsValid();
		const int FeaturedPrice = m_pClient->m_pPveRoguelite->ShopCost(ShopWeaponCost(pShop, FeaturedSlot));
		const bool CanAfford = CustomStuff()->m_Gold >= FeaturedPrice;
		const bool ConfirmPurchase = m_ShopConfirmSlot == FeaturedSlot;
		const float CoreY = ContentY + 2.0f;
		const float CoreH = 76.0f;
		const float SelectorW = 17.0f;
		CUIRect Prev = {InnerX, CoreY + 27.0f, SelectorW, 22.0f};
		CUIRect Next = {InnerX + InnerW - SelectorW, CoreY + 27.0f, SelectorW, 22.0f};
		CUIRect Featured = {InnerX + SelectorW + 3.0f,
			CoreY,
			InnerW - SelectorW * 2.0f - 6.0f,
			CoreH};
		const vec4 CoreAccent = FeaturedValid ? (CanAfford ? Cyan : Danger) : Border;
		Box(Featured, CoreAccent, 5.0f);
		Box({Featured.x + 1.0f, Featured.y + 1.0f, Featured.w - 2.0f, Featured.h - 2.0f},
			vec4(Graphite.r, Graphite.g, Graphite.b, 0.86f * Alpha),
			4.0f);
		const float FeaturedScanPhase = fmod(time_get() / (double)time_freq() * 0.45f, 1.0f);
		Box({Featured.x + 8.0f,
				 Featured.y + 13.0f + FeaturedScanPhase * (Featured.h - 29.0f),
				 Featured.w - 16.0f,
				 1.0f},
			vec4(CoreAccent.r, CoreAccent.g, CoreAccent.b, 0.24f * Alpha),
			0.0f);
		Label({Featured.x + 5.0f, Featured.y + 3.0f, Featured.w - 10.0f, 8.0f},
			  Localize("Shop"),
			  4.6f,
			  -1,
			  CoreAccent);
		if(FeaturedValid)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
			RenderTools()->SetShadersForWeapon(FeaturedSpec);
			RenderTools()->RenderWeapon(FeaturedSpec,
										vec2(Featured.x + Featured.w * 0.5f, Featured.y + 29.0f),
										vec2(1, 0),
										7.0f,
										true,
										0,
										1.0f,
										false,
										false,
										false,
										Alpha);
			Graphics()->ShaderEnd();
			char aName[128];
			Label({Featured.x + 5.0f, Featured.y + 45.0f, Featured.w - 10.0f, 9.0f},
				  WeaponDisplayName(FeaturedSpec, aName, sizeof(aName)),
				  5.6f,
				  0);
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s %d   %s %d",
					   Localize("Weapon level"),
					   FeaturedSpec.m_Level,
					   Localize("Price"),
					   FeaturedPrice);
			Label({Featured.x + 5.0f, Featured.y + 57.0f, Featured.w - 10.0f, 8.0f},
				  aBuf,
				  4.7f,
				  0,
				  CanAfford ? Amber : Danger);
		}
		else
			Label({Featured.x + 5.0f, Featured.y + 29.0f, Featured.w - 10.0f, 16.0f},
				  Localize("Empty slot"),
				  5.0f,
				  0,
				  vec4(0.66f, 0.70f, 0.72f, 1.0f));
		const vec4 PrevColor = Cyan;
		const vec4 NextColor = Cyan;
		Box(Prev, Surface, 4.0f);
		Box(Next, Surface, 4.0f);
		Box({Prev.x + Prev.w - 2.0f, Prev.y + 4.0f, 2.0f, Prev.h - 8.0f}, PrevColor, 1.0f);
		Box({Next.x, Next.y + 4.0f, 2.0f, Next.h - 8.0f}, NextColor, 1.0f);
		Label(Prev, "<", 8.0f, 0, PrevColor);
		Label(Next, ">", 8.0f, 0, NextColor);
		for(int Slot = 0; Slot < 5; ++Slot)
		{
			const float DotW = Slot == FeaturedSlot ? 7.0f : 3.0f;
			const float DotX = Featured.x + Featured.w * 0.5f + (Slot - 2) * 8.0f - DotW * 0.5f;
			Box({DotX, Featured.y + 70.0f, DotW, 2.0f},
				Slot == FeaturedSlot ? CoreAccent : vec4(Border.r, Border.g, Border.b, 0.72f * Alpha),
				1.0f);
		}
		auto SelectShopSlot = [&](int Slot)
		{
			m_SelectedSlot = (Slot + 5) % 5;
			m_ShopConfirmSlot = -1;
		};
		if(Click && Inside(Prev))
			SelectShopSlot(FeaturedSlot - 1);
		if(Click && Inside(Next))
			SelectShopSlot(FeaturedSlot + 1);
		if(Click && Inside(Featured))
			m_ShopConfirmSlot = -1;

		const float GridGap = 3.0f;
		const float CellW = (InnerW - GridGap * 3.0f) / 4.0f;
		const float CellH = 25.0f;
		const float GridY = CoreY + CoreH + 15.0f;
		Label({InnerX, GridY - 9.0f, InnerW, 8.0f}, Localize("Inventory"), 5.0f, -1, Cyan);
		for(int Slot = 0; Slot < 12; ++Slot)
		{
			const int Row = Slot < 4 ? 2 : (Slot - 4) / 4;
			const int Column = Slot % 4;
			CUIRect Cell = {InnerX + Column * (CellW + GridGap), GridY + Row * (CellH + GridGap), CellW, CellH};
			const bool CombatSlot = Slot < 4;
			Box(Cell, CombatSlot ? vec4(Cyan.r, Cyan.g, Cyan.b, 0.13f * Alpha) : Surface, 3.0f);
			str_format(aBuf, sizeof(aBuf), "%d", Slot + 1);
			Label({Cell.x + 2.0f, Cell.y + 1.0f, 9.0f, 7.0f}, aBuf, 4.1f, -1, CombatSlot ? Cyan : Border);
			const CWeaponSpec Spec = CustomStuff()->m_aItem[Slot];
			if(Spec.IsValid())
			{
				DrawWeaponRankIcon(Graphics(),
					RenderTools(),
					Spec,
					vec2(Cell.x + Cell.w * 0.5f, Cell.y + 7.5f),
					11.0f,
					Alpha);
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
				RenderTools()->SetShadersForWeapon(Spec);
				RenderTools()->RenderWeapon(Spec,
					vec2(Cell.x + Cell.w * 0.5f, Cell.y + Cell.h * 0.55f),
					vec2(1, 0),
					5.0f,
					true,
					0,
					1.0f,
					false,
					false,
					false,
					Alpha);
				Graphics()->ShaderEnd();
			}
		}
		Box({InnerX + 1.0f, GridY + 2.0f * (CellH + GridGap) - 2.0f, InnerW - 2.0f, 1.0f},
			vec4(Amber.r, Amber.g, Amber.b, 0.32f * Alpha),
			0.0f);

		CUIRect PriceRail = {InnerX, GridY + 3.0f * (CellH + GridGap) + 1.0f, InnerW, 13.0f};
		Box(PriceRail, vec4(CoreAccent.r, CoreAccent.g, CoreAccent.b, 0.12f * Alpha), 3.0f);
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s  %d %s",
				   Localize("Price"),
				   FeaturedPrice,
				   Localize("Gold"));
		Label(PriceRail, aBuf, 4.7f, 0, FeaturedValid ? (CanAfford ? Amber : Danger) : Border);
		CUIRect Buy = {InnerX, Panel.y + Panel.h - 25.0f, InnerW, 18.0f};
		const bool BuyEnabled = FeaturedValid && CanAfford;
		Box(Buy,
			ConfirmPurchase ? Amber : (BuyEnabled ? Cyan : vec4(Border.r, Border.g, Border.b, 0.64f * Alpha)),
			4.0f);
		Label(Buy,
			  ConfirmPurchase ? Localize("Confirm purchase") : Localize("Buy"),
			  6.0f,
			  0,
			  ConfirmPurchase || BuyEnabled ? vec4(0.06f, 0.08f, 0.08f, 1.0f) : vec4(0.68f, 0.71f, 0.72f, 1.0f));
		if(Click && Inside(Buy) && BuyEnabled)
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
		m_Tab = m_DebugTab == 2 ? InventoryLogic::TAB_FORGE : InventoryLogic::TAB_INVENTORY;
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
	const int CurrentForgeMode = ForgeMode();
	if(m_DebugTab != 2 && (m_Tab == InventoryLogic::TAB_FORGE || m_WantedTab == InventoryLogic::TAB_FORGE) &&
	   !InventoryLogic::ForgeTabVisible(CurrentForgeMode))
	{
		ClearForgeSelection();
		m_Tab = InventoryLogic::TAB_INVENTORY;
		m_WantedTab = -1;
	}
	// m_Render = m_Active;

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
			}
			if(m_Render)
			{
				m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(),
					m_Tab == InventoryLogic::TAB_INVENTORY ? 245.0f : 82.0f);
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
	if(!m_Render && m_AppearAmount < 0.01f)
		return;

	const CNetObj_Shop *pShop = NearbyShop();
	if(m_Tab == InventoryLogic::TAB_SHOP && !pShop)
	{
		m_Tab = InventoryLogic::TAB_INVENTORY;
		m_SelectedSlot = 0;
		m_ShopConfirmSlot = -1;
	}
	if(m_DebugTab != 2 && m_Tab == InventoryLogic::TAB_FORGE &&
	   !InventoryLogic::ForgeTabVisible(ForgeMode()))
	{
		m_Tab = InventoryLogic::TAB_INVENTORY;
		m_SelectedSlot = 0;
		ClearForgeSelection();
		m_ForgePending = false;
	}

	Graphics()->BlendNormal();
	const float HudW = 300.0f * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, HudW, 300.0f);
	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 0.0f, HudW - 8.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 0.0f, 292.0f);
	if(!m_Mouse1)
		m_Mouse1Loaded = true;
	DrawSidebar(pShop);
	if(m_Mouse1)
		m_Mouse1Loaded = false;
	if(m_Render)
		RenderMouse();
}
