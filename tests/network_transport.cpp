#include <base/system.h>
#include <engine/shared/network.h>

#include <assert.h>

class CMockTransport : public INetPacketTransport
{
public:
	unsigned char m_aPacket[NET_MAX_PACKETSIZE];
	int m_PacketSize;
	CMockTransport() : m_PacketSize(0) {}
	bool ConnectPeer(unsigned long long PeerID) { return PeerID != 0; }
	bool Listen(int VirtualPort) { return VirtualPort == 1; }
	void CloseListen() {}
	void ClosePeer() {}
	void Update() {}
	int RecvPacket(NETADDR *pAddr, void *pBuffer, int BufferSize)
	{
		(void)pAddr; (void)pBuffer; (void)BufferSize; return 0;
	}
	bool SendPacket(const NETADDR *pAddr, CNetPacketConstruct *pPacket)
	{
		(void)pAddr; m_PacketSize = CNetBase::PackPacket(pPacket, m_aPacket, sizeof(m_aPacket)); return m_PacketSize > 0;
	}
	bool SendControl(const NETADDR *pAddr, int Ack, int ControlMsg, const void *pExtra, int ExtraSize)
	{
		(void)pAddr; m_PacketSize = CNetBase::PackControl(Ack, ControlMsg, pExtra, ExtraSize, m_aPacket, sizeof(m_aPacket)); return m_PacketSize > 0;
	}
};

int main()
{
	CNetBase::Init();
	NETADDR ParsedAddress;
	char aAddress[64];
	assert(net_addr_from_str(&ParsedAddress, "steam:76561197960265728") == 0);
	assert(ParsedAddress.type == NETTYPE_STEAM);
	net_addr_str(&ParsedAddress, aAddress, sizeof(aAddress), true);
	assert(str_comp(aAddress, "steam:76561197960265728") == 0);
	assert(net_addr_from_str(&ParsedAddress, "steam:0") != 0);
	assert(net_addr_from_str(&ParsedAddress, "steam:-1") != 0);
	assert(net_addr_from_str(&ParsedAddress, "steam:+1") != 0);
	assert(net_addr_from_str(&ParsedAddress, "steam:1x") != 0);
	assert(net_addr_from_str(&ParsedAddress, "steam:18446744073709551616") != 0);

	CMockTransport Transport;
	NETSOCKET Socket = {0, -1, -1};
	CNetConnection Connection;
	Connection.Init(Socket, false, &Transport);
	NETADDR Address;
	mem_zero(&Address, sizeof(Address));
	Address.type = NETTYPE_STEAM;
	Address.ip[0] = 42;
	assert(Connection.Connect(&Address) == 0);
	assert(Transport.m_PacketSize > 0);
	CNetPacketConstruct Packet;
	assert(CNetBase::UnpackPacket(Transport.m_aPacket, Transport.m_PacketSize, &Packet) == 0);
	assert((Packet.m_Flags & NET_PACKETFLAG_CONTROL) != 0);
	assert(Packet.m_aChunkData[0] == NET_CTRLMSG_CONNECT);
	return 0;
}
