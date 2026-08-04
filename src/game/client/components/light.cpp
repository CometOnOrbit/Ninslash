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
}

void CLight::AddSimpleLight(vec2 Pos, vec4 Color, vec2 Size, bool CastShadow)
{
	// cl_lighting only controls optional local light sources. DarkVision is a
	// server-authoritative render rule and must keep its camera/light pool even
	// when a player disables ordinary dynamic lighting.
	if(!g_Config.m_ClLighting && !m_pClient->DarkVisionEnabled())
		return;

	if(m_LightCount >= MAX_LIGHTSOURCES)
		return;

	CastShadow = CastShadow && (Size.x > 32.0f || Size.y > 32.0f);
	const int Image = (Size.x <= 32.0f && Size.y <= 32.0f) ? IMAGE_SMALLLIGHT : IMAGE_LIGHTS;
	m_aLights[m_LightCount++].Set(Pos, Color, Size, 0.0f, Image, CastShadow, CastShadow ? 1 : 0);
}

void CLight::AddBoxLight(vec2 Pos, vec4 Color, vec2 Size, float Rot)
{
	if(!g_Config.m_ClLighting && !m_pClient->DarkVisionEnabled())
		return;

	if(m_LightCount >= MAX_LIGHTSOURCES)
		return;

	m_aLights[m_LightCount++].Set(Pos, Color, Size, Rot, IMAGE_BOXLIGHT, true, 2);
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
	if(!g_Config.m_ClLighting && !DarkVision)
		return;

	if(!Client()->IsGameWorldActive() || !g_Config.m_GfxMultiBuffering)
	{
		m_LightCount = 0;
		return;
	}

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);
	const bool UseShaderLight = g_Config.m_GfxShaders && g_Config.m_GfxMultiBuffering &&
		Graphics()->IsShaderAvailable(SHADER_LIGHT) && m_CollisionTexture >= 0;
	// Dark vision keeps the world outside the player's immediate pool black,
	// but the smaller pool made shader mode illuminate little more than the
	// player's own body. Keep a 4:3 pool large enough for nearby movement and
	// aiming while retaining a clearly bounded dark-vision area.
const vec2 CameraLightSize = DarkVision ? vec2(720.0f, 540.0f) : vec2(1100.0f, 850.0f);
	// The shadow radius must cover the complete light quad. Keeping this
	// relationship explicit prevents bright rectangular corners if the camera
	// light size is tuned later.
const float CameraRadius = max(DarkVision ? 560.0f : 700.0f,
		length(CameraLightSize * 0.5f) + 8.0f);
	const int TargetWidth = Graphics()->ScreenWidth();
	const int TargetHeight = Graphics()->ScreenHeight();
	// CPU visibility fans are intentionally budgeted more tightly than the GPU
	// path. Non-selected sources still keep their soft contribution, but do not
	// pay for a shadow fan this frame.
	const int MaxShadowCasters = UseShaderLight ? 12 : 8;

	auto DrawSource = [&](const SLightSource &Source, float ShadowRadius) {
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
				TargetHeight);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(Source.m_Rot);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
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
		vec4(1.0f, 1.0f, 1.0f, DarkVision ? 0.78f : 0.55f),
		CameraLightSize,
		0.0f,
		IMAGE_LIGHTS,
		true,
		3);
	DrawSource(CameraLight, CameraRadius);

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

	for(int i = 0; i < m_LightCount; i++)
	{
		SLightSource Source = m_aLights[i];
		Source.m_CastShadow = aCastShadow[i];
		DrawSource(Source, max(Source.m_Size.x, Source.m_Size.y) * 0.75f);
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

	Graphics()->RenderToScreen();
	Graphics()->BlendLight();
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->WrapClamp();
	Graphics()->TextureSet(-2, RENDERBUFFER_LIGHT);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(Graphics()->ScreenWidth() / 2,
		Graphics()->ScreenHeight() / 2,
		Graphics()->ScreenWidth(),
		-Graphics()->ScreenHeight());
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
	Graphics()->WrapNormal();
	Graphics()->TextureSet(-1);
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

void CLight::RenderGroup(int Group)
{
	RenderGroupRefactored(Group);
}
