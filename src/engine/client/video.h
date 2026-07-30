#ifndef ENGINE_CLIENT_VIDEO_H
#define ENGINE_CLIENT_VIDEO_H

#include <base/system.h>
#include <cstdio>

class IConsole;
class IStorage;

class CVideo
{
	FILE *m_pPipe;
	IConsole *m_pConsole;
	int m_Width;
	int m_Height;
	int m_FPS;
	int m_FrameCount;
	char m_aFilename[512];
	char m_aAbsPath[1024];

	static bool HasFFmpeg();

  public:
	CVideo();
	~CVideo();

	bool Start(IStorage *pStorage, IConsole *pConsole, const char *pFilename, int Width, int Height, int FPS);
	bool WriteFrame(const void *pData, unsigned Size);
	void Stop();

	bool IsRecording() const { return m_pPipe != 0; }
	int FrameCount() const { return m_FrameCount; }
	int Width() const { return m_Width; }
	int Height() const { return m_Height; }
	int FPS() const { return m_FPS; }
	const char *Filename() const { return m_aFilename; }
	const char *AbsPath() const { return m_aAbsPath; }
};

#endif
