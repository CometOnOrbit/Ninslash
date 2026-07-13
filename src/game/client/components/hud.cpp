#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/game_data.h>
#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/customstuff.h>

#include <game/questinfo.h>
#include <game/weapons.h>
#include <game/buildables.h>

#include "controls.h"
#include "camera.h"
#include "hud.h"
#include "voting.h"
#include "binds.h"

#define RAD 0.017453292519943295769236907684886f

CHud::CHud()
{
	// won't work if zero
	m_AverageFPS = 1.0f;
}

void CHud::OnReset()
{
}

void CHud::RenderGameTimer()
{
	if(!g_Config.m_ClShowhudTimer) return;
	float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;

	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH))
	{
		char Buf[32];
		int Time = 0;
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		{
			Time = m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit*60 - ((Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed());

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else
			Time = (Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed();

		if (Time < 0)
			Time = 0;
		
		str_format(Buf, sizeof(Buf), "%d:%02d", Time/60, Time%60);
		float FontSize = 10.0f;
		float w = TextRender()->TextWidth(0, FontSize, Buf, -1);
		// last 60 sec red, last 10 sec blink
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		{
			float Alpha = Time <= 10 && (2*time_get()/time_freq()) % 2 ? 0.5f : 1.0f;
			TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
		}
		TextRender()->Text(0, Half-w/2, 2, FontSize, Buf, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	
	// survival mode text
	if (m_pClient->Survival())
	{
		TextRender()->TextColor(1.0f, 1.0f, 0.0f, 1.0f);
		const char *pText = Localize("Survival mode");
		float FontSize = 7.0f;
		float w = TextRender()->TextWidth(0, FontSize,pText, -1);
		TextRender()->Text(0, 150.0f*Graphics()->ScreenAspect()+-w/2.0f, 12.0f, FontSize, pText, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CHud::RenderPauseNotification()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED &&
		!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		const char *pText = Localize("Game paused");
		float FontSize = 14.0f;
		float w = TextRender()->TextWidth(0, FontSize,pText, -1);
		float x = 150.0f*Graphics()->ScreenAspect()-w/2.0f;
		float y = 50.0f;
		float Pad = 12.0f;

		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.55f);
		RenderTools()->DrawRoundRect(x-Pad, y-Pad*0.5f, w+Pad*2.0f, FontSize+Pad, 8.0f);
		Graphics()->QuadsEnd();

		TextRender()->Text(0, x, y, FontSize, pText, -1);
	}
}

void CHud::RenderSuddenDeath()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->Text(0, Half-w/2, 2, FontSize, pText, -1);
	}
}

void CHud::RenderObjective()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_COOP)
	{
		if (!m_pClient->m_Snap.m_pGameDataObj)
			return;

		int Quest = m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed;
		int QuestProgressCounter = m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue;
		int Level = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed;
		int Pack = m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue;
		int Theme = Pack & 0xF;
		int WaveType = (Pack >> 4) & 0xF;
		int QuestsDone = (Pack >> 8) & 0xF;
		int QuestsTotal = (Pack >> 12) & 0xF;
		
		if (Quest)
		{
			// Original HUD anchor: upper-right block near x=296 (300-wide virtual screen).
			const float xRight = 296.0f*Graphics()->ScreenAspect();
			const float xMin = 10.0f*Graphics()->ScreenAspect();
			const float MaxTextW = xRight - xMin;

			auto DrawRight = [&](float y, float FontSize, const char *pText) {
				while (FontSize > 4.0f && TextRender()->TextWidth(0, FontSize, pText, -1) > MaxTextW)
					FontSize -= 0.5f;
				const float w = TextRender()->TextWidth(0, FontSize, pText, -1);
				TextRender()->Text(0, max(xMin, xRight - w), y, FontSize, pText, -1);
			};
			
			// level + theme (compact, above the original header)
			{
				TextRender()->TextColor(0.65f, 0.75f, 0.85f, 1.0f);
				if (Quest == QUEST_HORDE)
				{
					char aWaveBuf[48];
					str_format(aWaveBuf, sizeof(aWaveBuf), "%s %d", Localize("Wave"), Level);
					DrawRight(88.0f, 5.0f, aWaveBuf);
					char aKillBuf[48];
					str_format(aKillBuf, sizeof(aKillBuf), "%d %s", Pack, Localize("kills"));
					DrawRight(94.0f, 5.0f, aKillBuf);
				}
				else if (Quest == QUEST_EXTRACT)
				{
					char aTimeBuf[48];
					str_format(aTimeBuf, sizeof(aTimeBuf), "%d %s", Level, Localize("seconds remaining"));
					DrawRight(88.0f, 5.0f, aTimeBuf);
				}
				else
				{
					char aLevelBuf[32];
					str_format(aLevelBuf, sizeof(aLevelBuf), "%s %d", Localize("Level"), Level);
					DrawRight(88.0f, 5.0f, aLevelBuf);
					DrawRight(94.0f, 5.0f, Localize(GetThemeDisplayName(Theme)));
				}
			}

			// header (original y=100)
			{
				TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
				DrawRight(100.0f, 8.0f, Localize("Objective"));
			}

			// quest title (original y=112)
			{
				TextRender()->TextColor(0.8f, 0.8f, 0.8f, 1.0f);
				char aQuestBuf[96];
				const char *pWave = GetWaveDisplayName(WaveType);
				if (pWave[0] && (Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME || Quest == QUEST_KILLREMAININGENEMIES))
					str_format(aQuestBuf, sizeof(aQuestBuf), "%s (%s)", Localize(GetQuestDisplayName(Quest)), Localize(pWave));
				else
					str_copy(aQuestBuf, Localize(GetQuestDisplayName(Quest)), sizeof(aQuestBuf));
				if (Quest == QUEST_EXTRACT && ((Pack >> 8) & 0xFF) >= 1)
					str_copy(aQuestBuf, Localize("Reach the door"), sizeof(aQuestBuf));
				DrawRight(112.0f, 6.0f, aQuestBuf);
			}

			// step + progress (original y=120)
			{
				char aProgressBuf[96];
				if (Quest == QUEST_REACHDOOR && m_pClient->SurvivalAcid())
				{
					TextRender()->TextColor(0.9f, 0.55f, 0.35f, 1.0f);
					DrawRight(120.0f, 5.5f, Localize("Rising acid"));
				}
				else
				{
					TextRender()->TextColor(0.75f, 0.75f, 0.75f, 1.0f);
					const char *pDetail = "";
					if (Quest == QUEST_KILLREMAININGENEMIES || Quest == QUEST_SURVIVEWAVE || Quest == QUEST_KILL_BOSS || Quest == QUEST_HORDE)
					{
						const char *pText = Quest == QUEST_KILL_BOSS ? Localize("bosses remaining") : Localize("enemies remaining");
						str_format(aProgressBuf, sizeof(aProgressBuf), "%u %s", QuestProgressCounter, pText);
						pDetail = aProgressBuf;
					}
					else if (Quest == QUEST_SURVIVEWAVETIME || Quest == QUEST_DEFEND)
					{
						str_format(aProgressBuf, sizeof(aProgressBuf), "%u %s", QuestProgressCounter, Localize("seconds remaining"));
						pDetail = aProgressBuf;
					}
					else if (Quest == QUEST_ACTIVATE_SWITCHES || Quest == QUEST_FIND_SWITCH || (Quest == QUEST_EXTRACT && ((Pack >> 8) & 0xFF) == 0))
					{
						str_format(aProgressBuf, sizeof(aProgressBuf), "%u %s", QuestProgressCounter, Localize("switches remaining"));
						pDetail = aProgressBuf;
					}
					else if (Quest == QUEST_EXTRACT)
					{
						str_format(aProgressBuf, sizeof(aProgressBuf), "%u %s", QuestProgressCounter, Localize("to evacuate"));
						pDetail = aProgressBuf;
					}
					else
						aProgressBuf[0] = 0;

					if (QuestsTotal > 0 && Quest != QUEST_REACHDOOR)
					{
						char aLineBuf[128];
						char aStepBuf[32];
						str_format(aStepBuf, sizeof(aStepBuf), Localize("Objective %d/%d"), min(QuestsDone+1, QuestsTotal), QuestsTotal);
						if (pDetail[0])
							str_format(aLineBuf, sizeof(aLineBuf), "%s · %s", aStepBuf, pDetail);
						else
							str_copy(aLineBuf, aStepBuf, sizeof(aLineBuf));
						DrawRight(120.0f, 6.0f, aLineBuf);
					}
					else if (pDetail[0])
						DrawRight(120.0f, 6.0f, pDetail);
				}
			}
			
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
}



void CHud::RenderScoreHud()
{
	if(!g_Config.m_ClShowhudScore) return;
	// render small score hud
	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		int GameFlags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		float Whole = 300*Graphics()->ScreenAspect();
		float StartY = 229.0f;

		if (GameFlags&GAMEFLAG_TEAMS && !(GameFlags&GAMEFLAG_INFECTION) && m_pClient->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][32];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam)/2, "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);
			float aScoreTeamWidth[2] = { TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_RED], -1), TextRender()->TextWidth(0, 14.0f, aScoreTeam[TEAM_BLUE], -1) };
			int FlagCarrier[2] = { m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed, m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue };
			float ScoreWidthMax = max(max(aScoreTeamWidth[TEAM_RED], aScoreTeamWidth[TEAM_BLUE]), TextRender()->TextWidth(0, 14.0f, "100", -1));
			float Split = 3.0f;
			float ImageSize = GameFlags&GAMEFLAG_FLAGS ? 16.0f : Split;

			for(int t = 0; t < 2; t++)
			{
				// draw box
				Graphics()->BlendNormal();
				Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				
				if (GameFlags&GAMEFLAG_INFECTION)
				{
					if(t == 0)
						Graphics()->SetColor(1.0f, 0.7f, 0.7f, 0.3f);
					else
						Graphics()->SetColor(0.1f, 0.1f, 0.1f, 0.3f);
				}
				else
				{
					if(t == 0)
						Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.25f);
					else
						Graphics()->SetColor(0.0f, 0.0f, 1.0f, 0.25f);
				}
				
				RenderTools()->DrawRoundRectExt(Whole-ScoreWidthMax-ImageSize-2*Split, StartY+t*20, ScoreWidthMax+ImageSize+2*Split, 18.0f, 5.0f, CUI::CORNER_L);
				Graphics()->QuadsEnd();

				// draw score
				TextRender()->Text(0, Whole-ScoreWidthMax+(ScoreWidthMax-aScoreTeamWidth[t])/2-Split, StartY+t*20, 14.0f, aScoreTeam[t], -1);

				if(GameFlags&GAMEFLAG_FLAGS)
				{
					int BlinkTimer = (m_pClient->m_FlagDropTick[t] != 0 &&
										(Client()->GameTick()-m_pClient->m_FlagDropTick[t])/Client()->GameTickSpeed() >= 25) ? 10 : 20;
					if(FlagCarrier[t] == FLAG_ATSTAND || (FlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick()/BlinkTimer)&1)))
					{
						// draw flag
						Graphics()->BlendNormal();
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
						Graphics()->QuadsBegin();
						RenderTools()->SelectSprite(t==0?SPRITE_FLAG_RED:SPRITE_FLAG_BLUE);
						IGraphics::CQuadItem QuadItem(Whole-ScoreWidthMax-ImageSize, StartY+1.0f+t*20, ImageSize/2, ImageSize);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
						Graphics()->QuadsEnd();
					}
					else if(FlagCarrier[t] >= 0)
					{
						// draw name of the flag holder
						int ID = FlagCarrier[t]%MAX_CLIENTS;
						const char *pName = m_pClient->m_aClients[ID].m_aName;
						float w = TextRender()->TextWidth(0, 8.0f, pName, -1);
						TextRender()->Text(0, min(Whole-w-1.0f, Whole-ScoreWidthMax-ImageSize-2*Split), StartY+(t+1)*20.0f-3.0f, 8.0f, pName, -1);

						// draw tee of the flag holder
						//CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
						//Info.m_Size = 18.0f;
						//RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
						//	vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
					}
				}
				StartY += 8.0f;
			}
		}
		// dm, infection, co-op
		else
		{
			int Local = -1;
			int aPos[2] = { 1, 2 };
			const CNetObj_PlayerInfo *apPlayerInfo[2] = { 0, 0 };
			int i = 0;
			for(int t = 0; t < 2 && i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
			{
				if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
				{
					//if (!CustomStuff()->IsBot(m_pClient->m_Snap.m_paInfoByScore[i]->m_ClientID) || !(GameFlags&GAMEFLAG_COOP))
					if (!m_pClient->m_aClients[m_pClient->m_Snap.m_paInfoByScore[i]->m_ClientID].m_IsBot || !(GameFlags&GAMEFLAG_COOP))
					{
						apPlayerInfo[t] = m_pClient->m_Snap.m_paInfoByScore[i];
						if(apPlayerInfo[t]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
							Local = t;
						++t;
					}
				}
			}
			// search local player info if not a spectator, nor within top2 scores
			if(Local == -1 && m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
			{
				for(; i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
				{
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
						++aPos[1];
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
					{
						apPlayerInfo[1] = m_pClient->m_Snap.m_paInfoByScore[i];
						Local = 1;
						break;
					}
				}
			}
			char aScore[2][32];
			for(int t = 0; t < 2; ++t)
			{
				if(apPlayerInfo[t])
					str_format(aScore[t], sizeof(aScore)/2, "%d", apPlayerInfo[t]->m_Score);
				else
					aScore[t][0] = 0;
			}
			float aScoreWidth[2] = {TextRender()->TextWidth(0, 14.0f, aScore[0], -1), TextRender()->TextWidth(0, 14.0f, aScore[1], -1)};
			float ScoreWidthMax = max(max(aScoreWidth[0], aScoreWidth[1]), TextRender()->TextWidth(0, 14.0f, "10", -1));
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;

			for(int t = 0; t < 2; t++)
			{
				// draw box
				Graphics()->BlendNormal();
				Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				if(t == Local)
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
				else
					Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.25f);
				RenderTools()->DrawRoundRectExt(Whole-ScoreWidthMax-ImageSize-2*Split-PosSize, StartY+t*20, ScoreWidthMax+ImageSize+2*Split+PosSize, 18.0f, 5.0f, CUI::CORNER_L);
				Graphics()->QuadsEnd();

				// draw score
				TextRender()->Text(0, Whole-ScoreWidthMax+(ScoreWidthMax-aScoreWidth[t])/2-Split, StartY+t*20, 14.0f, aScore[t], -1);

				if(apPlayerInfo[t])
 				{
					// draw name
					int ID = apPlayerInfo[t]->m_ClientID;
					const char *pName = m_pClient->m_aClients[ID].m_aName;
					float w = TextRender()->TextWidth(0, 8.0f, pName, -1);
					TextRender()->Text(0, min(Whole-w-1.0f, Whole-ScoreWidthMax-ImageSize-2*Split-PosSize), StartY+(t+1)*20.0f-3.0f, 8.0f, pName, -1);

					// draw tee
					CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
 					Info.m_Size = 18.0f;
 					//RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
					RenderTools()->RenderPortrait(&Info,
						vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20 + 16), 0);
						
 					//	vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
					
				}

				// draw position
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				TextRender()->Text(0, Whole-ScoreWidthMax-ImageSize-Split-PosSize, StartY+2.0f+t*20, 10.0f, aBuf, -1);

				StartY += 8.0f;
			}
		}
	}
}

void CHud::RenderStartCountdown()
{
	if(!g_Config.m_ClShowhudTimer)
		return;

	if(!m_pClient->m_Snap.m_pGameInfoObj || !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		return;

	const char *pLabel = Localize("Warmup");
	float FontSize = 16.0f;
	float LabelW = TextRender()->TextWidth(0, FontSize, pLabel, -1);

	char aBuf[32];
	int Seconds = m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer/SERVER_TICK_SPEED;
	if(Seconds < 5)
		str_format(aBuf, sizeof(aBuf), "%d.%d", Seconds, (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer*10/SERVER_TICK_SPEED)%10);
	else
		str_format(aBuf, sizeof(aBuf), "%d", Seconds);
	float CountW = TextRender()->TextWidth(0, FontSize, aBuf, -1);

	float BoxW = max(LabelW, CountW) + 24.0f;
	float BoxH = FontSize * 2.0f + 20.0f;
	float x = 150.0f*Graphics()->ScreenAspect() - BoxW/2.0f;
	float y = 42.0f;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.55f);
	RenderTools()->DrawRoundRect(x, y, BoxW, BoxH, 8.0f);
	Graphics()->QuadsEnd();

	TextRender()->Text(0, x + (BoxW-LabelW)/2.0f, y + 8.0f, FontSize, pLabel, -1);
	TextRender()->Text(0, x + (BoxW-CountW)/2.0f, y + FontSize + 12.0f, FontSize, aBuf, -1);
}

void CHud::RenderReadyUpNotification()
{
	if(!m_pClient->m_Snap.m_pGameInfoObj || !m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)
		return;

	if(m_pClient->m_pControls->m_Ready)
		return;

	if(m_pClient->m_Snap.m_pLocalCharacter && (m_pClient->m_Snap.m_pLocalCharacter->m_PlayerFlags&PLAYERFLAG_READY))
		return;

	const char *pKey = m_pClient->m_pBinds->GetKey("ready_change");
	if(!pKey[0])
		pKey = "ready_change";

	char aText[128];
	str_format(aText, sizeof(aText), Localize("When ready, press <%s>"), pKey);

	float FontSize = 14.0f;
	float w = TextRender()->TextWidth(0, FontSize, aText, -1);
	float x = 150.0f*Graphics()->ScreenAspect() - w/2.0f;
	float y = 110.0f;
	float Pad = 10.0f;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.55f);
	RenderTools()->DrawRoundRect(x-Pad, y-Pad*0.5f, w+Pad*2.0f, FontSize+Pad, 8.0f);
	Graphics()->QuadsEnd();

	TextRender()->Text(0, x, y, FontSize, aText, -1);
}

void CHud::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), m_pClient->m_pCamera->m_Zoom, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CHud::RenderFps()
{
	if(g_Config.m_ClShowfps)
	{
		// calculate avg. fps
		float FPS = 1.0f / Client()->RenderFrameTime();
		m_AverageFPS = (m_AverageFPS*(1.0f-(1.0f/m_AverageFPS))) + (FPS*(1.0f/m_AverageFPS));
		char Buf[512];
		str_format(Buf, sizeof(Buf), "%d", (int)m_AverageFPS);
		TextRender()->Text(0, m_Width-10-TextRender()->TextWidth(0,12,Buf,-1), 5, 12, Buf, -1);
	}
}

void CHud::RenderConnectionWarning()
{
	if(Client()->ConnectionProblems())
	{
		const char *pText = Localize("Connection Problems...");
		float FontSize = 14.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		float x = 150.0f*Graphics()->ScreenAspect() - w/2.0f;
		float y = 40.0f; // below timer; avoid stacking with pause/warmup boxes
		float Pad = 12.0f;

		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.55f);
		RenderTools()->DrawRoundRect(x-Pad, y-Pad*0.5f, w+Pad*2.0f, FontSize+Pad, 8.0f);
		Graphics()->QuadsEnd();

		TextRender()->Text(0, x, y, FontSize, pText, -1);
	}
}

void CHud::RenderTeambalanceWarning()
{
	if (m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_INFECTION)
		return;
	
	// render prompt about team-balance
	bool Flash = time_get()/(time_freq()/2)%2 == 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		int TeamDiff = m_pClient->m_Snap.m_aTeamSize[TEAM_RED]-m_pClient->m_Snap.m_aTeamSize[TEAM_BLUE];
		if (g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			if(Flash)
				TextRender()->TextColor(1,1,0.5f,1);
			else
				TextRender()->TextColor(0.7f,0.7f,0.2f,1.0f);
			TextRender()->Text(0x0, 5, 108, 6, pText, -1);
			TextRender()->TextColor(1,1,1,1);
		}
	}
}


void CHud::RenderVoting()
{
	if(!m_pClient->m_pVoting->IsVoting() || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.55f);
	RenderTools()->DrawRoundRect(-12, 58-2, 100+10+4+9, 48, 8.0f);
	Graphics()->QuadsEnd();

	TextRender()->TextColor(1,1,1,1);

	CTextCursor Cursor;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), Localize("%ds left"), m_pClient->m_pVoting->SecondsLeft());
	
	if (m_pClient->m_pVoting->SecondsLeft() < 0)
		m_pClient->m_pVoting->Hide();
	
	float tw = TextRender()->TextWidth(0x0, 6, aBuf, -1);
	TextRender()->SetCursor(&Cursor, 5.0f+100.0f-tw, 60.0f, 6.0f, TEXTFLAG_RENDER);
	TextRender()->TextEx(&Cursor, aBuf, -1);

	TextRender()->SetCursor(&Cursor, 5.0f, 60.0f, 6.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = 100.0f-tw;
	Cursor.m_MaxLines = 3;
	char aVoteDesc[512];
	m_pClient->SanitizeSocialString(m_pClient->m_pVoting->VoteDescription(), aVoteDesc, sizeof(aVoteDesc));
	TextRender()->TextEx(&Cursor, aVoteDesc, -1);

	// reason
	char aReasonBuf[512];
	m_pClient->SanitizeSocialString(m_pClient->m_pVoting->VoteReason(), aReasonBuf, sizeof(aReasonBuf));
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("Reason:"), aReasonBuf);
	TextRender()->SetCursor(&Cursor, 5.0f, 79.0f, 6.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = 100.0f;
	TextRender()->TextEx(&Cursor, aBuf, -1);

	CUIRect Base = {5, 88, 100, 4};
	m_pClient->m_pVoting->RenderBars(Base, false);

	char aYesKeys[64], aNoKeys[64];
	m_pClient->m_pBinds->GetKeys("vote yes", aYesKeys, sizeof(aYesKeys));
	m_pClient->m_pBinds->GetKeys("vote no", aNoKeys, sizeof(aNoKeys));
	const char *pYesKey = aYesKeys[0] ? aYesKeys : "";
	const char *pNoKey = aNoKeys[0] ? aNoKeys : "";
	str_format(aBuf, sizeof(aBuf), "%s - %s", pYesKey, Localize("Vote yes"));
	Base.y += Base.h+1;
	UI()->DoLabel(&Base, aBuf, 6.0f, -1);

	str_format(aBuf, sizeof(aBuf), "%s - %s", Localize("Vote no"), pNoKey);
	UI()->DoLabel(&Base, aBuf, 6.0f, 1);
}

void CHud::RenderCursor()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if (CustomStuff()->m_Inventory)
		return;
	
	MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();

	// render cursor
	int Weapon = max(m_pClient->m_Snap.m_pLocalCharacter->m_Weapon, 0);
	
	RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[Weapon%NUM_WEAPONS].m_pSpriteCursor);
	float CursorSize = 64;
	RenderTools()->DrawSprite(m_pClient->m_pControls->m_TargetPos.x, m_pClient->m_pControls->m_TargetPos.y, CursorSize);
	Graphics()->QuadsEnd();
}


void CHud::DrawCircular(float x, float y, float r, int Segments, int FillAmount, int Max, bool Flip)
{
	float AOff = -pi/2;
	if (Flip)
		AOff = pi/2;
	
	IGraphics::CFreeformItem Array[32];
	int NumItems = 0;
	float FSegments = (float)Segments;
	for(int i = 0; i < Segments; i+=2)
	{
		if ((i*Max)/FSegments < FillAmount)
			continue;
		
		float a1 = i/FSegments * 1*pi +AOff;
		float a2 = (i+1)/FSegments * 1*pi +AOff;
		float a3 = (i+2)/FSegments * 1*pi +AOff;
		float Ca1 = cosf(a1);
		float Ca2 = cosf(a2);
		float Ca3 = cosf(a3);
		float Sa1 = sinf(a1);
		float Sa2 = sinf(a2);
		float Sa3 = sinf(a3);

		if (!Flip)
		{
			Array[NumItems++] = IGraphics::CFreeformItem(
				x, y,
				x+Ca1*r, y+Sa1*r,
				x+Ca3*r, y+Sa3*r,
				x+Ca2*r, y+Sa2*r);
		}
		else
		{
			Array[NumItems++] = IGraphics::CFreeformItem(
				x, y,
				x+Ca1*r, y-Sa1*r,
				x+Ca3*r, y-Sa3*r,
				x+Ca2*r, y-Sa2*r);
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



void CHud::RenderHealthAndAmmo(const CNetObj_Character *pCharacter)
{
	if(!g_Config.m_ClShowhudHealthAmmo) return;
	if(!pCharacter)
		return;

	//vec2 Area1Pos = vec2(0, 0);
	vec2 Area2Pos = vec2(8, 5);
	
	float x = Area2Pos.x; // 16
	float y = 5;
	
	int Weapon = pCharacter->m_Weapon;
	
	// render gui stuff
	
	


	
	// new health bar, healthbar, render health
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_HP].m_Id);
	Graphics()->QuadsBegin();
	
	vec2 HpSize = vec2(120, 12);
	vec2 FuelSize = vec2(60, 12);
	

	float hpf = min(pCharacter->m_Health, 100) / 100.0f;
		
	{ // hp fill
		Graphics()->SetColor(1, 0, 0, 1);
		Graphics()->QuadsSetSubsetFree(0, 0.5f, 1*hpf, 0.5f, 0, 1, 1*hpf, 1);

		IGraphics::CFreeformItem FreeFormItem(
			x, y,
			x+hpf*HpSize.x, y,
			x, y+HpSize.y,
			x+hpf*HpSize.x, y+HpSize.y);

		Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
	}
	
	{ // armor fill
		float armorf = min(pCharacter->m_Armor, 100) / 100.0f;
		
		if (armorf + hpf <= 1.0f)
		{
			Graphics()->SetColor(1.0f, 1.0f, 0.0f, 1.0f);
			Graphics()->QuadsSetSubsetFree(	hpf, 0.5f,
											hpf+armorf, 0.5f,
											hpf, 1,
											hpf+armorf, 1);

			IGraphics::CFreeformItem FreeFormItem(
				x+hpf*HpSize.x, y,
				x+(hpf+armorf)*HpSize.x, y,
				x+hpf*HpSize.x, y+HpSize.y,
				x+(hpf+armorf)*HpSize.x, y+HpSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
		}
		else
		{
			{
				Graphics()->SetColor(0.9f, 0.9f, 0.0f, 1.0f);
				Graphics()->QuadsSetSubsetFree(	hpf, 0.5f, 
												1, 0.5f, 
												hpf, 1, 
												1, 1);

				IGraphics::CFreeformItem FreeFormItem(
					x+(hpf)*HpSize.x, y,
					x+1*HpSize.x, y,
					x+(hpf)*HpSize.x, y+HpSize.y,
					x+1*HpSize.x, y+HpSize.y);

				Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
			}
			
			{
				Graphics()->SetColor(0.0f, 0.7f, 0.0f, 1.0f);
				Graphics()->QuadsSetSubsetFree(	1-armorf, 0.5f, 
												hpf, 0.5f, 
												1-armorf, 1, 
												hpf, 1);

				IGraphics::CFreeformItem FreeFormItem(
					x+(1-armorf)*HpSize.x, y,
					x+hpf*HpSize.x, y,
					x+(1-armorf)*HpSize.x, y+HpSize.y,
					x+hpf*HpSize.x, y+HpSize.y);

				Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
			}
			
			/*
			Graphics()->SetColor(0.7f, 0.7f, 0.0f, 1.0f);
			Graphics()->QuadsSetSubsetFree(	1-armorf, 0.5f, 
											1, 0.5f, 
											1-armorf, 1, 
											1, 1);

			IGraphics::CFreeformItem FreeFormItem(
				x+(1-armorf)*HpSize.x, y,
				x+1*HpSize.x, y,
				x+(1-armorf)*HpSize.x, y+HpSize.y,
				x+1*HpSize.x, y+HpSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
			*/
		}
		
		/*
		if (armorf > hpf)
		{
			Graphics()->QuadsSetSubsetFree(0, 0.5f, 1*armorf, 0.5f, 0, 1, 1*armorf, 1);

			IGraphics::CFreeformItem FreeFormItem(
				x, y,
				x+armorf*HpSize.x, y,
				x, y+HpSize.y,
				x+armorf*HpSize.x, y+HpSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
		}
		else
		{
			Graphics()->QuadsSetSubsetFree(	hpf-armorf, 0.5f,
											hpf, 0.5f,
											hpf-armorf, 1,
											hpf, 1);

			IGraphics::CFreeformItem FreeFormItem(
				x+(hpf-armorf)*HpSize.x, y,
				x+hpf*HpSize.x, y,
				x+(hpf-armorf)*HpSize.x, y+HpSize.y,
				x+hpf*HpSize.x, y+HpSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
		}
		*/
	}
	
	{ // hp frame
		Graphics()->SetColor(1, 1, 1, 1);
		Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 0.5f, 1, 0.5f); // nice way to pick a sprite

		IGraphics::CFreeformItem FreeFormItem(
			x, y,
			x+HpSize.x, y,
			x, y+HpSize.y,
			x+HpSize.x, y+HpSize.y);

		Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
	}
	
	// render jetpack
	{
		float Fuel = min(pCharacter->m_JetpackPower/2, 100) / 100.0f;		
		y += 16;
		//x += 4;
		
		// fill
		{
			Graphics()->SetColor(0.5f, 0.8f, 1, 1);
			Graphics()->QuadsSetSubsetFree(0, 0.5f, 1*Fuel, 0.5f, 0, 1, 1*Fuel, 1);

			IGraphics::CFreeformItem FreeFormItem(
				x, y,
				x+Fuel*FuelSize.x, y,
				x, y+FuelSize.y,
				x+Fuel*FuelSize.x, y+FuelSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
		}
		
		// frame
		{
			Graphics()->SetColor(1, 1, 1, 1);
			Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 0.5f, 1, 0.5f); // nice way to pick a sprite

			IGraphics::CFreeformItem FreeFormItem(
				x, y,
				x+FuelSize.x, y,
				x, y+FuelSize.y,
				x+FuelSize.x, y+FuelSize.y);

			Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
		}
		y -= 12;
		//x -= 4;
	}
	
	Graphics()->QuadsEnd();
	
	
	// new jetpack meter
	x = -1;
	y = -1;
	
	//vec2 FrameSize = vec2(38, 38);
	//int Fuel = pCharacter->m_JetpackPower/2;
	
	// buff duration
	x = -1;
	y = -1;
	
	//int BuffTime = 100 - (Client()->GameTick() - CustomStuff()->m_Local.m_BuffStartTick)*5.0f / Client()->GameTickSpeed();
	
	//if (CustomStuff()->m_Local.m_Buff < 0)
	//	BuffTime = -1;

	/*
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, Fuel*0.01f, 0, 1);
	DrawCircular(x+19, y+19, 12.5f, 64, 100-Fuel, 100);
	*/
	
	/*
	if (BuffTime < 0)
		DrawCircular(x+19, y+19, 12.5f, 64, 100-Fuel, 100, true);
	else
	{
		Graphics()->SetColor(0.15f, 0.5f+BuffTime*0.005f, 0.3f, 1);
		DrawCircular(x+19, y+19, 12.5f, 64, 100-BuffTime, 100, true);
	}
	*/
	
	//Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUINUMBERS].m_Id);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	
	x += 80;
	y += 4;
	
	if (WeaponUseAmmo(Weapon))
	{
		int n1 = pCharacter->m_AmmoCount;
		int n2 = 0;
		
		while (n1 >= 10)
		{
			n1 -= 10;
			n2++;
		}
		
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.9f, 0.9f, 0.9f, 1);
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_0+n2);
		RenderTools()->DrawSprite(x, y+24, 20);
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_0+n1);
		RenderTools()->DrawSprite(x+10, y+24, 20);
		Graphics()->QuadsEnd();
	}
	else
	{
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.9f, 0.9f, 0.9f, 1);
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_LINE);
		RenderTools()->DrawSprite(x+5, y+24, 20);
		Graphics()->QuadsEnd();
	}
	
	//Graphics()->QuadsEnd();
	
	
	// frame
	/*
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_FUEL].m_Id);
	Graphics()->QuadsBegin();
	{
		Graphics()->SetColor(1, 1, 1, 1);
		Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 1, 1, 1);

		IGraphics::CFreeformItem FreeFormItem(
			x, y,
			x+FrameSize.x, y,
			x, y+FrameSize.y,
			x+FrameSize.x, y+FrameSize.y);

		Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
	}
	Graphics()->QuadsEnd();
	*/

	/*
	
	if (BuffTime > 0)
	{
		x = Area1Pos.x+18.5f; // 16
		y = Area1Pos.y+18.5f;
		
		// buff
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ITEMS].m_Id);
		Graphics()->QuadsBegin();
		
		Graphics()->SetColor(1, 1, 1, 1);

		RenderTools()->SelectSprite(SPRITE_ITEM1+CustomStuff()->m_Local.m_Buff);
		RenderTools()->DrawSprite(x, y, 18);

		Graphics()->QuadsEnd();
	}
	*/

	
	x = Area2Pos.x; // 16
	y = Area2Pos.y;

	x += 14;
	//y += 6;
	
	y += 24;
	
	
	// weapons
	float Size = 0.2f;
	//int iw = pCharacter->m_Weapon;

	if (m_pClient->m_pControls->m_SignalWeapon >= 0)
	{
		CustomStuff()->m_WeaponSignalTimer = 1.0f;
		CustomStuff()->m_WeaponSignal = m_pClient->m_pControls->m_SignalWeapon;
		m_pClient->m_pControls->m_SignalWeapon = -1;
	}
	
	// weapons 1 - 4
	
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	
	x += 60*Size;
	y += 18;
	
	for (int i = 0; i < 4; i++)
	{
		int w = CustomStuff()->m_aSnapWeapon[i];
		
		// order num.
		
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.9f, 0.9f, 0.9f, 1);
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_1+i);
		if (i == CustomStuff()->m_WeaponSlot)
			RenderTools()->DrawSprite(x-20, y, 16);
		else
			RenderTools()->DrawSprite(x-20, y, 12);
		Graphics()->SetColor(1, 1, 1, 1);
		Graphics()->QuadsEnd();
		
		if (w != WEAPON_NONE)
		{
			// pickup icon
			/*
			if (CustomStuff()->m_WeaponpickTimer > 0.0f)
			{
				int pw = clamp(CustomStuff()->m_WeaponpickWeapon, 0, NUM_WEAPONS-1);
				if (i == pw)
				{
					Graphics()->QuadsBegin();
					float a = sin(CustomStuff()->m_WeaponpickTimer*pi)*sin(CustomStuff()->m_WeaponpickTimer*pi);
					
					Graphics()->SetColor(1, 1, 1, a);
					
					RenderTools()->SelectSprite(SPRITE_WEAPON_PICKUP);
					RenderTools()->DrawSprite(x, y, 32);
					Graphics()->QuadsEnd();
				}
			}
			*/
			
			// selected weapon / item
			if (i == CustomStuff()->m_WeaponSlot)
			{
				Graphics()->ShaderBegin(SHADER_GRAYSCALE, 0.0f);
				Graphics()->QuadsBegin();
				//RenderTools()->SelectSprite(SPRITE_WEAPON_SLOT);
			
				if (g_Config.m_GfxShaders)
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				else
					Graphics()->SetColor(0.0f, 0.0f, 0.0f, 1.0f);
				
				//RenderTools()->RenderWeapon(w, vec2(x, y), vec2(1, 0), 24.0f);
				RenderTools()->RenderWeapon(w, vec2(x-0.5f, y-0.5f), vec2(1, 0), WEAPON_GAME_SIZE/3);
				RenderTools()->RenderWeapon(w, vec2(x+0.5f, y-0.5f), vec2(1, 0), WEAPON_GAME_SIZE/3);
				RenderTools()->RenderWeapon(w, vec2(x-0.5f, y+0.5f), vec2(1, 0), WEAPON_GAME_SIZE/3);
				RenderTools()->RenderWeapon(w, vec2(x+0.5f, y+0.5f), vec2(1, 0), WEAPON_GAME_SIZE/3);

				Graphics()->QuadsEnd();
			}
			
			RenderTools()->SetShadersForWeapon(w);
			
			// weapon
			Graphics()->QuadsBegin();
			
			Graphics()->SetColor(1, 1, 1, 1);
			
			RenderTools()->RenderWeapon(w, vec2(x, y), vec2(1, 0), WEAPON_GAME_SIZE/3);
			
			/*
			if (i == CustomStuff()->m_WeaponSlot)
				RenderTools()->DrawSprite(x, y, g_pData->m_Weapons.m_aId[w].m_VisualSize * Size * 1.7f);
			else
				RenderTools()->DrawSprite(x, y, g_pData->m_Weapons.m_aId[w].m_VisualSize * Size);
			*/

			Graphics()->QuadsEnd();
		}
		
		//x += 140*Size;
		y += 14;
	}

	Graphics()->ShaderEnd();
	
	
	x = 110;
	y = 27;
	
	// kits & building
	
	int LocalKits = clamp(CustomStuff()->m_LocalKits ,0, 99);
	
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	Graphics()->QuadsBegin();
	
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	RenderTools()->SelectSprite(SPRITE_PICKUP_KIT);
	RenderTools()->DrawSprite(x, y, 26);
	

	float KitSize = 18.0f;
	
	Graphics()->SetColor(0.9f, 0.9f, 0.9f, 1.0f);
	if (LocalKits < 10)
	{
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_0+LocalKits);
		RenderTools()->DrawSprite(x, y, KitSize);
	}
	else
	{
		int Kits1 = (LocalKits - (LocalKits%10))/10;
		int Kits2 = LocalKits%10;
		
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_0+Kits1);
		RenderTools()->DrawSprite(x-4, y, KitSize-2.0f);
		RenderTools()->SelectSprite(SPRITE_GUINUMBER_0+Kits2);
		RenderTools()->DrawSprite(x+4, y, KitSize-2.0f);
	}
	
	Graphics()->QuadsEnd();
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectatorCount) return;
	// draw the box
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(m_Width-180.0f, m_Height-15.0f, 180.0f, 15.0f, 5.0f, CUI::CORNER_TL);
	Graphics()->QuadsEnd();

	// draw the text
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Spectate"), m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW ?
		m_pClient->m_aClients[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_aName : Localize("Free-View"));
	TextRender()->Text(0, m_Width-174.0f, m_Height-13.0f, 8.0f, aBuf, -1);
}

float CHud::BottomReservedHeight() const
{
	// Spectator strip at the very bottom.
	if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		return 15.0f;
	return 0.0f;
}

float CHud::ScoreHudTop() const
{
	// Score HUD occupies [229, 285] when enabled (same as RenderScoreHud).
	if(g_Config.m_ClShowhudScore && m_pClient->m_Snap.m_pGameInfoObj &&
		!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
		return 229.0f;
	return m_Height - BottomReservedHeight();
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
	float StartX = m_Width - BoxWidth;
	float StartY = ScoreHudTop() - BoxHeight - 4.0f;
	if(StartY < 20.0f)
		StartY = 20.0f;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.45f);
	RenderTools()->DrawRoundRectExt(StartX, StartY, BoxWidth, BoxHeight, 5.0f, CUI::CORNER_L);
	Graphics()->QuadsEnd();

	char aBuf[64];
	float y = StartY + 2.0f;
	const float LeftX = StartX + 2.0f;
	const float RightEdge = m_Width - 2.0f;

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
	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f*Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

	if(g_Config.m_ClShowhud)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
			RenderHealthAndAmmo(m_pClient->m_Snap.m_pLocalCharacter);
		else if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW)
				RenderHealthAndAmmo(&m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_Cur);
			RenderSpectatorHud();
		}

		RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		RenderScoreHud();
		RenderObjective();
		RenderStartCountdown();
		RenderReadyUpNotification();
		RenderFps();
		RenderMovementInformation();

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		RenderVoting();
	}
	RenderCursor();
}
