#ifndef GAME_CLIENT_DROID_VISUAL_H
#define GAME_CLIENT_DROID_VISUAL_H

#include <base/vmath.h>
#include <generated/protocol.h>

enum
{
	DROID_DRAW_NONE = 0,
	DROID_DRAW_WALKER,
	DROID_DRAW_STAR,
	DROID_DRAW_CRAWLER,
	DROID_DRAW_WARDEN,
	DROID_DRAW_ANGLER,
};

struct CDroidVisual
{
	int m_Draw;
	float m_Scale;
	vec4 m_Color;
	vec4 m_LegColor;
	vec4 m_Light;
	float m_LightSize;
	bool m_Stealth;
	bool m_AirSpin;
};

inline const CDroidVisual &DroidVisual(int Type)
{
	const vec4 White(1.0f, 1.0f, 1.0f, 1.0f);
	const vec4 NoLight(0.0f, 0.0f, 0.0f, 0.0f);
	const vec4 CrawlerLight(0.5f, 1.0f, 1.0f, 0.5f);
	static const CDroidVisual s_aVisual[NUM_DROIDTYPES] = {
		{DROID_DRAW_WALKER, 1.0f, White, White, NoLight, 0.0f, false, false},
		{DROID_DRAW_STAR, 1.0f, White, White, NoLight, 0.0f, false, false},
		{DROID_DRAW_CRAWLER, 1.0f, White, White, CrawlerLight, 100.0f, false, false},
		{DROID_DRAW_CRAWLER, 2.0f, vec4(0.3f, 0.3f, 0.3f, 1.0f), vec4(0.6f, 0.6f, 0.6f, 1.0f), CrawlerLight, 100.0f, false, false},
		{DROID_DRAW_NONE, 1.0f, White, White, NoLight, 0.0f, false, false},
		{DROID_DRAW_STAR, 1.7f, White, White, NoLight, 0.0f, false, false},
		{DROID_DRAW_WALKER, 1.8f, White, White, NoLight, 0.0f, false, false},
		{DROID_DRAW_CRAWLER, 1.6f, vec4(0.85f, 0.4f, 0.3f, 1.0f), vec4(0.85f, 0.45f, 0.35f, 1.0f), CrawlerLight, 100.0f, false, false},
		{DROID_DRAW_CRAWLER, 1.35f, vec4(0.90f, 0.35f, 0.15f, 1.0f), vec4(0.95f, 0.45f, 0.25f, 1.0f), vec4(1.0f, 0.28f, 0.08f, 0.75f), 145.0f, false, false},
		{DROID_DRAW_STAR, 1.15f, vec4(0.75f, 0.40f, 1.00f, 1.0f), White, vec4(0.72f, 0.25f, 1.0f, 0.72f), 125.0f, false, false},
		{DROID_DRAW_CRAWLER, 0.85f, vec4(0.75f, 1.00f, 0.35f, 1.0f), vec4(0.75f, 1.00f, 0.35f, 1.0f), vec4(0.72f, 1.0f, 0.18f, 0.65f), 110.0f, false, false},
		{DROID_DRAW_STAR, 0.80f, vec4(1.00f, 0.20f, 0.12f, 1.0f), White, vec4(1.0f, 0.08f, 0.04f, 0.85f), 135.0f, false, false},
		{DROID_DRAW_STAR, 1.05f, vec4(0.55f, 0.95f, 1.00f, 1.0f), White, vec4(0.55f, 1.0f, 1.0f, 0.78f), 140.0f, false, false},
		{DROID_DRAW_CRAWLER, 1.0f, vec4(0.30f, 0.95f, 0.40f, 1.0f), vec4(0.40f, 1.00f, 0.50f, 1.0f), vec4(0.2f, 1.0f, 0.35f, 0.7f), 120.0f, false, false},
		{DROID_DRAW_CRAWLER, 0.90f, vec4(0.45f, 0.25f, 0.70f, 1.0f), vec4(0.55f, 0.35f, 0.80f, 1.0f), NoLight, 0.0f, true, false},
		{DROID_DRAW_STAR, 1.10f, vec4(0.25f, 0.55f, 1.00f, 1.0f), White, vec4(0.12f, 0.55f, 1.0f, 0.88f), 145.0f, false, false},
		{DROID_DRAW_CRAWLER, 1.15f, vec4(1.00f, 0.70f, 0.20f, 1.0f), vec4(1.00f, 0.78f, 0.30f, 1.0f), NoLight, 0.0f, false, true},
		{DROID_DRAW_WARDEN, 0.85f, White, White, vec4(1.0f, 0.42f, 0.12f, 0.85f), 170.0f, false, false},
		{DROID_DRAW_ANGLER, 0.75f, White, White, vec4(0.3f, 0.85f, 1.0f, 0.55f), 190.0f, false, false},
	};
	static const CDroidVisual s_None = {DROID_DRAW_NONE, 1.0f, White, White, NoLight, 0.0f, false, false};
	if(Type < 0 || Type >= NUM_DROIDTYPES)
		return s_None;
	return s_aVisual[Type];
}

#endif
