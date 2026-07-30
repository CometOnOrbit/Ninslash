#include <base/system.h>
#include <engine/console.h>
#include <engine/storage.h>

#include "video.h"

#include <stdio.h>
#include <string.h>

#if defined(CONF_FAMILY_WINDOWS)
#define popen _popen
#define pclose _pclose
#endif

CVideo::CVideo()
{
	m_pPipe = 0;
	m_pConsole = 0;
	m_Width = 0;
	m_Height = 0;
	m_FPS = 30;
	m_FrameCount = 0;
	m_aFilename[0] = 0;
	m_aAbsPath[0] = 0;
}

CVideo::~CVideo()
{
	Stop();
}

bool CVideo::HasFFmpeg()
{
#if defined(CONF_FAMILY_WINDOWS)
	FILE *pTest = popen("where ffmpeg 2>NUL", "r");
#else
	FILE *pTest = popen("command -v ffmpeg 2>/dev/null", "r");
#endif
	if(!pTest)
		return false;

	char aLine[256];
	bool Found = fgets(aLine, sizeof(aLine), pTest) != 0;
	pclose(pTest);
	return Found;
}

bool CVideo::Start(IStorage *pStorage, IConsole *pConsole, const char *pFilename, int Width, int Height, int FPS)
{
	if(m_pPipe)
		Stop();

	m_pConsole = pConsole;
	m_Width = Width;
	m_Height = Height;
	m_FPS = FPS > 0 ? FPS : 30;
	m_FrameCount = 0;
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	m_aAbsPath[0] = 0;

	if(!HasFFmpeg())
	{
		if(m_pConsole)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", "ffmpeg not found in PATH");
		return false;
	}

	// resolve absolute path via storage write probe
	{
		IOHANDLE File =
			pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE, m_aAbsPath, sizeof(m_aAbsPath));
		if(!File)
		{
			if(m_pConsole)
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", "failed to open output path");
			return false;
		}
		io_close(File);
	}

	// even dimensions for yuv420p
	int OutW = m_Width & ~1;
	int OutH = m_Height & ~1;
	if(OutW < 2)
		OutW = 2;
	if(OutH < 2)
		OutH = 2;

	char aCmd[2048];
	str_format(aCmd,
			   sizeof(aCmd),
			   "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i pipe:0 "
			   "-vf scale=%d:%d -c:v libx264 -preset ultrafast -crf 23 -pix_fmt yuv420p \"%s\"",
			   m_Width,
			   m_Height,
			   m_FPS,
			   OutW,
			   OutH,
			   m_aAbsPath);

#if defined(CONF_FAMILY_WINDOWS)
	m_pPipe = popen(aCmd, "wb");
#else
	m_pPipe = popen(aCmd, "w");
#endif
	if(!m_pPipe)
	{
		if(m_pConsole)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", "failed to start ffmpeg");
		return false;
	}

	if(m_pConsole)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "recording to '%s' (%dx%d @ %d fps)", m_aAbsPath, m_Width, m_Height, m_FPS);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", aBuf);
	}

	return true;
}

bool CVideo::WriteFrame(const void *pData, unsigned Size)
{
	if(!m_pPipe || !pData || !Size)
		return false;

	unsigned Written = 0;
	const unsigned char *pBytes = (const unsigned char *)pData;
	while(Written < Size)
	{
		size_t N = fwrite(pBytes + Written, 1, Size - Written, m_pPipe);
		if(N == 0)
		{
			if(m_pConsole)
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", "ffmpeg pipe write failed");
			Stop();
			return false;
		}
		Written += (unsigned)N;
	}

	m_FrameCount++;
	return true;
}

void CVideo::Stop()
{
	if(!m_pPipe)
		return;

	// close stdin so ffmpeg sees EOF and can finish the container
	fflush(m_pPipe);
	int Status = pclose(m_pPipe);
	m_pPipe = 0;

	if(m_pConsole)
	{
		char aBuf[512];
		if(Status == 0)
			str_format(aBuf,
					   sizeof(aBuf),
					   "saved video '%s' (%d frames)",
					   m_aAbsPath[0] ? m_aAbsPath : m_aFilename,
					   m_FrameCount);
		else
			str_format(aBuf, sizeof(aBuf), "ffmpeg exited with status %d (%d frames)", Status, m_FrameCount);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "video", aBuf);
	}
}
