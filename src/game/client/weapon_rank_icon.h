#ifndef GAME_CLIENT_WEAPON_RANK_ICON_H
#define GAME_CLIENT_WEAPON_RANK_ICON_H

#include <base/math.h>
#include <base/vmath.h>
#include <engine/graphics.h>
#include <game/client/render.h>
#include <game/weapons/weapon_catalog.h>
#include <game/weapons/weapons.h>
#include <generated/game_data.h>

inline int WeaponRankSpriteId(const CWeaponSpec &Spec)
{
	if(!Spec.IsValid() || Spec.m_Level <= 0)
		return -1;
	CResolvedWeaponProfile Profile{};
	if(!CWeaponCatalog::TryResolve(Spec, &Profile) ||
	   WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_UPGRADE))
		return -1;
	const int MaxLevel = max(1, (int)Profile.m_Definition.m_MaxLevel);
	const int Level = min((int)Spec.m_Level, MaxLevel);
	const int Rank = 1 + (Level - 1) * 6 / max(1, MaxLevel - 1);
	return SPRITE_WEAPONRANK1 + clamp(Rank, 1, 7) - 1;
}

inline void DrawWeaponRankIcon(
	IGraphics *pGraphics, CRenderTools *pRenderTools, const CWeaponSpec &Spec, vec2 Pos, float Size, float Alpha = 1.0f)
{
	const int RankSprite = WeaponRankSpriteId(Spec);
	if(RankSprite < 0 || !pGraphics || !pRenderTools)
		return;
	pGraphics->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	pRenderTools->SelectSprite(RankSprite);
	pRenderTools->DrawSprite(Pos.x, Pos.y, Size);
	pGraphics->QuadsEnd();
}

// Siile inventory (0.5 era): rank sits on weapon center-X, above by size*1.6, sprite size size*4.
inline void DrawWeaponRankOverWeapon(
	IGraphics *pGraphics, CRenderTools *pRenderTools, const CWeaponSpec &Spec, vec2 WeaponPos, float WeaponSize, float Alpha = 1.0f)
{
	DrawWeaponRankIcon(pGraphics,
					   pRenderTools,
					   Spec,
					   vec2(WeaponPos.x, WeaponPos.y - WeaponSize * 1.6f),
					   WeaponSize * 4.0f,
					   Alpha);
}

#endif
