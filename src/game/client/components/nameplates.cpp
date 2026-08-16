
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/customstuff.h>
#include <game/client/customstuff/playerinfo.h>

#include <game/client/gameclient.h>
#include "nameplates.h"
#include "controls.h"

void CNamePlates::RenderNameplate(const CNetObj_Character *pPrevChar,
								  const CNetObj_Character *pPlayerChar,
								  const CNetObj_PlayerInfo *pPlayerInfo)
{
	const int ClientID = pPlayerInfo->m_ClientID;

	// Match the tee render position (players.cpp): local uses prediction.
	CNetObj_Character Prev = *pPrevChar;
	CNetObj_Character Player = *pPlayerChar;
	float IntraTick = Client()->IntraGameTick();

	if(pPlayerInfo->m_Local && g_Config.m_ClPredict && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter &&
		   !(m_pClient->m_Snap.m_pGameInfoObj &&
			 m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) &&
		   m_pClient->m_PredictedChar.IsReady() && m_pClient->m_PredictedPrevChar.IsReady())
		{
			m_pClient->m_PredictedChar.Write(&Player);
			m_pClient->m_PredictedPrevChar.Write(&Prev);
			IntraTick = Client()->PredIntraGameTick();
		}
	}

	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	// Prefer the position written by RenderPlayer this frame so nameplates stay glued to the tee.
	CPlayerInfo *pCustomPlayerInfo = &CustomStuff()->m_aPlayerInfo[ClientID];
	if(pCustomPlayerInfo->m_InUse)
		Position = pCustomPlayerInfo->m_Pos;

	// Default cl_nameplates_size=40 → ~26px (original formula).
	float FontSize = 18.0f + 20.0f * g_Config.m_ClNameplatesSize / 100.0f;

	float v = pCustomPlayerInfo->m_EffectIntensity[EFFECT_INVISIBILITY];

	if((CustomStuff()->m_LocalTeam == pPlayerInfo->m_Team && m_pClient->m_Snap.m_pGameInfoObj &&
		m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS) ||
	   CustomStuff()->m_LocalTeam == TEAM_SPECTATORS)
		v = 0.0f;

	if(pPlayerInfo->m_Team == 0 && m_pClient->m_Snap.m_pGameInfoObj &&
	   m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP)
		v = 0.0f;

	if((!pPlayerInfo->m_Local || g_Config.m_ClNamePlatesOwn) &&
	   (!CustomStuff()->IsBot(ClientID) || !(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_COOP)))
	{
		float a = 1;
		if(g_Config.m_ClNameplatesAlways == 0)
			a = clamp(1 - powf(distance(m_pClient->m_pControls->m_TargetPos, Position) / 200.0f, 16.0f), 0.0f, 1.0f);

		char aDisplayName[96];
		char aNameBuf[MAX_NAME_LENGTH];
		const char *pPlayerName = m_pClient->GetPlayerLabel(ClientID, aNameBuf, sizeof(aNameBuf));
		if(g_Config.m_ClShowsocial && g_Config.m_ClNamePlatesFriendMark && ClientID >= 0 && ClientID < MAX_CLIENTS &&
		   m_pClient->m_aClients[ClientID].m_Friend)
			str_format(aDisplayName, sizeof(aDisplayName), "\xe2\x99\xa5 %s", pPlayerName);
		else
			str_copy(aDisplayName, pPlayerName, sizeof(aDisplayName));
		const char *pName = aDisplayName;
		if(!g_Config.m_ClShowsocial && !pName[0])
			return;
		float tw = TextRender()->TextWidth(0, FontSize, pName, -1);

		// Sit just above the head (no extra -36 offset that floated names too high).
		float NameY = Position.y - FontSize - 76.0f;

		a *= 1.0f - v;

		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.5f * a);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, a);

		if(g_Config.m_ClNameplatesTeamcolors && m_pClient->m_Snap.m_pGameInfoObj &&
		   m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
		{
			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_INFECTION)
			{
				if(pPlayerInfo->m_Team == TEAM_RED)
					TextRender()->TextColor(255 / 255.0f, 200 / 255.0f, 200 / 255.0f, a);
				else if(pPlayerInfo->m_Team == TEAM_BLUE)
					TextRender()->TextColor(66 / 255.0f, 66 / 255.0f, 66 / 255.0f, a);
			}
			else
			{
				if(pPlayerInfo->m_Team == TEAM_RED)
					TextRender()->TextColor(250 / 255.0f, 100 / 255.0f, 0, a);
				else if(pPlayerInfo->m_Team == TEAM_BLUE)
					TextRender()->TextColor(0 / 255.0f, 100 / 255.0f, 230 / 255.0f, a);
			}
		}

		if(g_Config.m_ClNamePlatesIds)
		{
			char aIdBuf[32];
			str_format(aIdBuf, sizeof(aIdBuf), "%d", ClientID);
			float IdFontSize = FontSize * g_Config.m_ClNamePlatesIdsSize / 100.0f;
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.5f * a);
			TextRender()->TextColor(1.0f, 1.0f, 0.5f, a);
			TextRender()->Text(0, Position.x - tw / 2.0f, NameY - IdFontSize * 1.2f, IdFontSize, aIdBuf, -1);
		}

		TextRender()->Text(0, Position.x - tw / 2.0f, NameY, FontSize, pName, -1);

		if(g_Config.m_ClShowsocial && g_Config.m_ClNamePlatesClan && ClientID >= 0 && ClientID < MAX_CLIENTS &&
		   m_pClient->m_aClients[ClientID].m_aClan[0])
		{
			float ClanFontSize = FontSize * g_Config.m_ClNamePlatesClanSize / 100.0f;
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.5f * a);
			TextRender()->TextColor(0.5f, 0.5f, 0.5f, a);
			TextRender()->Text(0,
							   Position.x - tw / 2.0f,
							   NameY + FontSize * 1.2f,
							   ClanFontSize,
							   m_pClient->m_aClients[ClientID].m_aClan,
							   -1);
		}

		TextRender()->TextColor(1, 1, 1, 1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
	}
	else
		CustomStuff()->m_LocalTeam = pPlayerInfo->m_Team;
}

void CNamePlates::OnRender()
{
	if(!g_Config.m_ClNameplates)
		return;

	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paPlayerInfos[i];
		if(pInfo)
		{
			RenderNameplate(&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
							&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
							pInfo);
		}
	}
}
