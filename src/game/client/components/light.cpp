#include <base/math.h>
#include <engine/graphics.h>
#include <engine/demo.h>
#include <engine/shared/config.h>

#include <game/client/customstuff.h>
#include <game/client/components/camera.h>
#include <game/client/components/particles.h>
#include <generated/game_data.h>
#include <game/client/render.h>
#include <game/gamecore.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "light.h"

namespace
{
int MedianShadowDistance(int A, int B, int C)
{
	return A + B + C - min(A, min(B, C)) - max(A, max(B, C));
}
}

CLight::CLight()
{
	OnReset();
	m_RenderLight.m_pParts = this;
}

void CLight::OnMapLoad()
{
	if(m_CollisionTexture >= 0)
	{
		Graphics()->UnloadTexture(m_CollisionTexture);
		m_CollisionTexture = -1;
	}
	if(m_ShadowAtlasTexture >= 0)
	{
		Graphics()->UnloadTexture(m_ShadowAtlasTexture);
		m_ShadowAtlasTexture = -1;
	}
	std::fill(m_aShadowAtlas.begin(), m_aShadowAtlas.end(), 0xff);
	mem_zero(m_aPolarShadowCache, sizeof(m_aPolarShadowCache));
	for(auto &Cache : m_aPolarShadowCache)
		Cache.m_SourceIndex = -1;

	// Build the GPU collision texture (1 byte per tile, COLFLAG incl. ramps)
	// for the shader lighting pass; the client collision already stores the
	// flags in CTile::m_Index.
	CCollision *pCollision = m_pClient->Collision();
	const int Width = pCollision ? pCollision->GetWidth() : 0;
	const int Height = pCollision ? pCollision->GetHeight() : 0;
	m_CollisionWidth = (float)Width;
	m_CollisionHeight = (float)Height;
	if(!pCollision || Width <= 0 || Height <= 0)
	{
		return;
	}
	static std::vector<unsigned char> s_aFlags;
	s_aFlags.resize(Width * Height);
	const CTile *pTiles = pCollision->GetTiles();
	for(int i = 0; i < Width * Height; i++)
		s_aFlags[i] = pTiles[i].m_Index;
	m_CollisionTexture = Graphics()->LoadTextureRaw(
		Width, Height, CImageInfo::FORMAT_ALPHA, s_aFlags.data(), CImageInfo::FORMAT_ALPHA,
		IGraphics::TEXLOAD_NOMIPMAPS | IGraphics::TEXLOAD_NORESAMPLE | IGraphics::TEXLOAD_NEAREST |
			IGraphics::TEXLOAD_NOCOMPRESSION);
	EnsurePolarShadowAtlas();
}

void CLight::OnReset()
{
	// The collision texture is owned by OnMapLoad and must survive this reset
	// (OnConnected calls OnMapLoad then OnReset); do not invalidate it here.
	/*
	for(int i = 0; i < MAX_LIGHTSOURCES; i++)
	{
		m_aLightsource[i].m_PrevPart = i-1;
		m_aLightsource[i].m_NextPart = i+1;
	}

	m_aLightsource[0].m_PrevPart = 0;
	m_aLightsource[MAX_LIGHTSOURCES-1].m_NextPart = -1;
	m_FirstFree = 0;

	for(int i = 0; i < NUM_GROUPS; i++)
		m_aFirstPart[i] = -1;
	*/

	m_LightCount = 0;
	m_ForceLights = false;
	m_ShadowFrame = 0;
	m_ShadowUpdateBudget = 0;
	std::fill(m_aShadowAtlas.begin(), m_aShadowAtlas.end(), 0xff);
	if(m_ShadowAtlasTexture >= 0)
		Graphics()->LoadTextureRawSub(m_ShadowAtlasTexture,
			0,
			0,
			POLAR_SHADOW_SAMPLES,
			POLAR_SHADOW_ROWS,
			CImageInfo::FORMAT_RGBA,
			m_aShadowAtlas.data());
	for(auto &Cache : m_aPolarShadowCache)
	{
		Cache.m_SourceIndex = -1;
		Cache.m_Pos = vec2(0.0f, 0.0f);
		Cache.m_Radius = 0.0f;
		Cache.m_LastUpdateFrame = -1;
		Cache.m_Valid = false;
	}
}

void CLight::AddSimpleLight(vec2 Pos, vec4 Color, vec2 Size, bool CastShadow, bool Force)
{
	// cl_lighting only controls optional local light sources. DarkVision is a
	// server-authoritative render rule and must keep its camera/light pool even
	// when a player disables ordinary dynamic lighting.
	if(!Force && !g_Config.m_ClLighting && !m_pClient->DarkVisionEnabled())
		return;

	if(m_LightCount >= MAX_LIGHTSOURCES)
		return;
	if(Force)
		m_ForceLights = true;

	CastShadow = CastShadow && (Size.x > 32.0f || Size.y > 32.0f);
	const int Image = (Size.x <= 32.0f && Size.y <= 32.0f) ? IMAGE_SMALLLIGHT : IMAGE_LIGHTS;
	m_aLights[m_LightCount++].Set(Pos, Color, Size, 0.0f, Image, CastShadow, CastShadow ? 1 : 0);
}

void CLight::AddBoxLight(vec2 Pos, vec4 Color, vec2 Size, float Rot, bool CastShadow)
{
	if(!g_Config.m_ClLighting && !m_pClient->DarkVisionEnabled())
		return;

	if(m_LightCount >= MAX_LIGHTSOURCES)
		return;

	m_aLights[m_LightCount++].Set(Pos, Color, Size, Rot, IMAGE_BOXLIGHT, CastShadow, CastShadow ? 2 : 0);
}

void CLight::Update(float TimePassed)
{
}

void CLight::OnRender()
{
	// no updates
}

void CLight::RenderLight(vec2 Pos, vec2 Size, vec4 Color)
{
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	IGraphics::CQuadItem QuadItem(Pos.x, Pos.y, Size.x, Size.y);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CLight::RenderLight(ivec2 Pos)
{
	vec4 c1 = vec4(1.0f, 0.75f, 0.5f, 0.9f);
	vec4 c2 = vec4(1.0f, 0.75f, 0.5f, 0.0f);

	IGraphics::CColorVertex aColors[4] = {IGraphics::CColorVertex(0, c1.r, c1.g, c1.b, c1.a),
										  IGraphics::CColorVertex(1, c1.r, c1.g, c1.b, c1.a),
										  IGraphics::CColorVertex(2, c2.r, c2.g, c2.b, c2.a),
										  IGraphics::CColorVertex(3, c2.r, c2.g, c2.b, c2.a)};

	Graphics()->SetColorVertex(aColors, 4);

	vec2 From1 = vec2(Pos.x - 16, Pos.y);
	vec2 From2 = vec2(Pos.x + 16, Pos.y);
	vec2 To1 = vec2(Pos.x - 300, Pos.y + 600);
	vec2 To2 = vec2(Pos.x + 300, Pos.y + 600);

	if(Collision()->IntersectLine(From1, To1, 0x0, &To1))
		To1 -= normalize(From1 - To1) * 240.0f;
	if(Collision()->IntersectLine(From2, To2, 0x0, &To2))
		To2 -= normalize(From2 - To2) * 240.0f;

	IGraphics::CFreeformItem FreeFormItem(From1.x, From1.y, From2.x, From2.y, To1.x, To1.y, To2.x, To2.y);

	Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
}

void CLight::RenderLight(vec2 Pos1, vec2 Pos2, vec2 Pos3, vec2 Pos4, vec4 Color)
{
	IGraphics::CColorVertex aColors[4] = {IGraphics::CColorVertex(0, Color.r, Color.g, Color.b, Color.a),
										  IGraphics::CColorVertex(1, Color.r, Color.g, Color.b, Color.a),
										  IGraphics::CColorVertex(2, Color.r, Color.g, Color.b, 0),
										  IGraphics::CColorVertex(3, Color.r, Color.g, Color.b, 0)};

	Graphics()->SetColorVertex(aColors, 4);

	IGraphics::CFreeformItem FreeFormItem(Pos1.x, Pos1.y, Pos2.x, Pos2.y, Pos3.x, Pos3.y, Pos4.x, Pos4.y);

	Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
}

float CLight::VisibleDistance(vec2 Center, vec2 End) const
{
	const vec2 Direction = normalize(End - Center);
	vec2 Hit = End;
	vec2 BeforeHit = End;
	int HitFlag = Collision()->IntersectLine(Center, End, &Hit, &BeforeHit, false, false, true);
	bool Blocked = HitFlag != 0;
	if(Blocked && (HitFlag == CCollision::COLFLAG_RAMP_LEFT ||
			HitFlag == CCollision::COLFLAG_RAMP_RIGHT ||
			HitFlag == CCollision::COLFLAG_ROOFSLOPE_LEFT ||
			HitFlag == CCollision::COLFLAG_ROOFSLOPE_RIGHT) &&
		distance(Center, Hit) < 20.0f && Collision()->CheckPoint(Center))
	{
		const vec2 BiasedOrigin = Center + Direction * 20.0f;
		Hit = End;
		BeforeHit = End;
		HitFlag = Collision()->IntersectLine(BiasedOrigin, End, &Hit, &BeforeHit, false, false, true);
		Blocked = HitFlag != 0;
	}
	return max(distance(Center, Blocked ? BeforeHit : End), 2.0f);
}

void CLight::EnsurePolarShadowAtlas()
{
	if(m_ShadowAtlasTexture >= 0)
		return;
	m_ShadowAtlasTexture = Graphics()->LoadTextureRaw(POLAR_SHADOW_SAMPLES,
		POLAR_SHADOW_ROWS,
		CImageInfo::FORMAT_RGBA,
		m_aShadowAtlas.data(),
		CImageInfo::FORMAT_RGBA,
		IGraphics::TEXLOAD_NOMIPMAPS | IGraphics::TEXLOAD_NORESAMPLE | IGraphics::TEXLOAD_NEAREST |
			IGraphics::TEXLOAD_NOCOMPRESSION);
}

void CLight::UpdatePolarShadow(int Row, int SourceIndex, const SLightSource &Source, float Radius)
{
	if(Row < 0 || Row >= POLAR_SHADOW_ROWS || Radius <= 32.0f || !Collision()->GetTiles())
		return;
	EnsurePolarShadowAtlas();
	if(m_ShadowAtlasTexture < 0)
		return;
	unsigned char *pRow = m_aShadowAtlas.data() + Row * POLAR_SHADOW_SAMPLES * 4;
	const float TwoPi = 6.28318530717958647692f;
	std::array<int, POLAR_SHADOW_SAMPLES> aRawDistances;
	for(int Sample = 0; Sample < POLAR_SHADOW_SAMPLES; Sample++)
	{
		const float Angle = TwoPi * (float)Sample / (float)POLAR_SHADOW_SAMPLES;
		const vec2 Direction(std::cos(Angle), std::sin(Angle));
		const float Visible = clamp(VisibleDistance(Source.m_Pos, Source.m_Pos + Direction * Radius) / Radius, 0.0f, 1.0f);
		aRawDistances[Sample] = clamp((int)(Visible * 65535.0f + 0.5f), 0, 65535);
	}
	for(int Sample = 0; Sample < POLAR_SHADOW_SAMPLES; Sample++)
	{
		const int Previous = (Sample + POLAR_SHADOW_SAMPLES - 1) % POLAR_SHADOW_SAMPLES;
		const int Next = (Sample + 1) % POLAR_SHADOW_SAMPLES;
		const int Encoded = MedianShadowDistance(aRawDistances[Previous], aRawDistances[Sample], aRawDistances[Next]);
		pRow[Sample * 4] = (unsigned char)(Encoded & 0xff);
		pRow[Sample * 4 + 1] = (unsigned char)((Encoded >> 8) & 0xff);
		pRow[Sample * 4 + 2] = 0xff;
		pRow[Sample * 4 + 3] = 0xff;
	}
	Graphics()->LoadTextureRawSub(m_ShadowAtlasTexture,
		0,
		Row,
		POLAR_SHADOW_SAMPLES,
		1,
		CImageInfo::FORMAT_RGBA,
		pRow);
	SPolarShadowCache &Cache = m_aPolarShadowCache[Row];
	Cache.m_SourceIndex = SourceIndex;
	Cache.m_Pos = Source.m_Pos;
	Cache.m_Radius = Radius;
	Cache.m_LastUpdateFrame = m_ShadowFrame;
	Cache.m_Valid = true;
}

int CLight::AcquirePolarShadowRow(int SourceIndex)
{
	for(int Row = 1; Row < POLAR_SHADOW_ROWS; Row++)
		if(m_aPolarShadowCache[Row].m_SourceIndex == SourceIndex)
			return Row;
	int Candidate = 1;
	for(int Row = 1; Row < POLAR_SHADOW_ROWS; Row++)
	{
		if(!m_aPolarShadowCache[Row].m_Valid)
			return Row;
		if(m_aPolarShadowCache[Row].m_LastUpdateFrame < m_aPolarShadowCache[Candidate].m_LastUpdateFrame)
			Candidate = Row;
	}
	m_aPolarShadowCache[Candidate].m_SourceIndex = SourceIndex;
	m_aPolarShadowCache[Candidate].m_Valid = false;
	EnsurePolarShadowAtlas();
	if(m_ShadowAtlasTexture >= 0)
	{
		unsigned char *pRow = m_aShadowAtlas.data() + Candidate * POLAR_SHADOW_SAMPLES * 4;
		for(int Sample = 0; Sample < POLAR_SHADOW_SAMPLES; Sample++)
		{
			pRow[Sample * 4] = 0xff;
			pRow[Sample * 4 + 1] = 0xff;
			pRow[Sample * 4 + 2] = 0xff;
			pRow[Sample * 4 + 3] = 0xff;
		}
		Graphics()->LoadTextureRawSub(m_ShadowAtlasTexture,
			0,
			Candidate,
			POLAR_SHADOW_SAMPLES,
			1,
			CImageInfo::FORMAT_RGBA,
			pRow);
	}
	return Candidate;
}

bool CLight::PolarShadowNeedsUpdate(int Row, int SourceIndex, vec2 Pos, float Radius) const
{
	if(Row <= 0 || Row >= POLAR_SHADOW_ROWS)
		return true;
	const SPolarShadowCache &Cache = m_aPolarShadowCache[Row];
	if(!Cache.m_Valid || Cache.m_SourceIndex != SourceIndex)
		return true;
	return distance(Cache.m_Pos, Pos) > 8.0f || fabs(Cache.m_Radius - Radius) > max(4.0f, Radius * 0.02f);
}

void CLight::RenderCpuVisibility(const SLightSource &Source, float Radius)
{
	if(Radius <= 32.0f || !Collision()->GetTiles())
		return;

	const vec2 Center = Source.m_Pos;
	// Build a visibility polygon in world coordinates. Each ray ends immediately
	// before its first blocker, so the resulting fan describes the illuminated
	// area directly. This avoids subtractive shadow quads, whose orientation was
	// easy to invert when CFreeformItem reordered its last two vertices.
	// Use a denser angular budget around wall corners. The previous 0.5*Radius
	// budget could skip a thin wall at a corner, leaving a bright rectangular
	// slit between two adjacent shadow sectors.
	const int RayCount = clamp((int)(Radius * 0.75f), 256, 768);
	const float TwoPi = 6.28318530717958647692f;
	struct SRay
	{
		vec2 m_VisibleEnd;
	};
	std::vector<SRay> aRays;
	aRays.resize(RayCount);
	for(int i = 0; i < RayCount; i++)
	{
		const float Angle = TwoPi * (float)i / (float)RayCount;
		const vec2 Direction(std::cos(Angle), std::sin(Angle));
		const vec2 End = Center + Direction * Radius;
		vec2 Hit = End;
		vec2 BeforeHit = End;
		int HitFlag = Collision()->IntersectLine(Center,
			End,
			&Hit,
			&BeforeHit,
			false,
			false,
			true);
		bool Blocked = HitFlag != 0;
		if(Blocked && (HitFlag == CCollision::COLFLAG_RAMP_LEFT ||
				HitFlag == CCollision::COLFLAG_RAMP_RIGHT ||
				HitFlag == CCollision::COLFLAG_ROOFSLOPE_LEFT ||
				HitFlag == CCollision::COLFLAG_ROOFSLOPE_RIGHT) &&
			distance(Center, Hit) < 20.0f && Collision()->CheckPoint(Center))
		{
			// Match the shader's origin-tile bias. A light centered on a player
			// standing on a ramp must not turn the ramp's own half-plane into a
			// dark stripe directly under the player.
			const vec2 BiasedOrigin = Center + Direction * 20.0f;
			Hit = End;
			BeforeHit = End;
			HitFlag = Collision()->IntersectLine(BiasedOrigin,
				End,
				&Hit,
				&BeforeHit,
				false,
				false,
				true);
			Blocked = HitFlag != 0;
		}
		vec2 VisibleEnd = Blocked ? BeforeHit : End;
		if(distance(Center, VisibleEnd) < 2.0f)
			VisibleEnd = Center + Direction * 2.0f;
		aRays[i] = {VisibleEnd};
	}

	// Draw the light texture directly on the visibility fan. Sampling the whole
	// LIGHT2 framebuffer on a source-sized quad used to scale unrelated parts of
	// the screen into the light and produced the large bright wedges at both
	// sides. Inverse rotation converts each world vertex into the same UV space
	// as a regular rotated source quad.
	const float CosRotation = std::cos(Source.m_Rot);
	const float SinRotation = std::sin(Source.m_Rot);
	const float InvWidth = 1.0f / max(std::fabs(Source.m_Size.x), 1.0f);
	const float InvHeight = 1.0f / max(std::fabs(Source.m_Size.y), 1.0f);
	auto TextureCoord = [&](vec2 World) {
		const vec2 Delta = World - Center;
		const vec2 Local(Delta.x * CosRotation + Delta.y * SinRotation,
			-Delta.x * SinRotation + Delta.y * CosRotation);
		return vec2(Local.x * InvWidth + 0.5f, Local.y * InvHeight + 0.5f);
	};
	const vec2 CenterUv = TextureCoord(Center);
	Graphics()->SetColor(Source.m_Color.r, Source.m_Color.g, Source.m_Color.b, Source.m_Color.a);
	for(int i = 0; i < RayCount; i++)
	{
		const int Next = (i + 1) % RayCount;
		const vec2 RayUv = TextureCoord(aRays[i].m_VisibleEnd);
		const vec2 NextRayUv = TextureCoord(aRays[Next].m_VisibleEnd);
		Graphics()->QuadsSetSubsetFree(CenterUv.x,
			CenterUv.y,
			RayUv.x,
			RayUv.y,
			CenterUv.x,
			CenterUv.y,
			NextRayUv.x,
			NextRayUv.y);
		// Input order is TL, TR, BL, BR. The repeated center becomes the
		// fourth backend vertex, yielding the triangle Center -> A -> B.
		IGraphics::CFreeformItem VisibleTriangle(Center.x,
			Center.y,
			aRays[i].m_VisibleEnd.x,
			aRays[i].m_VisibleEnd.y,
			Center.x,
			Center.y,
			aRays[Next].m_VisibleEnd.x,
			aRays[Next].m_VisibleEnd.y);
		Graphics()->QuadsDrawFreeform(&VisibleTriangle, 1);
	}
}

void CLight::RenderGroupRefactored(int Group)
{
	const bool DarkVision = m_pClient->DarkVisionEnabled();
	if(!g_Config.m_ClLighting && !DarkVision && !m_ForceLights)
		return;

	if(!Client()->IsGameWorldActive() || !g_Config.m_GfxMultiBuffering)
	{
		m_LightCount = 0;
		m_ForceLights = false;
		return;
	}

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);
	const bool UseShaderLight = g_Config.m_GfxShaders && g_Config.m_GfxMultiBuffering &&
		Graphics()->IsShaderAvailable(SHADER_LIGHT) && m_CollisionTexture >= 0;
	const float LightingBrightness = clamp(m_pClient->LightingBrightness(), 0.0f, 1.0f);
	const float DarkFactor = 1.0f - LightingBrightness;
	const vec2 CameraLightSize = vec2(900.0f, 680.0f) * (1.0f - DarkFactor) +
		vec2(600.0f, 450.0f) * DarkFactor;
	// The shadow radius must cover the complete light quad. Keeping this
	// relationship explicit prevents bright rectangular corners if the camera
	// light size is tuned later.
	const float CameraRadius = max(600.0f * (1.0f - DarkFactor) + 460.0f * DarkFactor,
		length(CameraLightSize * 0.5f) + 8.0f);
	const int TargetWidth = max(1, Graphics()->ScreenWidth() / LIGHT_RENDER_SCALE);
	const int TargetHeight = max(1, Graphics()->ScreenHeight() / LIGHT_RENDER_SCALE);
	const bool UsePolarShadow = UseShaderLight && Graphics()->IsShaderAvailable(SHADER_LIGHT_POLAR) &&
		m_ShadowAtlasTexture >= 0;
	// CPU visibility fans are intentionally budgeted more tightly than the GPU
	// path. Non-selected sources still keep their soft contribution, but do not
	// pay for a shadow fan this frame.
	const int MaxShadowCasters = UsePolarShadow ? 8 : (UseShaderLight ? 12 : 8);
	std::vector<int> aShadowRows(m_LightCount, -1);

	auto DrawSource = [&](const SLightSource &Source, float ShadowRadius, int SourceIndex) {
		const float CullingRadius = max(ShadowRadius, max(Source.m_Size.x, Source.m_Size.y) * 0.5f);
		if(Source.m_Pos.x + CullingRadius < Screen.x || Source.m_Pos.x - CullingRadius > Screen.w ||
		   Source.m_Pos.y + CullingRadius < Screen.y || Source.m_Pos.y - CullingRadius > Screen.h)
			return;

		if(!Source.m_CastShadow)
		{
			Graphics()->RenderToTexture(RENDERBUFFER_LIGHT);
			Graphics()->BlendAdditive();
			Graphics()->TextureSet(g_pData->m_aImages[Source.m_Image].m_Id);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(Source.m_Rot);
			Graphics()->SetColor(Source.m_Color.r, Source.m_Color.g, Source.m_Color.b, Source.m_Color.a);
			IGraphics::CQuadItem Quad(Source.m_Pos.x, Source.m_Pos.y, Source.m_Size.x, Source.m_Size.y);
			Graphics()->QuadsDraw(&Quad, 1);
			Graphics()->QuadsEnd();
			return;
		}

		if(UseShaderLight)
		{
			// Sample the source image directly in the light shader. The previous
			// implementation first rendered it into the full-screen LIGHT2 buffer
			// and then sampled that whole buffer on a source-sized quad, shrinking
			// the apparent light and wasting an extra pass.
			Graphics()->RenderToTexture(RENDERBUFFER_LIGHT);
			Graphics()->BlendAdditive();
			Graphics()->WrapClamp();
			Graphics()->TextureSet(g_pData->m_aImages[Source.m_Image].m_Id);
			const int ShadowRow = SourceIndex < 0 ? 0 : aShadowRows[SourceIndex];
			Graphics()->LightShaderBegin(m_CollisionTexture,
				Source.m_Pos.x,
				Source.m_Pos.y,
				ShadowRadius,
				m_CollisionWidth,
				m_CollisionHeight,
				Screen.x,
				Screen.y,
				Screen.w,
				Screen.h,
				TargetWidth,
				TargetHeight,
				UsePolarShadow && ShadowRow >= 0 ? m_ShadowAtlasTexture : -1,
				ShadowRow,
				POLAR_SHADOW_ROWS,
				POLAR_SHADOW_SAMPLES,
				UsePolarShadow && ShadowRow >= 0);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(Source.m_Rot);
			Graphics()->SetColor(Source.m_Color.r, Source.m_Color.g, Source.m_Color.b, Source.m_Color.a);
			IGraphics::CQuadItem VisibleQuad(Source.m_Pos.x, Source.m_Pos.y, Source.m_Size.x, Source.m_Size.y);
			Graphics()->QuadsDraw(&VisibleQuad, 1);
			Graphics()->QuadsEnd();
			Graphics()->ShaderEnd();
		}
		else
		{
			// Fixed-function fallback: clip the textured source directly with the
			// CPU visibility fan. Keeping geometry and UVs in one pass avoids every
			// render-target origin conversion and any whole-buffer resampling.
			Graphics()->RenderToTexture(RENDERBUFFER_LIGHT);
			Graphics()->BlendAdditive();
			Graphics()->WrapClamp();
			Graphics()->TextureSet(g_pData->m_aImages[Source.m_Image].m_Id);
			Graphics()->QuadsBegin();
			RenderCpuVisibility(Source, ShadowRadius);
			Graphics()->QuadsEnd();
		}
	};

	SLightSource CameraLight;
	CameraLight.Set(m_pClient->m_pCamera->m_TargetCenter,
		vec4(1.0f, 1.0f, 1.0f, 1.0f),
		CameraLightSize,
		0.0f,
		IMAGE_LIGHTS,
		true,
		3);
	if(UsePolarShadow)
	{
		m_ShadowFrame++;
		UpdatePolarShadow(0, -1, CameraLight, CameraRadius);
	}
	DrawSource(CameraLight, CameraRadius, -1);

	std::vector<bool> aCastShadow(m_LightCount, false);
	std::vector<int> aShadowIndices;
	for(int i = 0; i < m_LightCount; i++)
		if(m_aLights[i].m_CastShadow)
			aShadowIndices.push_back(i);
	std::sort(aShadowIndices.begin(), aShadowIndices.end(), [&](int A, int B) {
		if(m_aLights[A].m_Priority != m_aLights[B].m_Priority)
			return m_aLights[A].m_Priority > m_aLights[B].m_Priority;
		return distance(m_aLights[A].m_Pos, CameraLight.m_Pos) < distance(m_aLights[B].m_Pos, CameraLight.m_Pos);
	});
	if((int)aShadowIndices.size() > MaxShadowCasters)
		aShadowIndices.resize(MaxShadowCasters);
	for(const int Index : aShadowIndices)
		aCastShadow[Index] = true;
	if(UsePolarShadow)
	{
		m_ShadowUpdateBudget = 2;
		std::vector<int> Pending;
		for(const int Index : aShadowIndices)
		{
			const float Radius = max(m_aLights[Index].m_Size.x, m_aLights[Index].m_Size.y) * 0.75f;
			const int Row = AcquirePolarShadowRow(Index);
			aShadowRows[Index] = Row;
			const bool NeedsUpdate = PolarShadowNeedsUpdate(Row, Index, m_aLights[Index].m_Pos, Radius);
			const bool LargeMove = m_aPolarShadowCache[Row].m_Valid &&
				distance(m_aPolarShadowCache[Row].m_Pos, m_aLights[Index].m_Pos) > 128.0f;
			if(NeedsUpdate && LargeMove)
				UpdatePolarShadow(Row, Index, m_aLights[Index], Radius);
			else if(NeedsUpdate)
				Pending.push_back(Index);
		}
		std::sort(Pending.begin(), Pending.end(), [&](int A, int B) {
			return m_aPolarShadowCache[aShadowRows[A]].m_LastUpdateFrame <
				m_aPolarShadowCache[aShadowRows[B]].m_LastUpdateFrame;
		});
		for(const int Index : Pending)
		{
			if(m_ShadowUpdateBudget <= 0)
				break;
			const int Row = aShadowRows[Index];
			UpdatePolarShadow(Row,
				Index,
				m_aLights[Index],
				max(m_aLights[Index].m_Size.x, m_aLights[Index].m_Size.y) * 0.75f);
			m_ShadowUpdateBudget--;
		}
	}

	for(int i = 0; i < m_LightCount; i++)
	{
		SLightSource Source = m_aLights[i];
		Source.m_CastShadow = aCastShadow[i];
		DrawSource(Source, max(Source.m_Size.x, Source.m_Size.y) * 0.75f, i);
	}

	// Powerupper is an existing custom freeform light. Keep it additive; it is
	// not a registered radial source and therefore has no per-source shader
	// shadow pass.
	Graphics()->RenderToTexture(RENDERBUFFER_LIGHT);
	Graphics()->BlendAdditive();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
		if(Item.m_Type == NETOBJTYPE_POWERUPPER)
		{
			const CNetObj_Powerupper *pBuilding = (const CNetObj_Powerupper *)pData;
			const vec2 p = vec2(pBuilding->m_X, pBuilding->m_Y - 22);
			RenderLight(p + vec2(-10, 0), p + vec2(+10, 0), p + vec2(-40, -70), p + vec2(+40, -70), vec4(0.25f, 1.0f, 0.5f, 1.0f));
		}
	}
	Graphics()->QuadsEnd();

	m_LightCount = 0;
	m_ForceLights = false;

	Graphics()->RenderToScreen();
	Graphics()->BlendLight();
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->WrapClamp();
	Graphics()->TextureSet(-2, RENDERBUFFER_LIGHT);
	const bool UseCompositeShader = g_Config.m_GfxShaders && Graphics()->IsShaderAvailable(SHADER_LIGHT_COMPOSITE);
	if(UseCompositeShader)
		Graphics()->LightCompositeShaderBegin(TargetWidth, TargetHeight);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(Graphics()->ScreenWidth() / 2,
		Graphics()->ScreenHeight() / 2,
		Graphics()->ScreenWidth(),
		-Graphics()->ScreenHeight());
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();
	if(UseCompositeShader)
		Graphics()->ShaderEnd();
	Graphics()->BlendNormal();
	Graphics()->WrapNormal();
	Graphics()->TextureSet(-1);
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

void CLight::RenderGroup(int Group)
{
	RenderGroupRefactored(Group);
}
