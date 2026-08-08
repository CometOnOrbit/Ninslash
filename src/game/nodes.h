#ifndef GAME_NODES_H
#define GAME_NODES_H

#include <base/vmath.h>

constexpr int NODES_BUILDING_NET_OFFSET = 100;
constexpr int NODES_MAX_BUILDINGS = 512;
constexpr int NODES_MAX_SPAWN_QUEUE = 64;
constexpr int NODES_BUILD_KIT_OFFSET = 100;

constexpr int NODES_ENTITY_REACTOR_RED = 58;
constexpr int NODES_ENTITY_SPAWN_RED = 59;
constexpr int NODES_ENTITY_REACTOR_BLUE = 60;
constexpr int NODES_ENTITY_SPAWN_BLUE = 61;
constexpr int NODES_ENTITY_CRATE_SPAWN = 62;

inline constexpr bool NodesWinCheckReady(int RedPlayers, int BluePlayers)
{
	return RedPlayers > 0 && BluePlayers > 0;
}

enum ENodesBuildingType
{
	NODES_REACTOR = 0,
	NODES_SPAWN,
	NODES_AMMO_SHOTGUN,
	NODES_HEALTH,
	NODES_REPEATER,
	NODES_TURRET_GUN,
	NODES_SHIELD,
	NODES_AMMO_GRENADE,
	NODES_TELEPORT,
	NODES_ARMOR,
	NODES_AMMO_LASER,
	NODES_TURRET_SHOTGUN,
	NODES_BUILDING_COUNT,
};

struct CNodesBuildingVisualInfo
{
	int m_Width;
	int m_Height;
	int m_Frames;
};

inline constexpr CNodesBuildingVisualInfo g_aNodesBuildingVisualInfo[NODES_BUILDING_COUNT] = {
	{3, 4, 4},
	{3, 4, 10},
	{2, 2, 4},
	{2, 2, 5},
	{2, 3, 2},
	{3, 3, 1},
	{3, 3, 10},
	{2, 2, 4},
	{3, 4, 10},
	{2, 2, 5},
	{2, 2, 4},
	{3, 3, 1},
};

inline constexpr CNodesBuildingVisualInfo NodesBuildingVisualInfo(int Type)
{
	return Type >= 0 && Type < NODES_BUILDING_COUNT ? g_aNodesBuildingVisualInfo[Type] : CNodesBuildingVisualInfo{1, 1, 1};
}

inline constexpr CNodesBuildingVisualInfo NodesBuildingPlacementVisualInfo(int Type)
{
	if(Type == NODES_TURRET_GUN || Type == NODES_TURRET_SHOTGUN)
		return {3, 4, 1};
	return NodesBuildingVisualInfo(Type);
}

inline constexpr int NodesBuildingAnimationFrame(int Type, bool Powered, bool Alive, int Tick)
{
	if(!Powered || !Alive || Tick < 0)
		return 0;
	const int SafeTick = Tick;
	switch(Type)
	{
		case NODES_REACTOR:
			return (SafeTick / 10) % 4;
		case NODES_SPAWN:
			return (SafeTick / 35) % 7;
		case NODES_REPEATER:
			return (SafeTick / 10) % 2;
		default:
			return 0;
	}
}

struct CNodesBuildingInfo
{
	const char *m_pName;
	int m_Price;
	int m_MaxHealth;
	float m_Radius;
	float m_Width;
	float m_Height;
	int m_TechLevel;
};

inline constexpr CNodesBuildingInfo g_aNodesBuildingInfo[NODES_BUILDING_COUNT] = {
	{"Reactor", 0, 500, 50.0f, 86.0f, 110.0f, 1},
	{"Spawn", 100, 260, 32.0f, 78.0f, 125.0f, 1},
	{"Shotgun ammo", 80, 140, 28.0f, 62.0f, 62.0f, 1},
	{"Health", 80, 140, 28.0f, 55.0f, 55.0f, 1},
	{"Repeater", 150, 220, 30.0f, 64.0f, 70.0f, 2},
	{"Gun turret", 250, 220, 32.0f, 40.0f, 93.0f, 2},
	{"Shield", 250, 300, 34.0f, 80.0f, 72.0f, 2},
	{"Grenade ammo", 120, 140, 28.0f, 62.0f, 62.0f, 2},
	{"Teleport", 350, 240, 34.0f, 96.0f, 25.0f, 3},
	{"Armor", 100, 140, 28.0f, 55.0f, 55.0f, 3},
	{"Laser ammo", 160, 140, 28.0f, 62.0f, 62.0f, 3},
	{"Shotgun turret", 320, 240, 32.0f, 40.0f, 93.0f, 3},
};

constexpr float NODES_BUILDING_BOTTOM_OFFSET = 16.0f;

struct CNodesBuildingBounds
{
	vec2 m_Min;
	vec2 m_Max;
};

inline CNodesBuildingBounds NodesBuildingBounds(vec2 Pos, const CNodesBuildingInfo &Info)
{
	const float Bottom = Pos.y + NODES_BUILDING_BOTTOM_OFFSET;
	return {vec2(Pos.x - Info.m_Width * 0.5f, Bottom - Info.m_Height), vec2(Pos.x + Info.m_Width * 0.5f, Bottom)};
}

inline bool NodesBuildingBoundsContains(const CNodesBuildingBounds &Bounds, vec2 Pos)
{
	return Pos.x >= Bounds.m_Min.x && Pos.x <= Bounds.m_Max.x && Pos.y >= Bounds.m_Min.y && Pos.y <= Bounds.m_Max.y;
}

inline bool NodesBuildingBoundsOverlap(const CNodesBuildingBounds &A, const CNodesBuildingBounds &B)
{
	return A.m_Min.x < B.m_Max.x && A.m_Max.x > B.m_Min.x && A.m_Min.y < B.m_Max.y && A.m_Max.y > B.m_Min.y;
}

inline constexpr int NodesBuildingKit(int Type)
{
	return NODES_BUILD_KIT_OFFSET + Type;
}

inline constexpr bool IsNodesBuildingKit(int Kit)
{
	return Kit >= NODES_BUILD_KIT_OFFSET && Kit < NODES_BUILD_KIT_OFFSET + NODES_BUILDING_COUNT;
}

inline constexpr int NodesBuildingTypeFromKit(int Kit)
{
	return Kit - NODES_BUILD_KIT_OFFSET;
}

inline constexpr int NodesNetworkType(int Type)
{
	return NODES_BUILDING_NET_OFFSET + Type;
}

inline constexpr bool IsNodesNetworkType(int Type)
{
	return Type >= NODES_BUILDING_NET_OFFSET && Type < NODES_BUILDING_NET_OFFSET + NODES_BUILDING_COUNT;
}

inline constexpr bool NodesBuildingPassAcceptsType(bool NodesPass, int Type)
{
	return !NodesPass || IsNodesNetworkType(Type);
}

inline constexpr bool NodesMapGenUsesRandomBoxes(bool NodesMode)
{
	return !NodesMode;
}

inline constexpr int NodesBuildingTypeFromNetwork(int Type)
{
	return Type - NODES_BUILDING_NET_OFFSET;
}

constexpr int NODES_STATUS_HEALTH_MASK = 0xff;
constexpr int NODES_STATUS_POWER = 1 << 8;
constexpr int NODES_STATUS_ALIVE = 1 << 9;
constexpr int NODES_STATUS_DECONSTRUCTION = 1 << 10;
constexpr int NODES_STATUS_ANIM_SHIFT = 12;
constexpr int NODES_STATUS_ANIM_MASK = 0xF << NODES_STATUS_ANIM_SHIFT;

inline constexpr int NodesStatusAnimationFrame(int Status)
{
	return (Status & NODES_STATUS_ANIM_MASK) >> NODES_STATUS_ANIM_SHIFT;
}

#endif
