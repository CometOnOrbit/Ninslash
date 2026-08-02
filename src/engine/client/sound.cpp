

#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/storage.h>

#include <engine/shared/config.h>

#include <base/audio_math.h>

#include <SDL3/SDL.h>

#include "sound.h"

extern "C"
{ // wavpack
#include <engine/external/wavpack/wavpack.h>
}
#include <math.h>

enum
{
	NUM_SAMPLES = 512,
	NUM_VOICES = 64,
	NUM_CHANNELS = 16,
};

struct CSample
{
	short *m_pData;
	int m_NumFrames;
	int m_Rate;
	int m_Channels;
	int m_LoopStart;
	int m_LoopEnd;
	int m_PausedAt;
};

struct CChannel
{
	int m_Vol;
	int m_Pan;
};

struct CVoice
{
	CSample *m_pSample;
	CChannel *m_pChannel;
	int m_Tick;
	int m_Vol; // 0 - 255
	int m_Flags;
	int m_X, m_Y;
};

static CSample m_aSamples[NUM_SAMPLES] = {{0}};
static CVoice m_aVoices[NUM_VOICES] = {{0}};
static CChannel m_aChannels[NUM_CHANNELS] = {{255, 0}};

static LOCK m_SoundLock = 0;

static int m_CenterX = 0;
static int m_CenterY = 0;

static int m_MixingRate = 48000;
static volatile int m_SoundVolume = 100;

static int m_NextVoice = 0;
static int *m_pMixBuffer = 0; // buffer only used by the thread callback function
static unsigned m_MaxFrames = 0;

// dynamic music layer state
struct SMusicLayer
{
	int m_Channel = -1;
	int m_SampleID = -1;
};
static constexpr int NUM_MUSIC_LAYERS = 4;
static SMusicLayer s_aMusicLayers[NUM_MUSIC_LAYERS];
static IFloat s_MusicState;
static float s_MusicTarget = 0.0f;
static bool s_MusicEnabled = true;
static AttackRelease s_MusicAR;
static int s_aMusicVol[4][NUM_SAMPLES];

// TODO: there should be a faster way todo this
static short Int2Short(int i)
{
	if(i > 0x7fff)
		return 0x7fff;
	else if(i < -0x7fff)
		return -0x7fff;
	return i;
}

static int IntAbs(int i)
{
	if(i < 0)
		return -i;
	return i;
}

static void Mix(short *pFinalOut, unsigned Frames)
{
	int MasterVol;
	mem_zero(m_pMixBuffer, m_MaxFrames * 2 * sizeof(int));
	Frames = min(Frames, m_MaxFrames);

	// aquire lock while we are mixing
	lock_wait(m_SoundLock);

	MasterVol = m_SoundVolume;

	// dynamic music: AttackRelease + per-sample table + two-phase interpolation
	mem_zero(s_aMusicVol, sizeof(s_aMusicVol));
	if(s_MusicEnabled)
	{
		float t0 = s_MusicAR.get();
		s_MusicState.set(s_MusicTarget, 12);
		float t1Raw = s_MusicState.next();
		float t1 = s_MusicAR.step(t1Raw, 0.3f, 0.05f);
		t0 = clamp(t0, 0.0f, 1.0f);
		t1 = clamp(t1, 0.0f, 1.0f);

		const float segSize = 1.0f / (NUM_MUSIC_LAYERS - 1);
		const unsigned itime = min(64u, Frames);

		for(unsigned s = 0; s < itime; s++)
		{
			float tp = t0 + (t1 - t0) * ((float)s / (float)itime);
			tp = clamp(tp, 0.0f, 1.0f);
			int seg = (int)(tp / segSize);
			if(seg < 0) seg = 0;
			if(seg >= NUM_MUSIC_LAYERS - 1) seg = NUM_MUSIC_LAYERS - 2;
			float segT = (tp - seg * segSize) / segSize;

			s_aMusicVol[seg][s]     = (int)(Crossfade<CrossfadeType::Smooth>(1.0f, 0.0f, segT) * 255.0f);
			s_aMusicVol[seg + 1][s] = (int)(Crossfade<CrossfadeType::Smooth>(0.0f, 1.0f, segT) * 255.0f);
		}
		{
			int seg1 = (int)(t1 / segSize);
			if(seg1 < 0) seg1 = 0;
			if(seg1 >= NUM_MUSIC_LAYERS - 1) seg1 = NUM_MUSIC_LAYERS - 2;
			float segT1 = (t1 - seg1 * segSize) / segSize;

			for(unsigned s = itime; s < Frames; s++)
			{
				s_aMusicVol[seg1][s]     = (int)(Crossfade<CrossfadeType::Smooth>(1.0f, 0.0f, segT1) * 255.0f);
				s_aMusicVol[seg1 + 1][s] = (int)(Crossfade<CrossfadeType::Smooth>(0.0f, 1.0f, segT1) * 255.0f);
			}
		}
	}

	for(unsigned i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample)
		{
			// mix voice
			CVoice *v = &m_aVoices[i];
			int *pOut = m_pMixBuffer;

			int Step = v->m_pSample->m_Channels; // setup input sources
			short *pInL = &v->m_pSample->m_pData[v->m_Tick * Step];
			short *pInR = &v->m_pSample->m_pData[v->m_Tick * Step + 1];

			unsigned End = v->m_pSample->m_NumFrames - v->m_Tick;

			int Rvol = v->m_pChannel->m_Vol;
			int Lvol = v->m_pChannel->m_Vol;

			// make sure that we don't go outside the sound data
			if(Frames < End)
				End = Frames;

			// check if we have a mono sound
			if(v->m_pSample->m_Channels == 1)
				pInR = pInL;

			// volume calculation
			if(v->m_Flags & ISound::FLAG_POS && v->m_pChannel->m_Pan)
			{
				// TODO: we should respect the channel panning value
				const int Range = 1500; // magic value, remove
				int dx = v->m_X - m_CenterX;
				int dy = v->m_Y - m_CenterY;
				int Dist = (int)sqrtf((float)dx * dx + dy * dy); // float here. nasty
				int p = IntAbs(dx);
				if(Dist >= 0 && Dist < Range)
				{
					// panning
					if(dx > 0)
						Lvol = ((Range - p) * Lvol) / Range;
					else
						Rvol = ((Range - p) * Rvol) / Range;

					// falloff
					Lvol = (Lvol * (Range - Dist)) / Range;
					Rvol = (Rvol * (Range - Dist)) / Range;
				}
				else
				{
					Lvol = 0;
					Rvol = 0;
				}
			}

			// check if this voice is a music layer
			int MusicLayer = -1;
			for(int j = 0; j < NUM_MUSIC_LAYERS; j++)
			{
				if(s_aMusicLayers[j].m_Channel >= 0 &&
					v->m_pChannel == &m_aChannels[s_aMusicLayers[j].m_Channel])
				{
					MusicLayer = j;
					break;
				}
			}

			// process all frames
			for(unsigned s = 0; s < End; s++)
			{
				int l = Lvol;
				int r = Rvol;
				if(MusicLayer >= 0)
				{
					int mv = s_aMusicVol[MusicLayer][s];
					l = (l * mv) / 255;
					r = (r * mv) / 255;
				}
				*pOut++ += (*pInL) * l;
				*pOut++ += (*pInR) * r;
				pInL += Step;
				pInR += Step;
				v->m_Tick++;
			}

			// free voice if not used any more
			if(v->m_Tick == v->m_pSample->m_NumFrames)
			{
				if(v->m_Flags & ISound::FLAG_LOOP)
					v->m_Tick = 0;
				else
					v->m_pSample = 0;
			}
		}
	}

	// release the lock
	lock_unlock(m_SoundLock);

	{
		// clamp accumulated values
		// TODO: this seams slow
		for(unsigned i = 0; i < Frames; i++)
		{
			int j = i << 1;
			int vl = ((m_pMixBuffer[j] * MasterVol) / 101) >> 8;
			int vr = ((m_pMixBuffer[j + 1] * MasterVol) / 101) >> 8;

			pFinalOut[j] = Int2Short(vl);
			pFinalOut[j + 1] = Int2Short(vr);
		}
	}

#if defined(CONF_ARCH_ENDIAN_BIG)
	swap_endian(pFinalOut, sizeof(short), Frames * 2);
#endif
}

static void SDLCallback(void *pUnused, Uint8 *pStream, int Len)
{
	(void)pUnused;
	Mix((short *)pStream, Len / 2 / 2);
}

static void SDLNewCallback(void *pUnused, SDL_AudioStream *pStream, int AdditionalAmount, int TotalAmount)
{
	/* Calculate a little more audio here, maybe using `userdata`, write it to `stream`
	 *
	 * If you want to use the original callback, you could do something like this:
	 */
	if(AdditionalAmount > 0)
	{
		Uint8 *pData = SDL_stack_alloc(Uint8, AdditionalAmount);
		if(pData)
		{
			SDLCallback(pUnused, pData, AdditionalAmount);
			SDL_PutAudioStreamData(pStream, pData, AdditionalAmount);
			SDL_stack_free(pData);
		}
	}
}

int CSound::Init()
{
	m_SoundEnabled = 0;
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_SoundLock = lock_create();

	if(!g_Config.m_SndEnable)
		return 0;

	if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		dbg_msg("gfx", "unable to init SDL audio: %s", SDL_GetError());
		return -1;
	}

	m_MixingRate = g_Config.m_SndRate;

	const SDL_AudioSpec Format = {SDL_AUDIO_S16, 2, g_Config.m_SndRate};
	SDL_AudioStream *pStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &Format, SDLNewCallback, 0);
	// Open the audio device and start playing sound!
	if(!pStream)
	{
		dbg_msg("client/sound", "unable to open audio: %s", SDL_GetError());
		return -1;
	}
	else
		dbg_msg("client/sound", "sound init successful");

	m_MaxFrames = g_Config.m_SndBufferSize * 2;
	m_pMixBuffer = (int *)mem_alloc(m_MaxFrames * 2 * sizeof(int), 1);

	SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(pStream));

	m_SoundEnabled = 1;
	Update(); // update the volume
	return 0;
}

int CSound::Update()
{
	// update volume
	int WantedVolume = g_Config.m_SndVolume;

	if(!m_pGraphics->WindowActive() && g_Config.m_SndNonactiveMute)
		WantedVolume = 0;

	if(WantedVolume != m_SoundVolume)
	{
		lock_wait(m_SoundLock);
		m_SoundVolume = WantedVolume;
		lock_unlock(m_SoundLock);
	}

	return 0;
}

int CSound::Shutdown()
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	lock_destroy(m_SoundLock);
	if(m_pMixBuffer)
	{
		mem_free(m_pMixBuffer);
		m_pMixBuffer = 0;
	}
	return 0;
}

int CSound::AllocID()
{
	// TODO: linear search, get rid of it
	for(unsigned SampleID = 0; SampleID < NUM_SAMPLES; SampleID++)
	{
		if(m_aSamples[SampleID].m_pData == 0x0)
			return SampleID;
	}

	return -1;
}

void CSound::RateConvert(int SampleID)
{
	CSample *pSample = &m_aSamples[SampleID];
	int NumFrames = 0;
	short *pNewData = 0;

	// make sure that we need to convert this sound
	if(!pSample->m_pData || pSample->m_Rate == m_MixingRate)
		return;

	// allocate new data
	NumFrames = max(1, (int)((pSample->m_NumFrames / (float)pSample->m_Rate) * m_MixingRate));
	pNewData = (short *)mem_alloc(NumFrames * pSample->m_Channels * sizeof(short), 1);

	for(int i = 0; i < NumFrames; i++)
	{
		// resample TODO: this should be done better, like linear atleast
		float a = i / (float)NumFrames;
		int f = (int)(a * pSample->m_NumFrames);
		if(f >= pSample->m_NumFrames)
			f = pSample->m_NumFrames - 1;

		// set new data
		if(pSample->m_Channels == 1)
			pNewData[i] = pSample->m_pData[f];
		else if(pSample->m_Channels == 2)
		{
			pNewData[i * 2] = pSample->m_pData[f * 2];
			pNewData[i * 2 + 1] = pSample->m_pData[f * 2 + 1];
		}
	}

	// free old data and apply new
	mem_free(pSample->m_pData);
	pSample->m_pData = pNewData;
	pSample->m_NumFrames = NumFrames;
}

int CSound::ReadData(void *pBuffer, int Size)
{
	return io_read(ms_File, pBuffer, Size);
}

int CSound::LoadWV(const char *pFilename)
{
	int SampleID = -1;
	char aError[100];
	WavpackContext *pContext = 0;

	// don't waste memory on sound when we are stress testing
	if(g_Config.m_DbgStress)
		return -1;

	// no need to load sound when we are running with no sound
	if(!m_SoundEnabled)
		return 1;

	if(!m_pStorage)
		return -1;

	SampleID = AllocID();
	if(SampleID < 0)
		return -1;

	ms_File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!ms_File)
	{
		dbg_msg("sound/wv", "failed to open file. filename='%s'", pFilename);
		return -1;
	}

	pContext = WavpackOpenFileInput(ReadData, aError);
	if(!pContext)
	{
		dbg_msg("sound/wv", "failed to open %s: %s", pFilename, aError);
		io_close(ms_File);
		ms_File = 0;
		return -1;
	}

	const int NumFrames = WavpackGetNumSamples(pContext);
	const int BitsPerSample = WavpackGetBitsPerSample(pContext);
	const unsigned int SampleRate = WavpackGetSampleRate(pContext);
	const int Channels = WavpackGetNumChannels(pContext);
	if(Channels < 1 || Channels > 2 || BitsPerSample != 16 || !SampleRate || NumFrames <= 0 ||
	   NumFrames > 16 * 1024 * 1024 / Channels)
	{
		dbg_msg("sound/wv",
				"invalid sound format. filename='%s' frames=%d channels=%d bps=%d rate=%u",
				pFilename,
				NumFrames,
				Channels,
				BitsPerSample,
				SampleRate);
		io_close(ms_File);
		ms_File = 0;
		return -1;
	}

	const int NumSamples = NumFrames * Channels;
	int *pData = (int *)mem_alloc(sizeof(int) * NumSamples, 1);
	const unsigned int Unpacked = WavpackUnpackSamples(pContext, pData, NumFrames);
	if(Unpacked != (unsigned int)NumFrames)
	{
		dbg_msg(
			"sound/wv", "truncated sound data. filename='%s' expected=%d decoded=%u", pFilename, NumFrames, Unpacked);
		mem_free(pData);
		io_close(ms_File);
		ms_File = 0;
		return -1;
	}

	short *pDecoded = (short *)mem_alloc(sizeof(short) * NumSamples, 1);
	for(int i = 0; i < NumSamples; i++)
		pDecoded[i] = (short)pData[i];
	mem_free(pData);
	io_close(ms_File);
	ms_File = 0;

	CSample *pSample = &m_aSamples[SampleID];
	pSample->m_pData = pDecoded;
	pSample->m_Channels = Channels;
	pSample->m_Rate = SampleRate;
	pSample->m_NumFrames = NumFrames;
	pSample->m_LoopStart = -1;
	pSample->m_LoopEnd = -1;
	pSample->m_PausedAt = 0;

	if(g_Config.m_Debug)
		dbg_msg("sound/wv", "loaded %s", pFilename);

	RateConvert(SampleID);
	return SampleID;
}

void CSound::SetListenerPos(float x, float y)
{
	m_CenterX = (int)x;
	m_CenterY = (int)y;
}

void CSound::SetChannel(int ChannelID, float Vol, float Pan)
{
	m_aChannels[ChannelID].m_Vol = (int)(Vol * 255.0f);
	m_aChannels[ChannelID].m_Pan = (int)(Pan * 255.0f); // TODO: this is only on and off right now
}

int CSound::Play(int ChannelID, int SampleID, int Flags, float x, float y)
{
	int VoiceID = -1;
	int i;

	lock_wait(m_SoundLock);

	// search for voice
	for(i = 0; i < NUM_VOICES; i++)
	{
		int id = (m_NextVoice + i) % NUM_VOICES;
		if(!m_aVoices[id].m_pSample)
		{
			VoiceID = id;
			m_NextVoice = id + 1;
			break;
		}
	}

	// voice found, use it
	if(VoiceID != -1)
	{
		m_aVoices[VoiceID].m_pSample = &m_aSamples[SampleID];
		m_aVoices[VoiceID].m_pChannel = &m_aChannels[ChannelID];
		if(Flags & FLAG_LOOP)
			m_aVoices[VoiceID].m_Tick = m_aSamples[SampleID].m_PausedAt;
		else
			m_aVoices[VoiceID].m_Tick = 0;
		m_aVoices[VoiceID].m_Vol = 255;
		m_aVoices[VoiceID].m_Flags = Flags;
		m_aVoices[VoiceID].m_X = (int)x;
		m_aVoices[VoiceID].m_Y = (int)y;
	}

	lock_unlock(m_SoundLock);
	return VoiceID;
}

int CSound::PlayAt(int ChannelID, int SampleID, int Flags, float x, float y)
{
	return Play(ChannelID, SampleID, Flags | ISound::FLAG_POS, x, y);
}

int CSound::Play(int ChannelID, int SampleID, int Flags)
{
	return Play(ChannelID, SampleID, Flags, 0, 0);
}

void CSound::Stop(int SampleID)
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	CSample *pSample = &m_aSamples[SampleID];
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample == pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
			m_aVoices[i].m_pSample = 0;
		}
	}
	lock_unlock(m_SoundLock);
}

void CSound::StopAll()
{
	// TODO: a nice fade out
	lock_wait(m_SoundLock);
	for(int i = 0; i < NUM_VOICES; i++)
	{
		if(m_aVoices[i].m_pSample)
		{
			if(m_aVoices[i].m_Flags & FLAG_LOOP)
				m_aVoices[i].m_pSample->m_PausedAt = m_aVoices[i].m_Tick;
			else
				m_aVoices[i].m_pSample->m_PausedAt = 0;
		}
		m_aVoices[i].m_pSample = 0;
	}
	lock_unlock(m_SoundLock);
}

void CSound::SetMusicThreat(float Threat)
{
	s_MusicTarget = Threat;
}

void CSound::ConfigureMusicLayer(int Layer, int Channel, int SampleID)
{
	if(Layer < 0 || Layer >= NUM_MUSIC_LAYERS)
		return;
	s_aMusicLayers[Layer].m_Channel = Channel;
	s_aMusicLayers[Layer].m_SampleID = SampleID;
}

void CSound::SetMusicEnabled(bool Enable)
{
	s_MusicEnabled = Enable;
}

IOHANDLE CSound::ms_File = 0;

IEngineSound *CreateEngineSound()
{
	return new CSound;
}
