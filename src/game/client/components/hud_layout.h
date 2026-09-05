#ifndef GAME_CLIENT_COMPONENTS_HUD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_HUD_LAYOUT_H

namespace HudLayout
{
constexpr float ObjectiveTop = 82.0f;

constexpr float SafeMargin(float ScreenWidth)
{
	return ScreenWidth >= 500.0f ? 8.0f : 6.0f;
}

constexpr bool CompactBottomHud(float ScreenWidth)
{
	return ScreenWidth < 420.0f;
}

constexpr float BuildEffectsLeft = 10.0f;
constexpr float BuildEffectsTop = 10.0f;
constexpr float BuildEffectsWidth = 220.0f;
constexpr int BuildEffectsColumns = 2;
constexpr float BuildEffectsRowHeight = 12.0f;

constexpr int BuildEffectsRows(int Lines)
{
	return (Lines + BuildEffectsColumns - 1) / BuildEffectsColumns;
}

constexpr float BuildEffectsHeight(int Lines)
{
	return 12.0f + BuildEffectsRows(Lines) * BuildEffectsRowHeight;
}

constexpr float BottomStatusTop(float ScreenHeight)
{
	return ScreenHeight - 38.0f;
}

constexpr float BottomStatusBottom(float ScreenHeight)
{
	return ScreenHeight - 4.0f;
}

constexpr float VitalCoreHeight = 50.0f;

constexpr float VitalCoreTop(float ScreenHeight)
{
	return BottomStatusBottom(ScreenHeight) - VitalCoreHeight;
}

constexpr float CombatBarTop(float ScreenHeight)
{
	return ScreenHeight - 30.0f;
}

constexpr float WeaponCardWidth = 94.0f;
constexpr float WeaponCardHeight = 34.0f;

constexpr float WeaponCardTop(float ScreenWidth, float ScreenHeight)
{
	return CompactBottomHud(ScreenWidth) ? CombatBarTop(ScreenHeight) - WeaponCardHeight - 4.0f : BottomStatusTop(ScreenHeight);
}

constexpr float SpectatorBarHeight = 15.0f;

constexpr float SpectatorBarTop(float ScreenWidth, float ScreenHeight)
{
	return WeaponCardTop(ScreenWidth, ScreenHeight) - SpectatorBarHeight -
		   (CompactBottomHud(ScreenWidth) ? 3.0f : 2.0f);
}

constexpr float CombatBarWidth = 128.0f;
constexpr float CombatBarSlotGap = 3.0f;
constexpr float CombatBarSlotHeight = 25.0f;
constexpr float CombatBarSelectedHeight = 27.0f;

constexpr float CombatBarSlotWidth()
{
	return (CombatBarWidth - CombatBarSlotGap * 3.0f) / 4.0f;
}

constexpr float CombatBarLeft(float ScreenWidth)
{
	return (ScreenWidth - CombatBarWidth) * 0.5f;
}

constexpr float CombatBarSlotX(float ScreenWidth, int Slot)
{
	return CombatBarLeft(ScreenWidth) + Slot * (CombatBarSlotWidth() + CombatBarSlotGap);
}

constexpr float ChatInputHeight = 16.0f;
constexpr float ChatStatusGap = 4.0f;

constexpr float ChatInputTop(float ScreenWidth, float ScreenHeight, bool Spectator)
{
	float Top = VitalCoreTop(ScreenHeight) - ChatStatusGap - ChatInputHeight;
	const float WeaponTop = WeaponCardTop(ScreenWidth, ScreenHeight) - ChatStatusGap - ChatInputHeight;
	if(Top > WeaponTop)
		Top = WeaponTop;
	if(Spectator)
	{
		const float SpectatorTop =
			SpectatorBarTop(ScreenWidth, ScreenHeight) - ChatStatusGap - ChatInputHeight;
		if(Top > SpectatorTop)
			Top = SpectatorTop;
	}
	return Top;
}

constexpr float ChatInputTop(float ScreenHeight)
{
	return VitalCoreTop(ScreenHeight) - ChatStatusGap - ChatInputHeight;
}

constexpr float ChatInputRight(float ScreenWidth)
{
	const float ScoreWidth = CompactBottomHud(ScreenWidth) ? 82.0f : 96.0f;
	return ScreenWidth - SafeMargin(ScreenWidth) - ScoreWidth - 7.0f;
}

constexpr int LowHealthThreshold = 60;
constexpr int CriticalHealthThreshold = 20;

constexpr float LowHealthAmount(int Health)
{
	if(Health >= LowHealthThreshold)
		return 0.0f;
	return Health <= 0 ? 1.0f : (LowHealthThreshold - Health) / (float)LowHealthThreshold;
}

constexpr float CriticalHealthAmount(int Health)
{
	if(Health >= CriticalHealthThreshold)
		return 0.0f;
	return Health <= 0 ? 1.0f : (CriticalHealthThreshold - Health) / (float)CriticalHealthThreshold;
}

constexpr float VitalCombinedAmount(float HealthAmount, float ArmorAmount)
{
	return HealthAmount + ArmorAmount < 1.0f ? HealthAmount + ArmorAmount : 1.0f;
}

constexpr float VitalOverlapAmount(float HealthAmount, float ArmorAmount)
{
	return HealthAmount + ArmorAmount > 1.0f ? HealthAmount + ArmorAmount - 1.0f : 0.0f;
}

constexpr float VitalHealthOnlyEnd(float HealthAmount, float ArmorAmount)
{
	const float End = HealthAmount - VitalOverlapAmount(HealthAmount, ArmorAmount);
	return End > 0.0f ? End : 0.0f;
}

constexpr bool ChatAvoidsBottomStatus(float ScreenHeight)
{
	return ChatInputTop(ScreenHeight) + ChatInputHeight + ChatStatusGap <= VitalCoreTop(ScreenHeight);
}
}

#endif
