

#ifndef ENGINE_CLIENT_SOUND_H
#define ENGINE_CLIENT_SOUND_H

#include <engine/sound.h>
#include <SDL3/SDL_audio.h>

class CSound : public IEngineSound
{
	int m_SoundEnabled;
	SDL_AudioStream *m_pStream = 0;

  public:
	IEngineGraphics *m_pGraphics;
	IStorage *m_pStorage;

	virtual int Init();

	int Update();
	int Shutdown();
	int AllocID();

	static void RateConvert(int SampleID);

	// TODO: Refactor: clean this mess up
	static IOHANDLE ms_File;
	static int ReadData(void *pBuffer, int Size);

	virtual bool IsSoundEnabled() { return m_SoundEnabled != 0; }

	virtual int LoadWV(const char *pFilename);

	virtual void SetListenerPos(float x, float y);
	virtual void SetChannel(int ChannelID, float Vol, float Pan);

	int Play(int ChannelID, int SampleID, int Flags, float x, float y);
	virtual int PlayAt(int ChannelID, int SampleID, int Flags, float x, float y);
	virtual int Play(int ChannelID, int SampleID, int Flags);
	virtual void Stop(int SampleID);
	virtual void StopAll();

	virtual void SetMusicThreat(float Threat);
	virtual void ConfigureMusicLayer(int Layer, int Channel, int SampleID);
	virtual void SetMusicEnabled(bool Enable);
};

#endif
