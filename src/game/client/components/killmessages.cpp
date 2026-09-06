

#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/platform_services.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>
#include <game/client/components/build_placement.h>
#include <game/client/components/inventory.h>
#include <game/client/components/scoreboard.h>
#include <game/weapons/weapon_catalog.h>
#include "hud_layout.h"
#include "killmessages.h"

void CKillMessages::OnReset()
{
	m_KillmsgCurrent = 0;
	for(int i = 0; i < MAX_KILLMSGS; i++)
		m_aKillmsgs[i].m_Tick = -100000;
}

void CKillMessages::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		// unpack messages
		CKillMsg Kill;
		Kill.m_VictimID = pMsg->m_Victim;
		Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimID].m_Team;
		str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_VictimID].m_aName, sizeof(Kill.m_aVictimName));
		Kill.m_VictimRenderInfo = m_pClient->m_aClients[Kill.m_VictimID].m_RenderInfo;
		Kill.m_KillerID = pMsg->m_Killer;
		Kill.m_KillerTeam = m_pClient->m_aClients[Kill.m_KillerID].m_Team;
		str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerID].m_aName, sizeof(Kill.m_aKillerName));
		Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerID].m_RenderInfo;
		CAttackSource Source;
		if(!CWeaponCatalog::TryAttackSourceFromProtocol(
			   pMsg->m_SourceKind, pMsg->m_SourceType, pMsg->m_WeaponDefinitionId, pMsg->m_WeaponLevel, &Source))
			return;
		Kill.m_Source = Source;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_Tick = Client()->GameTick();
		const int LocalID = m_pClient->m_Snap.m_LocalClientID;
		if(LocalID >= 0 && (Kill.m_VictimID == LocalID || Kill.m_KillerID == LocalID))
		{
			IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
			if(pPlatform)
			{
				CPlatformTimelineEvent Event;
				mem_zero(&Event, sizeof(Event));
				Event.m_SessionID = (unsigned long long)(uintptr_t)m_pClient;
				Event.m_ServerTick = Kill.m_Tick;
				Event.m_EventType = Kill.m_VictimID == LocalID ? 1 : 2;
				Event.m_ClipPriority =
					Kill.m_VictimID == LocalID ? PLATFORM_TIMELINE_CLIP_STANDARD : PLATFORM_TIMELINE_CLIP_NONE;
				str_copy(Event.m_aIcon,
						 Kill.m_VictimID == LocalID ? "steam_skull" : "steam_starburst",
						 sizeof(Event.m_aIcon));
				str_copy(Event.m_aTitle,
						 Kill.m_VictimID == LocalID ? "You were defeated" : "Elimination",
						 sizeof(Event.m_aTitle));
				str_format(Event.m_aDescription,
						   sizeof(Event.m_aDescription),
						   Kill.m_VictimID == LocalID ? "Defeated by %s" : "Defeated %s",
						   Kill.m_VictimID == LocalID ? Kill.m_aKillerName : Kill.m_aVictimName);
				pPlatform->AddTimelineEvent(Event);
				if(g_Config.m_ClSteamRumble)
					pPlatform->TriggerInputVibration(Kill.m_VictimID == LocalID ? 42000 : 12000,
													 Kill.m_VictimID == LocalID ? 52000 : 26000);
			}
		}

		if(!g_Config.m_ClShowsocial)
		{
			char aBuf[32];
			str_copy(Kill.m_aKillerName,
					 m_pClient->GetPlayerLabel(Kill.m_KillerID, aBuf, sizeof(aBuf)),
					 sizeof(Kill.m_aKillerName));
			str_copy(Kill.m_aVictimName,
					 m_pClient->GetPlayerLabel(Kill.m_VictimID, aBuf, sizeof(aBuf)),
					 sizeof(Kill.m_aVictimName));
		}

		// hide bot names in invasion
		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP)
		{
			if(m_pClient->m_aClients[Kill.m_KillerID].m_IsBot)
				str_copy(Kill.m_aKillerName, "", sizeof(""));

			if(m_pClient->m_aClients[Kill.m_VictimID].m_IsBot)
				str_copy(Kill.m_aVictimName, "", sizeof(""));
		}

		// add the message
		m_KillmsgCurrent = (m_KillmsgCurrent + 1) % MAX_KILLMSGS;
		m_aKillmsgs[m_KillmsgCurrent] = Kill;
	}
}

void CKillMessages::OnRender()
{
	if(!g_Config.m_ClShowKillMessages || m_pClient->m_pInventory->IsVisible() ||
	   m_pClient->m_pScoreboard->Active() || m_pClient->m_pBuildPlacement->PlacementActive() ||
	   m_pClient->m_Snap.m_SpecInfo.m_Active)
		return;

	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;

	Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);
	float StartX = Width * 1.5f - 10.0f;
	// The kill-message map is 1.5x the 1200-unit HUD map. Start below FPS
	// (18 logical units) and finish before the objective card at y=82.
	float y = 108.0f;

	for(int i = 1; i <= MAX_KILLMSGS; i++)
	{
		int r = (m_KillmsgCurrent + i) % MAX_KILLMSGS;
		const int AgeTicks = Client()->GameTick() - m_aKillmsgs[r].m_Tick;
		if(AgeTicks < 0 || AgeTicks > 50 * 10)
			continue;
		const float InAmount = clamp(AgeTicks / 8.0f, 0.0f, 1.0f);
		const float OutAmount = clamp((50.0f * 10.0f - AgeTicks) / 15.0f, 0.0f, 1.0f);
		const float Alpha = min(InAmount, OutAmount);
		const float Eased = 1.0f - (1.0f - InAmount) * (1.0f - InAmount) * (1.0f - InAmount);

		float FontSize = 36.0f;
		const float MaxNameWidth = max(40.0f, min(300.0f, (Width * 1.5f - 260.0f) * 0.5f));
		float KillerNameW = min(
			MaxNameWidth, TextRender()->TextWidth(0, FontSize, m_aKillmsgs[r].m_aKillerName, -1));
		float VictimNameW = min(
			MaxNameWidth, TextRender()->TextWidth(0, FontSize, m_aKillmsgs[r].m_aVictimName, -1));
		auto DrawName = [&](float X, const char *pName)
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, X, y, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = MaxNameWidth;
			Cursor.m_MaxLines = 1;
			TextRender()->TextEx(&Cursor, pName, -1);
		};

		float x = StartX + (1.0f - Eased) * 28.0f;
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);

		int Flags = m_pClient->m_Snap.m_pGameInfoObj ? m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags : 0;

		if(Flags & GAMEFLAG_TEAMS && !(Flags & GAMEFLAG_INFECTION))
		{
			if(m_aKillmsgs[r].m_VictimTeam == TEAM_RED)
				TextRender()->TextColor(250 / 255.0f, 100 / 255.0f, 0, Alpha);

			if(m_aKillmsgs[r].m_VictimTeam == TEAM_BLUE)
				TextRender()->TextColor(0 / 255.0f, 100 / 255.0f, 230 / 255.0f, Alpha);
		}

		// render victim name
		x -= VictimNameW;

		DrawName(x, m_aKillmsgs[r].m_aVictimName);

		// render victim tee
		x -= 32.0f;

		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
		{
			if(m_aKillmsgs[r].m_ModeSpecial & 1)
			{
				Graphics()->BlendNormal();
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
				Graphics()->QuadsBegin();

				if(m_aKillmsgs[r].m_VictimTeam == TEAM_RED)
					RenderTools()->SelectSprite(SPRITE_FLAG_BLUE);
				else
					RenderTools()->SelectSprite(SPRITE_FLAG_RED);

				float Size = 56.0f;
				IGraphics::CQuadItem QuadItem(x, y - 16, Size / 2, Size);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
		}

		// RenderTools()->RenderTee(CAnimState::GetIdle(), &m_aKillmsgs[r].m_VictimRenderInfo, EMOTE_PAIN, vec2(-1,0),
		// vec2(x, y+28));
		RenderTools()->RenderPortrait(&m_aKillmsgs[r].m_VictimRenderInfo, vec2(x, y + 86), 3);

		x -= 36.0f;

		// render weapon
		x -= 48.0f;

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		if(m_aKillmsgs[r].m_Source.m_Kind == EAttackSourceKind::PlayerWeapon)
		{
			RenderTools()->SetShadersForWeapon(m_aKillmsgs[r].m_Source.m_Weapon);
			RenderTools()->RenderWeapon(
				m_aKillmsgs[r].m_Source.m_Weapon, vec2(x + 5, y + 30), vec2(1, 0), 16, true, 0, Alpha, true);
			Graphics()->ShaderEnd();
		}

		/*
		if (m_aKillmsgs[r].m_Weapon >= 0)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_DEATHTYPES].m_Id);
			Graphics()->QuadsBegin();
			//RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[m_aKillmsgs[r].m_Weapon].m_pSpriteBody);
			RenderTools()->SelectSprite(SPRITE_DEATHTYPE1 + m_aKillmsgs[r].m_Weapon);
			//RenderTools()->DrawSprite(x, y+28, 64);
			IGraphics::CQuadItem QuadItem(x, y+28, 96, 96/2);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
		if (m_aKillmsgs[r].m_IsTurret)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_DEATHTYPES].m_Id);
			Graphics()->QuadsBegin();
			RenderTools()->SelectSprite(SPRITE_DEATHTYPE19);
			IGraphics::CQuadItem QuadItem(x, y+28, 96, 96/2);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
		*/
		x -= 56.0f;

		if(m_aKillmsgs[r].m_VictimID != m_aKillmsgs[r].m_KillerID)
		{
			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
			{
				if(m_aKillmsgs[r].m_ModeSpecial & 2)
				{
					Graphics()->BlendNormal();
					Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
					Graphics()->QuadsBegin();

					if(m_aKillmsgs[r].m_KillerTeam == TEAM_RED)
						RenderTools()->SelectSprite(SPRITE_FLAG_BLUE, SPRITE_FLAG_FLIP_X);
					else
						RenderTools()->SelectSprite(SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);

					float Size = 56.0f;
					IGraphics::CQuadItem QuadItem(x - 56, y - 16, Size / 2, Size);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
					Graphics()->QuadsEnd();
				}
			}

			if(Flags & GAMEFLAG_TEAMS && !(Flags & GAMEFLAG_INFECTION))
			{
				if(m_aKillmsgs[r].m_KillerTeam == TEAM_RED)
					TextRender()->TextColor(250 / 255.0f, 100 / 255.0f, 0, Alpha);

				if(m_aKillmsgs[r].m_KillerTeam == TEAM_BLUE)
					TextRender()->TextColor(0 / 255.0f, 100 / 255.0f, 230 / 255.0f, Alpha);
			}

			// render killer tee
			x -= 28.0f;
			// RenderTools()->RenderTee(CAnimState::GetIdle(), &m_aKillmsgs[r].m_KillerRenderInfo, EMOTE_ANGRY,
			// vec2(1,0), vec2(x, y+28));
			RenderTools()->RenderPortrait(&m_aKillmsgs[r].m_KillerRenderInfo, vec2(x, y + 86), 2);
			x -= 36.0f;

			// render killer name
			x -= KillerNameW;
			DrawName(x, m_aKillmsgs[r].m_aKillerName);
		}

		// y += 46.0f;
		y += 52.0f;

		TextRender()->TextColor(1, 1, 1, 1.0f);
	}
}
