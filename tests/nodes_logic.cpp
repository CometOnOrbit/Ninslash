#include <game/nodes.h>
#include <game/server/nodes/nodes_runtime.h>
#include <engine/message.h>
#include <generated/protocol.h>

static bool ProtocolAcceptsUseKit(int Kit)
{
	CMsgPacker Packer(NETMSGTYPE_CL_USEKIT);
	Packer.AddInt(Kit);
	Packer.AddInt(0);
	Packer.AddInt(0);
	CUnpacker Unpacker;
	Unpacker.Reset(Packer.Data() + Packer.HeaderSize(), Packer.Size() - Packer.HeaderSize());
	CNetObjHandler Handler;
	return Handler.SecureUnpackMsg(NETMSGTYPE_CL_USEKIT, &Unpacker) != nullptr;
}

int main()
{
	if(NodesWinCheckReady(1, 0) || NodesWinCheckReady(0, 1) || NodesWinCheckReady(0, 0))
		return 1;
	if(!NodesWinCheckReady(1, 1) || !NodesWinCheckReady(4, 2))
		return 2;

	CNodesInitialBuildingRegistry Initial;
	Initial.Add(vec2(128.0f, 64.0f), TEAM_BLUE, NODES_SPAWN);
	Initial.Add(vec2(2048.0f, 64.0f), TEAM_BLUE, NODES_REACTOR);
	Initial.Add(vec2(256.0f, 64.0f), TEAM_RED, NODES_SPAWN);
	Initial.Add(vec2(64.0f, 64.0f), TEAM_RED, NODES_REACTOR);
	Initial.SortForBootstrap();
	if(Initial.Count() != 4 || Initial.At(0).m_Type != NODES_REACTOR || Initial.At(1).m_Type != NODES_REACTOR)
		return 3;
	if(Initial.At(0).m_Team != TEAM_RED || Initial.At(1).m_Team != TEAM_BLUE)
		return 4;

	CNodesEconomy Economy;
	Economy.Reset(1000);
	if(Economy.BuildPoints(TEAM_RED) != 1000 || Economy.TechLevel(TEAM_BLUE) != 1 || !Economy.Spend(TEAM_RED, 250) ||
		Economy.BuildPoints(TEAM_RED) != 750)
		return 5;
	Economy.Refund(TEAM_RED, 80);
	if(Economy.BuildPoints(TEAM_RED) != 830)
		return 6;
	if(Economy.RegisterKill(TEAM_RED, 3) || Economy.RegisterKill(TEAM_RED, 3) || !Economy.RegisterKill(TEAM_RED, 3) ||
		Economy.TechLevel(TEAM_RED) != 2)
		return 7;

	CNodesSpawnQueue Queue;
	Queue.Reset();
	CPlayer *pFirst = reinterpret_cast<CPlayer *>(1);
	CPlayer *pSecond = reinterpret_cast<CPlayer *>(2);
	if(!Queue.Enqueue(TEAM_BLUE, pFirst, 180) || !Queue.Enqueue(TEAM_BLUE, pSecond, 360) || Queue.Count(TEAM_BLUE) != 2 ||
		Queue.At(TEAM_BLUE, 0)->m_pPlayer != pFirst || Queue.At(TEAM_BLUE, 1)->m_ReadyTick != 360)
		return 8;
	if(!Queue.Remove(pFirst) || Queue.Count(TEAM_BLUE) != 1 || Queue.At(TEAM_BLUE, 0)->m_pPlayer != pSecond)
		return 9;

	CNodesRuntime Runtime;
	Runtime.InitialBuildings().Add(vec2(1.0f, 1.0f), TEAM_RED, NODES_REACTOR);
	Runtime.Reset(400);
	if(Runtime.InitialBuildings().Count() != 1 || Runtime.Economy().BuildPoints(TEAM_BLUE) != 400 ||
		Runtime.State() != CNodesRuntime::EState::BOOTSTRAP)
		return 10;
	Runtime.BuildCooldowns().Reset();
	if(!Runtime.BuildCooldowns().Ready(3, 100))
		return 11;
	Runtime.BuildCooldowns().SetReadyTick(3, 200);
	if(Runtime.BuildCooldowns().Ready(3, 199) || !Runtime.BuildCooldowns().Ready(3, 200))
		return 11;
	const CNodesBuildingVisualInfo ReactorVisual = NodesBuildingVisualInfo(NODES_REACTOR);
	if(ReactorVisual.m_Width != 3 || ReactorVisual.m_Height != 4 || ReactorVisual.m_Frames != 4)
		return 12;
	const CNodesBuildingVisualInfo TurretVisual = NodesBuildingPlacementVisualInfo(NODES_TURRET_GUN);
	if(TurretVisual.m_Width != 3 || TurretVisual.m_Height != 4 || TurretVisual.m_Frames != 1)
		return 12;
	if(NodesBuildingAnimationFrame(NODES_SPAWN, true, true, 35) != 1 ||
		NodesBuildingAnimationFrame(NODES_SPAWN, true, true, 35 * 7) != 0 ||
		NodesBuildingAnimationFrame(NODES_SPAWN, false, true, 35) != 0 ||
		NodesBuildingAnimationFrame(NODES_AMMO_SHOTGUN, true, true, 35) != 0)
		return 13;
	const CNodesBuildingInfo &ReactorInfo = g_aNodesBuildingInfo[NODES_REACTOR];
	if(ReactorInfo.m_Width != 86.0f || ReactorInfo.m_Height != 110.0f)
		return 14;
	const CNodesBuildingBounds ReactorBounds = NodesBuildingBounds(vec2(128.0f, 144.0f), ReactorInfo);
	if(!NodesBuildingBoundsContains(ReactorBounds, vec2(128.0f, 80.0f)) ||
		NodesBuildingBoundsContains(ReactorBounds, vec2(128.0f, 220.0f)))
		return 15;
	const CNodesBuildingBounds BrickBounds{vec2(160.0f, 80.0f), vec2(192.0f, 112.0f)};
	if(!NodesBuildingBoundsOverlap(ReactorBounds, BrickBounds))
		return 16;
	if(!NodesBuildingPassAcceptsType(false, NodesNetworkType(NODES_REACTOR)) ||
		!NodesBuildingPassAcceptsType(false, NODES_BUILDING_NET_OFFSET - 1) ||
		!NodesBuildingPassAcceptsType(true, NodesNetworkType(NODES_REACTOR)) ||
		NodesBuildingPassAcceptsType(true, NODES_BUILDING_NET_OFFSET - 1))
		return 17;
	if(NodesMapGenUsesRandomBoxes(true) || !NodesMapGenUsesRandomBoxes(false))
		return 18;
	if(!ProtocolAcceptsUseKit(NodesBuildingKit(NODES_BUILDING_COUNT - 1)) ||
		ProtocolAcceptsUseKit(NodesBuildingKit(NODES_BUILDING_COUNT)))
		return 19;
	return 0;
}
