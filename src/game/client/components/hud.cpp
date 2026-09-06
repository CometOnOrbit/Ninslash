#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/game_data.h>
#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/customstuff.h>

#include <game/pve/questinfo.h>
#include <game/weapons/weapons.h>
#include <game/weapons/weapon_catalog.h>
#include <game/buildables.h>
#include <game/pve/pve_environment.h>

#include "controls.h"
#include "camera.h"
#include "effects.h"
#include "hud.h"
#include "hud_layout.h"
#include "inventory.h"
#include "menus.h"
#include "pve_roguelite.h"
#include "scoreboard.h"
#include "sounds.h"
#include "voting.h"
#include "binds.h"
#include <game/client/weapon_rank_icon.h>

#define RAD 0.017453292519943295769236907684886f

CHud::CHud()
{
	// won't work if zero
	m_AverageFPS = 1.0f;
	m_DebugStatusMask = 0;
	m_DebugStatusUntil = 0;
	m_DebugStatusScreenshotFrames = 0;
	m_LastObjectiveSignature = -1;
	m_LastObjectiveQuest = QUEST_NONE;
	m_ObjectiveTransitionStart = 0;
	m_ObjectiveNoticeUntil = 0;
	m_LastHitEvent = 0;
	m_LastHitSound = 0;
	m_HitMarkerUntil = 0;
	m_HitDamage = 0;
	m_HitTargetType = HIT_TARGET_FLESH;
	m_HitKilled = false;
	mem_zero(m_aStatusAppear, sizeof(m_aStatusAppear));
	m_LastAnimationTime = time_get();
}

void CHud::OnReset()
{
	m_LastObjectiveSignature = -1;
	m_LastObjectiveQuest = QUEST_NONE;
	m_ObjectiveTransitionStart = 0;
	m_ObjectiveNoticeUntil = 0;
	m_LastHitEvent = 0;
	m_LastHitSound = 0;
	m_HitMarkerUntil = 0;
	m_HitDamage = 0;
	m_HitTargetType = HIT_TARGET_FLESH;
	m_HitKilled = false;
	mem_zero(m_aStatusAppear, sizeof(m_aStatusAppear));
	m_LastAnimationTime = time_get();
	if(m_DebugStatusUntil <= time_get())
	{
		m_DebugStatusMask = 0;
		m_DebugStatusScreenshotFrames = 0;
	}
}

void CHud::OnHitConfirm(vec2 Pos, int Damage, int TargetType, bool Killed)
{
	if(g_Config.m_ClHitFeedback <= 0 || Damage <= 0)
		return;

	const int64 Now = time_get();
	const int64 MergeWindow = time_freq() * 50 / 1000;
	if(m_LastHitEvent && Now - m_LastHitEvent <= MergeWindow)
	{
		m_HitDamage += Damage;
		m_HitTargetType = max(m_HitTargetType, TargetType);
		m_HitKilled = m_HitKilled || Killed;
	}
	else
	{
		m_HitDamage = Damage;
		m_HitTargetType = TargetType;
		m_HitKilled = Killed;
	}
	m_LastHitEvent = Now;
	m_HitMarkerUntil = Now + time_freq() * (m_HitKilled ? 220 : 120) / 1000;

	const float Strength = g_Config.m_ClHitFeedback / 100.0f;
	vec4 SparkColor(0.95f, 0.2f, 0.16f, 1.0f);
	int HitSound = SOUND_HIT;
	if(TargetType == HIT_TARGET_METAL)
	{
		SparkColor = vec4(1.0f, 0.62f, 0.12f, 1.0f);
		HitSound = SOUND_METAL_HIT;
	}
	else if(TargetType == HIT_TARGET_SHIELD)
	{
		SparkColor = vec4(0.2f, 0.82f, 1.0f, 1.0f);
		HitSound = SOUND_SHIELD_HIT;
	}

	const int SparkCount = 1 + (int)(Strength * 2.0f);
	for(int i = 0; i < SparkCount; i++)
		m_pClient->m_pEffects->HitSpark(Pos, SparkColor);

	const int64 SoundInterval = time_freq() * 60 / 1000;
	if(Killed || !m_LastHitSound || Now - m_LastHitSound >= SoundInterval)
	{
		m_pClient->m_pSounds->SetHitFeedbackVolume(Strength);
		m_pClient->m_pSounds->Play(CSounds::CHN_HIT, HitSound, 1.0f);
		m_LastHitSound = Now;
	}

	float Shake = 0.6f + min(Damage, 50) * 0.03f;
	if(Killed)
		Shake *= 1.5f;
	CustomStuff()->AddCameraImpulse(vec2(0, 0), Shake, Strength);
}

void CHud::OnConsoleInit()
{
	Console()->Register(
		"hud_debug_status",
		"?i?i?i",
		CFGFLAG_CLIENT,
		ConDebugStatus,
		this,
		"Preview HUD status stack: mask 1 paused, 2 connection, 4 warmup, 8 ready; optional seconds and screenshot");
}

void CHud::ConDebugStatus(IConsole::IResult *pResult, void *pUserData)
{
	CHud *pSelf = (CHud *)pUserData;
	const int Mask = pResult->NumArguments() ? clamp(pResult->GetInteger(0), 0, 15) : 15;
	const int Seconds = pResult->NumArguments() > 1 ? clamp(pResult->GetInteger(1), 5, 60) : 25;
	pSelf->m_DebugStatusMask = Mask;
	pSelf->m_DebugStatusUntil = Mask ? time_get() + time_freq() * Seconds : 0;
	pSelf->m_DebugStatusScreenshotFrames = Mask && pResult->NumArguments() > 2 && pResult->GetInteger(2) ? 30 : 0;
}

bool CHud::DebugStatusActive(int Flag) const
{
	return m_DebugStatusUntil > time_get() && (m_DebugStatusMask & Flag) != 0;
}

bool CHud::WarmupActive() const
{
	return DebugStatusActive(DEBUG_STATUS_WARMUP) ||
		   (m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer);
}

bool CHud::PausedNoticeActive() const
{
	if(!m_pClient->m_Snap.m_pGameInfoObj || m_pClient->m_pPveRoguelite->ChoiceActive() ||
	   (m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		return false;
	return DebugStatusActive(DEBUG_STATUS_PAUSED) ||
		   (m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);
}

bool CHud::ReadyNoticeActive() const
{
	if(DebugStatusActive(DEBUG_STATUS_READY))
		return true;
	if(!m_pClient->m_Snap.m_pGameInfoObj || !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer ||
	   m_pClient->m_pControls->m_Ready)
		return false;
	return !m_pClient->m_Snap.m_pLocalCharacter ||
		   !(m_pClient->m_Snap.m_pLocalCharacter->m_PlayerFlags & PLAYERFLAG_READY);
}

bool CHud::ConnectionNoticeActive() const
{
	return DebugStatusActive(DEBUG_STATUS_CONNECTION) || Client()->ConnectionProblems();
}

float CHud::StatusStackY(int BeforeFlag) const
{
	float Y = 50.0f + 46.0f * m_aStatusAppear[3];
	if(BeforeFlag > STATUS_STACK_PAUSED)
		Y += 30.0f * m_aStatusAppear[STATUS_STACK_PAUSED];
	if(BeforeFlag > STATUS_STACK_READY)
		Y += 30.0f * m_aStatusAppear[STATUS_STACK_READY];
	if(BeforeFlag > STATUS_STACK_CONNECTION)
		Y += 30.0f * m_aStatusAppear[STATUS_STACK_CONNECTION];
	return Y;
}

void CHud::UpdateAnimations()
{
	const int64 Now = time_get();
	const float Dt = clamp((float)((Now - m_LastAnimationTime) / (double)time_freq()), 0.0f, 0.05f);
	const float Blend = 1.0f - expf(-14.0f * Dt);
	const float aTargets[4] = {PausedNoticeActive() ? 1.0f : 0.0f,
							   ReadyNoticeActive() ? 1.0f : 0.0f,
							   ConnectionNoticeActive() ? 1.0f : 0.0f,
							   WarmupActive() ? 1.0f : 0.0f};
	for(int i = 0; i < 4; ++i)
	{
		m_aStatusAppear[i] += (aTargets[i] - m_aStatusAppear[i]) * Blend;
		m_aStatusAppear[i] = clamp(m_aStatusAppear[i], 0.0f, 1.0f);
	}
	m_LastAnimationTime = Now;
}

void CHud::RenderStatusNotice(const char *pText, float Y, vec4 AccentColor, float Amount)
{
	const float Eased = 1.0f - (1.0f - Amount) * (1.0f - Amount) * (1.0f - Amount);
	Y -= (1.0f - Eased) * 5.0f;
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Text = CMenus::ThemeText();
	float FontSize = 8.0f;
	const float TextW = TextRender()->TextWidth(0, FontSize, pText, -1);
	const float W = clamp(TextW + 28.0f, 76.0f, 184.0f);
	const float H = 22.0f;
	const float X = (m_Width - W) * 0.5f;
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.40f * Amount);
	RenderTools()->DrawRoundRect(X + 1.0f, Y + 1.5f, W, H, 7.0f);
	Graphics()->SetColor(AccentColor.r, AccentColor.g, AccentColor.b, 0.72f * Amount);
	RenderTools()->DrawRoundRect(X - 0.7f, Y - 0.7f, W + 1.4f, H + 1.4f, 7.5f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.97f * Amount);
	RenderTools()->DrawRoundRect(X, Y, W, H, 7.0f);
	Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.44f * Amount);
	RenderTools()->DrawRoundRect(X + 5.0f, Y + 4.0f, W - 10.0f, H - 8.0f, 5.0f);
	Graphics()->SetColor(AccentColor.r, AccentColor.g, AccentColor.b, 0.96f * Amount);
	RenderTools()->DrawRoundRect(X, Y + 5.0f, 2.0f, H - 10.0f, 1.0f);
	Graphics()->QuadsEnd();
	TextRender()->TextColor(Text.r, Text.g, Text.b, Amount);
	TextRender()->Text(0, X + (W - TextW) * 0.5f, Y + 6.0f, FontSize, pText, -1);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CHud::RenderGameTimer()
{
	if(!g_Config.m_ClShowhudTimer)
		return;
	float Half = 300.0f * Graphics()->ScreenAspect() / 2.0f;

	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
	{
		char Buf[32];
		int Time = 0;
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		{
			Time = m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit * 60 -
				   ((Client()->GameTick() - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) /
					Client()->GameTickSpeed());

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else
			Time =
				(Client()->GameTick() - m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed();

		if(Time < 0)
			Time = 0;

		str_format(Buf, sizeof(Buf), "%d:%02d", Time / 60, Time % 60);
		float FontSize = 10.0f;
		float w = TextRender()->TextWidth(0, FontSize, Buf, -1);
		// last 60 sec red, last 10 sec blink
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 &&
		   !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		{
			const float Seconds = (float)(time_get() / (double)time_freq());
			float Alpha = Time <= 10 ? 0.72f + 0.28f * (0.5f + 0.5f * sinf(Seconds * 6.2831853f)) : 1.0f;
			vec4 Danger = CMenus::ThemeDanger();
			TextRender()->TextColor(Danger.r, Danger.g, Danger.b, Alpha);
		}
		else
		{
			vec4 Accent = CMenus::ThemeAccent();
			TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
		}
		TextRender()->Text(0, Half - w / 2, 2, FontSize, Buf, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// survival mode text
	if(m_pClient->Survival())
	{
		TextRender()->TextColor(1.0f, 1.0f, 0.0f, 1.0f);
		const char *pText = Localize("Survival mode");
		float FontSize = 7.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->Text(0, 150.0f * Graphics()->ScreenAspect() + -w / 2.0f, 12.0f, FontSize, pText, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CHud::RenderRaceTimer()
{
	if(!g_Config.m_ClShowhudTimer || !m_pClient->m_Snap.m_pRaceInfo)
		return;

	int ClientID = m_pClient->m_Snap.m_LocalClientID;
	if(m_pClient->m_Snap.m_SpecInfo.m_Active &&
	   m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		ClientID = m_pClient->m_Snap.m_SpecInfo.m_SpectatorID;
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	const CNetObj_RacePlayer *pRace = m_pClient->m_Snap.m_apRacePlayers[ClientID];
	if(!pRace)
		return;

	char aTime[32];
	char aCheckpoint[32];
	CGameClient::FormatRaceTime(m_pClient->RaceTime(ClientID), aTime, sizeof(aTime));
	str_format(aCheckpoint,
			   sizeof(aCheckpoint),
			   Localize("CP %d/%d"),
			   pRace->m_Checkpoint,
			   m_pClient->m_Snap.m_pRaceInfo->m_NumCheckpoints);

	const float Half = 300.0f * Graphics()->ScreenAspect() / 2.0f;
	const float TimeSize = 10.0f;
	const float CpSize = 6.0f;
	vec4 Accent = CMenus::ThemeAccent();
	TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
	TextRender()->Text(
		0, Half - TextRender()->TextWidth(0, TimeSize, aTime, -1) / 2.0f, 2.0f, TimeSize, aTime, -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.85f);
	TextRender()->Text(
		0, Half - TextRender()->TextWidth(0, CpSize, aCheckpoint, -1) / 2.0f, 13.0f, CpSize, aCheckpoint, -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CHud::RenderPauseNotification()
{
	if(m_aStatusAppear[STATUS_STACK_PAUSED] > 0.01f)
	{
		const char *pText = Localize("Game paused");
		RenderStatusNotice(
			pText, StatusStackY(STATUS_STACK_PAUSED), CMenus::ThemeAccent(), m_aStatusAppear[STATUS_STACK_PAUSED]);
	}
}

void CHud::RenderSuddenDeath()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = 300.0f * Graphics()->ScreenAspect() / 2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->Text(0, Half - w / 2, 2, FontSize, pText, -1);
	}
}

void CHud::RenderObjective()
{
	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;
	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP) || !m_pClient->m_Snap.m_pGameDataObj)
		return;

	const int Quest = m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed;
	const int QuestProgressCounter = max(0, m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);
	const int Level = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
	const int Pack = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue;
	if(!Quest)
	{
		// Between objectives: keep the panel alive with level/theme + stand-by.
		const int ThemeGap = Pack & 0xF;
		const int GapSignature = Level * 131 + ThemeGap;
		if(GapSignature != m_LastObjectiveSignature)
		{
			m_LastObjectiveSignature = GapSignature;
			// Stand-by is not a new objective — keep the panel, skip the slide-in.
			m_ObjectiveNoticeUntil = time_get() + time_freq() * 4;
		}
		m_LastObjectiveQuest = QUEST_NONE;

		const bool ScoreboardVisible = m_pClient->m_pScoreboard->Active();
		if((g_Config.m_ClPveObjectiveDisplay == 0 && !ScoreboardVisible) ||
		   (g_Config.m_ClPveObjectiveDisplay == 2 && time_get() >= m_ObjectiveNoticeUntil) ||
		   m_pClient->m_pPveRoguelite->ChoiceActive() || m_pClient->m_pVoting->IsVoting())
			return;

		char aMeta[128];
		char aQuest[128];
		str_format(
			aMeta, sizeof(aMeta), "%s %d · %s", Localize("Level"), Level, Localize(GetThemeDisplayName(ThemeGap)));
		str_copy(aQuest, Localize("Stand by"), sizeof(aQuest));

		const float MetaSize = 5.0f;
		const float QuestSize = 7.0f;
		float NaturalTextWidth = TextRender()->TextWidth(0, MetaSize, aMeta, -1);
		NaturalTextWidth = max(NaturalTextWidth, TextRender()->TextWidth(0, QuestSize, aQuest, -1));
		const float MaxCardWidth = min(112.0f, m_Width * 0.24f);
		const float CardWidth = clamp(NaturalTextWidth + 18.0f, 72.0f, MaxCardWidth);
		const float CardHeight = 27.0f;
		const float ObjectiveAge = m_ObjectiveTransitionStart
									   ? (float)((time_get() - m_ObjectiveTransitionStart) / (double)time_freq())
									   : 1.0f;
		const float ObjectiveIn = clamp(ObjectiveAge / 0.22f, 0.0f, 1.0f);
		const float ObjectiveEased = 1.0f - (1.0f - ObjectiveIn) * (1.0f - ObjectiveIn) * (1.0f - ObjectiveIn);
		const float CardRight = m_Width - 6.0f + (1.0f - ObjectiveEased) * 10.0f;
		CUIRect Shadow = {CardRight - CardWidth + 1.2f, HudLayout::ObjectiveTop + 1.2f, CardWidth, CardHeight};
		CUIRect Card = {CardRight - CardWidth, HudLayout::ObjectiveTop, CardWidth, CardHeight};
		const vec4 Panel = CMenus::ThemeBgPanel();
		const vec4 Inset = CMenus::ThemeBgInset();
		const vec4 Accent = CMenus::ThemeAccent();
		const vec4 AccentDim = CMenus::ThemeAccentDim();
		const vec4 Text = CMenus::ThemeText();
		RenderTools()->DrawUIRect(&Shadow, vec4(0, 0, 0, 0.32f), CUI::CORNER_ALL, 6.0f);
		RenderTools()->DrawUIRect(&Card, vec4(Panel.r, Panel.g, Panel.b, 0.90f), CUI::CORNER_ALL, 6.0f);
		CUIRect CardInset = {Card.x + 3.0f, Card.y + 3.0f, Card.w - 6.0f, Card.h - 6.0f};
		RenderTools()->DrawUIRect(&CardInset, vec4(Inset.r, Inset.g, Inset.b, 0.32f), CUI::CORNER_ALL, 4.0f);
		CUIRect Edge = {Card.x, Card.y + 6.0f, 2.0f, Card.h - 12.0f};
		RenderTools()->DrawUIRect(&Edge, vec4(Accent.r, Accent.g, Accent.b, 0.92f), CUI::CORNER_ALL, 1.0f);
		const float TextRight = Card.x + Card.w - 7.0f;
		const float TextLeft = Card.x + 8.0f;
		const float MaxTextWidth = TextRight - TextLeft;
		auto DrawRight = [&](float Y, float FontSize, float MinFontSize, const char *pText, vec4 Color)
		{
			while(FontSize > MinFontSize && TextRender()->TextWidth(0, FontSize, pText, -1) > MaxTextWidth)
				FontSize -= 0.25f;
			const float Width = TextRender()->TextWidth(0, FontSize, pText, -1);
			TextRender()->TextColor(Color.r, Color.g, Color.b, 1.0f);
			TextRender()->Text(0, max(TextLeft, TextRight - Width), Y, FontSize, pText, -1);
		};
		DrawRight(Card.y + 5.0f, MetaSize, 4.0f, aMeta, AccentDim);
		DrawRight(Card.y + 13.0f, QuestSize, 5.0f, aQuest, Text);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}

	const int Theme = Pack & 0xF;
	const int WaveType = (Pack >> 4) & 0xF;
	const int QuestsDone = (Pack >> 8) & 0xF;
	const int QuestsTotal = (Pack >> 12) & 0xF;
	const int ExtractStage = (Pack >> 8) & 0xFF;

	// Only stable objective content belongs in the signature. In particular,
	// timers, kills and remaining-enemy counters must not turn the transient
	// mode into a permanently visible panel — except Defend/timed survive where
	// the countdown is the objective itself.
	int ObjectiveSignature = Quest * 31 + Level * 131;
	if(Quest == QUEST_EXTRACT)
		ObjectiveSignature = ObjectiveSignature * 31 + ExtractStage;
	else if(Quest == QUEST_DEFEND || Quest == QUEST_SURVIVEWAVETIME || Quest == QUEST_HOLD_ZONE ||
		Quest == QUEST_PUSH_FORWARD)
		ObjectiveSignature =
			(((ObjectiveSignature * 31 + Theme) * 31 + WaveType) * 31 + QuestsDone) * 31 + QuestProgressCounter;
	else if(Quest != QUEST_HORDE)
		ObjectiveSignature = (((ObjectiveSignature * 31 + Theme) * 31 + WaveType) * 31 + QuestsDone) * 31 + QuestsTotal;
	if(ObjectiveSignature != m_LastObjectiveSignature)
	{
		// Slide-in only when an objective appears after having none. Progress
		// ticks (defend / timed survive countdown) still refresh notice mode.
		const bool PlayEntranceAnim = (m_LastObjectiveQuest == QUEST_NONE);
		m_LastObjectiveSignature = ObjectiveSignature;
		if(PlayEntranceAnim)
			m_ObjectiveTransitionStart = time_get();
		m_ObjectiveNoticeUntil = time_get() + time_freq() * 4;
	}
	m_LastObjectiveQuest = Quest;

	const bool ScoreboardVisible = m_pClient->m_pScoreboard->Active();
	if((g_Config.m_ClPveObjectiveDisplay == 0 && !ScoreboardVisible) ||
	   (g_Config.m_ClPveObjectiveDisplay == 2 && time_get() >= m_ObjectiveNoticeUntil) ||
	   m_pClient->m_pPveRoguelite->ChoiceActive() || m_pClient->m_pVoting->IsVoting())
		return;

	char aMeta[128];
	char aQuest[128];
	char aProgress[160];
	aProgress[0] = 0;

	if(Quest == QUEST_HORDE)
		str_format(aMeta, sizeof(aMeta), "%s %d · %d %s", Localize("Wave"), Level, Pack, Localize("kills"));
	else if(Quest == QUEST_EXTRACT)
		str_format(aMeta, sizeof(aMeta), "%d %s", Level, Localize("seconds remaining"));
	else
		str_format(aMeta, sizeof(aMeta), "%s %d · %s", Localize("Level"), Level, Localize(GetThemeDisplayName(Theme)));

	const char *pWave = GetWaveDisplayName(WaveType);
	if(pWave[0] &&
	   (Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME || Quest == QUEST_KILLREMAININGENEMIES))
		str_format(aQuest, sizeof(aQuest), "%s (%s)", Localize(GetQuestDisplayName(Quest)), Localize(pWave));
	else
		str_copy(aQuest, Localize(GetQuestDisplayName(Quest)), sizeof(aQuest));
	if(Quest == QUEST_EXTRACT && ExtractStage >= 1)
		str_copy(aQuest, Localize("Reach the door"), sizeof(aQuest));

	if(Quest == QUEST_REACHDOOR && m_pClient->SurvivalAcid())
		str_copy(aProgress, Localize("Rising acid"), sizeof(aProgress));
	else
	{
		char aDetail[96];
		aDetail[0] = 0;
		if(Quest == QUEST_KILLREMAININGENEMIES || Quest == QUEST_SURVIVEWAVE || Quest == QUEST_KILL_BOSS ||
		   Quest == QUEST_HORDE)
		{
			const char *pText = Quest == QUEST_KILL_BOSS ? Localize("bosses remaining") : Localize("enemies remaining");
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, pText);
		}
		else if(Quest == QUEST_SURVIVEWAVETIME || Quest == QUEST_DEFEND || Quest == QUEST_HOLD_ZONE)
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, Localize("seconds remaining"));
		else if(Quest == QUEST_PUSH_FORWARD)
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, Localize("forward points remaining"));
		else if(Quest == QUEST_ACTIVATE_SWITCHES || Quest == QUEST_FIND_SWITCH)
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, Localize("switches remaining"));
		else if(Quest == QUEST_EXTRACT && ExtractStage == 0)
		{
			// Task type is packed into the WaveType bits by the extract
			// controller: 1=switches, 2=eliminate, 3=defend, 4=collect, 5=timed.
			const char *pRemaining = "enemies remaining";
			if(WaveType == 1)
				pRemaining = "switches remaining";
			else if(WaveType == 3)
				pRemaining = "seconds remaining";
			else if(WaveType == 4)
				pRemaining = "supplies remaining";
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, Localize(pRemaining));
		}
		else if(Quest == QUEST_EXTRACT)
			str_format(aDetail, sizeof(aDetail), "%d %s", QuestProgressCounter, Localize("to evacuate"));

		if(QuestsTotal > 0 && Quest != QUEST_REACHDOOR && Quest != QUEST_HORDE && Quest != QUEST_EXTRACT)
		{
			char aStep[48];
			str_format(
				aStep, sizeof(aStep), Localize("Objective %d/%d"), min(QuestsDone + 1, QuestsTotal), QuestsTotal);
			if(aDetail[0])
				str_format(aProgress, sizeof(aProgress), "%s · %s", aStep, aDetail);
			else
				str_copy(aProgress, aStep, sizeof(aProgress));
		}
		else if(aDetail[0])
			str_copy(aProgress, aDetail, sizeof(aProgress));
	}

	const float MetaSize = 5.0f;
	const float QuestSize = 7.0f;
	const float ProgressSize = 5.5f;
	float NaturalTextWidth = TextRender()->TextWidth(0, MetaSize, aMeta, -1);
	NaturalTextWidth = max(NaturalTextWidth, TextRender()->TextWidth(0, QuestSize, aQuest, -1));
	if(aProgress[0])
		NaturalTextWidth = max(NaturalTextWidth, TextRender()->TextWidth(0, ProgressSize, aProgress, -1));

	const float MaxCardWidth = min(112.0f, m_Width * 0.24f);
	const float CardWidth = clamp(NaturalTextWidth + 18.0f, 72.0f, MaxCardWidth);
	const float CardHeight = aProgress[0] ? 35.0f : 27.0f;
	const float ObjectiveAge =
		m_ObjectiveTransitionStart ? (float)((time_get() - m_ObjectiveTransitionStart) / (double)time_freq()) : 1.0f;
	const float ObjectiveIn = clamp(ObjectiveAge / 0.22f, 0.0f, 1.0f);
	const float ObjectiveEased = 1.0f - (1.0f - ObjectiveIn) * (1.0f - ObjectiveIn) * (1.0f - ObjectiveIn);
	const float CardRight = m_Width - 6.0f + (1.0f - ObjectiveEased) * 10.0f;
	CUIRect Shadow = {CardRight - CardWidth + 1.2f, HudLayout::ObjectiveTop + 1.2f, CardWidth, CardHeight};
	CUIRect Card = {CardRight - CardWidth, HudLayout::ObjectiveTop, CardWidth, CardHeight};
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Text = CMenus::ThemeText();
	RenderTools()->DrawUIRect(&Shadow, vec4(0, 0, 0, 0.32f), CUI::CORNER_ALL, 6.0f);
	RenderTools()->DrawUIRect(&Card, vec4(Panel.r, Panel.g, Panel.b, 0.90f), CUI::CORNER_ALL, 6.0f);
	CUIRect CardInset = {Card.x + 3.0f, Card.y + 3.0f, Card.w - 6.0f, Card.h - 6.0f};
	RenderTools()->DrawUIRect(&CardInset, vec4(Inset.r, Inset.g, Inset.b, 0.32f), CUI::CORNER_ALL, 4.0f);
	CUIRect Edge = {Card.x, Card.y + 6.0f, 2.0f, Card.h - 12.0f};
	RenderTools()->DrawUIRect(&Edge, vec4(Accent.r, Accent.g, Accent.b, 0.92f), CUI::CORNER_ALL, 1.0f);

	const float TextRight = Card.x + Card.w - 7.0f;
	const float TextLeft = Card.x + 8.0f;
	const float MaxTextWidth = TextRight - TextLeft;
	auto DrawRight = [&](float Y, float FontSize, float MinFontSize, const char *pText, vec4 Color)
	{
		while(FontSize > MinFontSize && TextRender()->TextWidth(0, FontSize, pText, -1) > MaxTextWidth)
			FontSize -= 0.25f;
		const float Width = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->TextColor(Color.r, Color.g, Color.b, 1.0f);
		TextRender()->Text(0, max(TextLeft, TextRight - Width), Y, FontSize, pText, -1);
	};

	DrawRight(Card.y + 5.0f, MetaSize, 4.0f, aMeta, AccentDim);
	DrawRight(Card.y + 13.0f, QuestSize, 5.0f, aQuest, Text);
	if(aProgress[0])
		DrawRight(Card.y + 24.0f,
				  ProgressSize,
				  4.0f,
				  aProgress,
				  Quest == QUEST_REACHDOOR && m_pClient->SurvivalAcid() ? Accent : Text);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CHud::RenderPveEnvironment()
{
	if(m_pClient->PveEnvironmentBiome() == PVE_BIOME_NONE || !g_Config.m_ClShowhud)
		return;
	const char *pEnvironment = "Tide: Calm";
	vec4 Accent(0.35f, 0.95f, 1.0f, 1.0f);
	switch(m_pClient->PveEnvironmentBiome())
	{
		case PVE_BIOME_CITY_LOCKDOWN:
			pEnvironment = "Lockdown: Sector breach";
			Accent = vec4(1.0f, 0.45f, 0.25f, 1.0f);
			break;
		case PVE_BIOME_CITY_BLACKOUT:
			pEnvironment = "Blackout: Silent overload";
			Accent = vec4(0.55f, 0.65f, 1.0f, 1.0f);
			break;
		case PVE_BIOME_COLLAPSE_RETREAT:
			pEnvironment = "Collapse Retreat: Keep moving";
			Accent = vec4(1.0f, 0.30f, 0.20f, 1.0f);
			break;
		case PVE_BIOME_VERTICAL_RUINS:
			pEnvironment = "Vertical Ruins: Climb route";
			Accent = vec4(0.75f, 0.55f, 0.95f, 1.0f);
			break;
		case PVE_BIOME_STORM_FRONT:
			pEnvironment = "Storm Front: Cross the front";
			Accent = vec4(0.45f, 0.75f, 1.0f, 1.0f);
			break;
		case PVE_BIOME_ORBITAL:
			pEnvironment = "Orbital: Airlock route";
			Accent = vec4(0.45f, 0.9f, 0.8f, 1.0f);
			break;
		default:
			break;
	}
	char aText[96];
	if(m_pClient->PveEnvironmentBiome() == PVE_BIOME_BLUE_PLANET)
	{
		switch(m_pClient->PveEnvironmentPhase())
		{
			case PVE_ENV_PHASE_WARNING:
				pEnvironment = "Tide: Warning";
				Accent = vec4(1.0f, 0.78f, 0.25f, 1.0f);
				break;
			case PVE_ENV_PHASE_DARK:
				pEnvironment = "Tide: Dark";
				Accent = vec4(0.35f, 0.35f, 0.55f, 1.0f);
				break;
			case PVE_ENV_PHASE_RECOVERY:
				pEnvironment = "Tide: Recovery";
				Accent = vec4(0.55f, 0.9f, 1.0f, 1.0f);
				break;
			default: break;
		}
		const int Seconds = max(0, (m_pClient->PveEnvironmentPhaseEndTick() - Client()->GameTick()) /
			max(1, Client()->GameTickSpeed()));
		str_format(aText, sizeof(aText), "%s  %ds", Localize(pEnvironment), Seconds);
	}
	else
		str_copy(aText, Localize(pEnvironment), sizeof(aText));
	const float Width = min(170.0f, m_Width * 0.45f);
	// The team-contract HUD occupies the left column at y=112..147.
	CUIRect Panel = {6.0f, 151.0f, Width, 17.0f};
	RenderTools()->DrawUIRect(&Panel, vec4(0.03f, 0.10f, 0.14f, 0.82f), CUI::CORNER_ALL, 4.0f);
	CUIRect Edge = {Panel.x, Panel.y + 3.0f, 2.0f, Panel.h - 6.0f};
	RenderTools()->DrawUIRect(&Edge, Accent, CUI::CORNER_ALL, 1.0f);
	TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
	TextRender()->Text(0, Panel.x + 7.0f, Panel.y + 4.0f, 5.0f, aText, -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CHud::RenderScoreHud()
{
	if(!g_Config.m_ClShowhudScore || m_pClient->m_pScoreboard->Active() || m_pClient->m_pVoting->IsVoting())
		return;
	// render small score hud
	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		int GameFlags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		float Whole = 300 * Graphics()->ScreenAspect();
		const float StartY = ScoreHudTop();

		if(GameFlags & GAMEFLAG_TEAMS && !(GameFlags & GAMEFLAG_INFECTION) && m_pClient->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][32];
			str_format(
				aScoreTeam[TEAM_RED], sizeof(aScoreTeam) / 2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(
				aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam) / 2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);
			int FlagCarrier[2] = {m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed,
								  m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue};
			const vec4 Panel = CMenus::ThemeBgPanel();
			const float CardWidth = 76.0f;
			const float CardHeight = 17.0f;
			const float RowGap = 2.0f;
			const float CardX = Whole - CardWidth - 5.0f;
			for(int t = 0; t < 2; t++)
			{
				const float Y = StartY + t * (CardHeight + RowGap);
				const vec4 TeamColor =
					t == TEAM_RED ? vec4(0.92f, 0.24f, 0.30f, 1.0f) : vec4(0.26f, 0.42f, 0.92f, 1.0f);
				CUIRect Shadow = {CardX + 1.0f, Y + 1.5f, CardWidth, CardHeight};
				CUIRect Card = {CardX, Y, CardWidth, CardHeight};
				RenderTools()->DrawUIRect(&Shadow, vec4(0, 0, 0, 0.34f), CUI::CORNER_L, 6.0f);
				RenderTools()->DrawUIRect(&Card, vec4(Panel.r, Panel.g, Panel.b, 0.90f), CUI::CORNER_L, 6.0f);
				CUIRect TeamBadge = {Card.x + 3.0f, Card.y + 2.0f, 14.0f, Card.h - 4.0f};
				RenderTools()->DrawUIRect(
					&TeamBadge, vec4(TeamColor.r, TeamColor.g, TeamColor.b, 0.54f), CUI::CORNER_ALL, 5.0f);
				CUIRect TeamEdge = {Card.x, Card.y + 5.0f, 2.0f, Card.h - 10.0f};
				RenderTools()->DrawUIRect(&TeamEdge, TeamColor, CUI::CORNER_ALL, 1.0f);
				const float ScoreWidth = max(16.0f, TextRender()->TextWidth(0, 8.0f, aScoreTeam[t], -1));
				const float ScoreX = Card.x + Card.w - ScoreWidth - 4.0f;
				TextRender()->Text(0, ScoreX, Card.y + 4.0f, 8.0f, aScoreTeam[t], -1);
				const char *pLabel = Localize(t == TEAM_RED ? "Red team" : "Blue team");
				if(GameFlags & GAMEFLAG_FLAGS)
				{
					int BlinkTimer =
						(m_pClient->m_FlagDropTick[t] != 0 &&
						 (Client()->GameTick() - m_pClient->m_FlagDropTick[t]) / Client()->GameTickSpeed() >= 25)
							? 10
							: 20;
					if(FlagCarrier[t] == FLAG_ATSTAND ||
					   (FlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick() / BlinkTimer) & 1)))
					{
						// draw flag
						Graphics()->BlendNormal();
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
						Graphics()->QuadsBegin();
						RenderTools()->SelectSprite(t == 0 ? SPRITE_FLAG_RED : SPRITE_FLAG_BLUE);
						IGraphics::CQuadItem QuadItem(TeamBadge.x + 2.0f, TeamBadge.y + 1.0f, 5.0f, 11.0f);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
						Graphics()->QuadsEnd();
					}
					else if(FlagCarrier[t] >= 0)
					{
						const int ID = FlagCarrier[t] % MAX_CLIENTS;
						if(ID >= 0 && ID < MAX_CLIENTS && m_pClient->m_aClients[ID].m_aName[0])
							pLabel = m_pClient->m_aClients[ID].m_aName;
					}
				}
				else
					UI()->DoLabel(&TeamBadge, t == TEAM_RED ? "R" : "B", 5.5f, 0);
				const float LabelX = Card.x + 20.0f;
				const float LabelWidth = max(8.0f, ScoreX - LabelX - 4.0f);
				float LabelSize = 6.0f;
				while(LabelSize > 4.5f && TextRender()->TextWidth(0, LabelSize, pLabel, -1) > LabelWidth)
					LabelSize -= 0.25f;
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor,
										LabelX,
										Card.y + (Card.h - LabelSize) * 0.5f - 0.5f,
										LabelSize,
										TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = LabelWidth;
				Cursor.m_MaxLines = 1;
				TextRender()->TextEx(&Cursor, pLabel, -1);
			}
		}
		// dm, infection, co-op
		else
		{
			const CNetObj_PlayerInfo *apPlayerInfo[2] = {0, 0};
			int aPosition[2] = {0, 0};
			const CNetObj_PlayerInfo *pLocalInfo = 0;
			int LocalPosition = 0;
			int NumRows = 0;
			int Position = 0;
			for(int i = 0; i < MAX_CHARACTERS && m_pClient->m_Snap.m_paInfoByScore[i]; i++)
			{
				const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];
				if(pInfo->m_Team == TEAM_SPECTATORS)
					continue;
				const int ClientID = pInfo->m_ClientID;
				CGameClient::CClientData *pClient = m_pClient->ClientData(ClientID);
				if(!pClient)
					continue;
				if((GameFlags & GAMEFLAG_COOP) && !m_pClient->m_Snap.m_pRaceInfo && pClient->m_IsBot)
					continue;
				Position++;
				if(NumRows < 2)
				{
					apPlayerInfo[NumRows] = pInfo;
					aPosition[NumRows] = Position;
					NumRows++;
				}
				if(ClientID == m_pClient->m_Snap.m_LocalClientID)
				{
					pLocalInfo = pInfo;
					LocalPosition = Position;
				}
			}
			if(pLocalInfo && LocalPosition > 2)
			{
				const int Row = NumRows < 2 ? NumRows++ : 1;
				apPlayerInfo[Row] = pLocalInfo;
				aPosition[Row] = LocalPosition;
			}

			const vec4 Accent = CMenus::ThemeAccent();
			const vec4 Panel = CMenus::ThemeBgPanel();
			const vec4 Inset = CMenus::ThemeBgInset();
			const bool Race = m_pClient->m_Snap.m_pRaceInfo != 0;
			const float CardWidth = Race ? clamp(Whole * 0.23f, 104.0f, 116.0f) : clamp(Whole * 0.20f, 82.0f, 96.0f);
			const float CardHeight = 17.0f;
			const float RowGap = 2.0f;
			const float CardX = Whole - CardWidth - 5.0f;
			for(int Row = 0; Row < NumRows; Row++)
			{
				const CNetObj_PlayerInfo *pInfo = apPlayerInfo[Row];
				if(!pInfo)
					continue;
				const int ID = pInfo->m_ClientID;
				const bool Local = ID == m_pClient->m_Snap.m_LocalClientID;
				const float Y = StartY + Row * (CardHeight + RowGap);
				CUIRect Shadow = {CardX + 1.0f, Y + 1.5f, CardWidth, CardHeight};
				CUIRect Card = {CardX, Y, CardWidth, CardHeight};
				RenderTools()->DrawUIRect(&Shadow, vec4(0, 0, 0, 0.34f), CUI::CORNER_L, 6.0f);
				RenderTools()->DrawUIRect(&Card,
										  Local ? vec4(Accent.r, Accent.g, Accent.b, 0.32f)
												: vec4(Panel.r, Panel.g, Panel.b, 0.88f),
										  CUI::CORNER_L,
										  6.0f);
				CUIRect Rank = {Card.x + 3.0f, Card.y + 2.0f, 14.0f, Card.h - 4.0f};
				RenderTools()->DrawUIRect(&Rank, vec4(Inset.r, Inset.g, Inset.b, 0.88f), CUI::CORNER_ALL, 5.0f);
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%d.", aPosition[Row]);
				UI()->DoLabel(&Rank, aBuf, 6.0f, 0);

				CGameClient::CClientData *pClient = m_pClient->ClientData(ID);
				if(!pClient)
					continue;
				CTeeRenderInfo Info = pClient->m_RenderInfo;
				Info.m_Size = 11.5f;
				RenderTools()->RenderPortrait(
					&Info, vec2(Card.x + 24.0f, Card.y + Card.h * 0.5f + Info.m_Size * 0.55f + 1.5f), 0);

				char aScore[32];
				if(Race)
					CGameClient::FormatRaceTime(m_pClient->RaceTime(ID), aScore, sizeof(aScore));
				else
					str_format(aScore, sizeof(aScore), "%d", pInfo->m_Score);
				const float ScoreWidth = max(16.0f, TextRender()->TextWidth(0, 8.0f, aScore, -1));
				const float ScoreX = Card.x + Card.w - ScoreWidth - 4.0f;
				TextRender()->Text(0, ScoreX, Card.y + 4.0f, 8.0f, aScore, -1);

				const char *pName = pClient->m_aName;
				const float NameX = Card.x + 32.0f;
				const float NameWidth = max(8.0f, ScoreX - NameX - 4.0f);
				float NameSize = 6.5f;
				while(NameSize > 4.5f && TextRender()->TextWidth(0, NameSize, pName, -1) > NameWidth)
					NameSize -= 0.25f;
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor,
										NameX,
										Card.y + (Card.h - NameSize) * 0.5f - 0.5f,
										NameSize,
										TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = NameWidth;
				Cursor.m_MaxLines = 1;
				TextRender()->TextEx(&Cursor, pName, -1);
			}
		}
	}
}

void CHud::RenderStartCountdown()
{
	if(!g_Config.m_ClShowhudTimer)
		return;

	if(!WarmupActive())
		return;

	const char *pLabel = Localize("Warmup");
	char aBuf[32];
	int Seconds = DebugStatusActive(DEBUG_STATUS_WARMUP)
					  ? 12
					  : m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer / SERVER_TICK_SPEED;
	if(Seconds < 5)
		str_format(aBuf,
				   sizeof(aBuf),
				   "%d.%d",
				   Seconds,
				   (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer * 10 / SERVER_TICK_SPEED) % 10);
	else
		str_format(aBuf, sizeof(aBuf), "%d", Seconds);
	const float LabelSize = 7.0f;
	const float CountSize = 15.0f;
	const float LabelW = TextRender()->TextWidth(0, LabelSize, pLabel, -1);
	const float CountW = TextRender()->TextWidth(0, CountSize, aBuf, -1);
	const float BoxW = clamp(max(LabelW, CountW) + 30.0f, 78.0f, 112.0f);
	const float BoxH = 46.0f;
	const float x = (m_Width - BoxW) * 0.5f;
	const float y = 40.0f;
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.42f);
	RenderTools()->DrawRoundRect(x + 1.0f, y + 1.5f, BoxW, BoxH, 8.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.74f);
	RenderTools()->DrawRoundRect(x - 0.7f, y - 0.7f, BoxW + 1.4f, BoxH + 1.4f, 8.5f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.98f);
	RenderTools()->DrawRoundRect(x, y, BoxW, BoxH, 8.0f);
	Graphics()->SetColor(Inset.r, Inset.g, Inset.b, 0.52f);
	RenderTools()->DrawRoundRect(x + 6.0f, y + 6.0f, BoxW - 12.0f, BoxH - 12.0f, 6.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.98f);
	RenderTools()->DrawRoundRect(x, y + 7.0f, 2.0f, BoxH - 14.0f, 1.0f);
	Graphics()->QuadsEnd();
	TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
	TextRender()->Text(0, x + (BoxW - LabelW) * 0.5f, y + 7.0f, LabelSize, pLabel, -1);
	TextRender()->TextColor(Text.r, Text.g, Text.b, 1.0f);
	TextRender()->Text(0, x + (BoxW - CountW) * 0.5f, y + 19.0f, CountSize, aBuf, -1);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CHud::RenderReadyUpNotification()
{
	if(m_aStatusAppear[STATUS_STACK_READY] <= 0.01f)
		return;

	const char *pKey = m_pClient->m_pBinds->GetKey("ready_change");
	char aText[128];
	if(pKey[0])
		str_format(aText, sizeof(aText), Localize("When ready, press <%s>"), pKey);
	else
		str_copy(aText, Localize("Bind Ready in Settings"), sizeof(aText));

	RenderStatusNotice(
		aText, StatusStackY(STATUS_STACK_READY), CMenus::ThemeAccent(), m_aStatusAppear[STATUS_STACK_READY]);
}

void CHud::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX,
									CenterY,
									pGroup->m_ParallaxX / 100.0f,
									pGroup->m_ParallaxY / 100.0f,
									pGroup->m_OffsetX,
									pGroup->m_OffsetY,
									Graphics()->ScreenAspect(),
									m_pClient->m_pCamera->m_Zoom,
									Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CHud::RenderFps()
{
	if(g_Config.m_ClShowfps)
	{
		// calculate avg. fps
		float FPS = 1.0f / Client()->RenderFrameTime();
		m_AverageFPS = (m_AverageFPS * (1.0f - (1.0f / m_AverageFPS))) + (FPS * (1.0f / m_AverageFPS));
		char Buf[512];
		str_format(Buf, sizeof(Buf), "%d", (int)m_AverageFPS);
		const float Margin = HudLayout::SafeMargin(m_Width);
		TextRender()->Text(0, m_Width - Margin - TextRender()->TextWidth(0, 12, Buf, -1), Margin, 12, Buf, -1);
	}
}

void CHud::RenderConnectionWarning()
{
	if(m_aStatusAppear[STATUS_STACK_CONNECTION] > 0.01f)
	{
		const char *pText = Localize("Connection Problems...");
		RenderStatusNotice(pText,
						   StatusStackY(STATUS_STACK_CONNECTION),
						   CMenus::ThemeDanger(),
						   m_aStatusAppear[STATUS_STACK_CONNECTION]);
	}
}

void CHud::RenderTeambalanceWarning()
{
	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION)
		return;

	// render prompt about team-balance
	bool Flash = time_get() / (time_freq() / 2) % 2 == 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
	{
		int TeamDiff = m_pClient->m_Snap.m_aTeamSize[TEAM_RED] - m_pClient->m_Snap.m_aTeamSize[TEAM_BLUE];
		if(g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			vec4 Accent = Flash ? CMenus::ThemeAccent() : CMenus::ThemeAccentDim();
			TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
			TextRender()->Text(0x0, 5, 108, 6, pText, -1);
			TextRender()->TextColor(1, 1, 1, 1);
		}
	}
}

void CHud::RenderVoting()
{
	if(!m_pClient->m_pVoting->IsVoting() || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	const int SecondsLeft = m_pClient->m_pVoting->SecondsLeft();
	if(SecondsLeft < 0)
	{
		m_pClient->m_pVoting->Hide();
		return;
	}

	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Danger = CMenus::ThemeDanger();
	const float PanelW = clamp(m_Width * 0.42f, 154.0f, 192.0f);
	const float PanelH = 70.0f;
	const float PanelX = (m_Width - PanelW) * 0.5f;
	float PanelY = StatusStackY(STATUS_STACK_VOTE);
	if(!PausedNoticeActive() && !ReadyNoticeActive() && !ConnectionNoticeActive())
		PanelY += 12.0f;
	if(m_pClient->m_Snap.m_pGameInfoObj && (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP))
		PanelY = max(PanelY, 138.0f);

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.42f);
	RenderTools()->DrawRoundRect(PanelX + 1.5f, PanelY + 2.0f, PanelW, PanelH, 8.0f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.72f);
	RenderTools()->DrawRoundRect(PanelX - 0.8f, PanelY - 0.8f, PanelW + 1.6f, PanelH + 1.6f, 8.5f);
	Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.97f);
	RenderTools()->DrawRoundRect(PanelX, PanelY, PanelW, PanelH, 7.5f);
	Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.92f);
	RenderTools()->DrawRoundRect(PanelX, PanelY + 7.0f, 2.2f, PanelH - 14.0f, 1.1f);
	Graphics()->QuadsEnd();

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), Localize("%ds left"), SecondsLeft);
	const float TimerW = TextRender()->TextWidth(0, 6.0f, aBuf, -1) + 13.0f;
	CUIRect Timer = {PanelX + PanelW - TimerW - 7.0f, PanelY + 6.0f, TimerW, 13.0f};
	RenderTools()->DrawUIRect(&Timer, vec4(Inset.r, Inset.g, Inset.b, 0.94f), CUI::CORNER_ALL, 6.0f);
	UI()->DoLabel(&Timer, aBuf, 6.0f, 0);

	TextRender()->TextColor(Text.r, Text.g, Text.b, 1.0f);
	TextRender()->Text(0, PanelX + 8.0f, PanelY + 5.5f, 6.5f, Localize("Voting"), -1);
	CTextCursor Cursor;
	char aVoteDesc[512];
	m_pClient->SanitizeSocialString(m_pClient->m_pVoting->VoteDescription(), aVoteDesc, sizeof(aVoteDesc));
	float DescriptionSize = 7.0f;
	while(DescriptionSize > 5.0f && TextRender()->TextWidth(0, DescriptionSize, aVoteDesc, -1) > PanelW - 16.0f)
		DescriptionSize -= 0.25f;
	TextRender()->SetCursor(
		&Cursor, PanelX + 8.0f, PanelY + 18.0f, DescriptionSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = PanelW - 16.0f;
	Cursor.m_MaxLines = 1;
	TextRender()->TextEx(&Cursor, aVoteDesc, -1);

	char aReasonBuf[512];
	m_pClient->SanitizeSocialString(m_pClient->m_pVoting->VoteReason(), aReasonBuf, sizeof(aReasonBuf));
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("Reason:"), aReasonBuf);
	TextRender()->TextColor(Text.r, Text.g, Text.b, 0.72f);
	TextRender()->SetCursor(&Cursor, PanelX + 8.0f, PanelY + 28.0f, 5.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = PanelW - 16.0f;
	Cursor.m_MaxLines = 2;
	TextRender()->TextEx(&Cursor, aBuf, -1);

	CUIRect Bar = {PanelX + 8.0f, PanelY + 43.0f, PanelW - 16.0f, 4.0f};
	m_pClient->m_pVoting->RenderBars(Bar, false);

	char aYesKeys[64], aNoKeys[64];
	m_pClient->m_pBinds->GetKeys("vote yes", aYesKeys, sizeof(aYesKeys));
	m_pClient->m_pBinds->GetKeys("vote no", aNoKeys, sizeof(aNoKeys));
	const char *pYesKey = aYesKeys[0] ? aYesKeys : "—";
	const char *pNoKey = aNoKeys[0] ? aNoKeys : "—";
	CUIRect Yes = {PanelX + 8.0f, PanelY + 52.0f, (PanelW - 19.0f) * 0.5f, 12.0f};
	CUIRect No = {Yes.x + Yes.w + 3.0f, Yes.y, Yes.w, Yes.h};
	RenderTools()->DrawUIRect(&Yes, vec4(0.25f, 0.78f, 0.43f, 0.25f), CUI::CORNER_ALL, 5.0f);
	RenderTools()->DrawUIRect(&No, vec4(Danger.r, Danger.g, Danger.b, 0.25f), CUI::CORNER_ALL, 5.0f);
	str_format(aBuf, sizeof(aBuf), "%s  %s", pYesKey, Localize("Vote yes"));
	UI()->DoLabel(&Yes, aBuf, 5.5f, 0);
	str_format(aBuf, sizeof(aBuf), "%s  %s", pNoKey, Localize("Vote no"));
	UI()->DoLabel(&No, aBuf, 5.5f, 0);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CHud::RenderCursor()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(CustomStuff()->m_Inventory)
		return;

	int CursorWeapon = WEAPON_HAMMER;
	CWeaponSpec WeaponSpec;
	CResolvedWeaponProfile WeaponProfile;
	if(CWeaponCatalog::TryFromProtocol(m_pClient->m_Snap.m_pLocalCharacter->m_WeaponDefinitionId,
									   m_pClient->m_Snap.m_pLocalCharacter->m_WeaponLevel,
									   &WeaponSpec) &&
	   CWeaponCatalog::TryResolve(WeaponSpec, &WeaponProfile))
		CursorWeapon = WeaponProfile.m_Combat.m_CursorWeapon;

	MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[CursorWeapon].m_pSpriteCursor);
	float CursorSize = 64;
	RenderTools()->DrawSprite(m_pClient->m_pControls->m_TargetPos.x, m_pClient->m_pControls->m_TargetPos.y, CursorSize);
	Graphics()->QuadsEnd();

	const int64 Now = time_get();
	if(g_Config.m_ClHitFeedback <= 0 || Now >= m_HitMarkerUntil)
		return;

	const float Strength = g_Config.m_ClHitFeedback / 100.0f;
	const float Duration = (m_HitKilled ? 220.0f : 120.0f) / 1000.0f;
	const float Remaining = clamp((m_HitMarkerUntil - Now) / (float)time_freq() / Duration, 0.0f, 1.0f);
	const float Expansion = m_HitKilled ? (1.0f - Remaining) * 7.0f : (1.0f - Remaining) * 2.0f;
	const float Gap = 9.0f + Expansion;
	const float Length = (m_HitKilled ? 11.0f : 8.0f) * (0.75f + Strength * 0.25f);
	const vec2 Center = m_pClient->m_pControls->m_TargetPos;
	IGraphics::CLineItem aLines[4] = {
		IGraphics::CLineItem(Center.x - Gap - Length, Center.y - Gap - Length, Center.x - Gap, Center.y - Gap),
		IGraphics::CLineItem(Center.x + Gap, Center.y + Gap, Center.x + Gap + Length, Center.y + Gap + Length),
		IGraphics::CLineItem(Center.x + Gap, Center.y - Gap, Center.x + Gap + Length, Center.y - Gap - Length),
		IGraphics::CLineItem(Center.x - Gap - Length, Center.y + Gap + Length, Center.x - Gap, Center.y + Gap),
	};

	vec4 Color(1.0f, 1.0f, 1.0f, Remaining * Strength);
	if(m_HitKilled)
		Color = vec4(1.0f, 0.18f, 0.12f, Remaining * Strength);
	else if(m_HitTargetType == HIT_TARGET_METAL)
		Color = vec4(1.0f, 0.64f, 0.12f, Remaining * Strength);
	else if(m_HitTargetType == HIT_TARGET_SHIELD)
		Color = vec4(0.2f, 0.82f, 1.0f, Remaining * Strength);

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	Graphics()->LinesDraw(aLines, 4);
	Graphics()->LinesEnd();
}

void CHud::DrawCircular(float x, float y, float r, int Segments, int FillAmount, int Max, bool Flip)
{
	float AOff = -pi / 2;
	if(Flip)
		AOff = pi / 2;

	IGraphics::CFreeformItem Array[32];
	int NumItems = 0;
	float FSegments = (float)Segments;
	for(int i = 0; i < Segments; i += 2)
	{
		if((i * Max) / FSegments < FillAmount)
			continue;

		float a1 = i / FSegments * 1 * pi + AOff;
		float a2 = (i + 1) / FSegments * 1 * pi + AOff;
		float a3 = (i + 2) / FSegments * 1 * pi + AOff;
		float Ca1 = cosf(a1);
		float Ca2 = cosf(a2);
		float Ca3 = cosf(a3);
		float Sa1 = sinf(a1);
		float Sa2 = sinf(a2);
		float Sa3 = sinf(a3);

		if(!Flip)
		{
			Array[NumItems++] = IGraphics::CFreeformItem(
				x, y, x + Ca1 * r, y + Sa1 * r, x + Ca3 * r, y + Sa3 * r, x + Ca2 * r, y + Sa2 * r);
		}
		else
		{
			Array[NumItems++] = IGraphics::CFreeformItem(
				x, y, x + Ca1 * r, y - Sa1 * r, x + Ca3 * r, y - Sa3 * r, x + Ca2 * r, y - Sa2 * r);
		}

		if(NumItems == 32)
		{
			m_pClient->Graphics()->QuadsDrawFreeform(Array, 32);
			NumItems = 0;
		}
	}
	if(NumItems)
		m_pClient->Graphics()->QuadsDrawFreeform(Array, NumItems);
}

void CHud::RenderLowHealthVignette(const CNetObj_Character *pCharacter)
{
	if(!g_Config.m_ClShowhudHealthAmmo || !pCharacter)
		return;
	const float DangerAmount = HudLayout::LowHealthAmount(pCharacter->m_Health);
	if(DangerAmount <= 0.0f)
		return;

	if(g_Config.m_GfxShaders && Graphics()->IsShaderAvailable(SHADER_LOW_HEALTH))
	{
		Graphics()->CameraToShaders(Graphics()->ScreenWidth(), Graphics()->ScreenHeight(), 0, 0);
		Graphics()->ShaderBegin(SHADER_LOW_HEALTH, DangerAmount);
		Graphics()->TextureSet(-1);
		Graphics()->BlendNormal();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		IGraphics::CQuadItem Quad(0.0f, 0.0f, m_Width, m_Height);
		Graphics()->QuadsDrawTL(&Quad, 1);
		Graphics()->QuadsEnd();
		Graphics()->ShaderEnd();
		return;
	}

	const float CriticalAmount = HudLayout::CriticalHealthAmount(pCharacter->m_Health);
	float EntryAmount = clamp(DangerAmount / 0.10f, 0.0f, 1.0f);
	EntryAmount = EntryAmount * EntryAmount * (3.0f - 2.0f * EntryAmount);
	const float Time = time_get() / (double)time_freq();
	const float Pulse = 0.72f + 0.28f * (0.5f + 0.5f * sinf(Time * (5.2f + CriticalAmount * 2.8f)));
	const float BaseAlpha = (0.16f + 0.35f * DangerAmount + 0.12f * CriticalAmount) * Pulse * EntryAmount;
	const vec4 Danger = CMenus::ThemeDanger();
	for(int Band = 0; Band < 8; ++Band)
	{
		const float Inset = Band * 3.2f;
		const float Alpha = BaseAlpha * (1.0f - Band * 0.105f);
		const vec4 Color(Danger.r, Danger.g, Danger.b, Alpha);
		const float Thickness = 4.0f + DangerAmount * 3.0f;
		CUIRect Top = {Inset, Inset, m_Width - Inset * 2.0f, Thickness};
		CUIRect Bottom = {Inset, m_Height - Inset - Thickness, m_Width - Inset * 2.0f, Thickness};
		CUIRect Left = {Inset, Inset, Thickness, m_Height - Inset * 2.0f};
		CUIRect Right = {m_Width - Inset - Thickness, Inset, Thickness, m_Height - Inset * 2.0f};
		RenderTools()->DrawUIRect(&Top, Color, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Bottom, Color, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Left, Color, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Right, Color, CUI::CORNER_ALL, 0.0f);
	}

	if(CriticalAmount > 0.0f)
	{
		const float TunnelAlpha = (0.12f + CriticalAmount * 0.18f) * Pulse;
		const float TunnelDepth = 24.0f + CriticalAmount * 28.0f;
		const vec4 TunnelColor(0.10f, 0.0f, 0.015f, TunnelAlpha);
		CUIRect Top = {0.0f, 0.0f, m_Width, TunnelDepth};
		CUIRect Bottom = {0.0f, m_Height - TunnelDepth, m_Width, TunnelDepth};
		CUIRect Left = {0.0f, TunnelDepth, TunnelDepth, m_Height - TunnelDepth * 2.0f};
		CUIRect Right = {m_Width - TunnelDepth, TunnelDepth, TunnelDepth, m_Height - TunnelDepth * 2.0f};
		RenderTools()->DrawUIRect(&Top, TunnelColor, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Bottom, TunnelColor, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Left, TunnelColor, CUI::CORNER_ALL, 0.0f);
		RenderTools()->DrawUIRect(&Right, TunnelColor, CUI::CORNER_ALL, 0.0f);
	}
}

void CHud::RenderHealthAndAmmo(const CNetObj_Character *pCharacter)
{
	if(!g_Config.m_ClShowhudHealthAmmo || !pCharacter)
		return;

	CWeaponSpec ActiveWeapon;
	CResolvedWeaponProfile ActiveProfile{};
	const bool HasActiveWeapon =
		CWeaponCatalog::TryFromProtocol(pCharacter->m_WeaponDefinitionId, pCharacter->m_WeaponLevel, &ActiveWeapon);
	const bool HasActiveProfile = HasActiveWeapon && CWeaponCatalog::TryResolve(ActiveWeapon, &ActiveProfile);
	const vec4 Deep = CMenus::ThemeBgDeep();
	const vec4 Panel = CMenus::ThemeBgPanel();
	const vec4 Inset = CMenus::ThemeBgInset();
	const vec4 Accent = CMenus::ThemeAccent();
	const vec4 AccentDim = CMenus::ThemeAccentDim();
	const vec4 Danger = CMenus::ThemeDanger();
	const vec4 Text = CMenus::ThemeText();
	const vec4 Muted = CMenus::ThemeTextMuted();
	const vec4 ArmorColor(0.96f, 0.80f, 0.16f, 1.0f);
	const vec4 OverlapColor(0.22f, 0.82f, 0.34f, 1.0f);

	auto Box = [this](const CUIRect &Rect, vec4 Color, float Radius)
	{
		RenderTools()->DrawUIRect(&Rect, Color, CUI::CORNER_ALL, Radius);
	};
	auto TechShape = [&](const CUIRect &Rect, vec4 Color, float Cut)
	{
		if(Rect.w <= 0.0f || Rect.h <= 0.0f)
			return;
		Cut = clamp(Cut, 0.0f, min(Rect.w, Rect.h) * 0.45f);
		IGraphics::CFreeformItem aParts[3] = {
			IGraphics::CFreeformItem(Rect.x,
				Rect.y + Cut,
				Rect.x + Cut,
				Rect.y,
				Rect.x,
				Rect.y + Rect.h - Cut,
				Rect.x + Cut,
				Rect.y + Rect.h),
			IGraphics::CFreeformItem(Rect.x + Cut,
				Rect.y,
				Rect.x + Rect.w - Cut,
				Rect.y,
				Rect.x + Cut,
				Rect.y + Rect.h,
				Rect.x + Rect.w - Cut,
				Rect.y + Rect.h),
			IGraphics::CFreeformItem(Rect.x + Rect.w - Cut,
				Rect.y,
				Rect.x + Rect.w,
				Rect.y + Cut,
				Rect.x + Rect.w - Cut,
				Rect.y + Rect.h,
				Rect.x + Rect.w,
				Rect.y + Rect.h - Cut)};
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
		Graphics()->QuadsDrawFreeform(aParts, 3);
		Graphics()->QuadsEnd();
	};
	auto SmokedGlass = [&](const CUIRect &Rect, vec4 EdgeColor, bool Active, float Cut)
	{
		CUIRect Shadow = {Rect.x + 0.7f, Rect.y + 1.1f, Rect.w, Rect.h};
		TechShape(Shadow, vec4(0.0f, 0.008f, 0.014f, 0.32f), Cut);
		EdgeColor.a = Active ? 0.48f : 0.24f;
		TechShape(Rect, EdgeColor, Cut);
		CUIRect Inner = {Rect.x + 0.8f, Rect.y + 0.8f, Rect.w - 1.6f, Rect.h - 1.6f};
		const vec4 SmokedFill(Deep.r * 0.58f + Panel.r * 0.42f,
			Deep.g * 0.58f + Panel.g * 0.42f,
			Deep.b * 0.58f + Panel.b * 0.42f,
			Active ? 0.62f : 0.50f);
		TechShape(Inner, SmokedFill, max(0.0f, Cut - 0.8f));
		IGraphics::CLineItem aSurfaceLines[4] = {
			IGraphics::CLineItem(Rect.x + Cut + 1.0f, Rect.y + 0.7f, Rect.x + Rect.w * 0.48f, Rect.y + 0.7f),
			IGraphics::CLineItem(Rect.x + Rect.w - Cut - 8.0f, Rect.y + Rect.h - 0.7f, Rect.x + Rect.w - Cut, Rect.y + Rect.h - 0.7f),
			IGraphics::CLineItem(Inner.x + Cut, Inner.y + Inner.h * 0.38f, Inner.x + Inner.w - Cut, Inner.y + Inner.h * 0.38f),
			IGraphics::CLineItem(Inner.x + Cut, Inner.y + Inner.h * 0.72f, Inner.x + Inner.w - Cut, Inner.y + Inner.h * 0.72f)};
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(0.66f, 0.86f, 0.92f, Active ? 0.18f : 0.10f);
		Graphics()->LinesDraw(aSurfaceLines, 1);
		Graphics()->SetColor(0.0f, 0.01f, 0.018f, 0.38f);
		Graphics()->LinesDraw(aSurfaceLines + 1, 1);
		Graphics()->SetColor(0.52f, 0.72f, 0.78f, 0.035f);
		Graphics()->LinesDraw(aSurfaceLines + 2, 2);
		Graphics()->LinesEnd();
	};
	auto Label = [&](const CUIRect &Rect, const char *pText, float Size, int Align, vec4 Color)
	{
		const float Width = TextRender()->TextWidth(0, Size, pText, -1);
		float X = Rect.x;
		if(Align == 0)
			X = Rect.x + (Rect.w - Width) * 0.5f;
		else if(Align > 0)
			X = Rect.x + Rect.w - Width;
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
		TextRender()->Text(0, X, Rect.y + (Rect.h - Size) * 0.5f - 0.5f, Size, pText, Rect.w);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	};
	auto Bar = [&](const CUIRect &Rect, float Amount, vec4 Color)
	{
		Box(Rect, vec4(0.0f, 0.010f, 0.016f, 0.90f), 0.0f);
		const float WellInset = Rect.h >= 4.0f ? 0.55f : 0.40f;
		CUIRect Well = {Rect.x + WellInset,
			Rect.y + WellInset,
			max(0.0f, Rect.w - WellInset * 2.0f),
			max(0.0f, Rect.h - WellInset * 2.0f)};
		Box(Well, vec4(Inset.r * 0.45f, Inset.g * 0.45f, Inset.b * 0.45f, 0.68f), 0.0f);
		Amount = clamp(Amount, 0.0f, 1.0f);
		if(Amount > 0.0f)
		{
			CUIRect Fill = Well;
			Fill.w *= Amount;
			Box(Fill, vec4(Color.r, Color.g, Color.b, 0.52f), 0.0f);
			const float CoreInset = min(0.65f, Fill.h * 0.22f);
			CUIRect EnergyCore = {Fill.x,
				Fill.y + CoreInset,
				Fill.w,
				max(0.45f, Fill.h - CoreInset * 2.0f)};
			Box(EnergyCore, vec4(Color.r, Color.g, Color.b, 0.92f), 0.0f);
			CUIRect Highlight = {Fill.x + 0.35f,
				Fill.y + 0.25f,
				max(0.0f, Fill.w - 0.70f),
				min(0.42f, max(0.25f, Fill.h * 0.16f))};
			Box(Highlight, vec4(0.90f, 0.97f, 1.0f, 0.14f), 0.0f);
		}
		if(Rect.h >= 4.5f)
		{
			for(int Segment = 1; Segment < 4; ++Segment)
			{
				CUIRect Tick = {Rect.x + Rect.w * Segment * 0.25f,
					Rect.y + 0.8f,
					0.35f,
					max(1.0f, Rect.h - 1.6f)};
				Box(Tick, vec4(0.0f, 0.012f, 0.018f, 0.48f), 0.0f);
			}
		}
	};
	auto RingSegment = [&](vec2 Center,
		float InnerRadius,
		float OuterRadius,
		float StartAmount,
		float EndAmount,
		vec4 Color)
	{
		const int Segments = 64;
		const float StartAngle = pi * 0.75f;
		const float AngleSpan = pi * 1.5f;
		StartAmount = clamp(StartAmount, 0.0f, 1.0f);
		EndAmount = clamp(EndAmount, 0.0f, 1.0f);
		if(EndAmount <= StartAmount)
			return;
		IGraphics::CFreeformItem aSegments[Segments];
		int Count = 0;
		for(int Segment = 0; Segment < Segments; ++Segment)
		{
			const float T0 = max(StartAmount, Segment / (float)Segments);
			const float T1 = min(EndAmount, (Segment + 1) / (float)Segments);
			if(T1 <= T0)
				continue;
			const float A0 = StartAngle + AngleSpan * T0;
			const float A1 = StartAngle + AngleSpan * T1;
			const vec2 Inner0 = Center + vec2(cosf(A0), sinf(A0)) * InnerRadius;
			const vec2 Outer0 = Center + vec2(cosf(A0), sinf(A0)) * OuterRadius;
			const vec2 Inner1 = Center + vec2(cosf(A1), sinf(A1)) * InnerRadius;
			const vec2 Outer1 = Center + vec2(cosf(A1), sinf(A1)) * OuterRadius;
			aSegments[Count++] = IGraphics::CFreeformItem(
				Inner0.x, Inner0.y, Outer0.x, Outer0.y, Inner1.x, Inner1.y, Outer1.x, Outer1.y);
		}
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
		Graphics()->QuadsDrawFreeform(aSegments, Count);
		Graphics()->QuadsEnd();
	};

	if(m_pClient->m_pControls->m_SignalWeapon >= 0)
	{
		CustomStuff()->m_WeaponSignalTimer = 1.0f;
		CustomStuff()->m_WeaponSignal = m_pClient->m_pControls->m_SignalWeapon;
		m_pClient->m_pControls->m_SignalWeapon = -1;
	}

	const float X = HudLayout::SafeMargin(m_Width);
	const float CoreY = HudLayout::VitalCoreTop(m_Height);
	const int Health = max(0, pCharacter->m_Health);
	const int Armor = max(0, pCharacter->m_Armor);
	const int Fuel = clamp(pCharacter->m_JetpackPower / 2, 0, 100);
	const vec4 HealthColor =
		Health <= 25 ? Danger : vec4(0.92f, 0.22f, 0.18f, 1.0f);
	const float HealthAmount = clamp(Health / 100.0f, 0.0f, 1.0f);
	const float ArmorAmount = clamp(Armor / 100.0f, 0.0f, 1.0f);
	const float CombinedAmount = HudLayout::VitalCombinedAmount(HealthAmount, ArmorAmount);
	const float HealthOnlyEnd = HudLayout::VitalHealthOnlyEnd(HealthAmount, ArmorAmount);
	const float DangerAmount = HudLayout::LowHealthAmount(Health);
	const float DangerPulse = 0.5f + 0.5f * sinf((float)Client()->LocalTime() * 6.0f);
	const vec4 CoreEdge(AccentDim.r + (Danger.r - AccentDim.r) * DangerAmount,
		AccentDim.g + (Danger.g - AccentDim.g) * DangerAmount,
		AccentDim.b + (Danger.b - AccentDim.b) * DangerAmount,
		1.0f);

	CUIRect VitalPanel = {X + 25.0f, CoreY + 2.0f, 151.0f, 46.0f};
	SmokedGlass(VitalPanel, CoreEdge, false, 5.0f);
	const vec2 CoreCenter(X + 27.0f, CoreY + 25.0f);
	CUIRect CoreGlow = {CoreCenter.x - 24.0f, CoreCenter.y - 24.0f, 48.0f, 48.0f};
	Box(CoreGlow,
		vec4(CoreEdge.r, CoreEdge.g, CoreEdge.b, DangerAmount * (0.08f + DangerPulse * 0.10f)),
		24.0f);
	CUIRect CoreMetal = {CoreCenter.x - 19.0f, CoreCenter.y - 19.0f, 38.0f, 38.0f};
	Box(CoreMetal, vec4(0.075f, 0.095f, 0.105f, 0.98f), 19.0f);
	CUIRect CoreDisc = {CoreCenter.x - 15.5f, CoreCenter.y - 15.5f, 31.0f, 31.0f};
	Box(CoreDisc, vec4(Deep.r * 0.72f, Deep.g * 0.72f, Deep.b * 0.72f, 0.98f), 15.5f);
	RingSegment(CoreCenter,
		18.2f,
		22.0f,
		0.0f,
		1.0f,
		vec4(0.16f, 0.19f, 0.20f, 0.98f));
	const float EnergyRingInnerRadius = 18.7f;
	const float EnergyRingOuterRadius = 21.5f;
	RingSegment(CoreCenter,
		EnergyRingInnerRadius,
		EnergyRingOuterRadius,
		0.0f,
		HealthOnlyEnd,
		vec4(HealthColor.r, HealthColor.g, HealthColor.b, 0.92f + DangerAmount * DangerPulse * 0.08f));
	RingSegment(CoreCenter,
		EnergyRingInnerRadius,
		EnergyRingOuterRadius,
		HealthOnlyEnd,
		HealthAmount,
		vec4(OverlapColor.r, OverlapColor.g, OverlapColor.b, 0.94f));
	RingSegment(CoreCenter,
		EnergyRingInnerRadius,
		EnergyRingOuterRadius,
		HealthAmount,
		CombinedAmount,
		vec4(ArmorColor.r, ArmorColor.g, ArmorColor.b, 0.94f));
	IGraphics::CLineItem aCoreTicks[13];
	for(int Tick = 0; Tick < 13; ++Tick)
	{
		const float Angle = pi * 0.75f + pi * 1.5f * Tick / 12.0f;
		const vec2 Direction(cosf(Angle), sinf(Angle));
		const vec2 TickStart = CoreCenter + Direction * 22.7f;
		const vec2 TickEnd = CoreCenter + Direction * (Tick % 3 == 0 ? 25.0f : 24.1f);
		aCoreTicks[Tick] = IGraphics::CLineItem(TickStart.x, TickStart.y, TickEnd.x, TickEnd.y);
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(CoreEdge.r, CoreEdge.g, CoreEdge.b, 0.22f + DangerAmount * DangerPulse * 0.24f);
	Graphics()->LinesDraw(aCoreTicks, 13);
	Graphics()->LinesEnd();

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%d", Health);
	Label({CoreCenter.x - 15.0f, CoreCenter.y - 7.0f, 30.0f, 14.0f}, aBuf, 11.5f, 0, Text);

	const float DataX = X + 59.0f;
	const float DataW = 111.0f;
	Label({DataX, CoreY + 4.0f, 56.0f, 5.0f}, Localize("Health"), 4.1f, -1, HealthColor);
	Label({DataX + 60.0f, CoreY + 4.0f, DataW - 60.0f, 5.0f}, "/ 100", 3.8f, 1, Muted);
	Bar({DataX, CoreY + 10.0f, DataW, 5.0f}, HealthAmount, HealthColor);

	Label({DataX, CoreY + 17.0f, 58.0f, 5.0f}, Localize("Armor"), 3.9f, -1, Armor > 0 ? ArmorColor : Muted);
	str_format(aBuf, sizeof(aBuf), "%d", Armor);
	Label({DataX + 62.0f, CoreY + 17.0f, DataW - 62.0f, 5.0f}, aBuf, 4.0f, 1, Armor > 0 ? ArmorColor : Muted);
	Bar({DataX, CoreY + 23.0f, DataW, 3.0f}, ArmorAmount, ArmorColor);

	Label({DataX, CoreY + 28.0f, 58.0f, 5.0f}, Localize("Fuel"), 3.9f, -1, Fuel <= 20 ? ArmorColor : Muted);
	str_format(aBuf, sizeof(aBuf), "%d%%", Fuel);
	Label({DataX + 62.0f, CoreY + 28.0f, DataW - 62.0f, 5.0f}, aBuf, 4.0f, 1, Fuel <= 20 ? ArmorColor : Accent);
	Bar({DataX, CoreY + 34.0f, DataW, 3.0f}, Fuel / 100.0f, Fuel <= 20 ? ArmorColor : Accent);

	CUIRect KitsCard = {DataX, CoreY + 39.0f, DataW, 7.0f};
	TechShape(KitsCard, vec4(Deep.r, Deep.g, Deep.b, 0.88f), 1.4f);
	Label({KitsCard.x + 3.0f, KitsCard.y, 58.0f, KitsCard.h}, Localize("Kits"), 3.7f, -1, Muted);
	str_format(aBuf, sizeof(aBuf), "%d", clamp(CustomStuff()->m_LocalKits, 0, 99));
	Label({KitsCard.x + 64.0f, KitsCard.y, KitsCard.w - 67.0f, KitsCard.h}, aBuf, 4.4f, 1, ArmorColor);

	CUIRect WeaponCard = {m_Width - HudLayout::SafeMargin(m_Width) - HudLayout::WeaponCardWidth,
		HudLayout::WeaponCardTop(m_Width, m_Height),
		HudLayout::WeaponCardWidth,
		HudLayout::WeaponCardHeight};
	SmokedGlass(WeaponCard, Accent, HasActiveProfile, 4.0f);
	Label({WeaponCard.x + 50.0f, WeaponCard.y + 3.0f, 39.0f, 8.0f}, Localize("Ammo"), 4.2f, 1, Muted);
	if(HasActiveProfile && ActiveProfile.m_Combat.m_UsesAmmo)
		str_format(aBuf, sizeof(aBuf), "%d", max(0, pCharacter->m_AmmoCount));
	else
		str_copy(aBuf, Localize("Infinite"), sizeof(aBuf));
	Label({WeaponCard.x + 50.0f, WeaponCard.y + 12.0f, 39.0f, 14.0f}, aBuf, 7.0f, 1, Accent);
	if(HasActiveWeapon)
	{
		const vec2 WeaponPos(WeaponCard.x + 27.0f, WeaponCard.y + 20.0f);
		const float WeaponSize = 6.2f;
		DrawWeaponRankOverWeapon(Graphics(), RenderTools(), ActiveWeapon, WeaponPos, WeaponSize, 1.0f);
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		RenderTools()->SetShadersForWeapon(ActiveWeapon);
		RenderTools()->RenderWeapon(ActiveWeapon,
			WeaponPos,
			vec2(1, 0),
			WeaponSize,
			true,
			0,
			1.0f,
			false,
			false,
			false,
			1.0f);
		Graphics()->ShaderEnd();
	}

	const float SlotGap = HudLayout::CombatBarSlotGap;
	const float SlotW = HudLayout::CombatBarSlotWidth();
	const float SlotX = HudLayout::CombatBarLeft(m_Width);
	const float SlotY = HudLayout::CombatBarTop(m_Height);
	for(int Slot = 0; Slot < 4; ++Slot)
	{
		const bool Selected = Slot == CustomStuff()->m_WeaponSlot;
		CUIRect Cell = {SlotX + Slot * (SlotW + SlotGap),
			SlotY - (Selected ? 1.0f : 0.0f),
			SlotW,
			Selected ? HudLayout::CombatBarSelectedHeight : HudLayout::CombatBarSlotHeight};
		SmokedGlass(Cell, Selected ? Accent : AccentDim, Selected, 2.5f);
		if(Selected)
			TechShape({Cell.x + 4.0f, Cell.y + Cell.h - 2.0f, Cell.w - 8.0f, 2.0f}, Accent, 0.7f);
		str_format(aBuf, sizeof(aBuf), "%d", Slot + 1);
		Label({Cell.x + 3.0f, Cell.y + 2.0f, 7.0f, 7.0f}, aBuf, Selected ? 5.0f : 4.2f, -1, Selected ? Accent : Muted);

		const CWeaponSpec &Weapon = CustomStuff()->m_aSnapWeapon[Slot];
		if(!Weapon.IsValid())
		{
			Label({Cell.x + 8.0f, Cell.y + 7.0f, Cell.w - 12.0f, 11.0f}, "--", 5.0f, 0, Muted);
			continue;
		}

		const vec2 WeaponPos(Cell.x + Cell.w * 0.53f, Cell.y + 11.5f);
		const float WeaponSize = Selected ? 4.9f : 4.7f;
		DrawWeaponRankOverWeapon(Graphics(), RenderTools(), Weapon, WeaponPos, WeaponSize, 1.0f);
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		RenderTools()->SetShadersForWeapon(Weapon);
		RenderTools()->RenderWeapon(Weapon,
			WeaponPos,
			vec2(1, 0),
			WeaponSize,
			true,
			0,
			1.0f,
			false,
			false,
			false,
			1.0f);
		Graphics()->ShaderEnd();

		CResolvedWeaponProfile SlotProfile{};
		if(CWeaponCatalog::TryResolve(Weapon, &SlotProfile) && SlotProfile.m_Combat.m_UsesAmmo)
			str_format(aBuf, sizeof(aBuf), "%d", max(0, CustomStuff()->m_aItemAmmo[Slot]));
		else
			str_copy(aBuf, "--", sizeof(aBuf));
		Label({Cell.x + Cell.w - 15.0f, Cell.y + Cell.h - 8.0f, 12.0f, 6.0f}, aBuf, 3.8f, 1, Selected ? Text : Muted);
	}
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectatorCount)
		return;
	// draw the box
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	{
		vec4 Panel = CMenus::ThemeBgPanel();
		Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.90f);
	}
	const float Margin = HudLayout::SafeMargin(m_Width);
	const float Width = max(4.0f, min(180.0f, m_Width - Margin * 2.0f));
	const bool HasCharacter =
		m_pClient->m_Snap.m_pLocalCharacter ||
		(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW);
	const float Top = HasCharacter ? HudLayout::SpectatorBarTop(m_Width, m_Height)
								  : m_Height - HudLayout::SpectatorBarHeight - Margin;
	RenderTools()->DrawRoundRectExt(m_Width - Margin - Width, Top, Width, HudLayout::SpectatorBarHeight, 5.0f, CUI::CORNER_TL);
	Graphics()->QuadsEnd();

	// draw the text
	char aBuf[128];
	str_format(aBuf,
			   sizeof(aBuf),
			   "%s: %s",
			   Localize("Spectate"),
			   m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW
				   ? m_pClient->m_aClients[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_aName
				   : Localize("Free-View"));
	float FontSize = 8.0f;
	while(FontSize > 5.5f && TextRender()->TextWidth(0, FontSize, aBuf, -1) > Width - 12.0f)
		FontSize -= 0.25f;
	CTextCursor Cursor;
	TextRender()->SetCursor(
		&Cursor, m_Width - Margin - Width + 6.0f, Top + (HudLayout::SpectatorBarHeight - FontSize) * 0.5f, FontSize,
		TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = Width - 12.0f;
	Cursor.m_MaxLines = 1;
	TextRender()->TextEx(&Cursor, aBuf, -1);
}

float CHud::BottomReservedHeight() const
{
	const bool HasCharacter =
		m_pClient->m_Snap.m_pLocalCharacter ||
		(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW);
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && g_Config.m_ClShowhudSpectatorCount)
	{
		if(HasCharacter)
			return m_Height - HudLayout::SpectatorBarTop(m_Width, m_Height);
		return HudLayout::SpectatorBarHeight + HudLayout::SafeMargin(m_Width);
	}
	if(g_Config.m_ClShowhudHealthAmmo && HasCharacter)
		return m_Height - HudLayout::WeaponCardTop(m_Width, m_Height);
	return 0.0f;
}

float CHud::ScoreHudTop() const
{
	const float StackTop = m_Height - BottomReservedHeight();
	if(g_Config.m_ClShowhudScore && m_pClient->m_Snap.m_pGameInfoObj &&
	   !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		return StackTop - 39.0f - (StackTop < m_Height ? 4.0f : 0.0f);
	return StackTop;
}

void CHud::RenderMovementInformation()
{
	const bool ShowPos = g_Config.m_ClShowhudPlayerPosition != 0;
	const bool ShowSpeed = g_Config.m_ClShowhudPlayerSpeed != 0;
	const bool ShowAngle = g_Config.m_ClShowhudPlayerAngle != 0;
	if(!ShowPos && !ShowSpeed && !ShowAngle)
		return;

	// Prefer spectated character when spectating, else local.
	int ClientID = m_pClient->m_Snap.m_LocalClientID;
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
		ClientID = m_pClient->m_Snap.m_SpecInfo.m_SpectatorID;

	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active)
		return;

	const CNetObj_Character *pPrev = &m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev;
	const CNetObj_Character *pCur = &m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur;
	const float Intra = Client()->IntraGameTick();
	const vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCur->m_X, pCur->m_Y), Intra) / 32.0f;
	const vec2 VelRaw = mix(vec2(pPrev->m_VelX, pPrev->m_VelY), vec2(pCur->m_VelX, pCur->m_VelY), Intra);
	float VelX = VelRaw.x / 256.0f * Client()->GameTickSpeed();
	float VelY = VelRaw.y / 256.0f * Client()->GameTickSpeed();
	if(VelRaw.x >= -1.0f && VelRaw.x <= 1.0f)
		VelX = 0.0f;
	if(VelRaw.y >= -128.0f && VelRaw.y <= 128.0f)
		VelY = 0.0f;
	const float SpeedX = VelX / 32.0f;
	const float SpeedY = VelY / 32.0f;

	float Angle = mix((float)pPrev->m_Angle, (float)pCur->m_Angle, Intra) / 256.0f;
	if(Angle < 0.0f)
		Angle += 2.0f * pi;
	const float AngleDeg = Angle * 180.0f / pi;

	const float LineH = 8.0f;
	const float FontSize = 6.0f;
	const float BoxWidth = 62.0f;
	float BoxHeight = 2.0f;
	if(ShowPos)
		BoxHeight += 3.0f * LineH;
	if(ShowSpeed)
		BoxHeight += 3.0f * LineH;
	if(ShowAngle)
		BoxHeight += 2.0f * LineH;

	// BR stack (bottom → top): spectator bar → score HUD → movement info.
	// Place movement immediately above the score HUD (or free bottom if score off).
	const float Margin = HudLayout::SafeMargin(m_Width);
	float StartX = m_Width - Margin - BoxWidth;
	float StartY = ScoreHudTop() - BoxHeight - 4.0f;
	if(StartY < 20.0f)
		StartY = 20.0f;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	{
		vec4 Panel = CMenus::ThemeBgPanel();
		Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.90f);
	}
	RenderTools()->DrawRoundRectExt(StartX, StartY, BoxWidth, BoxHeight, 5.0f, CUI::CORNER_L);
	Graphics()->QuadsEnd();

	char aBuf[64];
	float y = StartY + 2.0f;
	const float LeftX = StartX + 2.0f;
	const float RightEdge = m_Width - Margin - 2.0f;

	if(ShowPos)
	{
		TextRender()->Text(0, LeftX, y, FontSize, Localize("Position:"), -1);
		y += LineH;

		str_format(aBuf, sizeof(aBuf), "%.2f", Pos.x);
		TextRender()->Text(0, LeftX, y, FontSize, "X:", -1);
		TextRender()->Text(0, RightEdge - TextRender()->TextWidth(0, FontSize, aBuf, -1), y, FontSize, aBuf, -1);
		y += LineH;

		str_format(aBuf, sizeof(aBuf), "%.2f", Pos.y);
		TextRender()->Text(0, LeftX, y, FontSize, "Y:", -1);
		TextRender()->Text(0, RightEdge - TextRender()->TextWidth(0, FontSize, aBuf, -1), y, FontSize, aBuf, -1);
		y += LineH;
	}

	if(ShowSpeed)
	{
		TextRender()->Text(0, LeftX, y, FontSize, Localize("Speed:"), -1);
		y += LineH;

		str_format(aBuf, sizeof(aBuf), "%.2f", SpeedX);
		TextRender()->Text(0, LeftX, y, FontSize, "X:", -1);
		TextRender()->Text(0, RightEdge - TextRender()->TextWidth(0, FontSize, aBuf, -1), y, FontSize, aBuf, -1);
		y += LineH;

		str_format(aBuf, sizeof(aBuf), "%.2f", SpeedY);
		TextRender()->Text(0, LeftX, y, FontSize, "Y:", -1);
		TextRender()->Text(0, RightEdge - TextRender()->TextWidth(0, FontSize, aBuf, -1), y, FontSize, aBuf, -1);
		y += LineH;
	}

	if(ShowAngle)
	{
		TextRender()->Text(0, LeftX, y, FontSize, Localize("Angle:"), -1);
		y += LineH;

		str_format(aBuf, sizeof(aBuf), "%.2f", AngleDeg);
		TextRender()->Text(0, RightEdge - TextRender()->TextWidth(0, FontSize, aBuf, -1), y, FontSize, aBuf, -1);
	}
}

void CHud::OnRender()
{
	if(m_pClient->m_pMenus->IsResearchPageActive())
		return;

	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
	UpdateAnimations();

	if(g_Config.m_ClShowhud)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter &&
		   !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		{
			RenderLowHealthVignette(m_pClient->m_Snap.m_pLocalCharacter);
			RenderHealthAndAmmo(m_pClient->m_Snap.m_pLocalCharacter);
		}
		else if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
			{
				RenderLowHealthVignette(&m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_Cur);
				RenderHealthAndAmmo(&m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_Cur);
			}
			RenderSpectatorHud();
		}

		if(m_pClient->m_Snap.m_pRaceInfo)
			RenderRaceTimer();
		else
			RenderGameTimer();
		RenderPveEnvironment();
		RenderSuddenDeath();
		RenderScoreHud();
		if(!m_pClient->m_pScoreboard->Active())
			RenderObjective();
		RenderStartCountdown();
		RenderPauseNotification();
		RenderReadyUpNotification();
		RenderFps();
		RenderMovementInformation();

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		RenderVoting();
	}
	if(g_Config.m_ClInputDebug)
	{
		float AimX = 0.0f, AimY = 0.0f;
		Input()->GetGamepadAim(&AimX, &AimY);
		char aInput[160];
		str_format(aInput, sizeof(aInput), "INPUT %s  aim %.2f %.2f  assist %d%%", Input()->UsingGamepad() ? "PAD" : "KBM", AimX, AimY, g_Config.m_ClGamepadAimAssist);
		TextRender()->TextColor(0.35f, 0.9f, 1.0f, 0.95f);
		TextRender()->Text(0, 6.0f, 270.0f, 5.5f, aInput, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
	RenderCursor();
	if(m_DebugStatusScreenshotFrames > 0 && --m_DebugStatusScreenshotFrames == 0)
		Graphics()->TakeScreenshot(0);
}
