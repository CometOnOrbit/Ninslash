#include <base/math.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/game_data.h>
#include <generated/protocol.h>

#include <game/buildables.h>
#include <game/client/components/binds.h>
#include <game/client/components/camera.h>
#include <game/client/components/controls.h>
#include <game/client/components/inventory.h>
#include <game/client/components/pve_roguelite.h>
#include <game/client/components/sounds.h>
#include <game/client/customstuff.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/collision.h>
#include <game/nodes.h>
#include <game/nodes_collision.h>

#include "build_placement.h"

using namespace BuildPlacementLogic;

static const char *s_apBuildNames[NUM_BUILDABLES] = {"Block",
													 "Hard block",
													 "Barrel",
													 "Power barrel",
													 "Turret stand",
													 "Flamer",
													 "Electric wall",
													 "Teslacoil",
						 "Shield generator"};

static int BuildCount(const CGameClient *pClient)
{
	return pClient->IsNodes() ? (int)NODES_BUILDING_COUNT : (int)NUM_BUILDABLES;
}

static const char *BuildName(const CGameClient *pClient, int Building)
{
	if(pClient->IsNodes())
		return g_aNodesBuildingInfo[clamp(Building, 0, NODES_BUILDING_COUNT - 1)].m_pName;
	return s_apBuildNames[clamp(Building, 0, NUM_BUILDABLES - 1)];
}

static int BuildSprite(const CGameClient *pClient, int Building)
{
	if(!pClient->IsNodes())
		return SPRITE_KIT_BLOCK1 + Building;
	switch(Building)
	{
		case NODES_REACTOR: return SPRITE_REACTOR_DESTROYED;
		case NODES_SPAWN: return SPRITE_KIT_GENERATOR;
		case NODES_REPEATER: return SPRITE_GENERATOR;
		case NODES_SHIELD: return SPRITE_SHIELD1;
		case NODES_TURRET_GUN:
		case NODES_TURRET_SHOTGUN: return SPRITE_KIT_TURRET;
		default: return SPRITE_POWERUPPER;
	}
}

static int NodesBuildingImage(int Building, int Team)
{
	if(Team != TEAM_BLUE)
		Team = TEAM_RED;
	switch(Building)
	{
		case NODES_REACTOR: return Team == TEAM_BLUE ? IMAGE_BUILDING_REACTOR_BLUE : IMAGE_BUILDING_REACTOR_RED;
		case NODES_SPAWN: return Team == TEAM_BLUE ? IMAGE_BUILDING_SPAWN_BLUE : IMAGE_BUILDING_SPAWN_RED;
		case NODES_AMMO_SHOTGUN: return Team == TEAM_BLUE ? IMAGE_BUILDING_AMMO1_BLUE : IMAGE_BUILDING_AMMO1_RED;
		case NODES_HEALTH: return Team == TEAM_BLUE ? IMAGE_BUILDING_HEALTH_BLUE : IMAGE_BUILDING_HEALTH_RED;
		case NODES_REPEATER: return Team == TEAM_BLUE ? IMAGE_BUILDING_REPEATER_BLUE : IMAGE_BUILDING_REPEATER_RED;
		case NODES_SHIELD: return Team == TEAM_BLUE ? IMAGE_BUILDING_SHIELD_BLUE : IMAGE_BUILDING_SHIELD_RED;
		case NODES_AMMO_GRENADE: return Team == TEAM_BLUE ? IMAGE_BUILDING_AMMO2_BLUE : IMAGE_BUILDING_AMMO2_RED;
		case NODES_TELEPORT: return Team == TEAM_BLUE ? IMAGE_BUILDING_TELEPORT_BLUE : IMAGE_BUILDING_TELEPORT_RED;
		case NODES_ARMOR: return Team == TEAM_BLUE ? IMAGE_BUILDING_ARMOR_BLUE : IMAGE_BUILDING_ARMOR_RED;
		case NODES_AMMO_LASER: return Team == TEAM_BLUE ? IMAGE_BUILDING_AMMO3_BLUE : IMAGE_BUILDING_AMMO3_RED;
		default: return IMAGE_BUILDINGS;
	}
}

static int NodesBuildingSprite(int Building, int Team)
{
	if(Team != TEAM_BLUE)
		Team = TEAM_RED;
	switch(Building)
	{
		case NODES_REACTOR: return Team == TEAM_BLUE ? SPRITE_BUILDING_REACTOR_BLUE : SPRITE_BUILDING_REACTOR_RED;
		case NODES_SPAWN: return Team == TEAM_BLUE ? SPRITE_BUILDING_SPAWN_BLUE : SPRITE_BUILDING_SPAWN_RED;
		case NODES_AMMO_SHOTGUN: return Team == TEAM_BLUE ? SPRITE_BUILDING_AMMO1_BLUE : SPRITE_BUILDING_AMMO1_RED;
		case NODES_HEALTH: return Team == TEAM_BLUE ? SPRITE_BUILDING_HEALTH_BLUE : SPRITE_BUILDING_HEALTH_RED;
		case NODES_REPEATER: return Team == TEAM_BLUE ? SPRITE_BUILDING_REPEATER_BLUE : SPRITE_BUILDING_REPEATER_RED;
		case NODES_SHIELD: return Team == TEAM_BLUE ? SPRITE_BUILDING_SHIELD_BLUE : SPRITE_BUILDING_SHIELD_RED;
		case NODES_AMMO_GRENADE: return Team == TEAM_BLUE ? SPRITE_BUILDING_AMMO2_BLUE : SPRITE_BUILDING_AMMO2_RED;
		case NODES_TELEPORT: return Team == TEAM_BLUE ? SPRITE_BUILDING_TELEPORT_BLUE : SPRITE_BUILDING_TELEPORT_RED;
		case NODES_ARMOR: return Team == TEAM_BLUE ? SPRITE_BUILDING_ARMOR_BLUE : SPRITE_BUILDING_ARMOR_RED;
		case NODES_AMMO_LASER: return Team == TEAM_BLUE ? SPRITE_BUILDING_AMMO3_BLUE : SPRITE_BUILDING_AMMO3_RED;
		case NODES_TURRET_GUN:
		case NODES_TURRET_SHOTGUN: return SPRITE_STAND;
		default: return SPRITE_POWERUPPER;
	}
}

static bool IsBlock(int Building)
{
	return Building == BUILDABLE_BLOCK1 || Building == BUILDABLE_BLOCK2;
}

static bool CanMountOnCeiling(int Building)
{
	return Building == BUILDABLE_TURRET || Building == BUILDABLE_TESLACOIL;
}

static bool IsForceFieldExempt(int Building)
{
	return Building == BUILDABLE_BARREL || Building == BUILDABLE_POWERBARREL;
}

static float WheelAngle(int Building, int Count)
{
	return -pi / 2.0f + 2.0f * pi * Building / Count;
}

static vec2 WheelItemPosition(vec2 Center, float Radius, int Building, int Count)
{
	const float Angle = WheelAngle(Building, Count);
	return Center + vec2(cosf(Angle), sinf(Angle)) * Radius;
}

static void DrawDisc(IGraphics *pGraphics, vec2 Center, float Radius, vec4 Color, int Segments = 48)
{
	IGraphics::CFreeformItem aItems[32];
	int NumItems = 0;
	pGraphics->SetColor(Color.r, Color.g, Color.b, Color.a);
	for(int i = 0; i < Segments; i += 2)
	{
		const float A0 = 2.0f * pi * i / Segments;
		const float A1 = 2.0f * pi * (i + 1) / Segments;
		const float A2 = 2.0f * pi * (i + 2) / Segments;
		aItems[NumItems++] = IGraphics::CFreeformItem(Center.x,
													  Center.y,
													  Center.x + cosf(A0) * Radius,
													  Center.y + sinf(A0) * Radius,
													  Center.x + cosf(A2) * Radius,
													  Center.y + sinf(A2) * Radius,
													  Center.x + cosf(A1) * Radius,
													  Center.y + sinf(A1) * Radius);
	}
	pGraphics->QuadsDrawFreeform(aItems, NumItems);
}

CBuildPlacement::CBuildPlacement()
{
	m_DebugWheel = 0;
	m_DebugBuilding = -1;
	m_DebugValidity = -1;
	OnReset();
}

void CBuildPlacement::OnReset()
{
	m_State.Cancel();
	m_Trigger.SetDown(false);
	m_WheelCursor = vec2(0, 0);
	m_ConfirmPressed = false;
}

void CBuildPlacement::OnConsoleInit()
{
	Console()->Register("+buildmenu", "", CFGFLAG_CLIENT, ConKeyBuildMenu, this, "Open build wheel");
	Console()->Register("dbg_build_wheel", "i", CFGFLAG_CLIENT, ConDebugWheel, this, "Force build wheel preview");
	Console()->Register("dbg_build_placement",
						"ii",
						CFGFLAG_CLIENT,
						ConDebugPlacement,
						this,
						"Force building preview: building validity (-1 disables)");
}

void CBuildPlacement::ConKeyBuildMenu(IConsole::IResult *pResult, void *pUserData)
{
	CBuildPlacement *pSelf = static_cast<CBuildPlacement *>(pUserData);
	if(pResult->GetInteger(0))
		pSelf->OpenWheel();
	else
		pSelf->ReleaseWheel();
}

void CBuildPlacement::ConDebugWheel(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CBuildPlacement *>(pUserData)->m_DebugWheel = pResult->GetInteger(0) != 0;
}

void CBuildPlacement::ConDebugPlacement(IConsole::IResult *pResult, void *pUserData)
{
	CBuildPlacement *pSelf = static_cast<CBuildPlacement *>(pUserData);
	pSelf->m_DebugBuilding = pResult->GetInteger(0);
	pSelf->m_DebugValidity = pResult->GetInteger(1);
	if(pSelf->m_DebugBuilding < 0 || pSelf->m_DebugBuilding >= NUM_BUILDABLES)
		pSelf->m_DebugBuilding = -1;
}

bool CBuildPlacement::CanOpenWheel() const
{
	return m_pClient->BuildingEnabled() && CustomStuff()->m_LocalAlive && !m_pClient->m_Snap.m_SpecInfo.m_Active &&
		   Client()->State() != IClient::STATE_DEMOPLAYBACK && !m_pClient->m_pInventory->IsVisible() &&
		   !m_pClient->GameplayInputFullyCaptured();
}

int CBuildPlacement::SelectedBuilding() const
{
	return m_DebugBuilding >= 0 ? m_DebugBuilding : m_State.Selected();
}

int CBuildPlacement::BuildingPrice(int Building) const
{
	if(m_pClient->IsNodes())
		return Building >= 0 && Building < NODES_BUILDING_COUNT ? g_aNodesBuildingInfo[Building].m_Price : 0;
	return m_pClient->m_pPveRoguelite->BuildingCost(BuildableCost[Building]);
}

bool CBuildPlacement::CanAfford(int Building) const
{
	return Building >= 0 && Building < BuildCount(m_pClient) && CustomStuff()->m_LocalKits >= BuildingPrice(Building);
}

void CBuildPlacement::OpenWheel()
{
	if(!CanOpenWheel() || WheelActive())
		return;
	m_State.OpenWheel();
	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	if(!Input()->UsingGamepad())
	{
		float IgnoredX = 0.0f;
		float IgnoredY = 0.0f;
		Input()->GetMousePosition(&IgnoredX, &IgnoredY);
	}
	const vec2 Aim = m_pClient->m_pControls->m_MousePos;
	m_WheelCursor = length(Aim) > 0.001f ? normalize(Aim) * 76.0f : vec2(0, 0);
}

void CBuildPlacement::ReleaseWheel()
{
	if(m_State.State() != STATE_WHEEL)
		return;
	const int Hovered = WheelSector(m_WheelCursor, 38.0f, BuildCount(m_pClient));
	const bool Affordable = CanAfford(Hovered);
	if(Hovered >= 0 && !Affordable)
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_GUI_DENIED1, 0);
	else if(Hovered >= 0)
		m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_UI_PICK, 0);
	m_State.ReleaseWheel(Hovered, Affordable);
}

void CBuildPlacement::Cancel()
{
	m_State.Cancel();
	m_ConfirmPressed = false;
	m_Trigger.SetDown(false);
}

bool CBuildPlacement::OnMouseMove(float x, float y)
{
	if(!WheelActive())
		return false;
	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);
	const float MaxRadius = 132.0f;
	if(Input()->UsingGamepad())
	{
		Input()->GetGamepadAim(&x, &y);
		m_WheelCursor = vec2(x, y) * MaxRadius;
	}
	else
	{
		Input()->GetRelativePosition(&x, &y);
		m_WheelCursor += vec2(x, y);
	}
	if(length(m_WheelCursor) > MaxRadius)
		m_WheelCursor = normalize(m_WheelCursor) * MaxRadius;
	return true;
}

bool CBuildPlacement::OnInput(IInput::CEvent Event)
{
	if(!Active())
		return false;
	const bool Press = (Event.m_Flags & IInput::FLAG_PRESS) != 0;
	const bool Release = (Event.m_Flags & IInput::FLAG_RELEASE) != 0;
	const char *pBinding = m_pClient->m_pBinds->Get(Event.m_Key);
	if(str_comp(pBinding, "+buildmenu") == 0)
		return false;

	if(WheelActive())
	{
		static const char *s_apMovement[] = {"+left",
											 "+right",
											 "+down",
											 "+jump",
											 "+charge",
											 "+gamepadleft",
											 "+gamepadright",
											 "+gamepaddown",
											 "+gamepadjump"};
		for(const char *pMovement : s_apMovement)
			if(str_comp(pBinding, pMovement) == 0)
				return false;
		return true;
	}

	if((Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_MOUSE_2 || Event.m_Key == KEY_GAMEPAD_BUTTON_B) && Press)
	{
		Cancel();
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_1 || Event.m_Key == KEY_GAMEPAD_TRIGGER_RIGHT)
	{
		if(Press)
		{
			m_ConfirmPressed = true;
			m_Trigger.SetDown(true);
		}
		if(Release)
		{
			m_Trigger.SetDown(false);
		}
		return true;
	}
	return false;
}

void CBuildPlacement::EvaluatePlacement()
{
	const int Selected = SelectedBuilding();
	m_Result = CBuildPlacementResult{};
	if(Selected < 0 || Selected >= BuildCount(m_pClient))
		return;
	m_Result.m_Price = BuildingPrice(Selected);
	if(m_pClient->IsNodes())
	{
		const CNodesBuildingInfo &Info = g_aNodesBuildingInfo[Selected];
		const vec2 Requested = m_pClient->m_pControls->m_TargetPos;
		vec2 Pos(Requested.x, Requested.y);
		Pos.x = floorf(Pos.x / 32.0f) * 32.0f + 16.0f;
		vec2 Ground;
		if(NodesBuildingFindGround(Collision(), Requested, Info, &Pos, &Ground))
		{
			m_Result.m_HasAnchor = true;
			m_Result.m_AnchorPosition = Ground;
		}
		m_Result.m_PreviewPosition = Pos;
		m_Result.m_Position = Pos;
		m_Result.m_MinimumDistance = Info.m_Radius + 28.0f;
		const CNodesBuildingBounds Bounds = NodesBuildingBounds(Pos, Info);
		const bool InBounds = NodesBuildingInBounds(Collision(), Bounds);
		const bool Clear = !NodesBuildingSpaceOccupied(Collision(), Bounds);
		const bool Grounded = m_Result.m_HasAnchor && NodesBuildingHasGroundSupport(Collision(), Bounds);
		const bool TooClose = m_pClient->BuildingNear(Pos, m_Result.m_MinimumDistance);
		m_Result.m_Valid = InBounds && Clear && Grounded && !TooClose && CanAfford(Selected);
		if(m_Result.m_Valid)
			m_Result.m_Reason = INVALID_NONE;
		else if(!CanAfford(Selected))
			m_Result.m_Reason = INVALID_NO_KITS;
		else if(TooClose)
			m_Result.m_Reason = INVALID_TOO_CLOSE;
		else if(!m_Result.m_HasAnchor || !Grounded)
			m_Result.m_Reason = INVALID_NO_SUPPORT;
		else
			m_Result.m_Reason = INVALID_OCCUPIED;
		return;
	}
	m_Result.m_MinimumDistance = Selected == BUILDABLE_TESLACOIL ? 74.0f : (IsBlock(Selected) ? 32.0f : 48.0f);
	vec2 Pos = m_pClient->m_pControls->m_TargetPos;
	bool Valid = false;
	EInvalidReason Reason = INVALID_OCCUPIED;
	const int SnapRange = 128;

	if(IsBlock(Selected))
	{
		Pos.x = floorf(Pos.x / 32.0f) * 32.0f + 16.0f;
		Pos.y = floorf(Pos.y / 32.0f) * 32.0f + 16.0f;
		m_Result.m_AnchorPosition = Pos;
		m_Result.m_HasAnchor = true;
		if(Collision()->IsTileSolid(Pos.x, Pos.y) || !Collision()->CanBuildBlock(Pos))
			Reason = INVALID_OCCUPIED;
		else if(!Collision()->IsTileSolid(Pos.x, Pos.y - 32) && !Collision()->IsTileSolid(Pos.x, Pos.y + 32) &&
				!Collision()->IsTileSolid(Pos.x - 32, Pos.y) && !Collision()->IsTileSolid(Pos.x + 32, Pos.y))
			Reason = INVALID_NO_SUPPORT;
		else
			Valid = true;
	}
	else if(Collision()->IsTileSolid(Pos.x, Pos.y))
		Reason = INVALID_OCCUPIED;
	else if(Selected == BUILDABLE_FLAMETRAP)
	{
		vec2 Hit;
		vec2 To = Pos + vec2(SnapRange, 0);
		if(Collision()->IntersectLine(Pos, To, 0x0, &Hit))
		{
			Pos = Hit - vec2(BuildableOffset[Selected], 0);
			m_Result.m_AnchorPosition = Hit;
			m_Result.m_HasAnchor = true;
			m_Result.m_FlipHorizontal = true;
			Valid = true;
		}
		else
		{
			To = Pos - vec2(SnapRange, 0);
			if(Collision()->IntersectLine(Pos, To, 0x0, &Hit))
			{
				Pos = Hit + vec2(BuildableOffset[Selected], 0);
				m_Result.m_AnchorPosition = Hit;
				m_Result.m_HasAnchor = true;
				Valid = true;
			}
		}
		if(!Valid)
			Reason = INVALID_NO_WALL;
		else
		{
			const int Cx = m_Result.m_FlipHorizontal ? 16 : -16;
			const vec2 A = m_Result.m_AnchorPosition;
			if(!Collision()->IsTileSolid(A.x + Cx, A.y - 26) || !Collision()->IsTileSolid(A.x + Cx, A.y + 26))
			{
				Valid = false;
				Reason = INVALID_NO_WALL;
			}
			else if(Collision()->IsTileSolid(A.x - Cx, A.y - 26) || Collision()->IsTileSolid(A.x - Cx, A.y + 26))
			{
				Valid = false;
				Reason = INVALID_NO_CLEARANCE;
			}
		}
	}
	else
	{
		const vec2 Original = Pos;
		vec2 Hit;
		vec2 To = Pos + vec2(0, SnapRange);
		if(Collision()->IntersectLine(Pos, To, 0x0, &Hit))
		{
			Pos = Hit + vec2(0, BuildableOffset[Selected]);
			m_Result.m_AnchorPosition = Hit;
			m_Result.m_HasAnchor = true;
			Valid = true;
			if(!Collision()->IsTileSolid(Hit.x - 22, Hit.y + 2) || !Collision()->IsTileSolid(Hit.x + 22, Hit.y + 2))
			{
				Valid = false;
				Reason = INVALID_NO_SUPPORT;
			}
			else if(Collision()->IsTileSolid(Hit.x, Hit.y - 64))
			{
				Valid = false;
				Reason = INVALID_NO_CLEARANCE;
			}
			else if(!IsForceFieldExempt(Selected) && Collision()->IsForceTile(Hit.x, Hit.y + 16))
			{
				Valid = false;
				Reason = INVALID_FORCE_TILE;
			}
		}
		else
			Reason = INVALID_NO_SUPPORT;

		if(!Valid && CanMountOnCeiling(Selected))
		{
			To = Original - vec2(0, SnapRange);
			if(Collision()->IntersectLine(Original, To, 0x0, &Hit))
			{
				Pos = Hit - vec2(0, BuildableOffset[Selected]);
				m_Result.m_AnchorPosition = Hit;
				m_Result.m_HasAnchor = true;
				m_Result.m_FlipVertical = true;
				Valid = true;
				if(!Collision()->IsTileSolid(Hit.x - 22, Hit.y - 12) ||
				   !Collision()->IsTileSolid(Hit.x + 22, Hit.y - 12))
				{
					Valid = false;
					Reason = INVALID_NO_CEILING;
				}
				else if(Collision()->IsTileSolid(Hit.x, Hit.y + 64) || Collision()->IsTileSolid(Hit.x, Hit.y + 6))
				{
					Valid = false;
					Reason = INVALID_NO_CLEARANCE;
				}
			}
			else
				Reason = INVALID_NO_CEILING;
		}
		if(Valid && Selected == BUILDABLE_LIGHTNINGWALL)
		{
			To = Pos - vec2(0, 550);
			if(!Collision()->IntersectLine(Pos, To, 0x0, &Hit))
			{
				Valid = false;
				Reason = INVALID_NO_CEILING;
			}
		}
		if(Valid && (Collision()->IsTileSolid(Pos.x - 12, Pos.y) || Collision()->IsTileSolid(Pos.x + 12, Pos.y)))
		{
			Valid = false;
			Reason = INVALID_NO_CLEARANCE;
		}
	}

	if(Valid && m_pClient->BuildingNear(Pos, m_Result.m_MinimumDistance))
	{
		Valid = false;
		Reason = INVALID_TOO_CLOSE;
	}
	if(m_Result.m_Price > CustomStuff()->m_LocalKits)
	{
		Valid = false;
		Reason = INVALID_NO_KITS;
	}

	m_Result.m_PreviewPosition = Pos;
	m_Result.m_Position = Pos + (IsBlock(Selected) ? vec2(0, 0) : vec2(0, 18));
	if(m_Result.m_FlipVertical)
	{
		if(Selected == BUILDABLE_TURRET)
			m_Result.m_Position.y += BuildableOffset[Selected] - 18;
		else if(Selected == BUILDABLE_TESLACOIL)
			m_Result.m_Position.y += BuildableOffset[Selected] - 38;
	}
	m_Result.m_Valid = Valid;
	m_Result.m_Reason = Valid ? INVALID_NONE : Reason;
	if(m_DebugBuilding >= 0 && m_DebugValidity >= 0)
	{
		m_Result.m_Valid = m_DebugValidity != 0;
		m_Result.m_Reason = m_Result.m_Valid ? INVALID_NONE : INVALID_OCCUPIED;
	}
}

void CBuildPlacement::SendPlacement()
{
	if(!m_Result.m_Valid || m_DebugBuilding >= 0)
		return;
	const int Selected = m_State.Selected();
	const bool Continuous = !m_pClient->IsNodes() && IsBlock(Selected);
	const int GridX = round_to_int(m_Result.m_Position.x / 32.0f);
	const int GridY = round_to_int(m_Result.m_Position.y / 32.0f);
	if(!m_Trigger.ShouldSend(Continuous, m_ConfirmPressed, GridX, GridY))
		return;
	CNetMsg_Cl_UseKit Msg;
	Msg.m_Kit = m_pClient->IsNodes() ? NodesBuildingKit(Selected) : Selected;
	Msg.m_X = round_to_int(m_Result.m_Position.x);
	Msg.m_Y = round_to_int(m_Result.m_Position.y);
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CBuildPlacement::MapGameGroup()
{
	CMapItemGroup *pGroup = Layers()->GameGroup();
	float aPoints[4];
	RenderTools()->MapscreenToWorld(m_pClient->m_pCamera->m_Center.x,
									m_pClient->m_pCamera->m_Center.y,
									pGroup->m_ParallaxX / 100.0f,
									pGroup->m_ParallaxY / 100.0f,
									pGroup->m_OffsetX,
									pGroup->m_OffsetY,
									Graphics()->ScreenAspect(),
									m_pClient->m_pCamera->m_Zoom,
									aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

const char *CBuildPlacement::InvalidReasonText(EInvalidReason Reason) const
{
	static const char *s_apReasons[] = {0,
										"Not enough kits",
										"Position occupied",
										"Needs ground support",
										"Not enough clearance",
										"Too close to another building",
										"Needs a wall",
										"Needs a ceiling anchor",
										"Blocked by force field"};
	if(Reason == INVALID_NONE)
		return "";
	return Localize(s_apReasons[clamp((int)Reason, 0, (int)INVALID_FORCE_TILE)]);
}

void CBuildPlacement::RenderPlacement()
{
	const int Selected = SelectedBuilding();
	if(Selected < 0 || Selected >= BuildCount(m_pClient))
		return;
	MapGameGroup();
	const vec4 Color = m_Result.m_Valid ? vec4(0.16f, 0.92f, 0.40f, 0.9f) : vec4(0.95f, 0.18f, 0.16f, 0.9f);

	Graphics()->TextureSet(-1);
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, 0.55f);
	if(m_Result.m_HasAnchor)
	{
		IGraphics::CLineItem AnchorLine(m_Result.m_AnchorPosition.x,
										m_Result.m_AnchorPosition.y,
										m_Result.m_PreviewPosition.x,
										m_Result.m_PreviewPosition.y);
		Graphics()->LinesDraw(&AnchorLine, 1);
	}
	IGraphics::CLineItem aRange[32];
	for(int i = 0; i < 32; ++i)
	{
		const float A0 = 2.0f * pi * i / 32.0f;
		const float A1 = 2.0f * pi * (i + 1) / 32.0f;
		aRange[i] = IGraphics::CLineItem(m_Result.m_PreviewPosition.x + cosf(A0) * m_Result.m_MinimumDistance,
										 m_Result.m_PreviewPosition.y + sinf(A0) * m_Result.m_MinimumDistance,
										 m_Result.m_PreviewPosition.x + cosf(A1) * m_Result.m_MinimumDistance,
										 m_Result.m_PreviewPosition.y + sinf(A1) * m_Result.m_MinimumDistance);
	}
	Graphics()->LinesDraw(aRange, 32);
	Graphics()->LinesEnd();

	const int LocalTeam = m_pClient->m_Snap.m_pLocalInfo ? m_pClient->m_Snap.m_pLocalInfo->m_Team : TEAM_RED;
	Graphics()->TextureSet(g_pData->m_aImages[m_pClient->IsNodes() ? NodesBuildingImage(Selected, LocalTeam) : IMAGE_BUILDINGS].m_Id);
	Graphics()->ShaderBegin(SHADER_GRAYSCALE, 0.0f);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, 0.92f);
	int Flags = 0;
	if(m_Result.m_FlipHorizontal)
		Flags |= SPRITE_FLAG_FLIP_X;
	if(m_Result.m_FlipVertical)
		Flags |= SPRITE_FLAG_FLIP_Y;
	RenderTools()->SelectSprite(m_pClient->IsNodes() ? NodesBuildingSprite(Selected, LocalTeam) : BuildSprite(m_pClient, Selected), Flags);
	if(m_pClient->IsNodes())
	{
		const CNodesBuildingVisualInfo Visual = NodesBuildingPlacementVisualInfo(Selected);
		const float Size = 64.0f;
		const float Scale = 0.5f;
		const float Width = Visual.m_Width * Size * Scale;
		const float Height = Visual.m_Height * Size * Scale;
		const float PosY = m_Result.m_PreviewPosition.y + Size * 0.25f;
		IGraphics::CQuadItem Preview(m_Result.m_PreviewPosition.x, PosY - Height * 0.5f, Width, Height);
		Graphics()->QuadsDraw(&Preview, 1);
	}
	else
		RenderTools()->DrawSprite(m_Result.m_PreviewPosition.x, m_Result.m_PreviewPosition.y, BuildableSize[Selected]);
	Graphics()->QuadsEnd();
	Graphics()->ShaderEnd();

	if(!m_Result.m_Valid)
	{
		const float W = 300.0f * Graphics()->ScreenAspect();
		Graphics()->MapScreen(0, 0, W, 300.0f);
		CUIRect Reason = {W * 0.5f - 90.0f, 255.0f, 180.0f, 18.0f};
		RenderTools()->DrawUIRect(&Reason, vec4(0.08f, 0.06f, 0.06f, 0.88f), CUI::CORNER_ALL, 3.0f);
		TextRender()->TextColor(1.0f, 0.35f, 0.30f, 1.0f);
		UI()->DoLabel(&Reason, InvalidReasonText(m_Result.m_Reason), 7.0f, 0);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CBuildPlacement::RenderWheel()
{
	const float W = 300.0f * Graphics()->ScreenAspect();
	const float H = 300.0f;
	const vec2 Center(W * 0.5f, H * 0.5f);
	const float Radius = clamp(112.0f * UI()->Scale(), 112.0f, 132.0f);
	const float Inner = 48.0f;
	const int Hovered = WheelSector(m_WheelCursor, 38.0f, BuildCount(m_pClient));
	Graphics()->MapScreen(0, 0, W, H);
	CUIRect Dim = {0, 0, W, H};
	RenderTools()->DrawUIRect(&Dim, vec4(0.015f, 0.02f, 0.024f, 0.48f), CUI::CORNER_ALL, 0.0f);
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	DrawDisc(Graphics(), Center, Radius + 5.0f, vec4(0.025f, 0.032f, 0.038f, 0.96f));
	const int Count = BuildCount(m_pClient);
	for(int Sector = 0; Sector < Count; ++Sector)
	{
		const bool Affordable = CanAfford(Sector);
		const vec4 Color =
			Sector == Hovered
				? (Affordable ? vec4(0.96f, 0.64f, 0.14f, 0.96f) : vec4(0.48f, 0.20f, 0.18f, 0.94f))
				: (Affordable ? vec4(0.085f, 0.105f, 0.115f, 0.96f) : vec4(0.055f, 0.062f, 0.066f, 0.94f));
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
		for(int Slice = 0; Slice < 5; ++Slice)
		{
			const float A0 = -pi / 2.0f + 2.0f * pi * (Sector - 0.5f + Slice / 5.0f) / Count;
			const float A1 = -pi / 2.0f + 2.0f * pi * (Sector - 0.5f + (Slice + 1) / 5.0f) / Count;
			IGraphics::CFreeformItem Quad(Center.x + cosf(A0) * Inner,
										  Center.y + sinf(A0) * Inner,
										  Center.x + cosf(A0) * Radius,
										  Center.y + sinf(A0) * Radius,
										  Center.x + cosf(A1) * Inner,
										  Center.y + sinf(A1) * Inner,
										  Center.x + cosf(A1) * Radius,
										  Center.y + sinf(A1) * Radius);
			Graphics()->QuadsDrawFreeform(&Quad, 1);
		}
	}
	DrawDisc(Graphics(), Center, Inner - 2.0f, vec4(0.035f, 0.045f, 0.052f, 0.99f));
	Graphics()->QuadsEnd();

	Graphics()->LinesBegin();
	Graphics()->SetColor(0.40f, 0.46f, 0.48f, 0.42f);
	IGraphics::CLineItem aSeparators[NODES_BUILDING_COUNT];
	for(int Sector = 0; Sector < Count; ++Sector)
	{
		const float Angle = WheelAngle(Sector, Count) - pi / Count;
		aSeparators[Sector] = IGraphics::CLineItem(Center.x + cosf(Angle) * (Inner + 2.0f),
												   Center.y + sinf(Angle) * (Inner + 2.0f),
												   Center.x + cosf(Angle) * (Radius - 2.0f),
												   Center.y + sinf(Angle) * (Radius - 2.0f));
	}
	Graphics()->LinesDraw(aSeparators, Count);
	Graphics()->LinesEnd();

	const int LocalTeam = m_pClient->m_Snap.m_pLocalInfo ? m_pClient->m_Snap.m_pLocalInfo->m_Team : TEAM_RED;
	for(int Sector = 0; Sector < Count; ++Sector)
	{
		const vec2 Pos = WheelItemPosition(Center, Inner + (Radius - Inner) * 0.42f, Sector, Count);
		const bool Affordable = CanAfford(Sector);
		Graphics()->TextureSet(m_pClient->IsNodes() ? g_pData->m_aImages[NodesBuildingImage(Sector, LocalTeam)].m_Id
																	 : g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1, 1, 1, Affordable ? 1.0f : 0.34f);
		RenderTools()->SelectSprite(m_pClient->IsNodes() ? NodesBuildingSprite(Sector, LocalTeam) : BuildSprite(m_pClient, Sector));
		RenderTools()->DrawSprite(Pos.x, Pos.y - 2.0f, Sector == Hovered ? 31.0f : 26.0f);
		Graphics()->QuadsEnd();
	}

	for(int Sector = 0; Sector < Count; ++Sector)
	{
		const vec2 Pos = WheelItemPosition(Center, Radius - 15.0f, Sector, Count);
		const bool Affordable = CanAfford(Sector);
		char aCost[16];
		str_format(aCost, sizeof(aCost), "%d", BuildingPrice(Sector));
		CUIRect Badge = {Pos.x - 11.0f, Pos.y - 5.0f, 22.0f, 10.0f};
		RenderTools()->DrawUIRect(&Badge,
								  Sector == Hovered ? vec4(0.08f, 0.065f, 0.035f, 0.94f)
													: vec4(0.025f, 0.032f, 0.036f, 0.92f),
								  CUI::CORNER_ALL,
								  3.0f);
		TextRender()->TextColor(
			Affordable ? 1.0f : 0.95f, Affordable ? 0.92f : 0.36f, Affordable ? 0.70f : 0.32f, 1.0f);
		UI()->DoLabel(&Badge, aCost, 6.2f, 0);
	}
	const int Display = Hovered >= 0 ? Hovered : (m_State.Selected() >= 0 ? m_State.Selected() : 0);
	const bool DisplayAffordable = CanAfford(Display);
	char aCenter[64];
	CUIRect NameLabel = {Center.x - Inner + 4.0f, Center.y - 27.0f, Inner * 2.0f - 8.0f, 12.0f};
	TextRender()->TextColor(0.97f, 0.98f, 0.98f, 1.0f);
	UI()->DoLabel(&NameLabel, Localize(BuildName(m_pClient, Display)), 7.0f, 0);
	str_format(aCenter, sizeof(aCenter), "%s  %d", Localize("Cost"), BuildingPrice(Display));
	CUIRect CostLabel = {Center.x - Inner + 4.0f, Center.y - 12.0f, Inner * 2.0f - 8.0f, 10.0f};
	TextRender()->TextColor(
		DisplayAffordable ? 1.0f : 0.98f, DisplayAffordable ? 0.72f : 0.30f, DisplayAffordable ? 0.24f : 0.25f, 1.0f);
	UI()->DoLabel(&CostLabel, aCenter, 5.8f, 0);
	str_format(aCenter, sizeof(aCenter), "%s  %d", Localize("Kits"), CustomStuff()->m_LocalKits);
	CUIRect KitsLabel = {Center.x - Inner + 4.0f, Center.y + 17.0f, Inner * 2.0f - 8.0f, 10.0f};
	TextRender()->TextColor(0.35f, 0.86f, 0.90f, 1.0f);
	UI()->DoLabel(&KitsLabel, aCenter, 5.8f, 0);
	TextRender()->TextColor(1, 1, 1, 1);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem Cursor(Center.x + m_WheelCursor.x, Center.y + m_WheelCursor.y, 18.0f, 18.0f);
	Graphics()->QuadsDrawTL(&Cursor, 1);
	Graphics()->QuadsEnd();
}

void CBuildPlacement::OnRender()
{
	if(!m_pClient->BuildingEnabled() || !CustomStuff()->m_LocalAlive || m_pClient->m_Snap.m_SpecInfo.m_Active)
	{
		if(m_DebugBuilding < 0 && !m_DebugWheel)
			Cancel();
		return;
	}
	if(PlacementActive())
	{
		EvaluatePlacement();
		RenderPlacement();
		if(m_State.State() == STATE_PLACEMENT)
		{
			SendPlacement();
			if(m_Result.m_Reason == INVALID_NO_KITS)
				Cancel();
		}
		m_ConfirmPressed = false;
	}
	if(WheelActive())
		RenderWheel();
}
