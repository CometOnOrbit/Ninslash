#ifndef GAME_CLIENT_COMPONENTS_LIGHT_H
#define GAME_CLIENT_COMPONENTS_LIGHT_H
#include <array>
#include <base/vmath.h>
#include <game/client/component.h>

struct SLightSource
{
	vec2 m_Pos;
	vec4 m_Color;
	vec2 m_Size;
	float m_Rot;
	int m_Image;
	bool m_CastShadow;
	int m_Priority;

	void Set(vec2 Pos, vec4 Color, vec2 Size, float Rot, int Image, bool CastShadow, int Priority)
	{
		m_Pos = Pos;
		m_Color = Color;
		m_Size = Size;
		m_Rot = Rot;
		m_Image = Image;
		m_CastShadow = CastShadow;
		m_Priority = Priority;
	}
};

class CLight : public CComponent
{
	friend class CGameClient;

  public:
	enum
	{
		GROUP_LIGHTSOURCE = 0,
		NUM_GROUPS
	};

	CLight();

	void AddSimpleLight(vec2 Pos, vec4 Color, vec2 Size, bool CastShadow = true, bool Force = false);
	void AddBoxLight(vec2 Pos, vec4 Color, vec2 Size, float Rot = 0.0f, bool CastShadow = true);

	virtual void OnReset();
	virtual void OnMapLoad();
	virtual void OnRender();

  private:
	// GPU texture of collision tile flags (COLFLAG incl. ramps) for the shader
	// lighting pass; -1 when unavailable.
	int m_CollisionTexture = -1;
	float m_CollisionWidth = 0.0f;
	float m_CollisionHeight = 0.0f;
	enum
	{
		MAX_LIGHTSOURCES = 1024 * 2,
		POLAR_SHADOW_SAMPLES = 512,
		POLAR_SHADOW_ROWS = 17,
	};
	struct SPolarShadowCache
	{
		int m_SourceIndex;
		vec2 m_Pos;
		float m_Radius;
		int m_LastUpdateFrame;
		bool m_Valid;
	};

	int m_LightCount;
	bool m_ForceLights;
	SLightSource m_aLights[MAX_LIGHTSOURCES];
	int m_ShadowAtlasTexture = -1;
	int m_ShadowFrame = 0;
	int m_ShadowUpdateBudget = 0;
	SPolarShadowCache m_aPolarShadowCache[POLAR_SHADOW_ROWS];
	std::array<unsigned char, POLAR_SHADOW_SAMPLES * POLAR_SHADOW_ROWS * 4> m_aShadowAtlas{};

	void RenderLight(vec2 Pos, vec2 Size, vec4 Color);
	void RenderLight(ivec2 Pos);
	void RenderLight(vec2 Pos1, vec2 Pos2, vec2 Pos3, vec2 Pos4, vec4 Color);
	void RenderCpuVisibility(const SLightSource &Source, float Radius);
	float VisibleDistance(vec2 Center, vec2 End) const;
	void EnsurePolarShadowAtlas();
	void UpdatePolarShadow(int Row, int SourceIndex, const SLightSource &Source, float Radius);
	int AcquirePolarShadowRow(int SourceIndex);
	bool PolarShadowNeedsUpdate(int Row, int SourceIndex, vec2 Pos, float Radius) const;

	int m_FirstFree;
	int m_aFirstPart[NUM_GROUPS];

	void RenderGroup(int Group);
	void RenderGroupRefactored(int Group);
	void Update(float TimePassed);

	template <int TGROUP> class CRenderGroup : public CComponent
	{
	  public:
		CLight *m_pParts;
		virtual void OnRender() { m_pParts->RenderGroup(TGROUP); }
	};

	CRenderGroup<GROUP_LIGHTSOURCE> m_RenderLight;
};
#endif
