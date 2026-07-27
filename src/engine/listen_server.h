#ifndef ENGINE_LISTEN_SERVER_H
#define ENGINE_LISTEN_SERVER_H

class INetPacketTransport;

struct CListenServerSettings
{
	int m_Port;
	int m_MaxClients;
	int m_Register;
	int m_RegisterSteam;
	int m_Official;
	int m_SteamAuth;
	char m_aBindAddress[128];
	char m_aName[128];
	char m_aPassword[128];
	char m_aMap[128];
	char m_aGameType[32];
	char m_aModHash[65];
	char m_aModIDs[1024];
	char m_aModWhitelist[1024];
};

class IListenServerRuntime
{
public:
	virtual ~IListenServerRuntime() {}
	virtual bool Start(INetPacketTransport *pTransport, const CListenServerSettings &Settings) = 0;
	virtual void Stop() = 0;
	virtual bool Running() const = 0;
	virtual bool Ready() const = 0;
};

IListenServerRuntime *CreateListenServerRuntime();

#endif
