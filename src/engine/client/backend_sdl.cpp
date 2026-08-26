#include <SDL3/SDL.h>
#include <GL/glew.h>
#include <base/detect.h>
#include <base/math.h>
#include <ctime>

#if defined(CONF_PLATFORM_MACOSX)
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

#include <base/tl/threading.h>

#include "graphics_threaded.h"
#include "shaders.h"
#include "backend_sdl.h"

#include <engine/shared/config.h>

// ------------ CGraphicsBackend_Threaded

void CGraphicsBackend_Threaded::ThreadFunc(void *pUser)
{
	CGraphicsBackend_Threaded *pThis = (CGraphicsBackend_Threaded *)pUser;

	while(!pThis->m_Shutdown)
	{
#ifdef CONF_PLATFORM_MACOSX
		CAutoreleasePool AutoreleasePool;
#endif
		pThis->m_Activity.wait();
		if(pThis->m_pBuffer)
		{
			pThis->m_pProcessor->RunBuffer(pThis->m_pBuffer);
			sync_barrier();
			pThis->m_pBuffer = 0x0;
			pThis->m_BufferDone.signal();
		}
	}
}

CGraphicsBackend_Threaded::CGraphicsBackend_Threaded()
{
	m_pBuffer = 0x0;
	m_pProcessor = 0x0;
	m_pThread = 0x0;
}

void CGraphicsBackend_Threaded::StartProcessor(ICommandProcessor *pProcessor)
{
	m_Shutdown = false;
	m_pProcessor = pProcessor;
	m_pThread = thread_init(ThreadFunc, this);
	m_BufferDone.signal();
}

void CGraphicsBackend_Threaded::StopProcessor()
{
	m_Shutdown = true;
	m_Activity.signal();
	thread_wait(m_pThread);
	thread_destroy(m_pThread);
}

void CGraphicsBackend_Threaded::RunBuffer(CCommandBuffer *pBuffer)
{
	WaitForIdle();
	m_pBuffer = pBuffer;
	m_Activity.signal();
}

bool CGraphicsBackend_Threaded::IsIdle() const
{
	return m_pBuffer == 0x0;
}

void CGraphicsBackend_Threaded::WaitForIdle()
{
	while(m_pBuffer != 0x0)
		m_BufferDone.wait();
}

// ------------ CCommandProcessorFragment_General

void CCommandProcessorFragment_General::Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand)
{
	pCommand->m_pSemaphore->signal();
}

bool CCommandProcessorFragment_General::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
		case CCommandBuffer::CMD_NOP:
			break;
		case CCommandBuffer::CMD_SIGNAL:
			Cmd_Signal(static_cast<const CCommandBuffer::SCommand_Signal *>(pBaseCommand));
			break;
		default:
			return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_OpenGL

int CCommandProcessorFragment_OpenGL::TexFormatToOpenGLFormat(int TexFormat)
{
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGB)
		return GL_RGB;
	if(TexFormat == CCommandBuffer::TEXFORMAT_ALPHA)
		return GL_ALPHA;
	if(TexFormat == CCommandBuffer::TEXFORMAT_RGBA)
		return GL_RGBA;
	return GL_RGBA;
}

unsigned char CCommandProcessorFragment_OpenGL::Sample(
	int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v + y) * w + (u + x)) * Bpp + Offset];
	return Value / (ScaleW * ScaleH);
}

void *CCommandProcessorFragment_OpenGL::Rescale(
	int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;
	int ScaleW = Width / NewWidth;
	int ScaleH = Height / NewHeight;

	int Bpp = 3;
	if(Format == CCommandBuffer::TEXFORMAT_RGBA)
		Bpp = 4;

	pTmpData = (unsigned char *)mem_alloc(NewWidth * NewHeight * Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c * Bpp] = Sample(Width, Height, pData, x * ScaleW, y * ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c * Bpp + 1] = Sample(Width, Height, pData, x * ScaleW, y * ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c * Bpp + 2] = Sample(Width, Height, pData, x * ScaleW, y * ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c * Bpp + 3] = Sample(Width, Height, pData, x * ScaleW, y * ScaleH, 3, ScaleW, ScaleH, Bpp);
		}

	return pTmpData;
}

void CCommandProcessorFragment_OpenGL::SetState(const CCommandBuffer::SState &State)
{
	// The light shader temporarily uses texture unit 1 for the collision map.
	// Keep the fixed-function state machine deterministic by always selecting
	// unit 0 before binding the source texture for a draw. Without this, a
	// previous shader command can leave unit 1 active and the light pass then
	// samples whatever stale texture happens to be bound on unit 0 (typically
	// the 1x1 white fallback), which produces a completely white frame.
	if(glActiveTextureARB)
		glActiveTextureARB(GL_TEXTURE0);

	// blend
	switch(State.m_BlendMode)
	{
		case CCommandBuffer::BLEND_NONE:
			glDisable(GL_BLEND);
			break;
		case CCommandBuffer::BLEND_ALPHA:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case CCommandBuffer::BLEND_ADDITIVE:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		case CCommandBuffer::BLEND_BUFFER:
			glEnable(GL_BLEND);
			glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case CCommandBuffer::BLEND_LIGHT:
			glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
			break;
		default:
			dbg_msg("render", "unknown blendmode %d\n", State.m_BlendMode);
	};

	// clip
	if(State.m_ClipEnable)
	{
		const bool LightTarget = State.m_RenderTarget == CCommandBuffer::RENDERTARGET_TEXTURE &&
			State.m_RenderBuffer == RENDERBUFFER_LIGHT;
		const int Scale = LightTarget ? LIGHT_RENDER_SCALE : 1;
		glScissor(State.m_ClipX / Scale, State.m_ClipY / Scale, State.m_ClipW / Scale, State.m_ClipH / Scale);
		glEnable(GL_SCISSOR_TEST);
	}
	else
		glDisable(GL_SCISSOR_TEST);

	// render target (screen or texture)
	if(m_MultiBuffering)
	{
		if(State.m_RenderTarget == CCommandBuffer::RENDERTARGET_SCREEN)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
		}
		if(State.m_RenderTarget == CCommandBuffer::RENDERTARGET_TEXTURE)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, textureBuffer[State.m_RenderBuffer]);
			glViewport(0, 0, m_aRenderBufferWidth[State.m_RenderBuffer], m_aRenderBufferHeight[State.m_RenderBuffer]);
		}
	}
	else
	{
		if((State.m_RenderTarget == CCommandBuffer::RENDERTARGET_SCREEN ||
			State.m_RenderTarget == CCommandBuffer::RENDERTARGET_TEXTURE)
			&& glBindFramebuffer)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
		}
	}

	// screen texture buffer
	if(State.m_Texture == -2 && m_MultiBuffering)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderedTexture[State.m_BufferTexture]);
	}
	// texture
	else if(State.m_Texture >= 0 && State.m_Texture < CCommandBuffer::MAX_TEXTURES)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_aTextures[State.m_Texture].m_Tex);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_PixelTexture);
	}

	switch(State.m_WrapMode)
	{
		case CCommandBuffer::WRAP_REPEAT:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			break;
		case CCommandBuffer::WRAP_CLAMP:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		default:
			dbg_msg("render", "unknown wrapmode %d\n", State.m_WrapMode);
	};

	// screen mapping
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(State.m_ScreenTL.x, State.m_ScreenBR.x, State.m_ScreenBR.y, State.m_ScreenTL.y, 1.0f, 10.f);
}

void CCommandProcessorFragment_OpenGL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_MultiBuffering = false;
	m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;

	m_ScreenWidth = 640;
	m_ScreenHeight = 480;
	m_CameraX = 0;
	m_CameraY = 0;
	m_ShadersLoaded = false;
	m_AmbientR = 0.18f;
	m_AmbientG = 0.18f;
	m_AmbientB = 0.22f;
}

void CCommandProcessorFragment_OpenGL::Cmd_SetViewport(const CCommandBuffer::SCommand_SetViewport *pCommand)
{
	m_ScreenWidth = max(1, pCommand->m_Width);
	m_ScreenHeight = max(1, pCommand->m_Height);
	glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand)
{
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);
	glTexSubImage2D(GL_TEXTURE_2D,
					0,
					pCommand->m_X,
					pCommand->m_Y,
					pCommand->m_Width,
					pCommand->m_Height,
					TexFormatToOpenGLFormat(pCommand->m_Format),
					GL_UNSIGNED_BYTE,
					pCommand->m_pData);
	mem_free(pCommand->m_pData);
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand)
{
	if(pCommand->m_Slot < 0 || pCommand->m_Slot >= CCommandBuffer::MAX_TEXTURES)
		return;
	if(m_aTextures[pCommand->m_Slot].m_Tex)
		glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	if(m_pTextureMemoryUsage)
		*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
	m_aTextures[pCommand->m_Slot].m_Tex = 0;
	m_aTextures[pCommand->m_Slot].m_MemSize = 0;
}

void CCommandProcessorFragment_OpenGL::Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand)
{
	if(pCommand->m_Slot < 0 || pCommand->m_Slot >= CCommandBuffer::MAX_TEXTURES)
	{
		mem_free(pCommand->m_pData);
		return;
	}
	if(m_aTextures[pCommand->m_Slot].m_Tex)
	{
		glDeleteTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
		if(m_pTextureMemoryUsage)
			*m_pTextureMemoryUsage -= m_aTextures[pCommand->m_Slot].m_MemSize;
		m_aTextures[pCommand->m_Slot].m_Tex = 0;
		m_aTextures[pCommand->m_Slot].m_MemSize = 0;
	}

	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;
	void *pTexData = pCommand->m_pData;

	// resample if needed
	if(pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGBA || pCommand->m_Format == CCommandBuffer::TEXFORMAT_RGB)
	{
		int MaxTexSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTexSize);
		if(Width > MaxTexSize || Height > MaxTexSize)
		{
			do
			{
				Width >>= 1;
				Height >>= 1;
			} while(Width > MaxTexSize || Height > MaxTexSize);

			void *pTmpData = Rescale(pCommand->m_Width,
									 pCommand->m_Height,
									 Width,
									 Height,
									 pCommand->m_Format,
									 static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
		else if(Width > 16 && Height > 16 && (pCommand->m_Flags & CCommandBuffer::TEXFLAG_QUALITY) == 0)
		{
			Width >>= 1;
			Height >>= 1;

			void *pTmpData = Rescale(pCommand->m_Width,
									 pCommand->m_Height,
									 Width,
									 Height,
									 pCommand->m_Format,
									 static_cast<const unsigned char *>(pCommand->m_pData));
			mem_free(pTexData);
			pTexData = pTmpData;
		}
	}

	int Oglformat = TexFormatToOpenGLFormat(pCommand->m_Format);
	int StoreOglformat = TexFormatToOpenGLFormat(pCommand->m_StoreFormat);

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_COMPRESSED)
	{
		switch(StoreOglformat)
		{
			case GL_RGB:
				StoreOglformat = GL_COMPRESSED_RGB_ARB;
				break;
			case GL_ALPHA:
				StoreOglformat = GL_COMPRESSED_ALPHA_ARB;
				break;
			case GL_RGBA:
				StoreOglformat = GL_COMPRESSED_RGBA_ARB;
				break;
			default:
				StoreOglformat = GL_COMPRESSED_RGBA_ARB;
		}
	}
	glGenTextures(1, &m_aTextures[pCommand->m_Slot].m_Tex);
	glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_Slot].m_Tex);

	if(pCommand->m_Flags & CCommandBuffer::TEXFLAG_NOMIPMAPS)
	{
		const GLint Filter = (pCommand->m_Flags & CCommandBuffer::TEXFLAG_NEAREST) ? GL_NEAREST : GL_LINEAR;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Filter);
		glTexImage2D(GL_TEXTURE_2D, 0, StoreOglformat, Width, Height, 0, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, StoreOglformat, Width, Height, Oglformat, GL_UNSIGNED_BYTE, pTexData);
	}

	// calculate memory usage
	m_aTextures[pCommand->m_Slot].m_MemSize = Width * Height * pCommand->m_PixelSize;
	while(Width > 2 && Height > 2)
	{
		Width >>= 1;
		Height >>= 1;
		m_aTextures[pCommand->m_Slot].m_MemSize += Width * Height * pCommand->m_PixelSize;
	}
	*m_pTextureMemoryUsage += m_aTextures[pCommand->m_Slot].m_MemSize;

	mem_free(pTexData);
}

void CCommandProcessorFragment_OpenGL::DestroyTextureBuffers()
{
	for(int i = 0; i < NUM_RENDERBUFFERS; i++)
	{
		if(textureBuffer[i] && glDeleteFramebuffers)
			glDeleteFramebuffers(1, &textureBuffer[i]);
		if(renderedTexture[i])
			glDeleteTextures(1, &renderedTexture[i]);
		textureBuffer[i] = 0;
		renderedTexture[i] = 0;
		m_aRenderBufferWidth[i] = 0;
		m_aRenderBufferHeight[i] = 0;
	}
	if(m_PixelTexture)
	{
		glDeleteTextures(1, &m_PixelTexture);
		m_PixelTexture = 0;
	}
	m_MultiBuffering = false;
}

static bool FramebufferFunctionsAvailable()
{
	return glGenFramebuffers && glBindFramebuffer && glDeleteFramebuffers && glFramebufferTexture2D &&
		glDrawBuffers && glCheckFramebufferStatus;
}

void CCommandProcessorFragment_OpenGL::Cmd_DestroyTextureBuffer(
	const CCommandBuffer::SCommand_DestroyTextureBuffer *pCommand)
{
	(void)pCommand;
	DestroyTextureBuffers();
}

void CCommandProcessorFragment_OpenGL::Cmd_CreateTextureBuffer(
	const CCommandBuffer::SCommand_CreateTextureBuffer *pCommand)
{
	dbg_msg("render", "creating texture buffers");
	dbg_msg("render", "creating render buffers (shader=%d)", m_ShadersLoaded ? 1 : 0);
	DestroyTextureBuffers();
	if(!FramebufferFunctionsAvailable())
	{
		dbg_msg("render", "framebuffer creation skipped: required OpenGL functions unavailable");
		return;
	}

	int Width = pCommand->m_Width;
	int Height = pCommand->m_Height;

	// create 1x1 white texture for shaders
	glGenTextures(1, &m_PixelTexture);
	glBindTexture(GL_TEXTURE_2D, m_PixelTexture);

	GLubyte texData[] = {255, 255, 255, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// create texture buffers
	for(int i = 0; i < NUM_RENDERBUFFERS - 1; i++)
	{
		const int BufferWidth = i == RENDERBUFFER_LIGHT ? max(1, Width / LIGHT_RENDER_SCALE) : Width;
		const int BufferHeight = i == RENDERBUFFER_LIGHT ? max(1, Height / LIGHT_RENDER_SCALE) : Height;
		m_aRenderBufferWidth[i] = BufferWidth;
		m_aRenderBufferHeight[i] = BufferHeight;
		textureBuffer[i] = 0;
		glGenFramebuffers(1, &textureBuffer[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, textureBuffer[i]);

		// create texture
		glGenTextures(1, &renderedTexture[i]);
		glBindTexture(GL_TEXTURE_2D, renderedTexture[i]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, BufferWidth, BufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// attach texture to buffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture[i], 0);

		GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, DrawBuffers);

		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			dbg_msg("render", "framebuffer incomplete");

		// glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// menu buffer, smaller one
	int i = NUM_RENDERBUFFERS - 1;
	{
		m_aRenderBufferWidth[i] = max(1, Width / 4);
		m_aRenderBufferHeight[i] = max(1, Height / 4);
		textureBuffer[i] = 0;
		glGenFramebuffers(1, &textureBuffer[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, textureBuffer[i]);

		// create texture
		glGenTextures(1, &renderedTexture[i]);
		glBindTexture(GL_TEXTURE_2D, renderedTexture[i]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_aRenderBufferWidth[i], m_aRenderBufferHeight[i], 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// attach texture to buffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture[i], 0);

		GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, DrawBuffers);

		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			dbg_msg("render", "framebuffer incomplete");

		// glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	dbg_msg("render", "texture buffers created (%d, %d)", Width, Height);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_MultiBuffering = true;
}

static bool ShaderFunctionsAvailable()
{
	return GLEW_VERSION_2_0 && glAttachObjectARB && glCompileShaderARB && glCreateProgramObjectARB &&
		   glCreateShaderObjectARB && glDeleteObjectARB && glGetInfoLogARB && glGetObjectParameterivARB &&
		   glGetUniformLocationARB && glLinkProgramARB && glShaderSourceARB && glUniform1iARB && glUniform1fARB &&
		   glUniform2fv && glUniform4fv && glUseProgramObjectARB && glActiveTextureARB;
}

void CCommandProcessorFragment_OpenGL::Cmd_LoadShaders(const CCommandBuffer::SCommand_LoadShaders *pCommand)
{
	m_ShadersLoaded = false;
	if(pCommand->m_pAvailable)
		for(int i = 0; i < NUM_SHADERS; i++)
			pCommand->m_pAvailable[i] = 0;
	if(!ShaderFunctionsAvailable())
	{
		dbg_msg("gfx", "shader loading skipped: required OpenGL 2.0 functions unavailable");
		return;
	}

	m_aShader[SHADER_PLAYER] = LoadShader("data/shaders/basic.vert", "data/shaders/player.frag");
	m_aShader[SHADER_BALL] = LoadShader("data/shaders/basic.vert", "data/shaders/ball.frag");
	m_aShader[SHADER_ELECTRIC] = LoadShader("data/shaders/basic.vert", "data/shaders/electric.frag");
	m_aShader[SHADER_DEATHRAY] = LoadShader("data/shaders/basic.vert", "data/shaders/deathray.frag");
	m_aShader[SHADER_COLORSWAP] = LoadShader("data/shaders/basic.vert", "data/shaders/colorswap.frag");
	m_aShader[SHADER_SPAWN] = LoadShader("data/shaders/basic.vert", "data/shaders/spawn.frag");
	m_aShader[SHADER_DAMAGE] = LoadShader("data/shaders/basic.vert", "data/shaders/damage.frag");
	m_aShader[SHADER_SHIELD] = LoadShader("data/shaders/basic.vert", "data/shaders/shield.frag");
	m_aShader[SHADER_INVISIBILITY] = LoadShader("data/shaders/basic.vert", "data/shaders/invisibility.frag");
	m_aShader[SHADER_RAGE] = LoadShader("data/shaders/basic.vert", "data/shaders/rage.frag");
	m_aShader[SHADER_FUEL] = LoadShader("data/shaders/basic.vert", "data/shaders/fuel.frag");
	m_aShader[SHADER_BLOOD] = LoadShader("data/shaders/basic.vert", "data/shaders/blood.frag");
	m_aShader[SHADER_ACID] = LoadShader("data/shaders/basic.vert", "data/shaders/acid.frag");
	m_aShader[SHADER_GRAYSCALE] = LoadShader("data/shaders/basic.vert", "data/shaders/grayscale.frag");
	m_aShader[SHADER_MENU] = LoadShader("data/shaders/basic.vert", "data/shaders/menu.frag");
	m_aShader[SHADER_LIGHT] = LoadShader("data/shaders/basic.vert", "data/shaders/light.frag");
	m_aShader[SHADER_LIGHT_POLAR] = LoadShader("data/shaders/basic.vert", "data/shaders/light_polar.frag");
	m_aShader[SHADER_LIGHT_COMPOSITE] = LoadShader("data/shaders/basic.vert", "data/shaders/light_composite.frag");
	m_aShader[SHADER_LOW_HEALTH] = LoadShader("data/shaders/basic.vert", "data/shaders/low_health.frag");
	for(int i = 0; i < NUM_SHADERS; i++)
		if(!m_aShader[i].Handle())
		{
			dbg_msg("gfx", "shader initialization incomplete; using fixed-function rendering");
			return;
		}

	if(pCommand->m_pAvailable)
		for(int i = 0; i < NUM_SHADERS; i++)
			pCommand->m_pAvailable[i] = 1;
	m_ShadersLoaded = true;
}

void CCommandProcessorFragment_OpenGL::Cmd_CameraToShaders(const CCommandBuffer::SCommand_CameraToShaders *pCommand)
{
	(void)pCommand->m_ScreenWidth;
	(void)pCommand->m_ScreenHeight;
	m_CameraX = pCommand->m_CameraX;
	m_CameraY = pCommand->m_CameraY;
}

float CCommandProcessorFragment_OpenGL::GetTime()
{
	return static_cast<float>(clock()) / CLOCKS_PER_SEC;
}

void CCommandProcessorFragment_OpenGL::Cmd_ShaderBegin(const CCommandBuffer::SCommand_ShaderBegin *pCommand)
{
	if(!m_ShadersLoaded || pCommand->m_Shader < 0 || pCommand->m_Shader >= NUM_SHADERS ||
	   !m_aShader[pCommand->m_Shader].Handle())
		return;

	CShader *pShader = &(m_aShader[pCommand->m_Shader]);
	glActiveTextureARB(GL_TEXTURE0);
	glUseProgramObjectARB(pShader->Handle());

	// Every shader in the legacy pipeline samples its source from unit 0. Set
	// this explicitly because the light pass also uses unit 1.
	GLint location = pShader->getUniformLocation("texture");
	if(location >= 0)
		glUniform1iARB(location, 0);

	location = pShader->getUniformLocation("rnd");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(frandom()));

	// float Time = time_get() / 200000.0f;
	float Time = GetTime() * 100.0f;

	location = pShader->getUniformLocation("time");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(Time));

	location = pShader->getUniformLocation("intensity");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_Intensity));

	location = pShader->getUniformLocation("colorswap");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_ColorSwap));

	location = pShader->getUniformLocation("weaponcharge");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_WeaponCharge));

	location = pShader->getUniformLocation("weapon_tint");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_WeaponTint));

	location = pShader->getUniformLocation("visibility");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_Visibility));

	location = pShader->getUniformLocation("electro");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_Electro));

	location = pShader->getUniformLocation("damage");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_Damage));

	location = pShader->getUniformLocation("deathray");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_Deathray));

	location = pShader->getUniformLocation("screenwidth");
	if(location >= 0)
		glUniform1iARB(location, GLint(m_ScreenWidth));

	location = pShader->getUniformLocation("screenheight");
	if(location >= 0)
		glUniform1iARB(location, GLint(m_ScreenHeight));

	location = pShader->getUniformLocation("camerax");
	if(location >= 0)
		glUniform1iARB(location, GLint(m_CameraX));

	location = pShader->getUniformLocation("cameray");
	if(location >= 0)
		glUniform1iARB(location, GLint(m_CameraY));

	// SHADER_LIGHT extras: light center/radius and the collision texture on
	// unit 1 (the light buffer stays on unit 0).
	location = pShader->getUniformLocation("lightcenter");
	if(location >= 0)
	{
		const GLfloat aLightCenter[2] = {pCommand->m_LightCenterX, pCommand->m_LightCenterY};
		glUniform2fv(location, 1, aLightCenter);
	}

	location = pShader->getUniformLocation("lightradius");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_LightRadius));

	location = pShader->getUniformLocation("collisionsize");
	if(location >= 0)
	{
		const GLfloat aCollisionSize[2] = {pCommand->m_CollisionWidth, pCommand->m_CollisionHeight};
		glUniform2fv(location, 1, aCollisionSize);
	}

	location = pShader->getUniformLocation("collision");
	if(location >= 0)
	{
		if(pCommand->m_ExtraTexture >= 0 && pCommand->m_ExtraTexture < CCommandBuffer::MAX_TEXTURES)
		{
			glActiveTextureARB(GL_TEXTURE1);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_ExtraTexture].m_Tex);
			glUniform1iARB(location, 1);
			glActiveTextureARB(GL_TEXTURE0);
		}
		else
			glUniform1iARB(location, -1);
	}

	location = pShader->getUniformLocation("shadowmap");
	if(location >= 0)
	{
		if(pCommand->m_ShadowTexture >= 0 && pCommand->m_ShadowTexture < CCommandBuffer::MAX_TEXTURES)
		{
			glActiveTextureARB(GL_TEXTURE2);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_aTextures[pCommand->m_ShadowTexture].m_Tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glUniform1iARB(location, 2);
			glActiveTextureARB(GL_TEXTURE0);
		}
		else
			glUniform1iARB(location, -1);
	}

	location = pShader->getUniformLocation("shadowrow");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_ShadowRow));

	location = pShader->getUniformLocation("shadowrows");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_ShadowRows));

	location = pShader->getUniformLocation("shadowsamples");
	if(location >= 0)
		glUniform1fARB(location, GLfloat(pCommand->m_ShadowSamples));

	location = pShader->getUniformLocation("viewtl");
	if(location >= 0)
	{
		const GLfloat aView[2] = {pCommand->m_ViewTLX, pCommand->m_ViewTLY};
		glUniform2fv(location, 1, aView);
	}

	location = pShader->getUniformLocation("viewbr");
	if(location >= 0)
	{
		const GLfloat aView[2] = {pCommand->m_ViewBRX, pCommand->m_ViewBRY};
		glUniform2fv(location, 1, aView);
	}

	location = pShader->getUniformLocation("targetsize");
	if(location >= 0)
	{
		const GLfloat aTarget[2] = {pCommand->m_TargetWidth, pCommand->m_TargetHeight};
		glUniform2fv(location, 1, aTarget);
	}
}

void CCommandProcessorFragment_OpenGL::Cmd_ShaderEnd(const CCommandBuffer::SCommand_ShaderEnd *pCommand)
{
	if(glUseProgramObjectARB)
		glUseProgramObjectARB(0);
	// The collision sampler is bound on a separate fixed-function texture
	// unit. Always restore that unit, including when shader loading failed.
	if(glActiveTextureARB)
	{
		glActiveTextureARB(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glActiveTextureARB(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glActiveTextureARB(GL_TEXTURE0);
	}
}

GLint CCommandProcessorFragment_OpenGL::CShader::getUniformLocation(const GLcharARB *pName)
{
	GLint &rCachePos = m_aUniformLocationCache[pName].value;
	if(rCachePos > -2)
		return rCachePos;

	return (rCachePos = glGetUniformLocationARB(m_Program, pName));
}

void CCommandProcessorFragment_OpenGL::Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
{
	glClearColor(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CCommandProcessorFragment_OpenGL::Cmd_ClearBufferTexture(
	const CCommandBuffer::SCommand_ClearBufferTexture *pCommand)
{
	if(!m_MultiBuffering)
		return;

	for(int i = 0; i < NUM_RENDERBUFFERS - 1; i++)
	{
		if(i == RENDERBUFFER_LIGHT)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, textureBuffer[i]);
			// The brightness target originates at the server and is interpolated by
			// the client. Scaling the ambient clear color here keeps the transition
			// smooth even outside the camera light quad.
			const float Brightness = clamp(pCommand->m_LightingBrightness, 0.0f, 1.0f);
			glClearColor(m_AmbientR * Brightness,
				m_AmbientG * Brightness,
				m_AmbientB * Brightness,
				1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			continue;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, textureBuffer[i]);
		glClearColor(0, 0, 0, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// not needed
	/*
	glEnable(GL_TEXTURE_2D);
	GLuint clearColor[4] = {0, 0, 0, 1};

	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glClearTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &clearColor);
	*/
}

void CCommandProcessorFragment_OpenGL::Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
{
	SetState(pCommand->m_State);

	glVertexPointer(3, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 3);
	glColorPointer(4, GL_FLOAT, sizeof(CCommandBuffer::SVertex), (char *)pCommand->m_pVertices + sizeof(float) * 5);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	switch(pCommand->m_PrimType)
	{
		case CCommandBuffer::PRIMTYPE_QUADS:
			glDrawArrays(GL_QUADS, 0, pCommand->m_PrimCount * 4);
			break;
		case CCommandBuffer::PRIMTYPE_LINES:
			glDrawArrays(GL_LINES, 0, pCommand->m_PrimCount * 2);
			break;
		default:
			dbg_msg("render", "unknown primtype %d\n", pCommand->m_Cmd);
	};
}

void CCommandProcessorFragment_OpenGL::Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand)
{
	// fetch image data
	GLint aViewport[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, aViewport);

	int w = aViewport[2];
	int h = aViewport[3];

	// we allocate one more row to use when we are flipping the texture
	unsigned char *pPixelData = (unsigned char *)mem_alloc(w * (h + 1) * 3, 1);
	unsigned char *pTempRow = pPixelData + w * h * 3;

	// fetch the pixels
	GLint Alignment;
	glGetIntegerv(GL_PACK_ALIGNMENT, &Alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pPixelData);
	glPixelStorei(GL_PACK_ALIGNMENT, Alignment);

	// flip the pixel because opengl works from bottom left corner
	for(int y = 0; y < h / 2; y++)
	{
		mem_copy(pTempRow, pPixelData + y * w * 3, w * 3);
		mem_copy(pPixelData + y * w * 3, pPixelData + (h - y - 1) * w * 3, w * 3);
		mem_copy(pPixelData + (h - y - 1) * w * 3, pTempRow, w * 3);
	}

	// fill in the information
	pCommand->m_pImage->m_Width = w;
	pCommand->m_pImage->m_Height = h;
	pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGB;
	pCommand->m_pImage->m_pData = pPixelData;
}

CCommandProcessorFragment_OpenGL::CCommandProcessorFragment_OpenGL()
{
	mem_zero(m_aTextures, sizeof(m_aTextures));
	mem_zero(textureBuffer, sizeof(textureBuffer));
	mem_zero(renderedTexture, sizeof(renderedTexture));
	m_pTextureMemoryUsage = 0;
	m_PixelTexture = 0;
	mem_zero(m_aRenderBufferWidth, sizeof(m_aRenderBufferWidth));
	mem_zero(m_aRenderBufferHeight, sizeof(m_aRenderBufferHeight));
}

bool CCommandProcessorFragment_OpenGL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
		case CMD_INIT:
			Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_SETVIEWPORT:
			Cmd_SetViewport(static_cast<const CCommandBuffer::SCommand_SetViewport *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_TEXTURE_CREATE:
			Cmd_Texture_Create(static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_TEXTURE_DESTROY:
			Cmd_Texture_Destroy(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_TEXTURE_UPDATE:
			Cmd_Texture_Update(static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_CLEAR:
			Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_CLEARBUFFERTEXTURE:
			Cmd_ClearBufferTexture(static_cast<const CCommandBuffer::SCommand_ClearBufferTexture *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_RENDER:
			Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_SCREENSHOT:
			Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_Screenshot *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_CREATETEXTUREBUFFER:
			Cmd_CreateTextureBuffer(static_cast<const CCommandBuffer::SCommand_CreateTextureBuffer *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_DESTROYTEXTUREBUFFER:
			Cmd_DestroyTextureBuffer(static_cast<const CCommandBuffer::SCommand_DestroyTextureBuffer *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_LOADSHADERS:
			Cmd_LoadShaders(static_cast<const CCommandBuffer::SCommand_LoadShaders *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_SHADERBEGIN:
			Cmd_ShaderBegin(static_cast<const CCommandBuffer::SCommand_ShaderBegin *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_SHADEREND:
			Cmd_ShaderEnd(static_cast<const CCommandBuffer::SCommand_ShaderEnd *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_CAMERATOSHADERS:
			Cmd_CameraToShaders(static_cast<const CCommandBuffer::SCommand_CameraToShaders *>(pBaseCommand));
			break;
		default:
			return false;
	}

	return true;
}

// ------------ CCommandProcessorFragment_SDL

void CCommandProcessorFragment_SDL::Cmd_Init(const SCommand_Init *pCommand)
{
	m_GLContext = pCommand->m_GLContext;
	m_pWindow = pCommand->m_pWindow;
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
	if(!SDL_GL_SetSwapInterval(pCommand->m_VSync ? 1 : 0))
		dbg_msg("gfx", "unable to set initial V-Sync interval: %s", SDL_GetError());

	// set some default settings
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glAlphaFunc(GL_GREATER, 0);
	glEnable(GL_ALPHA_TEST);
	glDepthMask(0);

	glewInit();

	// init shaders
	glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)SDL_GL_GetProcAddress("glAttachObjectARB");
	glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)SDL_GL_GetProcAddress("glCompileShaderARB");
	glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glCreateProgramObjectARB");
	glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)SDL_GL_GetProcAddress("glCreateShaderObjectARB");
	glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)SDL_GL_GetProcAddress("glDeleteObjectARB");
	glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)SDL_GL_GetProcAddress("glGetInfoLogARB");
	glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)SDL_GL_GetProcAddress("glGetObjectParameterivARB");
	glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)SDL_GL_GetProcAddress("glGetUniformLocationARB");
	glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)SDL_GL_GetProcAddress("glLinkProgramARB");
	glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)SDL_GL_GetProcAddress("glShaderSourceARB");
	glUniform1iARB = (PFNGLUNIFORM1IARBPROC)SDL_GL_GetProcAddress("glUniform1iARB");
	glUniform1fARB = (PFNGLUNIFORM1FARBPROC)SDL_GL_GetProcAddress("glUniform1fARB");
	glUniform2fv = (PFNGLUNIFORM2FVPROC)SDL_GL_GetProcAddress("glUniform2fv");
	glUniform4fv = (PFNGLUNIFORM4FVPROC)SDL_GL_GetProcAddress("glUniform4fv");
	// PFNGLUNIFORM2FVPROC glUniform2fv;
	glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glUseProgramObjectARB");
	if(ShaderFunctionsAvailable())
	{
		dbg_msg("gfx", "shaders ok!");
	}
	else
	{
		dbg_msg("gfx", "unable to init shaders");
	}
}

void CCommandProcessorFragment_SDL::Cmd_SetVSync(const CCommandBuffer::SCommand_SetVSync *pCommand)
{
	*pCommand->m_pSuccess = SDL_GL_SetSwapInterval(pCommand->m_Enabled ? 1 : 0);
	if(!*pCommand->m_pSuccess)
		dbg_msg("gfx", "unable to set V-Sync interval: %s", SDL_GetError());
}

void CCommandProcessorFragment_SDL::Cmd_Shutdown(const SCommand_Shutdown *pCommand)
{
	// Release the context from this thread
	SDL_GL_MakeCurrent(0, 0);
}

void CCommandProcessorFragment_SDL::Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
{
	SDL_GL_SwapWindow(m_pWindow);

	if(pCommand->m_Finish)
		glFinish();
}

void CCommandProcessorFragment_SDL::Cmd_VideoModes(const CCommandBuffer::SCommand_VideoModes *pCommand)
{
	int DisplayNum = 0;
	SDL_DisplayID *pDisplayIds = SDL_GetDisplays(&DisplayNum);
	SDL_DisplayID DisplayID = 0;
	if(pDisplayIds && DisplayNum > 0)
	{
		const int Index = clamp(pCommand->m_Screen, 0, DisplayNum - 1);
		DisplayID = pDisplayIds[Index];
	}
	SDL_free(pDisplayIds);

	int MaxModes = 0;
	int NumModes = 0;
	SDL_DisplayMode **ppModes = SDL_GetFullscreenDisplayModes(DisplayID, &MaxModes);
	for(int i = 0; i < MaxModes; i++)
	{
		if(!ppModes[i])
		{
			dbg_msg("gfx", "unable to get display mode: %s", SDL_GetError());
			continue;
		}
		bool Skip = false;
		for(int j = 0; j < NumModes; j++)
		{
			if(pCommand->m_pModes[j].m_Width == ppModes[i]->w && pCommand->m_pModes[j].m_Height == ppModes[i]->h)
			{
				Skip = true;
				break;
			}
		}
		if(Skip)
			continue;

		if(ppModes[i]->w <= 0 || ppModes[i]->h <= 0)
			continue;

		if(NumModes >= pCommand->m_MaxModes)
			break;

		pCommand->m_pModes[NumModes].m_Width = ppModes[i]->w;
		pCommand->m_pModes[NumModes].m_Height = ppModes[i]->h;
		pCommand->m_pModes[NumModes].m_Red = 8;
		pCommand->m_pModes[NumModes].m_Green = 8;
		pCommand->m_pModes[NumModes].m_Blue = 8;
		NumModes++;
	}
	SDL_free(ppModes);
	*pCommand->m_pNumModes = NumModes;
}

CCommandProcessorFragment_SDL::CCommandProcessorFragment_SDL()
{
}

bool CCommandProcessorFragment_SDL::RunCommand(const CCommandBuffer::SCommand *pBaseCommand)
{
	switch(pBaseCommand->m_Cmd)
	{
		case CCommandBuffer::CMD_SWAP:
			Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_SETVSYNC:
			Cmd_SetVSync(static_cast<const CCommandBuffer::SCommand_SetVSync *>(pBaseCommand));
			break;
		case CCommandBuffer::CMD_VIDEOMODES:
			Cmd_VideoModes(static_cast<const CCommandBuffer::SCommand_VideoModes *>(pBaseCommand));
			break;
		case CMD_INIT:
			Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
			break;
		case CMD_SHUTDOWN:
			Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand));
			break;
		default:
			return false;
	}

	return true;
}

// ------------ CCommandProcessor_SDL_OpenGL

void CCommandProcessor_SDL_OpenGL::RunBuffer(CCommandBuffer *pBuffer)
{
	unsigned CmdIndex = 0;
	while(1)
	{
		const CCommandBuffer::SCommand *pBaseCommand = pBuffer->GetCommand(&CmdIndex);
		if(pBaseCommand == 0x0)
			break;

		if(m_OpenGL.RunCommand(pBaseCommand))
			continue;

		if(m_SDL.RunCommand(pBaseCommand))
			continue;

		if(m_General.RunCommand(pBaseCommand))
			continue;

		dbg_msg("graphics", "unknown command %d", pBaseCommand->m_Cmd);
	}
}

// ------------ CGraphicsBackend_SDL_OpenGL

CGraphicsBackend_SDL_OpenGL::CGraphicsBackend_SDL_OpenGL()
	: m_pWindow(0), m_GLContext(0), m_OffscreenCapture(false), m_pProcessor(0), m_TextureMemoryUsage(0)
{
}

void CGraphicsBackend_SDL_OpenGL::CleanupFailedInit()
{
	if(m_GLContext)
	{
		SDL_GL_DestroyContext(m_GLContext);
		m_GLContext = 0;
	}
	if(m_pWindow)
	{
		SDL_DestroyWindow(m_pWindow);
		m_pWindow = 0;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// Keep gfx_screen as a 0-based index. Older builds wrongly saved SDL_DisplayID here.
int CGraphicsBackend_SDL_OpenGL::ResolveScreenIndex(int Screen) const
{
	int DisplayNum = 0;
	SDL_DisplayID *pDisplayIds = SDL_GetDisplays(&DisplayNum);
	if(!pDisplayIds || DisplayNum <= 0)
		return 0;

	int Result = 0;
	if(Screen >= 0 && Screen < DisplayNum)
		Result = Screen;
	else
	{
		for(int i = 0; i < DisplayNum; i++)
		{
			if((int)pDisplayIds[i] == Screen)
			{
				Result = i;
				break;
			}
		}
	}

	SDL_free(pDisplayIds);
	return Result;
}

SDL_DisplayID CGraphicsBackend_SDL_OpenGL::DisplayIDFromIndex(int Index) const
{
	int DisplayNum = 0;
	SDL_DisplayID *pDisplayIds = SDL_GetDisplays(&DisplayNum);
	if(!pDisplayIds || DisplayNum <= 0)
		return 0;

	Index = clamp(Index, 0, DisplayNum - 1);
	const SDL_DisplayID DisplayID = pDisplayIds[Index];
	SDL_free(pDisplayIds);
	return DisplayID;
}
an issue where ninslash is at a higher resolution
static void ClampWindowSizeToDesktop(int *pWidth, int *pHeight, int DesktopWidth, int DesktopHeight)
{
	if(!pWidth || !pHeight || DesktopWidth <= 0 || DesktopHeight <= 0)
		return;
	const int OldWidth = *pWidth;
	const int OldHeight = *pHeight;
	if(*pWidth > DesktopWidth)
		*pWidth = DesktopWidth;
	if(*pHeight > DesktopHeight)
		*pHeight = DesktopHeight;
	if(OldWidth != *pWidth || OldHeight != *pHeight)
		dbg_msg("gfx",
				"clamped window size from %dx%d to %dx%d (desktop %dx%d)",
				OldWidth,
				OldHeight,
				*pWidth,
				*pHeight,
				DesktopWidth,
				DesktopHeight);
}

int CGraphicsBackend_SDL_OpenGL::Init(const char *pName,
									  int *Width,
									  int *Height,
									  int *pScreen,
									  int FsaaSamples,
									  int Flags,
									  int *pDesktopWidth,
									  int *pDesktopHeight)
{
	const char *pOffscreenCapture = SDL_getenv("NINSLASH_OFFSCREEN");
	m_OffscreenCapture = pOffscreenCapture && pOffscreenCapture[0] && SDL_strcmp(pOffscreenCapture, "0") != 0;

#if defined(CONF_PLATFORM_LINUX)
	// Do not force SDL_VIDEODRIVER=wayland. SDL defaults to X11 (XWayland) on a
	// Wayland session — same as Teeworlds — where SetWindowPosition can center
	// windowed/borderless windows after a resize. Native Wayland rejects
	// client-side moves for normal toplevels ("wayland cannot position
	// non-popup windows"), so borderless resolution changes keep the old
	// top-left and never look centered.
	// Opt into native Wayland only when needed: SDL_VIDEODRIVER=wayland.
#endif

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
		{
			dbg_msg("gfx", "unable to init SDL video: %s", SDL_GetError());
			return -1;
		}
	}

	dbg_msg("gfx", "SDL video driver: %s", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(none)");

	SDL_Rect ScreenPos;
	int NumScreens = GetNumScreens();
	if(NumScreens > 0)
	{
		*pScreen = ResolveScreenIndex(*pScreen);
		const SDL_DisplayID DisplayID = DisplayIDFromIndex(*pScreen);
		if(!SDL_GetDisplayBounds(DisplayID, &ScreenPos))
		{
			dbg_msg("gfx", "unable to retrieve screen information: %s", SDL_GetError());
			CleanupFailedInit();
			return -1;
		}

		const SDL_DisplayMode *pDisplayMode = SDL_GetDesktopDisplayMode(DisplayID);
		if(!pDisplayMode)
		{
			dbg_msg("gfx", "unable to get desktop resolution: %s", SDL_GetError());
			CleanupFailedInit();
			return -1;
		}

		*pDesktopWidth = pDisplayMode->w;
		*pDesktopHeight = pDisplayMode->h;
		if(*Width <= 0 || *Height <= 0)
		{
			*Width = *pDesktopWidth;
			*Height = *pDesktopHeight;
		}
		if(!(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN))
			ClampWindowSizeToDesktop(Width, Height, *pDesktopWidth, *pDesktopHeight);
	}
	else
	{
		dbg_msg("gfx", "unable to retrieve number of screens: %s", SDL_GetError());
		CleanupFailedInit();
		return -1;
	}

	if(FsaaSamples)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, FsaaSamples);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// set flags
	int SDLFlags = SDL_WINDOW_OPENGL;
	if(!m_OffscreenCapture && Flags & IGraphicsBackend::INITFLAG_RESIZABLE)
		SDLFlags |= SDL_WINDOW_RESIZABLE;
	if(!m_OffscreenCapture && Flags & IGraphicsBackend::INITFLAG_BORDERLESS)
		SDLFlags |= SDL_WINDOW_BORDERLESS;
	if(!m_OffscreenCapture && Flags & IGraphicsBackend::INITFLAG_FULLSCREEN)
		SDLFlags |= SDL_WINDOW_FULLSCREEN;
	if(m_OffscreenCapture)
		SDLFlags |= SDL_WINDOW_HIDDEN;

	dbg_assert(!(Flags & IGraphicsBackend::INITFLAG_BORDERLESS) || !(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN),
			   "only one of borderless and fullscreen may be activated at the same time");

	// disable desktop auto scaling on windows
	SDLFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

	// CreateWindow apparently doesn't care about the window position in fullscreen
	const char *pForceWindowFailure = SDL_getenv("NINSLASH_TEST_FORCE_WINDOW_FAILURE");
	if(pForceWindowFailure && pForceWindowFailure[0] && SDL_strcmp(pForceWindowFailure, "0") != 0)
	{
		SDL_SetError("forced window creation failure for startup test");
		m_pWindow = 0;
	}
	else
	{
		m_pWindow = SDL_CreateWindow(pName, *Width, *Height, SDLFlags);
	}
	if(m_pWindow == 0)
	{
		dbg_msg("gfx", "unable to create window: %s", SDL_GetError());
		CleanupFailedInit();
		return -1;
	}
	// teeworlds: center windowed/borderless on the selected display when the
	// desktop is larger than the window; otherwise pin to the display origin.
	{
		int OffsetX = 0;
		int OffsetY = 0;
		if(!(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN) && *pDesktopWidth > *Width &&
			*pDesktopHeight > *Height)
		{
			OffsetX = (*pDesktopWidth - *Width) / 2;
			OffsetY = (*pDesktopHeight - *Height) / 2;
		}
		SDL_SetWindowPosition(m_pWindow, ScreenPos.x + OffsetX, ScreenPos.y + OffsetY);
	}
	if(m_OffscreenCapture)
		dbg_msg("gfx", "using hidden SDL OpenGL context for offscreen capture");

#if 0
	int RenderFlags = SDL_RENDERER_ACCELERATED;
	if(Flags&IGraphicsBackend::INITFLAG_VSYNC)
		RenderFlags |= SDL_RENDERER_PRESENTVSYNC;
#endif

	m_GLContext = SDL_GL_CreateContext(m_pWindow);

	if(m_GLContext == 0)
	{
		dbg_msg("gfx", "unable to create OpenGL context: %s", SDL_GetError());
		CleanupFailedInit();
		return -1;
	}

	// release the current GL context from this thread
	SDL_GL_MakeCurrent(0, 0);

	// start the command processor
	m_pProcessor = new CCommandProcessor_SDL_OpenGL;
	StartProcessor(m_pProcessor);

	// issue init commands for OpenGL and SDL
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_OpenGL::SCommand_Init CmdOpenGL;
	CmdOpenGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
	CmdBuffer.AddCommand(CmdOpenGL);
	CCommandProcessorFragment_SDL::SCommand_Init CmdSDL;
	CmdSDL.m_GLContext = m_GLContext;
	CmdSDL.m_pWindow = m_pWindow;
	CmdSDL.m_VSync = (Flags & IGraphicsBackend::INITFLAG_VSYNC) != 0;
	CmdBuffer.AddCommand(CmdSDL);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	return 0;
}

int CGraphicsBackend_SDL_OpenGL::Shutdown()
{
	if(!m_pProcessor)
	{
		CleanupFailedInit();
		return 0;
	}
	// issue a shutdown command
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_SDL::SCommand_Shutdown Cmd;
	CmdBuffer.AddCommand(Cmd);
	RunBuffer(&CmdBuffer);
	WaitForIdle();

	// stop and delete the processor
	StopProcessor();
	delete m_pProcessor;
	m_pProcessor = 0;

	SDL_GL_DestroyContext(m_GLContext);
	m_GLContext = 0;
	SDL_DestroyWindow(m_pWindow);
	m_pWindow = 0;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

bool CGraphicsBackend_SDL_OpenGL::ApplyWindowSettings(int *pWidth, int *pHeight, int Screen, bool Fullscreen, bool Borderless)
{
	if(!m_pWindow || m_OffscreenCapture || !pWidth || !pHeight)
		return false;

	const int ResolvedScreen = ResolveScreenIndex(Screen);
	int WantWidth = max(1, *pWidth);
	int WantHeight = max(1, *pHeight);
	if(Fullscreen)
		Borderless = false;
	else
	{
		const SDL_DisplayMode *pDesktopMode = SDL_GetDesktopDisplayMode(DisplayIDFromIndex(ResolvedScreen));
		if(pDesktopMode)
			ClampWindowSizeToDesktop(&WantWidth, &WantHeight, pDesktopMode->w, pDesktopMode->h);
	}

	const SDL_WindowFlags WindowFlags = SDL_GetWindowFlags(m_pWindow);
	const bool WasFullscreen = (WindowFlags & SDL_WINDOW_FULLSCREEN) != 0;
	const bool WasBorderless = (WindowFlags & SDL_WINDOW_BORDERLESS) != 0;
	int PreviousWidth = 0;
	int PreviousHeight = 0;
	SDL_GetWindowSize(m_pWindow, &PreviousWidth, &PreviousHeight);
	const SDL_DisplayMode *pPreviousMode = SDL_GetWindowFullscreenMode(m_pWindow);
	SDL_DisplayMode PreviousMode = {};
	const bool HadPreviousMode = pPreviousMode != nullptr;
	if(HadPreviousMode)
		PreviousMode = *pPreviousMode;

	const SDL_DisplayID DisplayID = DisplayIDFromIndex(ResolvedScreen);
	SDL_DisplayMode FullscreenMode = {};
	if(Fullscreen &&
		!SDL_GetClosestFullscreenDisplayMode(DisplayID, WantWidth, WantHeight, 0.0f, false, &FullscreenMode))
	{
		dbg_msg("gfx", "unable to find fullscreen mode %dx%d: %s", WantWidth, WantHeight, SDL_GetError());
		return false;
	}
	const SDL_DisplayMode *pDesktopMode = Fullscreen ? SDL_GetDesktopDisplayMode(DisplayID) : nullptr;
	const bool UseDesktopFullscreen = Fullscreen && pDesktopMode && pDesktopMode->w == FullscreenMode.w &&
		pDesktopMode->h == FullscreenMode.h;
	const SDL_DisplayMode *pTargetMode = Fullscreen && !UseDesktopFullscreen ? &FullscreenMode : nullptr;

	auto SyncWindow = [&]() { return SDL_SyncWindow(m_pWindow); };
	auto RestoreWindow = [&]() {
		bool Restored = true;
		if(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN)
		{
			Restored = SDL_SetWindowFullscreen(m_pWindow, false);
			if(Restored)
				Restored = SyncWindow();
		}
		if(WasFullscreen)
		{
			if(Restored)
				Restored = SDL_SetWindowFullscreenMode(m_pWindow, HadPreviousMode ? &PreviousMode : nullptr);
			if(Restored)
				Restored = SDL_SetWindowFullscreen(m_pWindow, true);
		}
		else
		{
			if(Restored)
				Restored = SDL_SetWindowBordered(m_pWindow, !WasBorderless);
			if(Restored)
				Restored = SDL_SetWindowSize(m_pWindow, max(1, PreviousWidth), max(1, PreviousHeight));
		}
		if(Restored)
			Restored = SyncWindow();
		return Restored;
	};
	auto Fail = [&]() {
		RestoreWindow();
		return false;
	};

	// Match teeworlds: leaving exclusive fullscreen is a dedicated step.
	if(WasFullscreen && !Fullscreen)
	{
		if(!SDL_SetWindowFullscreen(m_pWindow, false) || !SyncWindow())
		{
			dbg_msg("gfx", "unable to leave fullscreen: %s", SDL_GetError());
			return Fail();
		}
	}

	if(Fullscreen)
	{
		if(!WasFullscreen || SDL_GetDisplayForWindow(m_pWindow) != DisplayID)
		{
			SDL_SetWindowPosition(
				m_pWindow, SDL_WINDOWPOS_UNDEFINED_DISPLAY(ResolvedScreen), SDL_WINDOWPOS_UNDEFINED);
		}
		if(!SDL_SetWindowFullscreenMode(m_pWindow, pTargetMode))
		{
			dbg_msg("gfx", "unable to set fullscreen mode %dx%d: %s", FullscreenMode.w, FullscreenMode.h, SDL_GetError());
			return Fail();
		}
		if(!SDL_SetWindowFullscreen(m_pWindow, true))
		{
			dbg_msg("gfx", "unable to enter fullscreen: %s", SDL_GetError());
			return Fail();
		}
	}
	else
	{
		// teeworlds split: borderless is only SetWindowBordered; resolution is
		// only SetWindowSize. Position uses the same centering rule as Init —
		// never SDL_WINDOWPOS_UNDEFINED (that caused focus geometry bugs).
		const bool WantBorderless = Borderless;
		const bool SizeChanged = PreviousWidth != WantWidth || PreviousHeight != WantHeight || WasFullscreen;
		const bool BorderChanged = WasBorderless != WantBorderless;
		const bool ScreenChanged = SDL_GetDisplayForWindow(m_pWindow) != DisplayID;

		if(!SDL_SetWindowBordered(m_pWindow, !WantBorderless))
		{
			dbg_msg("gfx", "unable to set window border: %s", SDL_GetError());
			return Fail();
		}
		if(SizeChanged)
		{
			if(!SDL_SetWindowSize(m_pWindow, WantWidth, WantHeight))
			{
				dbg_msg("gfx", "unable to resize window to %dx%d: %s", WantWidth, WantHeight, SDL_GetError());
				return Fail();
			}
		}
		if(SizeChanged || BorderChanged || ScreenChanged)
		{
			SDL_Rect Bounds = {};
			const SDL_DisplayMode *pDesk = SDL_GetDesktopDisplayMode(DisplayID);
			if(SDL_GetDisplayBounds(DisplayID, &Bounds) && pDesk)
			{
				int OffsetX = 0;
				int OffsetY = 0;
				if(pDesk->w > WantWidth && pDesk->h > WantHeight)
				{
					OffsetX = (pDesk->w - WantWidth) / 2;
					OffsetY = (pDesk->h - WantHeight) / 2;
				}
				const int PosX = Bounds.x + OffsetX;
				const int PosY = Bounds.y + OffsetY;
				if(!SDL_SetWindowPosition(m_pWindow, PosX, PosY))
				{
					dbg_msg("gfx",
							"unable to center window at %d,%d (%s) — on Wayland set SDL_VIDEODRIVER=x11 for Teeworlds-like positioning",
							PosX,
							PosY,
							SDL_GetError());
				}
			}
		}
	}

	if(!SyncWindow())
	{
		dbg_msg("gfx", "unable to synchronize window settings: %s", SDL_GetError());
		return Fail();
	}

	int ActualWidth = 0;
	int ActualHeight = 0;
	SDL_GetWindowSizeInPixels(m_pWindow, &ActualWidth, &ActualHeight);
	if(Fullscreen && (ActualWidth != FullscreenMode.w || ActualHeight != FullscreenMode.h))
	{
		dbg_msg("gfx",
				"fullscreen mode stayed at %dx%d after requesting %dx%d",
				ActualWidth,
				ActualHeight,
				FullscreenMode.w,
				FullscreenMode.h);
		return Fail();
	}

	// Write back the logical size the WM actually gave us (may be clamped).
	int LogicalWidth = 0;
	int LogicalHeight = 0;
	if(!SDL_GetWindowSize(m_pWindow, &LogicalWidth, &LogicalHeight) || LogicalWidth <= 0 || LogicalHeight <= 0)
	{
		LogicalWidth = WantWidth;
		LogicalHeight = WantHeight;
	}
	*pWidth = LogicalWidth;
	*pHeight = LogicalHeight;
	return true;
}

int CGraphicsBackend_SDL_OpenGL::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphicsBackend_SDL_OpenGL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::Maximize()
{
	SDL_MaximizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL_OpenGL::GrabWindow(bool grab)
{
	SDL_SetWindowMouseGrab(m_pWindow, grab ? true : false);
}

void CGraphicsBackend_SDL_OpenGL::WarpMouse(int x, int y)
{
	SDL_WarpMouseInWindow(m_pWindow, x, y);
}

int CGraphicsBackend_SDL_OpenGL::WindowActive()
{
	return m_OffscreenCapture || (SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS);
}

int CGraphicsBackend_SDL_OpenGL::WindowOpen()
{
	return m_OffscreenCapture || !(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_HIDDEN);
}

void CGraphicsBackend_SDL_OpenGL::GetViewportSize(int *pWidth, int *pHeight) const
{
	if(!pWidth || !pHeight || !SDL_GetWindowSizeInPixels(m_pWindow, pWidth, pHeight))
	{
		if(pWidth)
			*pWidth = 0;
		if(pHeight)
			*pHeight = 0;
	}
}

int CGraphicsBackend_SDL_OpenGL::GetNumScreens()
{
	int Num;
	SDL_GetDisplays(&Num);
	if(Num < 1)
		Num = 1;
	return Num;
}

void *CGraphicsBackend_SDL_OpenGL::GetWindowHandle()
{
	return m_pWindow;
}

IGraphicsBackend *CreateGraphicsBackend()
{
	return new CGraphicsBackend_SDL_OpenGL;
}
