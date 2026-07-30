#include <base/math.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/serverbrowser.h>

#include <game/pve_roguelite.h>
#include <game/weapons.h>
#include <generated/game_data.h>
#include <game/client/render.h>

#include <game/client/components/camera.h>
#include <game/client/components/effects.h>

#include <game/gamecore.h>
#include "radar.h"

void CRadar::RenderRadar(const CNetObj_Radar *pCurrent, const CNetObj_Radar *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_TargetX, pPrev->m_TargetY),
				   vec2(pCurrent->m_TargetX, pCurrent->m_TargetY),
				   Client()->IntraGameTick());
	vec2 CameraPos = m_pClient->m_pCamera->m_Center;
	float a = GetAngle(Pos - CameraPos);

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	const bool HordeDefenseArea =
		pCurrent->m_Type == RADAR_REACTOR &&
		(str_comp(ServerInfo.m_aGameType, "Horde") == 0 || str_comp(ServerInfo.m_aGameType, "HORDE") == 0);
	const bool InvasionReactorObjective =
		pCurrent->m_Type == RADAR_REACTOR &&
		(str_comp(ServerInfo.m_aGameType, "Invasion") == 0 || str_comp(ServerInfo.m_aGameType, "Tutorial") == 0 ||
		 str_comp(ServerInfo.m_aGameType, "coop") == 0 || str_comp(ServerInfo.m_aGameType, "tutorial") == 0 ||
		 str_comp(ServerInfo.m_aGameType, "INV") == 0 || str_comp(ServerInfo.m_aGameType, "TUT") == 0);
	if(HordeDefenseArea || InvasionReactorObjective)
	{
		const float ZoneRadius = HordeDefenseArea ? (float)PVE_HORDE_DEFENSE_RADIUS : 220.0f;
		const bool Inside = m_pClient->m_Snap.m_pLocalCharacter &&
							(HordeDefenseArea ? distance(m_pClient->m_LocalCharacterPos, Pos) <= ZoneRadius
											  : (fabs(m_pClient->m_LocalCharacterPos.x - Pos.x) < 220.0f &&
												 fabs(m_pClient->m_LocalCharacterPos.y - Pos.y) < 240.0f));
		const float Pulse = 0.5f + 0.5f * sinf((float)Client()->LocalTime() * (Inside ? 4.0f : 2.5f));
		IGraphics::CLineItem aLines[40];
		int NumLines = 0;
		const int Segments = 48;
		for(int i = 0; i < Segments; i += 2)
		{
			const float A1 = i * 2.0f * pi / Segments;
			const float A2 = (i + 1) * 2.0f * pi / Segments;
			aLines[NumLines++] = IGraphics::CLineItem(Pos.x + cosf(A1) * ZoneRadius,
													  Pos.y + sinf(A1) * ZoneRadius,
													  Pos.x + cosf(A2) * ZoneRadius,
													  Pos.y + sinf(A2) * ZoneRadius);
		}
		for(int i = 0; i < 4; i++)
		{
			const float A = i * pi * 0.5f;
			aLines[NumLines++] = IGraphics::CLineItem(Pos.x + cosf(A) * (ZoneRadius - 14.0f),
													  Pos.y + sinf(A) * (ZoneRadius - 14.0f),
													  Pos.x + cosf(A) * (ZoneRadius + 14.0f),
													  Pos.y + sinf(A) * (ZoneRadius + 14.0f));
		}
		Graphics()->BlendNormal();
		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();
		if(InvasionReactorObjective && Inside)
			Graphics()->SetColor(0.20f, 0.95f, 1.0f, 0.55f + Pulse * 0.25f);
		else if(InvasionReactorObjective)
			Graphics()->SetColor(0.15f, 0.75f, 1.0f, 0.28f + Pulse * 0.12f);
		else
			Graphics()->SetColor(Inside ? 0.25f : 0.18f,
								 Inside ? 1.0f : 0.72f,
								 Inside ? 0.48f : 0.58f,
								 (Inside ? 0.60f : 0.28f) + Pulse * 0.10f);
		Graphics()->LinesDraw(aLines, NumLines);
		Graphics()->LinesEnd();

		// Ground marker cross at objective center for Invasion hold/reactors/turrets.
		if(InvasionReactorObjective)
		{
			const float Cross = 28.0f + Pulse * 6.0f;
			IGraphics::CLineItem aCross[2] = {
				IGraphics::CLineItem(Pos.x - Cross, Pos.y, Pos.x + Cross, Pos.y),
				IGraphics::CLineItem(Pos.x, Pos.y - Cross, Pos.x, Pos.y + Cross),
			};
			Graphics()->LinesBegin();
			Graphics()->SetColor(0.35f, 1.0f, 1.0f, 0.45f + Pulse * 0.35f);
			Graphics()->LinesDraw(aCross, 2);
			Graphics()->LinesEnd();
		}
	}
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());

	vec2 SPos = vec2(cos(a), sin(a)) * (Graphics()->ScreenHeight() / 2.1f);

	vec2 RPos = CameraPos + SPos;

	if(distance(RPos, CameraPos) > distance(Pos, CameraPos))
		RPos = Pos;

	RPos += vec2(Graphics()->ScreenWidth() / 2, Graphics()->ScreenHeight() / 2) - CameraPos;

	if((pCurrent->m_Type == RADAR_CHARACTER || pCurrent->m_Type == RADAR_HUMAN) && abs(Pos.x - CameraPos.x) < 1000 &&
	   abs(Pos.y - CameraPos.y) < 800)
	{
	}
	else
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_RADAR].m_Id);
		Graphics()->QuadsBegin();

		float ca = min(1.0f, distance(Pos, CameraPos) * 0.001f);

		if(HordeDefenseArea)
			ca *= 0.5f;
		else if(InvasionReactorObjective)
			ca = max(ca, 0.82f);

		if(pCurrent->m_Type == RADAR_CHARACTER || pCurrent->m_Type == RADAR_HUMAN)
			Graphics()->QuadsSetRotation(a);
		else
			Graphics()->QuadsSetRotation(0);

		Graphics()->SetColor(0, 0, 0, ca);
		RenderTools()->SelectSprite(SPRITE_RADAR1 + pCurrent->m_Type);
		RenderTools()->DrawSprite(RPos.x, RPos.y, InvasionReactorObjective ? 94 : 82);
		Graphics()->SetColor(
			InvasionReactorObjective ? 0.35f : 1.0f, InvasionReactorObjective ? 0.95f : 1.0f, 1.0f, ca);
		RenderTools()->DrawSprite(RPos.x, RPos.y, InvasionReactorObjective ? 80 : 70);

		Graphics()->QuadsEnd();
	}

	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

void CRadar::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;

	int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

		if(Item.m_Type == NETOBJTYPE_RADAR)
		{
			const struct CNetObj_Radar *pCurrent = (const CNetObj_Radar *)pData;
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);

			RenderRadar(pCurrent, pPrev ? (const CNetObj_Radar *)pPrev : pCurrent);
		}
	}
}
