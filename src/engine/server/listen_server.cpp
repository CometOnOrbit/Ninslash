#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/listen_server.h>
#include <engine/localization.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "server.h"

namespace
{
void CaptureSettings(CListenServerSettings *pSettings)
{
	pSettings->m_Port = g_Config.m_SvPort;
	pSettings->m_MaxClients = g_Config.m_SvMaxClients;
	pSettings->m_Register = g_Config.m_SvRegister;
	pSettings->m_RegisterSteam = g_Config.m_SvRegisterSteam;
	pSettings->m_Official = g_Config.m_SvOfficial;
	pSettings->m_SteamAuth = g_Config.m_SvSteamAuth;
	pSettings->m_MapGen = g_Config.m_SvMapGen != 0;
	pSettings->m_MapGenLevel = g_Config.m_SvMapGenLevel;
	pSettings->m_MapGenSeed = g_Config.m_SvMapGenSeed;
	pSettings->m_MapGenRandomSeed = g_Config.m_SvMapGenRandSeed;
	pSettings->m_Bots = g_Config.m_SvNumBots;
	pSettings->m_BotLevel = g_Config.m_SvBotLevel;
	pSettings->m_ScoreLimit = g_Config.m_SvScorelimit;
	pSettings->m_TimeLimit = g_Config.m_SvTimelimit;
	pSettings->m_PveRoguelite = g_Config.m_SvPveRoguelite;
	pSettings->m_PveContracts = g_Config.m_SvPveContracts;
	pSettings->m_InvasionUseCheckpoint = g_Config.m_SvInvasionUseCheckpoint;
	str_copy(pSettings->m_aBindAddress, g_Config.m_Bindaddr, sizeof(pSettings->m_aBindAddress));
	str_copy(pSettings->m_aName, g_Config.m_SvName, sizeof(pSettings->m_aName));
	str_copy(pSettings->m_aPassword, g_Config.m_Password, sizeof(pSettings->m_aPassword));
	str_copy(pSettings->m_aMap, g_Config.m_SvMap, sizeof(pSettings->m_aMap));
	str_copy(pSettings->m_aGameType, g_Config.m_SvGametype, sizeof(pSettings->m_aGameType));
	str_copy(pSettings->m_aModHash, g_Config.m_SvModHash, sizeof(pSettings->m_aModHash));
	str_copy(pSettings->m_aModIDs, g_Config.m_SvModIds, sizeof(pSettings->m_aModIDs));
	str_copy(pSettings->m_aModWhitelist, g_Config.m_SvModWhitelist, sizeof(pSettings->m_aModWhitelist));
}

void ApplySettings(const CListenServerSettings &Settings)
{
	g_Config.m_SvPort = Settings.m_Port;
	g_Config.m_SvMaxClients = Settings.m_MaxClients;
	g_Config.m_SvRegister = Settings.m_Register;
	g_Config.m_SvRegisterSteam = Settings.m_RegisterSteam;
	g_Config.m_SvOfficial = Settings.m_Official;
	g_Config.m_SvSteamAuth = Settings.m_SteamAuth;
	g_Config.m_SvMapGen = Settings.m_MapGen ? 1 : 0;
	g_Config.m_SvMapGenLevel = Settings.m_MapGenLevel;
	g_Config.m_SvMapGenSeed = Settings.m_MapGenSeed;
	g_Config.m_SvMapGenRandSeed = Settings.m_MapGenRandomSeed;
	g_Config.m_SvNumBots = Settings.m_Bots;
	g_Config.m_SvBotLevel = Settings.m_BotLevel;
	g_Config.m_SvScorelimit = Settings.m_ScoreLimit;
	g_Config.m_SvTimelimit = Settings.m_TimeLimit;
	g_Config.m_SvPveRoguelite = Settings.m_PveRoguelite;
	g_Config.m_SvPveContracts = Settings.m_PveContracts;
	g_Config.m_SvInvasionUseCheckpoint = Settings.m_InvasionUseCheckpoint;
	str_copy(g_Config.m_Bindaddr, Settings.m_aBindAddress, sizeof(g_Config.m_Bindaddr));
	str_copy(g_Config.m_SvName, Settings.m_aName, sizeof(g_Config.m_SvName));
	str_copy(g_Config.m_Password, Settings.m_aPassword, sizeof(g_Config.m_Password));
	str_copy(g_Config.m_SvMap, Settings.m_aMap, sizeof(g_Config.m_SvMap));
	str_copy(g_Config.m_SvGametype, Settings.m_aGameType, sizeof(g_Config.m_SvGametype));
	str_copy(g_Config.m_SvModHash, Settings.m_aModHash, sizeof(g_Config.m_SvModHash));
	str_copy(g_Config.m_SvModIds, Settings.m_aModIDs, sizeof(g_Config.m_SvModIds));
	str_copy(g_Config.m_SvModWhitelist, Settings.m_aModWhitelist, sizeof(g_Config.m_SvModWhitelist));
}

class CListenServerRuntime : public IListenServerRuntime
{
	void *m_pThread;
	volatile bool m_Running;
	volatile bool m_Ready;
	CServer *volatile m_pServer;
	INetPacketTransport *m_pTransport;
	CListenServerSettings m_Settings;
	CListenServerSettings m_PreviousSettings;
	bool m_HavePreviousSettings;

	static void ThreadEntry(void *pUser) { static_cast<CListenServerRuntime *>(pUser)->Run(); }

	void Run()
	{
		ApplySettings(m_Settings);
		const char *apArgs[] = {"ninslash-listen"};
		CServer *pServer = new CServer();
		m_pServer = pServer;
		IKernel *pKernel = IKernel::Create();
		// The client process already owns the global debug loggers. Registering
		// them again would duplicate every subsequent log line.
		IEngine *pEngine = CreateEngine("Ninslash Listen Server", false);
		IEngineMap *pEngineMap = CreateEngineMap();
		IGameServer *pGameServer = CreateGameServer();
		IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON);
		IEngineMasterServer *pMasterServer = CreateEngineMasterServer();
		IStorage *pStorage = CreateStorage("Ninslash", IStorage::STORAGETYPE_SERVER, 1, apArgs);
		IConfig *pConfig = CreateConfig();
		ILocalization *pLocalization = CreateLocalization(pStorage);
		pLocalization->Init();

		pServer->InitRegister(&pServer->m_NetServer, pMasterServer, pConsole);
		bool Failed = false;
		Failed = Failed || !pKernel->RegisterInterface(pServer);
		Failed = Failed || !pKernel->RegisterInterface(pEngine);
		Failed = Failed || !pKernel->RegisterInterface(static_cast<IEngineMap *>(pEngineMap));
		Failed = Failed || !pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap));
		Failed = Failed || !pKernel->RegisterInterface(pGameServer);
		Failed = Failed || !pKernel->RegisterInterface(pConsole);
		Failed = Failed || !pKernel->RegisterInterface(pStorage);
		Failed = Failed || !pKernel->RegisterInterface(pConfig);
		Failed = Failed || !pKernel->RegisterInterface(pLocalization);
		Failed = Failed || !pKernel->RegisterInterface(static_cast<IEngineMasterServer *>(pMasterServer));
		Failed = Failed || !pKernel->RegisterInterface(static_cast<IMasterServer *>(pMasterServer));

		if(!Failed)
		{
			pEngine->Init();
			pMasterServer->Init();
			pMasterServer->Load();
			pServer->RegisterCommands();
			if(m_Settings.m_aConfig[0])
			{
				pConsole->ExecuteFile(m_Settings.m_aConfig);
				// Mode configs provide their baseline; the room form wins for every
				// value the player can edit.
				ApplySettings(m_Settings);
			}
			pServer->LoadAISkins();
			pServer->LoadGameVotes();
			pServer->SetListenTransport(m_pTransport);
			pServer->SetListenReadyFlag(&m_Ready);
			pServer->Run();
		}

		m_pServer = 0;
		delete pServer;
		delete pKernel;
		delete pEngineMap;
		delete pGameServer;
		delete pConsole;
		delete pMasterServer;
		delete pStorage;
		delete pConfig;
		delete pLocalization;
		if(m_HavePreviousSettings)
		{
			ApplySettings(m_PreviousSettings);
			m_HavePreviousSettings = false;
		}
		m_Running = false;
	}

  public:
	CListenServerRuntime()
		: m_pThread(0), m_Running(false), m_Ready(false), m_pServer(0), m_pTransport(0), m_HavePreviousSettings(false)
	{
		mem_zero(&m_Settings, sizeof(m_Settings));
		mem_zero(&m_PreviousSettings, sizeof(m_PreviousSettings));
	}
	~CListenServerRuntime() { Stop(); }
	bool Start(INetPacketTransport *pTransport, const CListenServerSettings &Settings)
	{
		if(m_Running || !pTransport)
			return false;
		CaptureSettings(&m_PreviousSettings);
		m_HavePreviousSettings = true;
		m_Settings = Settings;
		m_pTransport = pTransport;
		m_Ready = false;
		m_Running = true;
		m_pThread = thread_init(ThreadEntry, this);
		if(!m_pThread)
		{
			m_Running = false;
			ApplySettings(m_PreviousSettings);
			m_HavePreviousSettings = false;
		}
		return m_Running;
	}
	void Stop()
	{
		if(m_pServer)
			m_pServer->RequestStop();
		if(m_pThread)
		{
			thread_wait(m_pThread);
			m_pThread = 0;
		}
		m_Running = false;
		m_Ready = false;
		if(m_HavePreviousSettings)
		{
			ApplySettings(m_PreviousSettings);
			m_HavePreviousSettings = false;
		}
	}
	bool Running() const { return m_Running; }
	bool Ready() const { return m_Ready; }
};
} // namespace

IListenServerRuntime *CreateListenServerRuntime()
{
	return new CListenServerRuntime();
}
