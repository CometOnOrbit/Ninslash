#include <engine/engine.h>
#include <engine/sound.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <generated/game_data.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/client/components/camera.h>
#include <game/client/components/menus.h>
#include "sounds.h"

// Configurable music layer sound mapping.
// TODO
static int s_aMusicLayerSounds[4] = {SOUND_BG1, SOUND_BG2, SOUND_BG3, SOUND_BG4};

static void LoadMusicLayerConfig(IStorage *pStorage)
{
	IOHANDLE File = pStorage->OpenFile("music_layers.cfg", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return;
	// TODO!
}

struct CUserData
{
	CGameClient *m_pGameClient;
	bool m_Render;
} g_UserData;

static int LoadSoundsThread(void *pUser)
{
	CUserData *pData = static_cast<CUserData *>(pUser);

	for(int s = 0; s < g_pData->m_NumSounds; s++)
	{
		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; i++)
		{
			int Id = pData->m_pGameClient->Sound()->LoadWV(g_pData->m_aSounds[s].m_aSounds[i].m_pFilename);
			g_pData->m_aSounds[s].m_aSounds[i].m_Id = Id;
		}

		if(pData->m_Render)
			pData->m_pGameClient->m_pMenus->RenderLoading();
	}

	return 0;
}

int CSounds::GetSampleId(int SetId)
{
	if(!g_Config.m_SndEnable || !Sound()->IsSoundEnabled() || m_WaitForSoundJob || SetId < 0 ||
	   SetId >= g_pData->m_NumSounds)
		return -1;

	CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	if(!pSet->m_NumSounds)
		return -1;

	if(pSet->m_NumSounds == 1)
		return pSet->m_aSounds[0].m_Id;

	// return random one
	int Id;
	do
	{
		Id = rand() % pSet->m_NumSounds;
	} while(Id == pSet->m_Last);
	pSet->m_Last = Id;
	return pSet->m_aSounds[Id].m_Id;
}

void CSounds::OnInit()
{
	ApplySettings();

	Sound()->ConfigureMusicLayer(0, CHN_MUSIC_CALM, -1);
	Sound()->ConfigureMusicLayer(1, CHN_MUSIC_TENSION, -1);
	Sound()->ConfigureMusicLayer(2, CHN_MUSIC_COMBAT, -1);
	Sound()->ConfigureMusicLayer(3, CHN_MUSIC_BOSS, -1);

	Sound()->SetListenerPos(0.0f, 0.0f);

	// load sounds
	if(g_Config.m_ClThreadsoundloading)
	{
		g_UserData.m_pGameClient = m_pClient;
		g_UserData.m_Render = false;
		m_pClient->Engine()->AddJob(&m_SoundJob, LoadSoundsThread, &g_UserData);
		m_WaitForSoundJob = true;
	}
	else
	{
		g_UserData.m_pGameClient = m_pClient;
		g_UserData.m_Render = true;
		LoadSoundsThread(&g_UserData);
		m_WaitForSoundJob = false;
	}
}

void CSounds::ApplySettings()
{
	Sound()->SetChannel(CSounds::CHN_GUI, 1.0f, 0.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC, 1.0f, 0.0f);
	Sound()->SetChannel(CSounds::CHN_WORLD, 0.9f, 1.0f);
	Sound()->SetChannel(CSounds::CHN_GLOBAL, 1.0f, 0.0f);
	Sound()->SetChannel(CSounds::CHN_HIT, g_Config.m_ClHitFeedback / 100.0f, 0.0f);

	// dynamic music layers at max - engine Mix() controls per-sample volume
	SetMusicVolume(g_Config.m_SndMusicVolume / 100.0f);

	ClearQueue();
	m_MusicInitialized = false;
}

void CSounds::SetHitFeedbackVolume(float Volume)
{
	Sound()->SetChannel(CSounds::CHN_HIT, clamp(Volume, 0.0f, 1.0f), 0.0f);
}

void CSounds::SetMusicVolume(float Volume)
{
	const float V = clamp(Volume, 0.0f, 1.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC, V, 0.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC_CALM, V, 0.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC_TENSION, V, 0.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC_COMBAT, V, 0.0f);
	Sound()->SetChannel(CSounds::CHN_MUSIC_BOSS, V, 0.0f);
}

void CSounds::OnReset()
{
	if(Client()->State() >= IClient::STATE_ONLINE)
	{
		Sound()->StopAll();
		ClearQueue();
		m_MusicInitialized = false;
	}
}

void CSounds::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
		OnReset();
	else if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_ONLINE)
	{
		Sound()->SetMusicEnabled(false);
		for(int i = 0; i < 4; i++)
			Stop(s_aMusicLayerSounds[i]);
		m_MusicInitialized = false;
	}
}

void CSounds::OnRender()
{
	// check for sound initialisation
	if(m_WaitForSoundJob)
	{
		if(m_SoundJob.Status() == CJob::STATE_DONE)
			m_WaitForSoundJob = false;
		else
			return;
	}

	// set listner pos
	Sound()->SetListenerPos(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y);

	// start music layers once sounds are loaded
	if(!m_MusicInitialized && Client()->State() >= IClient::STATE_ONLINE && g_Config.m_SndMusic)
	{
		LoadMusicLayerConfig(Storage());
		for(int i = 0; i < 4; i++)
			Play(CHN_MUSIC_CALM + i, s_aMusicLayerSounds[i], 1.0f);
		Sound()->SetMusicEnabled(true);
		m_MusicInitialized = true;
	}

	if(!g_Config.m_SndMusic && m_MusicInitialized)
	{
		Sound()->SetMusicEnabled(false);
		for(int i = 0; i < 4; i++)
			Stop(s_aMusicLayerSounds[i]);
		m_MusicInitialized = false;
	}

	// play sound from queue
	if(m_QueuePos > 0)
	{
		int64 Now = time_get();
		if(m_QueueWaitTime <= Now)
		{
			Play(m_aQueue[0].m_Channel, m_aQueue[0].m_SetId, 1.0f);
			m_QueueWaitTime = Now + time_freq() * 3 / 10; // wait 300ms before playing the next one
			if(--m_QueuePos > 0)
				mem_move(m_aQueue, m_aQueue + 1, m_QueuePos * sizeof(QueueEntry));
		}
	}
}

void CSounds::ClearQueue()
{
	mem_zero(m_aQueue, sizeof(m_aQueue));
	m_QueuePos = 0;
	m_QueueWaitTime = time_get();
}

void CSounds::Enqueue(int Channel, int SetId)
{
	// add sound to the queue
	if(m_QueuePos < QUEUE_SIZE)
	{
		if(Channel == CHN_MUSIC || !g_Config.m_ClEditor)
		{
			m_aQueue[m_QueuePos].m_Channel = Channel;
			m_aQueue[m_QueuePos++].m_SetId = SetId;
		}
	}
}

void CSounds::PlayAndRecord(int Chn, int SetId, float Vol, vec2 Pos)
{
	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = SetId;
	Client()->SendPackMsg(&Msg, MSGFLAG_NOSEND | MSGFLAG_RECORD);

	Play(Chn, SetId, Vol);
}

void CSounds::Play(int Chn, int SetId, float Vol)
{
	if((Chn == CHN_MUSIC || Chn >= CHN_MUSIC_CALM) && !g_Config.m_SndMusic)
		return;

	int SampleId = GetSampleId(SetId);
	if(SampleId == -1)
		return;

	int Flags = 0;
	if(Chn == CHN_MUSIC || Chn >= CHN_MUSIC_CALM)
		Flags = ISound::FLAG_LOOP;

	Sound()->Play(Chn, SampleId, Flags);
}

void CSounds::PlayAt(int Chn, int SetId, float Vol, vec2 Pos)
{
	if((Chn == CHN_MUSIC || Chn >= CHN_MUSIC_CALM) && !g_Config.m_SndMusic)
		return;

	int SampleId = GetSampleId(SetId);
	if(SampleId == -1)
		return;

	int Flags = 0;
	if(Chn == CHN_MUSIC || Chn >= CHN_MUSIC_CALM)
		Flags = ISound::FLAG_LOOP;

	Sound()->PlayAt(Chn, SampleId, Flags, Pos.x, Pos.y);
}

void CSounds::Stop(int SetId)
{
	if(m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return;

	CDataSoundset *pSet = &g_pData->m_aSounds[SetId];

	for(int i = 0; i < pSet->m_NumSounds; i++)
		Sound()->Stop(pSet->m_aSounds[i].m_Id);
}

void CSounds::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_MUSICTHREAT)
	{
		CNetMsg_Sv_MusicThreat *pMsg = (CNetMsg_Sv_MusicThreat *)pRawMsg;
		Sound()->SetMusicThreat(pMsg->m_Threat / 255.0f);
	}
}
