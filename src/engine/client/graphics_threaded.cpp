#include <base/detect.h>
#include <base/math.h>
#include <base/tl/threading.h>

#include <base/system.h>
#include <engine/external/pnglite/pnglite.h>
#if defined(CONF_JPEG)
#include <cstdio>
extern "C"
{
#include <jpeglib.h>
}
#include <setjmp.h>
#endif

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/keys.h>
#include <engine/console.h>

#include <math.h> // cosf, sinf

#include "graphics_threaded.h"

#include "shaders.h"
//#include "glshader.hpp"

static CVideoMode g_aFakeModes[] = {
	{320, 200, 8, 8, 8},   {320, 240, 8, 8, 8},	  {400, 300, 8, 8, 8},	 {512, 384, 8, 8, 8},	{640, 400, 8, 8, 8},
	{640, 480, 8, 8, 8},   {720, 400, 8, 8, 8},	  {768, 576, 8, 8, 8},	 {800, 600, 8, 8, 8},	{1024, 600, 8, 8, 8},
	{1024, 768, 8, 8, 8},  {1152, 864, 8, 8, 8},  {1280, 600, 8, 8, 8},	 {1280, 720, 8, 8, 8},	{1280, 768, 8, 8, 8},
	{1280, 800, 8, 8, 8},  {1280, 960, 8, 8, 8},  {1280, 1024, 8, 8, 8}, {1360, 768, 8, 8, 8},	{1366, 768, 8, 8, 8},
	{1368, 768, 8, 8, 8},  {1400, 1050, 8, 8, 8}, {1440, 900, 8, 8, 8},	 {1440, 1050, 8, 8, 8}, {1600, 900, 8, 8, 8},
	{1600, 1000, 8, 8, 8}, {1600, 1200, 8, 8, 8}, {1680, 1050, 8, 8, 8}, {1792, 1344, 8, 8, 8}, {1800, 1440, 8, 8, 8},
	{1856, 1392, 8, 8, 8}, {1920, 1080, 8, 8, 8}, {1920, 1200, 8, 8, 8}, {1920, 1440, 8, 8, 8}, {1920, 2400, 8, 8, 8},
	{2048, 1536, 8, 8, 8}};

static void LimitWorkshopPreviewSize(CImageInfo *pImage, const char *pFilename)
{
	if(!pImage || !pImage->m_pData || !pFilename || !str_find(pFilename, "workshop_cache/previews/") ||
	   max(pImage->m_Width, pImage->m_Height) <= 512)
		return;
	const int Channels = pImage->m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;
	const float Scale = 512.0f / max(pImage->m_Width, pImage->m_Height);
	const int Width = max(1, (int)(pImage->m_Width * Scale));
	const int Height = max(1, (int)(pImage->m_Height * Scale));
	unsigned char *pData = (unsigned char *)mem_alloc((size_t)Width * Height * Channels, 1);
	if(!pData)
		return;
	for(int y = 0; y < Height; y++)
		for(int x = 0; x < Width; x++)
		{
			const int SourceX = x * pImage->m_Width / Width, SourceY = y * pImage->m_Height / Height;
			mem_copy(pData + ((size_t)y * Width + x) * Channels,
					 (unsigned char *)pImage->m_pData + ((size_t)SourceY * pImage->m_Width + SourceX) * Channels,
					 Channels);
		}
	mem_free(pImage->m_pData);
	pImage->m_pData = pData;
	pImage->m_Width = Width;
	pImage->m_Height = Height;
}

void CGraphics_Threaded::FlushVertices()
{
	if(m_NumVertices == 0)
		return;

	int NumVerts = m_NumVertices;
	m_NumVertices = 0;

	CCommandBuffer::SCommand_Render Cmd;
	Cmd.m_State = m_State;

	if(m_Drawing == DRAWING_QUADS)
	{
		Cmd.m_PrimType = CCommandBuffer::PRIMTYPE_QUADS;
		Cmd.m_PrimCount = NumVerts / 4;
	}
	else if(m_Drawing == DRAWING_LINES)
	{
		Cmd.m_PrimType = CCommandBuffer::PRIMTYPE_LINES;
		Cmd.m_PrimCount = NumVerts / 2;
	}
	else
		return;

	Cmd.m_pVertices =
		(CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(sizeof(CCommandBuffer::SVertex) * NumVerts);
	if(Cmd.m_pVertices == 0x0)
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices =
			(CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(sizeof(CCommandBuffer::SVertex) * NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}
	}

	// check if we have enough free memory in the commandbuffer
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		// kick command buffer and try again
		KickCommandBuffer();

		Cmd.m_pVertices =
			(CCommandBuffer::SVertex *)m_pCommandBuffer->AllocData(sizeof(CCommandBuffer::SVertex) * NumVerts);
		if(Cmd.m_pVertices == 0x0)
		{
			dbg_msg("graphics", "failed to allocate data for vertices");
			return;
		}

		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			dbg_msg("graphics", "failed to allocate memory for render command");
			return;
		}
	}

	mem_copy(Cmd.m_pVertices, m_aVertices, sizeof(CCommandBuffer::SVertex) * NumVerts);
}

void CGraphics_Threaded::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= MAX_VERTICES)
		FlushVertices();
}

void CGraphics_Threaded::Rotate4(const CCommandBuffer::SPoint &rCenter, CCommandBuffer::SVertex *pPoints)
{
	float c = cosf(m_Rotation);
	float s = sinf(m_Rotation);
	float x, y;
	int i;

	for(i = 0; i < 4; i++)
	{
		x = pPoints[i].m_Pos.x - rCenter.x;
		y = pPoints[i].m_Pos.y - rCenter.y;
		pPoints[i].m_Pos.x = x * c - y * s + rCenter.x;
		pPoints[i].m_Pos.y = x * s + y * c + rCenter.y;
	}
}

CGraphics_Threaded::CGraphics_Threaded()
{
	m_State.m_ScreenTL.x = 0;
	m_State.m_ScreenTL.y = 0;
	m_State.m_ScreenBR.x = 0;
	m_State.m_ScreenBR.y = 0;
	m_State.m_ClipEnable = false;
	m_State.m_ClipX = 0;
	m_State.m_ClipY = 0;
	m_State.m_ClipW = 0;
	m_State.m_ClipH = 0;
	m_State.m_Texture = -1;
	m_State.m_BlendMode = CCommandBuffer::BLEND_NONE;
	m_State.m_WrapMode = CCommandBuffer::WRAP_REPEAT;
	m_State.m_RenderTarget = CCommandBuffer::RENDERTARGET_SCREEN;

	m_CurrentCommandBuffer = 0;
	m_pCommandBuffer = 0x0;
	m_apCommandBuffers[0] = 0x0;
	m_apCommandBuffers[1] = 0x0;

	m_NumVertices = 0;

	m_ScreenWidth = -1;
	m_ScreenHeight = -1;
	m_DesktopScreenWidth = 0;
	m_DesktopScreenHeight = 0;

	m_Rotation = 0;
	m_Drawing = 0;
	m_InvalidTexture = 0;

	m_TextureMemoryUsage = 0;
	mem_zero((void *)m_aShaderAvailable, sizeof(m_aShaderAvailable));
	mem_zero(m_aTextureSources, sizeof(m_aTextureSources));

	m_RenderEnable = true;
	m_ScreenshotRequestCount = 0;
	m_NextScreenshotRequestID = 0;
	m_ScreenshotResultCount = 0;
	mem_zero(m_aScreenshotRequests, sizeof(m_aScreenshotRequests));
	mem_zero(m_aScreenshotResults, sizeof(m_aScreenshotResults));
}

void CGraphics_Threaded::ClipEnable(int x, int y, int w, int h)
{
	if(x < 0)
		w += x;
	if(y < 0)
		h += y;

	x = clamp(x, 0, ScreenWidth());
	y = clamp(y, 0, ScreenHeight());
	w = clamp(w, 0, ScreenWidth() - x);
	h = clamp(h, 0, ScreenHeight() - y);

	m_State.m_ClipEnable = true;
	m_State.m_ClipX = x;
	m_State.m_ClipY = ScreenHeight() - (y + h);
	m_State.m_ClipW = w;
	m_State.m_ClipH = h;
}

void CGraphics_Threaded::ClipDisable()
{
	m_State.m_ClipEnable = false;
}

void CGraphics_Threaded::BlendNone()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_NONE;
}

void CGraphics_Threaded::BlendNormal()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_ALPHA;
}

void CGraphics_Threaded::BlendAdditive()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_ADDITIVE;
}

void CGraphics_Threaded::BlendBuffer()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_BUFFER;
}

void CGraphics_Threaded::BlendLight()
{
	m_State.m_BlendMode = CCommandBuffer::BLEND_LIGHT;
}

void CGraphics_Threaded::WrapNormal()
{
	m_State.m_WrapMode = CCommandBuffer::WRAP_REPEAT;
}

void CGraphics_Threaded::WrapClamp()
{
	m_State.m_WrapMode = CCommandBuffer::WRAP_CLAMP;
}

int CGraphics_Threaded::MemoryUsage() const
{
	return m_pBackend->MemoryUsage();
}

void CGraphics_Threaded::MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY)
{
	m_State.m_ScreenTL.x = TopLeftX;
	m_State.m_ScreenTL.y = TopLeftY;
	m_State.m_ScreenBR.x = BottomRightX;
	m_State.m_ScreenBR.y = BottomRightY;
}

void CGraphics_Threaded::GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY)
{
	*pTopLeftX = m_State.m_ScreenTL.x;
	*pTopLeftY = m_State.m_ScreenTL.y;
	*pBottomRightX = m_State.m_ScreenBR.x;
	*pBottomRightY = m_State.m_ScreenBR.y;
}

void CGraphics_Threaded::LinesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->LinesBegin twice");
	m_Drawing = DRAWING_LINES;
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::LinesEnd()
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::LinesDraw(const CLineItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesDraw without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 2 * i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 2 * i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 2 * i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 2 * i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 2 * i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 2 * i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 2 * i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 2 * i + 1].m_Color = m_aColor[1];
	}

	AddVertices(2 * Num);
}

void CGraphics_Threaded::TrianglesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TrianglesBegin twice");
	m_Drawing = DRAWING_TRIANGLES;
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::TrianglesEnd()
{
	dbg_assert(m_Drawing == DRAWING_TRIANGLES, "called Graphics()->TrianglesEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::TrianglesDraw(const CTriangleItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_TRIANGLES, "called Graphics()->TrianglesDraw without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 3 * i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 3 * i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 3 * i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 3 * i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 3 * i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 3 * i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 3 * i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 3 * i + 1].m_Color = m_aColor[1];

		m_aVertices[m_NumVertices + 3 * i + 2].m_Pos.x = pArray[i].m_X2;
		m_aVertices[m_NumVertices + 3 * i + 2].m_Pos.y = pArray[i].m_Y2;
		m_aVertices[m_NumVertices + 3 * i + 2].m_Tex = m_aTexture[2];
		m_aVertices[m_NumVertices + 3 * i + 2].m_Color = m_aColor[2];
	}

	AddVertices(3 * Num);
}

int CGraphics_Threaded::UnloadTexture(int Index)
{
	if(Index == m_InvalidTexture)
		return 0;

	if(Index < 0)
		return 0;

	FreeTextureSource(Index);

	CCommandBuffer::SCommand_Texture_Destroy Cmd;
	Cmd.m_Slot = Index;
	m_pCommandBuffer->AddCommand(Cmd);

	m_aTextureIndices[Index] = m_FirstFreeTexture;
	m_FirstFreeTexture = Index;
	return 0;
}

static int ImageFormatToTexFormat(int Format)
{
	if(Format == CImageInfo::FORMAT_RGB)
		return CCommandBuffer::TEXFORMAT_RGB;
	if(Format == CImageInfo::FORMAT_RGBA)
		return CCommandBuffer::TEXFORMAT_RGBA;
	if(Format == CImageInfo::FORMAT_ALPHA)
		return CCommandBuffer::TEXFORMAT_ALPHA;
	return CCommandBuffer::TEXFORMAT_RGBA;
}

static int ImageFormatToPixelSize(int Format)
{
	switch(Format)
	{
		case CImageInfo::FORMAT_RGB:
			return 3;
		case CImageInfo::FORMAT_ALPHA:
			return 1;
		default:
			return 4;
	}
}

void CGraphics_Threaded::FreeTextureSource(int Index)
{
	if(Index < 0 || Index >= MAX_TEXTURES || !m_aTextureSources[Index].m_pData)
		return;

	mem_free(m_aTextureSources[Index].m_pData);
	mem_zero(&m_aTextureSources[Index], sizeof(m_aTextureSources[Index]));
}

int CGraphics_Threaded::TextureCommandFlags(int Flags) const
{
	int CommandFlags = 0;
	if(Flags & IGraphics::TEXLOAD_NOMIPMAPS)
		CommandFlags |= CCommandBuffer::TEXFLAG_NOMIPMAPS;
	if(Flags & IGraphics::TEXLOAD_NEAREST)
		CommandFlags |= CCommandBuffer::TEXFLAG_NEAREST;
	if(Flags & IGraphics::TEXLOAD_NOCOMPRESSION)
		CommandFlags |= CCommandBuffer::TEXFLAG_NOCOMPRESSION;
	if(g_Config.m_GfxTextureCompression && !(Flags & IGraphics::TEXLOAD_NOCOMPRESSION))
		CommandFlags |= CCommandBuffer::TEXFLAG_COMPRESSED;
	if(g_Config.m_GfxTextureQuality || Flags & IGraphics::TEXLOAD_NORESAMPLE)
		CommandFlags |= CCommandBuffer::TEXFLAG_QUALITY;
	return CommandFlags;
}

bool CGraphics_Threaded::QueueTextureCreate(int Slot, const STextureSource &Source)
{
	if(!m_pCommandBuffer || !Source.m_pData || Slot < 0 || Slot >= MAX_TEXTURES)
		return false;

	CCommandBuffer::SCommand_Texture_Create Cmd;
	Cmd.m_Slot = Slot;
	Cmd.m_Width = Source.m_Width;
	Cmd.m_Height = Source.m_Height;
	Cmd.m_PixelSize = ImageFormatToPixelSize(Source.m_Format);
	Cmd.m_Format = ImageFormatToTexFormat(Source.m_Format);
	Cmd.m_StoreFormat = ImageFormatToTexFormat(Source.m_StoreFormat);
	Cmd.m_Flags = TextureCommandFlags(Source.m_Flags);
	const int MemSize = Source.m_Width * Source.m_Height * Cmd.m_PixelSize;
	void *pTmpData = mem_alloc(MemSize, sizeof(void *));
	if(!pTmpData)
		return false;
	mem_copy(pTmpData, Source.m_pData, MemSize);
	Cmd.m_pData = pTmpData;

	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		KickCommandBuffer();
		if(!m_pCommandBuffer->AddCommand(Cmd))
		{
			mem_free(pTmpData);
			return false;
		}
	}
	return true;
}

int CGraphics_Threaded::LoadTextureRawSub(
	int TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
	if(TextureID >= 0 && TextureID < MAX_TEXTURES && pData && m_aTextureSources[TextureID].m_pData &&
		m_aTextureSources[TextureID].m_Format == Format && x >= 0 && y >= 0 && Width >= 0 && Height >= 0 &&
		x + Width <= m_aTextureSources[TextureID].m_Width && y + Height <= m_aTextureSources[TextureID].m_Height)
	{
		const int PixelSize = ImageFormatToPixelSize(Format);
		for(int Row = 0; Row < Height; Row++)
		{
			unsigned char *pDst = static_cast<unsigned char *>(m_aTextureSources[TextureID].m_pData) +
				((y + Row) * m_aTextureSources[TextureID].m_Width + x) * PixelSize;
			const unsigned char *pSrc = static_cast<const unsigned char *>(pData) + Row * Width * PixelSize;
			mem_copy(pDst, pSrc, Width * PixelSize);
		}
	}

	CCommandBuffer::SCommand_Texture_Update Cmd;
	Cmd.m_Slot = TextureID;
	Cmd.m_X = x;
	Cmd.m_Y = y;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	Cmd.m_Format = ImageFormatToTexFormat(Format);

	// calculate memory usage
	int MemSize = Width * Height * ImageFormatToPixelSize(Format);

	// copy texture data
	void *pTmpData = mem_alloc(MemSize, sizeof(void *));
	mem_copy(pTmpData, pData, MemSize);
	Cmd.m_pData = pTmpData;

	//
	m_pCommandBuffer->AddCommand(Cmd);
	return 0;
}

void CGraphics_Threaded::CreateTextureBuffer(int Width, int Height)
{
	CCommandBuffer::SCommand_CreateTextureBuffer Cmd;
	Cmd.m_Width = Width;
	Cmd.m_Height = Height;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::DestroyTextureBuffer()
{
	CCommandBuffer::SCommand_DestroyTextureBuffer Cmd;
	m_pCommandBuffer->AddCommand(Cmd);
}

bool CGraphics_Threaded::ReloadTextureSettings()
{
	if(!m_pCommandBuffer)
		return false;

	bool Success = true;
	int Reloaded = 0;
	for(int i = 0; i < MAX_TEXTURES; i++)
	{
		if(!m_aTextureSources[i].m_pData)
			continue;
		if(!QueueTextureCreate(i, m_aTextureSources[i]))
			Success = false;
		else
			Reloaded++;
	}

	if(Reloaded > 0)
	{
		KickCommandBuffer();
		WaitForIdle();
	}
	return Success;
}

void CGraphics_Threaded::LoadShaders()
{
	CCommandBuffer::SCommand_LoadShaders Cmd;
	Cmd.m_pAvailable = m_aShaderAvailable;
	m_pCommandBuffer->AddCommand(Cmd);
	// Shader availability is consumed by the game render path. Make the
	// result visible before texture buffers and the first frame are queued.
	KickCommandBuffer();
	WaitForIdle();
}

bool CGraphics_Threaded::IsShaderAvailable(int Shader) const
{
	return Shader >= 0 && Shader < NUM_SHADERS && m_aShaderAvailable[Shader] != 0;
}

void CGraphics_Threaded::ShaderBegin(int Shader, float Intensity, float ColorSwap, float WeaponCharge)
{
	CCommandBuffer::SCommand_ShaderBegin Cmd;
	Cmd.m_Shader = Shader;
	Cmd.m_Intensity = Intensity;
	Cmd.m_ColorSwap = ColorSwap;
	Cmd.m_WeaponCharge = WeaponCharge;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::LightShaderBegin(
	int CollisionTexture,
	float LightCenterX,
	float LightCenterY,
	float LightRadius,
	float CollisionWidth,
	float CollisionHeight,
	float ViewTLX,
	float ViewTLY,
	float ViewBRX,
	float ViewBRY,
	float TargetWidth,
	float TargetHeight)
{
	CCommandBuffer::SCommand_ShaderBegin Cmd;
	Cmd.m_Shader = SHADER_LIGHT;
	Cmd.m_Intensity = 1.0f;
	Cmd.m_ExtraTexture = CollisionTexture;
	Cmd.m_LightCenterX = LightCenterX;
	Cmd.m_LightCenterY = LightCenterY;
	Cmd.m_LightRadius = LightRadius;
	Cmd.m_CollisionWidth = CollisionWidth;
	Cmd.m_CollisionHeight = CollisionHeight;
	Cmd.m_ViewTLX = ViewTLX;
	Cmd.m_ViewTLY = ViewTLY;
	Cmd.m_ViewBRX = ViewBRX;
	Cmd.m_ViewBRY = ViewBRY;
	Cmd.m_TargetWidth = TargetWidth;
	Cmd.m_TargetHeight = TargetHeight;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::BallShaderBegin(float Speed, float Speed2)
{
	CCommandBuffer::SCommand_ShaderBegin Cmd;
	Cmd.m_Shader = SHADER_BALL;
	Cmd.m_Intensity = Speed;
	Cmd.m_WeaponCharge = Speed2;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::PlayerShaderBegin(
	float colorG,
	float colorB,
	float Charge,
	float Visibility,
	float Electro,
	float Damage,
	float Deathray,
	float WeaponTint)
{
	CCommandBuffer::SCommand_ShaderBegin Cmd;
	Cmd.m_Shader = SHADER_PLAYER;
	Cmd.m_Intensity = colorB;
	Cmd.m_ColorSwap = colorG;
	Cmd.m_Visibility = Visibility;
	Cmd.m_Electro = Electro;
	Cmd.m_Damage = Damage;
	Cmd.m_Deathray = Deathray;
	Cmd.m_WeaponCharge = Charge;
	Cmd.m_WeaponTint = WeaponTint;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::CameraToShaders(int ScreenWidth, int ScreenHeight, int CameraX, int CameraY)
{
	CCommandBuffer::SCommand_CameraToShaders Cmd;
	Cmd.m_ScreenWidth = ScreenWidth;
	Cmd.m_ScreenHeight = ScreenHeight;
	Cmd.m_CameraX = CameraX;
	Cmd.m_CameraY = CameraY;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::ShaderEnd()
{
	CCommandBuffer::SCommand_ShaderEnd Cmd;
	m_pCommandBuffer->AddCommand(Cmd);
}

int CGraphics_Threaded::LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags)
{
	// don't waste memory on texture if we are stress testing
	if(g_Config.m_DbgStress)
		return m_InvalidTexture;

	// grab texture
	int Tex = m_FirstFreeTexture;
	m_FirstFreeTexture = m_aTextureIndices[Tex];
	m_aTextureIndices[Tex] = -1;

	STextureSource &Source = m_aTextureSources[Tex];
	Source.m_Width = Width;
	Source.m_Height = Height;
	Source.m_Format = Format;
	Source.m_StoreFormat = StoreFormat;
	Source.m_Flags = Flags;
	const int MemSize = Width * Height * ImageFormatToPixelSize(Format);
	Source.m_pData = mem_alloc(MemSize, sizeof(void *));
	if(!Source.m_pData)
	{
		m_aTextureIndices[Tex] = m_FirstFreeTexture;
		m_FirstFreeTexture = Tex;
		return m_InvalidTexture;
	}
	mem_copy(Source.m_pData, pData, MemSize);
	if(!QueueTextureCreate(Tex, Source))
	{
		FreeTextureSource(Tex);
		m_aTextureIndices[Tex] = m_FirstFreeTexture;
		m_FirstFreeTexture = Tex;
		return m_InvalidTexture;
	}

	return Tex;
}

// simple uncompressed RGBA loaders
int CGraphics_Threaded::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	int ID;
	CImageInfo Img;

	if(l < 3)
		return -1;
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		LimitWorkshopPreviewSize(&Img, pFilename);
		if(StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags);
		mem_free(Img.m_pData);
		if(ID != m_InvalidTexture && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return ID;
	}
	if(LoadJPEG(&Img, pFilename, StorageType))
	{
		LimitWorkshopPreviewSize(&Img, pFilename);
		if(StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;
		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags);
		mem_free(Img.m_pData);
		return ID;
	}

	return m_InvalidTexture;
}

int CGraphics_Threaded::LoadJPEG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
#if defined(CONF_JPEG)
	struct CError
	{
		jpeg_error_mgr m_Base;
		jmp_buf m_Jump;
	};
	auto ErrorExit = [](j_common_ptr pInfo)
	{
		CError *pError = (CError *)pInfo->err;
		longjmp(pError->m_Jump, 1);
	};
	char aCompleteFilename[512];
	IOHANDLE Probe =
		m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(!Probe)
		return 0;
	io_close(Probe);
	FILE *pFile = fopen(aCompleteFilename, "rb");
	if(!pFile)
		return 0;
	jpeg_decompress_struct Info;
	CError Error;
	Info.err = jpeg_std_error(&Error.m_Base);
	Error.m_Base.error_exit = ErrorExit;
	volatile bool Created = false;
	if(setjmp(Error.m_Jump))
	{
		if(Created)
			jpeg_destroy_decompress(&Info);
		fclose(pFile);
		return 0;
	}
	jpeg_create_decompress(&Info);
	Created = true;
	jpeg_stdio_src(&Info, pFile);
	if(jpeg_read_header(&Info, TRUE) != JPEG_HEADER_OK || Info.image_width == 0 || Info.image_height == 0 ||
	   Info.image_width > 4096 || Info.image_height > 4096)
	{
		jpeg_destroy_decompress(&Info);
		fclose(pFile);
		return 0;
	}
	Info.out_color_space = JCS_RGB;
	jpeg_start_decompress(&Info);
	const size_t RowSize = (size_t)Info.output_width * 3;
	unsigned char *pData = (unsigned char *)mem_alloc(RowSize * Info.output_height, 1);
	if(!pData)
	{
		jpeg_destroy_decompress(&Info);
		fclose(pFile);
		return 0;
	}
	while(Info.output_scanline < Info.output_height)
	{
		JSAMPROW pRow = pData + RowSize * Info.output_scanline;
		jpeg_read_scanlines(&Info, &pRow, 1);
	}
	pImg->m_Width = Info.output_width;
	pImg->m_Height = Info.output_height;
	pImg->m_Format = CImageInfo::FORMAT_RGB;
	pImg->m_pData = pData;
	jpeg_finish_decompress(&Info);
	jpeg_destroy_decompress(&Info);
	fclose(pFile);
	return 1;
#else
	(void)pImg;
	(void)pFilename;
	(void)StorageType;
	return 0;
#endif
}

int CGraphics_Threaded::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0, 0); // ignore_convention

	IOHANDLE File =
		m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.width == 0 || Png.height == 0 || Png.width > 4096 || Png.height > 4096 || Png.depth != 8 ||
	   (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer);											   // ignore_convention
	png_close_file(&Png);													   // ignore_convention

	pImg->m_Width = Png.width;			// ignore_convention
	pImg->m_Height = Png.height;		// ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

void CGraphics_Threaded::KickCommandBuffer()
{
	m_pBackend->RunBuffer(m_pCommandBuffer);

	// swap buffer
	m_CurrentCommandBuffer ^= 1;
	m_pCommandBuffer = m_apCommandBuffers[m_CurrentCommandBuffer];
	m_pCommandBuffer->Reset();
}

bool CGraphics_Threaded::CaptureFrame(CImageInfo *pImage)
{
	if(!pImage)
		return false;

	mem_zero(pImage, sizeof(*pImage));

	CCommandBuffer::SCommand_Screenshot Cmd;
	Cmd.m_pImage = pImage;
	m_pCommandBuffer->AddCommand(Cmd);

	KickCommandBuffer();
	WaitForIdle();

	return pImage->m_pData != 0;
}

bool CGraphics_Threaded::ScreenshotDirect(const char *pFilename, CScreenshotResult *pResult)
{
	CImageInfo Image;
	if(!CaptureFrame(&Image))
		return false;

	// find filename
	char aWholePath[1024];
	png_t Png; // ignore_convention

	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE, aWholePath, sizeof(aWholePath));
	if(File)
		io_close(File);

	// save png
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "saved screenshot to '%s'", aWholePath);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
	const int OpenResult = png_open_file_write(&Png, aWholePath); // ignore_convention
	if(OpenResult == PNG_NO_ERROR)
	{
		png_set_data(
			&Png, Image.m_Width, Image.m_Height, 8, PNG_TRUECOLOR, (unsigned char *)Image.m_pData); // ignore_convention
		png_close_file(&Png);																		// ignore_convention
	}
	if(pResult)
	{
		pResult->m_Success = OpenResult == PNG_NO_ERROR;
		pResult->m_Width = Image.m_Width;
		pResult->m_Height = Image.m_Height;
		str_copy(pResult->m_aAbsolutePath, aWholePath, sizeof(pResult->m_aAbsolutePath));
	}

	mem_free(Image.m_pData);
	return OpenResult == PNG_NO_ERROR;
}

void CGraphics_Threaded::TextureSet(int TextureID, int BufferTexture)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");
	m_State.m_Texture = TextureID;

	if(BufferTexture >= 0)
	{
		m_State.m_BufferTexture = BufferTexture;
	}
}

void CGraphics_Threaded::RenderToScreen()
{
	if(!g_Config.m_GfxMultiBuffering)
		return;

	m_State.m_RenderTarget = CCommandBuffer::RENDERTARGET_SCREEN;
	// m_State.m_Texture = -1;
}

void CGraphics_Threaded::RenderToTexture(int RenderBuffer)
{
	if(!g_Config.m_GfxMultiBuffering)
		return;

	m_State.m_RenderTarget = CCommandBuffer::RENDERTARGET_TEXTURE;
	m_State.m_RenderBuffer = RenderBuffer;
}

void CGraphics_Threaded::Clear(float r, float g, float b)
{
	CCommandBuffer::SCommand_Clear Cmd;
	Cmd.m_Color.r = r;
	Cmd.m_Color.g = g;
	Cmd.m_Color.b = b;
	Cmd.m_Color.a = 0;
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::ClearBufferTexture(float LightingBrightness)
{
	CCommandBuffer::SCommand_ClearBufferTexture Cmd;
	Cmd.m_LightingBrightness = clamp(LightingBrightness, 0.0f, 1.0f);
	m_pCommandBuffer->AddCommand(Cmd);
}

void CGraphics_Threaded::QuadsBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->QuadsBegin twice");
	m_Drawing = DRAWING_QUADS;

	QuadsSetSubset(0, 0, 1, 1);
	QuadsSetRotation(0);
	SetColor(1, 1, 1, 1);
}

void CGraphics_Threaded::QuadsEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	FlushVertices();
	m_Drawing = 0;
}

void CGraphics_Threaded::QuadsSetRotation(float Angle)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetRotation without begin");
	m_Rotation = Angle;
}

void CGraphics_Threaded::SetColorVertex(const CColorVertex *pArray, int Num)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColorVertex without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aColor[pArray[i].m_Index].r = pArray[i].m_R;
		m_aColor[pArray[i].m_Index].g = pArray[i].m_G;
		m_aColor[pArray[i].m_Index].b = pArray[i].m_B;
		m_aColor[pArray[i].m_Index].a = pArray[i].m_A;
	}
}

void CGraphics_Threaded::SetColor(float r, float g, float b, float a)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColor without begin");
	CColorVertex Array[4] = {CColorVertex(0, r, g, b, a),
							 CColorVertex(1, r, g, b, a),
							 CColorVertex(2, r, g, b, a),
							 CColorVertex(3, r, g, b, a)};
	SetColorVertex(Array, 4);
}

void CGraphics_Threaded::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV, bool FreeForm)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetSubset without begin");

	m_aTexture[0].u = TlU;
	m_aTexture[1].u = BrU;
	m_aTexture[0].v = TlV;
	m_aTexture[1].v = TlV;

	if(FreeForm)
	{
		m_aTexture[2].u = TlU;
		m_aTexture[3].u = BrU;
		m_aTexture[2].v = BrV;
		m_aTexture[3].v = BrV;
	}
	else
	{
		m_aTexture[3].u = TlU;
		m_aTexture[2].u = BrU;
		m_aTexture[3].v = BrV;
		m_aTexture[2].v = BrV;
	}
}

void CGraphics_Threaded::QuadsSetSubsetFree(
	float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
	m_aTexture[0].u = x0;
	m_aTexture[0].v = y0;
	m_aTexture[1].u = x1;
	m_aTexture[1].v = y1;
	m_aTexture[2].u = x2;
	m_aTexture[2].v = y2;
	m_aTexture[3].u = x3;
	m_aTexture[3].v = y3;
}

void CGraphics_Threaded::QuadsDraw(CQuadItem *pArray, int Num)
{
	for(int i = 0; i < Num; ++i)
	{
		pArray[i].m_X -= pArray[i].m_Width / 2;
		pArray[i].m_Y -= pArray[i].m_Height / 2;
	}

	QuadsDrawTL(pArray, Num);
}

void CGraphics_Threaded::QuadsDrawTL(const CQuadItem *pArray, int Num)
{
	CCommandBuffer::SPoint Center;
	Center.z = 0;

	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawTL without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 4 * i].m_Pos.x = pArray[i].m_X;
		m_aVertices[m_NumVertices + 4 * i].m_Pos.y = pArray[i].m_Y;
		m_aVertices[m_NumVertices + 4 * i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 4 * i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
		m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.y = pArray[i].m_Y;
		m_aVertices[m_NumVertices + 4 * i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 4 * i + 1].m_Color = m_aColor[1];

		m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
		m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
		m_aVertices[m_NumVertices + 4 * i + 2].m_Tex = m_aTexture[2];
		m_aVertices[m_NumVertices + 4 * i + 2].m_Color = m_aColor[2];

		m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.x = pArray[i].m_X;
		m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
		m_aVertices[m_NumVertices + 4 * i + 3].m_Tex = m_aTexture[3];
		m_aVertices[m_NumVertices + 4 * i + 3].m_Color = m_aColor[3];

		if(m_Rotation != 0)
		{
			Center.x = pArray[i].m_X + pArray[i].m_Width / 2;
			Center.y = pArray[i].m_Y + pArray[i].m_Height / 2;

			Rotate4(Center, &m_aVertices[m_NumVertices + 4 * i]);
		}
	}

	AddVertices(4 * Num);
}

void CGraphics_Threaded::QuadsDrawFreeform(const CFreeformItem *pArray, int Num)
{

	// CCommandBuffer::SPoint Center;
	// Center.z = 0;

	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawFreeform without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 4 * i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 4 * i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 4 * i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 4 * i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 4 * i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 4 * i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 4 * i + 1].m_Color = m_aColor[1];

		m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.x = pArray[i].m_X3;
		m_aVertices[m_NumVertices + 4 * i + 2].m_Pos.y = pArray[i].m_Y3;
		m_aVertices[m_NumVertices + 4 * i + 2].m_Tex = m_aTexture[3];
		m_aVertices[m_NumVertices + 4 * i + 2].m_Color = m_aColor[3];

		m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.x = pArray[i].m_X2;
		m_aVertices[m_NumVertices + 4 * i + 3].m_Pos.y = pArray[i].m_Y2;
		m_aVertices[m_NumVertices + 4 * i + 3].m_Tex = m_aTexture[2];
		m_aVertices[m_NumVertices + 4 * i + 3].m_Color = m_aColor[2];

		/*
		if(m_Rotation != 0)
		{
			Center.x = (pArray[i].m_X0+pArray[i].m_X1+pArray[i].m_X2+pArray[i].m_X3)/4.0f;
			Center.y = (pArray[i].m_Y0+pArray[i].m_Y1+pArray[i].m_Y2+pArray[i].m_Y3)/4.0f;

			Rotate4(Center, &m_aVertices[m_NumVertices + 4*i]);
		}
		*/
	}

	AddVertices(4 * Num);
}

void CGraphics_Threaded::QuadsText(float x, float y, float Size, const char *pText)
{
	float StartX = x;

	while(*pText)
	{
		char c = *pText;
		pText++;

		if(c == '\n')
		{
			x = StartX;
			y += Size;
		}
		else
		{
			QuadsSetSubset(
				(c % 16) / 16.0f, (c / 16) / 16.0f, (c % 16) / 16.0f + 1.0f / 16.0f, (c / 16) / 16.0f + 1.0f / 16.0f);

			CQuadItem QuadItem(x, y, Size, Size);
			QuadsDrawTL(&QuadItem, 1);
			x += Size / 2;
		}
	}
}

int CGraphics_Threaded::IssueInit()
{
	int Flags = 0;
	if(g_Config.m_GfxBorderless && g_Config.m_GfxFullscreen)
	{
		dbg_msg("gfx", "both borderless and fullscreen activated, disabling borderless");
		g_Config.m_GfxBorderless = 0;
	}

	if(g_Config.m_GfxBorderless)
		Flags |= IGraphicsBackend::INITFLAG_BORDERLESS;
	else if(g_Config.m_GfxFullscreen)
		Flags |= IGraphicsBackend::INITFLAG_FULLSCREEN;
	if(g_Config.m_GfxVsync)
		Flags |= IGraphicsBackend::INITFLAG_VSYNC;
	if(g_Config.m_DbgResizable)
		Flags |= IGraphicsBackend::INITFLAG_RESIZABLE;

	return m_pBackend->Init("Ninslash",
							&g_Config.m_GfxScreenWidth,
							&g_Config.m_GfxScreenHeight,
							&g_Config.m_GfxScreen,
							g_Config.m_GfxFsaaSamples,
							Flags,
							&m_DesktopScreenWidth,
							&m_DesktopScreenHeight);
}

int CGraphics_Threaded::InitWindow()
{
	bool ForceSafeMode = false;
#if defined(CONF_FAMILY_WINDOWS)
	ForceSafeMode = windows_startup_recovery_requested() != 0;
#endif
	if(ForceSafeMode)
	{
		dbg_msg("gfx", "startup recovery requested; skipping saved graphics configuration");
		g_Config.m_GfxFsaaSamples = 0;
		g_Config.m_GfxScreen = 0;
		g_Config.m_GfxFullscreen = 0;
		g_Config.m_GfxBorderless = 0;
		g_Config.m_GfxScreenWidth = 1280;
		g_Config.m_GfxScreenHeight = 720;
		if(IssueInit() == 0)
		{
			dbg_msg("gfx", "safe graphics mode initialized successfully");
			return 0;
		}
		dbg_msg("gfx", "safe graphics mode failed; see the preceding backend error");
		return -1;
	}

	dbg_msg("gfx",
			"trying saved graphics configuration: screen=%d size=%dx%d fullscreen=%d borderless=%d fsaa=%d",
			g_Config.m_GfxScreen,
			g_Config.m_GfxScreenWidth,
			g_Config.m_GfxScreenHeight,
			g_Config.m_GfxFullscreen,
			g_Config.m_GfxBorderless,
			g_Config.m_GfxFsaaSamples);
	if(IssueInit() == 0)
		return 0;

	// Retry the same configuration once without multisampling.
	if(g_Config.m_GfxFsaaSamples)
	{
		g_Config.m_GfxFsaaSamples = 0;
		dbg_msg("gfx", "saved graphics configuration failed; retrying with FSAA disabled");
		if(IssueInit() == 0)
			return 0;
	}

	// Last resort: a conservative window on the primary display.
	dbg_msg("gfx", "retrying in safe graphics mode: primary display, 1280x720 window, no FSAA");
	g_Config.m_GfxFsaaSamples = 0;
	g_Config.m_GfxScreen = 0;
	g_Config.m_GfxFullscreen = 0;
	g_Config.m_GfxBorderless = 0;
	g_Config.m_GfxScreenWidth = 1280;
	g_Config.m_GfxScreenHeight = 720;
	if(IssueInit() == 0)
	{
		dbg_msg("gfx", "safe graphics mode initialized successfully; recovered settings will be saved");
		return 0;
	}

	dbg_msg("gfx", "all graphics initialization attempts failed; see the preceding backend error");

	return -1;
}

int CGraphics_Threaded::Init()
{
	// fetch pointers
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	// Set all z to -5.0f
	for(int i = 0; i < MAX_VERTICES; i++)
		m_aVertices[i].m_Pos.z = -5.0f;

	// init textures
	m_FirstFreeTexture = 0;
	for(int i = 0; i < MAX_TEXTURES - 1; i++)
		m_aTextureIndices[i] = i + 1;
	m_aTextureIndices[MAX_TEXTURES - 1] = -1;

	m_pBackend = CreateGraphicsBackend();
	if(InitWindow() != 0)
		return -1;

	// Rendering, clipping and input must use the drawable pixel size. This can
	// differ from the requested window size on high-DPI and offscreen backends.
	m_pBackend->GetViewportSize(&m_ScreenWidth, &m_ScreenHeight);
	if(m_ScreenWidth <= 0 || m_ScreenHeight <= 0)
	{
		m_ScreenWidth = g_Config.m_GfxScreenWidth;
		m_ScreenHeight = g_Config.m_GfxScreenHeight;
	}

	// create command buffers
	for(int i = 0; i < NUM_CMDBUFFERS; i++)
		m_apCommandBuffers[i] = new CCommandBuffer(128 * 1024, 2 * 1024 * 1024);
	m_pCommandBuffer = m_apCommandBuffers[0];

	// create null texture, will get id=0
	static const unsigned char aNullTextureData[] = {
		0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
		0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff,
		0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff,
	};

	m_InvalidTexture =
		LoadTextureRaw(4, 4, CImageInfo::FORMAT_RGBA, aNullTextureData, CImageInfo::FORMAT_RGBA, TEXLOAD_NORESAMPLE);

	/*
	if (InitShaders())
	{
		g_Config.m_Shaders = 1;
		LoadShaders();
	}
	else
	{
		g_Config.m_Shaders = 0;
		dbg_msg("gfx", "initializing shaders failed");
	}
	*/

	return 0;
}

void CGraphics_Threaded::Shutdown()
{
	// shutdown the backend
	m_pBackend->Shutdown();
	delete m_pBackend;
	m_pBackend = 0x0;
	for(int i = 0; i < MAX_TEXTURES; i++)
		FreeTextureSource(i);

	// delete the command buffers
	for(int i = 0; i < NUM_CMDBUFFERS; i++)
		delete m_apCommandBuffers[i];
}

bool CGraphics_Threaded::ApplyWindowSettings(int Width, int Height, int Screen, bool Fullscreen, bool Borderless)
{
	if(!m_pBackend)
		return false;

	KickCommandBuffer();
	WaitForIdle();
	if(!m_pBackend->ApplyWindowSettings(Width, Height, Screen, Fullscreen, Borderless))
		return false;

	m_pBackend->GetViewportSize(&m_ScreenWidth, &m_ScreenHeight);
	if(m_ScreenWidth <= 0 || m_ScreenHeight <= 0)
	{
		m_ScreenWidth = Width;
		m_ScreenHeight = Height;
	}

	CCommandBuffer::SCommand_SetViewport Cmd;
	Cmd.m_Width = m_ScreenWidth;
	Cmd.m_Height = m_ScreenHeight;
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		KickCommandBuffer();
		if(!m_pCommandBuffer->AddCommand(Cmd))
			return false;
	}
	KickCommandBuffer();
	WaitForIdle();
	return true;
}

bool CGraphics_Threaded::ApplyVSync(bool Enabled)
{
	if(!m_pCommandBuffer)
		return false;

	bool Success = false;
	CCommandBuffer::SCommand_SetVSync Cmd;
	Cmd.m_Enabled = Enabled;
	Cmd.m_pSuccess = &Success;
	if(!m_pCommandBuffer->AddCommand(Cmd))
	{
		KickCommandBuffer();
		if(!m_pCommandBuffer->AddCommand(Cmd))
			return false;
	}
	KickCommandBuffer();
	WaitForIdle();
	return Success;
}

void CGraphics_Threaded::Minimize()
{
	m_pBackend->Minimize();
}

void CGraphics_Threaded::Maximize()
{
	// TODO: SDL
	m_pBackend->Maximize();
}

void CGraphics_Threaded::GrabWindow(bool grab)
{
	m_pBackend->GrabWindow(grab);
}

void CGraphics_Threaded::WarpMouse(int x, int y)
{
	m_pBackend->WarpMouse(x, y);
}

int CGraphics_Threaded::WindowActive()
{
	return m_pBackend->WindowActive();
}

int CGraphics_Threaded::WindowOpen()
{
	return m_pBackend->WindowOpen();
}

unsigned CGraphics_Threaded::TakeScreenshot(const char *pFilename)
{
	if(m_ScreenshotRequestCount >= MAX_SCREENSHOT_QUEUE)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "Screenshot queue is full");
		return 0;
	}
	char aDate[20];
	str_timestamp(aDate, sizeof(aDate));
	unsigned RequestID = ++m_NextScreenshotRequestID;
	if(!RequestID)
		RequestID = ++m_NextScreenshotRequestID;
	CScreenshotRequest &Request = m_aScreenshotRequests[m_ScreenshotRequestCount++];
	Request.m_RequestID = RequestID;
	str_format(Request.m_aName,
			   sizeof(Request.m_aName),
			   "screenshots/%s_%s_%u.png",
			   pFilename ? pFilename : "screenshot",
			   aDate,
			   RequestID);
	return RequestID;
}

bool CGraphics_Threaded::ConsumeScreenshotResult(CScreenshotResult *pResult, unsigned RequestID)
{
	if(!pResult || m_ScreenshotResultCount <= 0)
		return false;
	int ResultIndex = 0;
	if(RequestID)
	{
		for(; ResultIndex < m_ScreenshotResultCount; ResultIndex++)
			if(m_aScreenshotResults[ResultIndex].m_RequestID == RequestID)
				break;
		if(ResultIndex == m_ScreenshotResultCount)
			return false;
	}
	*pResult = m_aScreenshotResults[ResultIndex];
	for(int i = ResultIndex + 1; i < m_ScreenshotResultCount; i++)
		m_aScreenshotResults[i - 1] = m_aScreenshotResults[i];
	m_ScreenshotResultCount--;
	return true;
}

void CGraphics_Threaded::Swap()
{
	if(m_ScreenshotRequestCount > 0)
	{
		const CScreenshotRequest Request = m_aScreenshotRequests[0];
		for(int i = 1; i < m_ScreenshotRequestCount; i++)
			m_aScreenshotRequests[i - 1] = m_aScreenshotRequests[i];
		m_ScreenshotRequestCount--;
		if(m_ScreenshotResultCount >= MAX_SCREENSHOT_QUEUE)
		{
			for(int i = 1; i < m_ScreenshotResultCount; i++)
				m_aScreenshotResults[i - 1] = m_aScreenshotResults[i];
			m_ScreenshotResultCount--;
		}
		CScreenshotResult &Result = m_aScreenshotResults[m_ScreenshotResultCount++];
		mem_zero(&Result, sizeof(Result));
		Result.m_RequestID = Request.m_RequestID;
		if(WindowActive())
			ScreenshotDirect(Request.m_aName, &Result);
	}

	// add swap command
	CCommandBuffer::SCommand_Swap Cmd;
	Cmd.m_Finish = g_Config.m_GfxFinish;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the command buffer
	KickCommandBuffer();
}

// syncronization
void CGraphics_Threaded::InsertSignal(semaphore *pSemaphore)
{
	CCommandBuffer::SCommand_Signal Cmd;
	Cmd.m_pSemaphore = pSemaphore;
	m_pCommandBuffer->AddCommand(Cmd);
}

bool CGraphics_Threaded::IsIdle()
{
	return m_pBackend->IsIdle();
}

void CGraphics_Threaded::WaitForIdle()
{
	m_pBackend->WaitForIdle();
}

void *CGraphics_Threaded::GetWindowHandle()
{
	return m_pBackend->GetWindowHandle();
}

int CGraphics_Threaded::GetVideoModes(CVideoMode *pModes, int MaxModes, int Screen)
{
	if(g_Config.m_GfxDisplayAllModes)
	{
		int Count = sizeof(g_aFakeModes) / sizeof(CVideoMode);
		mem_copy(pModes, g_aFakeModes, sizeof(g_aFakeModes));
		if(MaxModes < Count)
			Count = MaxModes;
		return Count;
	}

	int NumModes = 0;
	CCommandBuffer::SCommand_VideoModes Cmd;
	Cmd.m_pModes = pModes;
	Cmd.m_MaxModes = MaxModes;
	Cmd.m_pNumModes = &NumModes;
	Cmd.m_Screen = Screen;
	m_pCommandBuffer->AddCommand(Cmd);

	// kick the buffer and wait for the result and return it
	KickCommandBuffer();
	WaitForIdle();

	// Some drivers (and secondary displays) only report the native mode.
	// Merge common windowed sizes that fit the desktop so the list stays usable.
	const int DeskW = DesktopWidth();
	const int DeskH = DesktopHeight();
	const int FakeCount = (int)(sizeof(g_aFakeModes) / sizeof(g_aFakeModes[0]));
	for(int i = 0; i < FakeCount && NumModes < MaxModes; i++)
	{
		if(DeskW > 0 && DeskH > 0 && (g_aFakeModes[i].m_Width > DeskW || g_aFakeModes[i].m_Height > DeskH))
			continue;

		bool Found = false;
		for(int j = 0; j < NumModes; j++)
		{
			if(pModes[j].m_Width == g_aFakeModes[i].m_Width && pModes[j].m_Height == g_aFakeModes[i].m_Height)
			{
				Found = true;
				break;
			}
		}
		if(Found)
			continue;

		pModes[NumModes] = g_aFakeModes[i];
		NumModes++;
	}

	return NumModes;
}

int CGraphics_Threaded::GetNumScreens()
{
	return m_pBackend->GetNumScreens();
}

extern IEngineGraphics *CreateEngineGraphicsThreaded()
{
	return new CGraphics_Threaded();
}
