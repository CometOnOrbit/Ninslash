#include <base/math.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/demo.h>
#include <generated/protocol.h>
#include <generated/game_data.h>
#include <engine/shared/config.h>

#include <game/gamecore.h> // get_angle
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/render.h>

#include <game/client/customstuff.h>

#include <game/weapons/weapons.h>
#include <game/weapons/weapon_catalog.h>
#include <game/weapons/forge.h>
#include <game/client/components/flow.h>
#include <game/client/components/effects.h>
#include <game/client/components/sounds.h>
#include <game/client/skelebank.h>

#include "buildings.h"
#include <game/client/weapon_resources.h>
#include "binds.h"

void CBuildings::OnReset()
{
}

void CBuildings::RenderPowerupper(const struct CNetObj_Powerupper *pCurrent)
{
	// draw base
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_POWERUPPER);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y - 12, 96);
	Graphics()->QuadsEnd();

	if(pCurrent->m_Item >= 0)
	{
		// draw background effect
		{
			Graphics()->ShaderBegin(SHADER_SPAWN, 0.5f);
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ITEMS].m_Id);
			Graphics()->QuadsBegin();

			RenderTools()->SelectSprite(SPRITE_ITEM1 + pCurrent->m_Item);

			if(g_Config.m_GfxShaders)
				Graphics()->SetColor(1, 1, 1, 1);
			else
				Graphics()->SetColor(1, 1, 1, 0.3f);

			Graphics()->QuadsSetRotation(0);

			RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y - 64, 96);
			Graphics()->QuadsEnd();
			Graphics()->ShaderEnd();
		}

		// draw pickable item / buff
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ITEMS].m_Id);
			Graphics()->QuadsBegin();

			RenderTools()->SelectSprite(SPRITE_ITEM1 + pCurrent->m_Item);

			Graphics()->SetColor(1, 1, 1, 1);
			Graphics()->QuadsSetRotation(0);

			RenderTools()->DrawSprite(pCurrent->m_X + sin(CustomStuff()->m_SawbladeAngle / 8) * 2.0f,
									  pCurrent->m_Y + cos(CustomStuff()->m_SawbladeAngle / 4) * 4.0f - 64,
									  64);
			Graphics()->QuadsEnd();
		}
	}
}

void CBuildings::RenderSawblade(const struct CNetObj_Building *pCurrent)
{
	/*
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_SAWBLADE);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(CustomStuff()->m_SawbladeAngle);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y, 128+32);

	Graphics()->QuadsEnd();
	*/
}

void CBuildings::RenderDoor1(const struct CNetObj_Building *pCurrent)
{
	float Time = 0.0f;

	int Anim = pCurrent->m_Status & (1 << BSTATUS_ON) ? ANIM_OPENED : ANIM_CLOSED;

	if(Anim == ANIM_CLOSED && pCurrent->m_Status & (1 << BSTATUS_EVENT))
	{
		Anim = ANIM_OPEN;
		if(CustomStuff()->m_DoorTimer == 0.0f)
			CustomStuff()->m_DoorTimer = 0.01f;

		Time = CustomStuff()->m_DoorTimer * 0.5f;
	}
	RenderTools()->Skelebank()->RenderTools()->RenderSkeleton(vec2(pCurrent->m_X, pCurrent->m_Y + 16),
															  ATLAS_DOOR1,
															  RenderTools()->Skelebank()->GetAnimList(Anim),
															  Time,
															  vec2(1.0f, 1.0f) * 0.3f,
															  1,
															  0);
}

void CBuildings::RenderScreen(const struct CNetObj_Building *pCurrent)
{
	float Time = CustomStuff()->m_SawbladeAngle * 0.1f;

	int Anim = ANIM_IDLE1 + (pCurrent->m_X / 32) % 3;

	RenderTools()->RenderSkeleton(vec2(pCurrent->m_X, pCurrent->m_Y + 18),
								  ATLAS_SCREEN,
								  RenderTools()->Skelebank()->GetAnimList(Anim),
								  Time,
								  vec2(1.0f, 1.0f) * 0.7f,
								  0,
								  0);

	vec4 c;
	switch(int(pCurrent->m_X / 32) % 4)
	{
		case 0:
			c = vec4(0.5f, 0.7f, 1.0f, 0.95f);
			break;
		case 1:
			c = vec4(0.4f, 1.0f, 0.4f, 0.95f);
			break;
		case 2:
			c = vec4(1.0f, 0.5f, 0.5f, 0.95f);
			break;
		case 3:
			c = vec4(0.2f, 1.0f, 1.0f, 0.95f);
			break;
		default:
			c = vec4(0.5f, 0.7f, 1.0f, 0.95f);
			break;
	}
	m_pClient->m_pEffects->SimpleLight(vec2(pCurrent->m_X, pCurrent->m_Y - 96), c, vec2(620, 400));
	m_pClient->m_pEffects->SimpleLight(vec2(pCurrent->m_X, pCurrent->m_Y - 88),
		vec4(c.r, c.g, c.b, 0.48f),
		vec2(260, 170),
		false);

	const CNetObj_GameInfo *pGameInfo = m_pClient->m_Snap.m_pGameInfoObj;
	const vec2 ScreenPos(pCurrent->m_X, pCurrent->m_Y);
	if(pGameInfo && pGameInfo->m_ForgeMode >= 2 && CustomStuff()->m_LocalAlive &&
	   distance(CustomStuff()->m_LocalPos, ScreenPos) <= FORGE_SCREEN_RANGE)
	{
		char aInventoryKeys[64];
		m_pClient->m_pBinds->GetKeys("+inventory", aInventoryKeys, sizeof(aInventoryKeys));
		char aHint[128];
		str_format(aHint, sizeof(aHint), Localize("Press %s to open forge"), aInventoryKeys[0] ? aInventoryKeys : "?");
		const float FontSize = 18.0f;
		const float TextWidth = TextRender()->TextWidth(0, FontSize, aHint, -1);
		TextRender()->TextColor(0.2f, 0.9f, 0.35f, 1.0f);
		TextRender()->Text(0, ScreenPos.x - TextWidth * 0.5f, ScreenPos.y - 154.0f, FontSize, aHint, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CBuildings::RenderShop(const CNetObj_Shop *pCurrent)
{
	float Time = CustomStuff()->m_SawbladeAngle * 0.1f;

	int Anim = ANIM_IDLE;

	RenderTools()->RenderSkeleton(vec2(pCurrent->m_X, pCurrent->m_Y + 18),
								  ATLAS_SHOP,
								  RenderTools()->Skelebank()->GetAnimList(Anim),
								  Time,
								  vec2(1.0f, 1.0f) * 0.7f,
								  0,
								  0);

	m_pClient->m_pEffects->SimpleLight(
		vec2(pCurrent->m_X, pCurrent->m_Y - 74), vec4(0.5f, 0.75f, 1.0f, 0.82f), vec2(360, 460));

	if(CustomStuff()->m_LocalAlive &&
	   abs(CustomStuff()->m_LocalPos.x - pCurrent->m_X) <= 100 &&
	   abs(CustomStuff()->m_LocalPos.y - pCurrent->m_Y) <= 100)
	{
		char aInventoryKeys[64];
		m_pClient->m_pBinds->GetKeys("+inventory", aInventoryKeys, sizeof(aInventoryKeys));
		char aHint[128];
		str_format(aHint, sizeof(aHint), Localize("Press %s to open shop"), aInventoryKeys[0] ? aInventoryKeys : "?");
		const float FontSize = 18.0f;
		const float TextWidth = TextRender()->TextWidth(0, FontSize, aHint, -1);
		TextRender()->TextColor(0.35f, 0.75f, 1.0f, 1.0f);
		TextRender()->Text(0, pCurrent->m_X - TextWidth * 0.5f, pCurrent->m_Y - 120.0f, FontSize, aHint, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CBuildings::RenderGenerator(const struct CNetObj_Building *pCurrent, const CNetObj_Building *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

	// render
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->SelectSprite(SPRITE_GENERATOR);
	RenderTools()->DrawSprite(Pos.x, Pos.y, 192);

	float c = sin(CustomStuff()->m_SawbladeAngle * 0.25f) * 0.3f + 0.7f;
	vec4 GeneratorColor(0.5f, c, 1.0f, 1.0f);

	// team color
	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		int Flags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		int Team = pCurrent->m_Team;

		if((Flags & GAMEFLAG_TEAMS) && !(Flags & GAMEFLAG_INFECTION))
		{
			if(Team == TEAM_RED)
				GeneratorColor = vec4(1.0f, c, 0.0f, 1.0f);
			else if(Team == TEAM_BLUE)
				GeneratorColor = vec4(0.0f, c, 1.0f, 1.0f);
		}
		else if(Team == TEAM_RED)
		{
			vec4 pc = CustomStuff()->m_LocalColor;
			GeneratorColor = vec4(pc.r, pc.g, pc.b, 1.0f);
		}
	}
	Graphics()->SetColor(GeneratorColor.r, GeneratorColor.g, GeneratorColor.b, GeneratorColor.a);

	RenderTools()->SelectSprite(SPRITE_GENERATOR_COLOR);
	RenderTools()->DrawSprite(Pos.x, Pos.y, 192);
	Graphics()->QuadsEnd();

	bool Repair = pCurrent->m_Status & (1 << BSTATUS_REPAIR);
	float Time = Pos.x * 0.432f + pCurrent->m_Y * 0.2354f + CustomStuff()->m_SawbladeAngle * 0.1f;

	if(Repair)
	{
		Time += CustomStuff()->m_SawbladeAngle * 0.15f;

		if(frandom() < 0.15f)
			m_pClient->m_pEffects->Electrospark(vec2(Pos.x, Pos.y) +
													vec2(frandom() - frandom(), frandom() - frandom()) * 50.0f,
												40 + frandom() * 20,
												vec2(0, 0));
	}

	// repair sprite
	if(Repair && (CustomStuff()->LocalTick() / 12 + (pCurrent->m_X / 8 + pCurrent->m_Y / 32)) % 8 < 4)
	{
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_STATUS_REPAIR);
		Graphics()->SetColor(1, 1, 1, 1);

		Graphics()->QuadsSetRotation(0);

		RenderTools()->DrawSprite(Pos.x - 34, Pos.y - 52, 52);
		Graphics()->QuadsEnd();
	}

	const vec2 GeneratorLightPos = Pos + vec2(0, -96);
	m_pClient->m_pEffects->SimpleLight(
		GeneratorLightPos, vec4(GeneratorColor.r, GeneratorColor.g, GeneratorColor.b, 0.95f), vec2(720, 720));
	m_pClient->m_pEffects->SimpleLight(
		GeneratorLightPos, vec4(GeneratorColor.r, GeneratorColor.g, GeneratorColor.b, 0.42f), vec2(300, 300), false);
}

void CBuildings::RenderOverseerShieldNode(const CNetObj_Building *pCurrent)
{
	// Overseer Core shield node visual only.
	const vec2 Pos(pCurrent->m_X, pCurrent->m_Y);
	const vec4 Accent(0.20f, 0.78f, 1.0f, 1.0f);

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PVE_SHIELD_RELAY].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0.0f);
	RenderTools()->SelectSprite(SPRITE_PVE_SHIELD_RELAY);
	RenderTools()->DrawSprite(Pos.x, Pos.y - 12, 176.0f);
	Graphics()->QuadsEnd();

	Graphics()->ShaderBegin(SHADER_ELECTRIC, 0.45f);
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PVE_SHIELD_RELAY].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.45f, 0.85f, 1.0f, 0.35f);
	RenderTools()->SelectSprite(SPRITE_PVE_SHIELD_RELAY);
	RenderTools()->DrawSprite(Pos.x, Pos.y - 12, 176.0f);
	Graphics()->QuadsEnd();
	Graphics()->ShaderEnd();

	m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -16), vec4(Accent.r, Accent.g, Accent.b, 0.95f), 260.0f);
	m_pClient->m_pEffects->SimpleLight(
		Pos + vec2(0, -16), vec4(Accent.r, Accent.g, Accent.b, 0.48f), vec2(130, 130), false);
}

void CBuildings::RenderSwitch(const struct CNetObj_Building *pCurrent)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_SWITCH);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y - 14, 96);
	Graphics()->QuadsEnd();

	Graphics()->ShaderBegin(SHADER_ELECTRIC, frandom());
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(pCurrent->m_Status & (1 << BSTATUS_ON) ? SPRITE_SWITCH_ON : SPRITE_SWITCH_OFF);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y - 14, 96);
	Graphics()->QuadsEnd();
	Graphics()->ShaderEnd();
}

void CBuildings::RenderMine(const struct CNetObj_Building *pCurrent)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_MINE2);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y, 48);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderElectromine(const struct CNetObj_Building *pCurrent)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_MINE1);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y, 48);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderBarrel(const CNetObj_Building *pCurrent, const CNetObj_Building *pPrev, int SpriteVariation)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	// RenderTools()->SelectSprite(SPRITE_BARREL1+(pCurrent->m_X/32+pCurrent->m_Y)%8);
	RenderTools()->SelectSprite(SPRITE_BARREL1 + SpriteVariation);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(Pos.x, Pos.y - 16, 96 + 12);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderPowerBarrel(const CNetObj_Building *pCurrent, const CNetObj_Building *pPrev, int SpriteVariation)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_POWERBARREL + SpriteVariation);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(Pos.x, Pos.y - 16, 96 + 12);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderFlametrap(const CNetObj_Building *pCurrent, const CNetObj_Building *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());
	// render the actual building in buildings2

	// render frame
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	int s = pCurrent->m_Status;
	bool Flip = s & (1 << BSTATUS_MIRROR);

	RenderTools()->SelectSprite(SPRITE_FLAMETRAP, Flip ? SPRITE_FLAG_FLIP_X : 0);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	IGraphics::CQuadItem QuadItem(Pos.x + (Flip ? -13 : 13), Pos.y, 64, 64);
	Graphics()->QuadsDraw(&QuadItem, 1);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderBase(const struct CNetObj_Building *pCurrent)
{
	/*
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_KIT_BASE);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	//RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y+8, 64*1.3f);

	float Scale = 0.8f;
	IGraphics::CQuadItem QuadItem(pCurrent->m_X, pCurrent->m_Y+7.5f, 96*Scale, 32*Scale); // -5
	Graphics()->QuadsDraw(&QuadItem, 1);

	Graphics()->QuadsEnd();
	*/
}

void CBuildings::RenderReactor(const struct CNetObj_Building *pCurrent)
{
	int Anim = ANIM_IDLE;
	int s = pCurrent->m_Status;
	bool Repair = s & (1 << BSTATUS_REPAIR);
	const bool Objective = s & (1 << BSTATUS_ON);

	float Time = pCurrent->m_X * 0.432f + pCurrent->m_Y * 0.2354f + CustomStuff()->m_SawbladeAngle * 0.1f;

	if(Repair)
		Time += CustomStuff()->m_SawbladeAngle * 0.15f;

	const float ObjectivePulse = 0.5f + 0.5f * sinf((float)Client()->LocalTime() * 4.0f);
	const vec4 ReactorLightColor(0.25f, 0.85f, 1.0f, 1.0f);
	m_pClient->m_pEffects->SimpleLight(vec2(pCurrent->m_X, pCurrent->m_Y - 30),
		ReactorLightColor,
		Objective ? 640 + ObjectivePulse * 100 : 440);
	m_pClient->m_pEffects->SimpleLight(vec2(pCurrent->m_X, pCurrent->m_Y - 24),
		vec4(ReactorLightColor.r, ReactorLightColor.g, ReactorLightColor.b, 0.52f),
		Objective ? vec2(320, 280) : vec2(240, 210),
		false);
	// m_pClient->m_pEffects->SimpleLight(vec2(pCurrent->m_X, pCurrent->m_Y-0), 320);

	if(Objective)
	{
		const vec2 Center(pCurrent->m_X, pCurrent->m_Y + 12.0f);
		const float Radius = 86.0f + ObjectivePulse * 14.0f;
		const int Segments = 40;
		IGraphics::CLineItem aLines[Segments + 5];
		for(int i = 0; i < Segments; i++)
		{
			const float A1 = i * 2.0f * pi / Segments;
			const float A2 = (i + 1) * 2.0f * pi / Segments;
			aLines[i] = IGraphics::CLineItem(Center.x + cosf(A1) * Radius,
											 Center.y + sinf(A1) * Radius,
											 Center.x + cosf(A2) * Radius,
											 Center.y + sinf(A2) * Radius);
		}
		aLines[Segments] = IGraphics::CLineItem(Center.x, Center.y - Radius, Center.x, Center.y - 300.0f);
		aLines[Segments + 1] =
			IGraphics::CLineItem(Center.x - 22.0f, Center.y - 250.0f, Center.x + 22.0f, Center.y - 250.0f);
		aLines[Segments + 2] =
			IGraphics::CLineItem(Center.x - 14.0f, Center.y - 272.0f, Center.x + 14.0f, Center.y - 272.0f);
		aLines[Segments + 3] =
			IGraphics::CLineItem(Center.x - Radius - 12.0f, Center.y, Center.x - Radius + 12.0f, Center.y);
		aLines[Segments + 4] =
			IGraphics::CLineItem(Center.x + Radius - 12.0f, Center.y, Center.x + Radius + 12.0f, Center.y);

		Graphics()->BlendNormal();
		Graphics()->TextureSet(-1);
		Graphics()->LinesBegin();
		Graphics()->SetColor(0.18f, 0.92f, 1.0f, 0.48f + ObjectivePulse * 0.34f);
		Graphics()->LinesDraw(aLines, Segments + 5);
		Graphics()->LinesEnd();
	}

	RenderTools()->RenderSkeleton(vec2(pCurrent->m_X, pCurrent->m_Y + 16 + 50),
								  ATLAS_REACTOR,
								  RenderTools()->Skelebank()->GetAnimList(Anim),
								  Time,
								  vec2(1.0f, 1.0f) * 0.8f,
								  1,
								  0);

	// damage mask
	if(Repair)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_REACTOR_DAMAGE].m_Id);
		Graphics()->QuadsBegin();

		RenderTools()->SelectSprite(SPRITE_REACTOR_DAMAGE);

		Graphics()->SetColor(1, 1, 1, 1);
		Graphics()->QuadsSetRotation(0);

		RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y, 70);

		Graphics()->QuadsEnd();
	}

	// repair sprite
	if(Repair && (CustomStuff()->LocalTick() / 12 + (pCurrent->m_X / 8 + pCurrent->m_Y / 32)) % 8 < 4)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_STATUS_REPAIR);
		Graphics()->SetColor(1, 1, 1, 1);

		Graphics()->QuadsSetRotation(0);

		RenderTools()->DrawSprite(pCurrent->m_X - 20, pCurrent->m_Y - 102, 52);
		Graphics()->QuadsEnd();
	}

	// int f = CustomStuff()->m_SawbladeAngle * 100;

	if(Repair && frandom() < 0.15f)
		m_pClient->m_pEffects->Electrospark(vec2(pCurrent->m_X, pCurrent->m_Y - 10 + frandom() * 30 - frandom() * 30) +
												vec2(frandom() - frandom(), frandom() - frandom()) * 20.0f,
											20 + frandom() * 20,
											vec2(0, 0));

	/*
	if (Repair && CustomStuff()->LocalTick()%30 == 1)
		m_pClient->m_pEffects->Electrospark(vec2(pCurrent->m_X, pCurrent->m_Y-30-frandom()*10)+vec2(frandom()-frandom(),
	frandom()-frandom()) * 20.0f, 20+frandom()*20, vec2(0, 0)); if (Repair && CustomStuff()->LocalTick()%30 == 11)
		m_pClient->m_pEffects->Electrospark(vec2(pCurrent->m_X, pCurrent->m_Y+20-frandom()*10)+vec2(frandom()-frandom(),
	frandom()-frandom()) * 20.0f, 20+frandom()*20, vec2(0, 0));
	*/
}

void CBuildings::RenderTeslacoil(const CNetObj_Building *pCurrent, const CNetObj_Building *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());
	int Anim = ANIM_IDLE;
	int s = pCurrent->m_Status;
	bool Repair = s & (1 << BSTATUS_REPAIR);

	s = pCurrent->m_Status;
	int FlipY = (s & (1 << BSTATUS_MIRROR)) ? -1 : 1;

	float Time = Pos.x * 0.432f + Pos.y * 0.2354f + CustomStuff()->m_SawbladeAngle * 0.06f;

	if(Repair)
		Time += CustomStuff()->m_SawbladeAngle * 0.1f;

	// m_pClient->m_pEffects->Light(vec2(pCurrent->m_X, pCurrent->m_Y-30), 320);
	// m_pClient->m_pEffects->Light(vec2(pCurrent->m_X, pCurrent->m_Y-0), 320);

	RenderTools()->RenderSkeleton(vec2(Pos.x, Pos.y + 20 * FlipY),
								  ATLAS_TESLACOIL,
								  RenderTools()->Skelebank()->GetAnimList(Anim),
								  Time,
								  vec2(1.0f, 1.0f * FlipY) * 0.55f,
								  1,
								  0,
								  pCurrent->m_Team);

	// repair sprite
	if(Repair && (CustomStuff()->LocalTick() / 12 + int(Pos.x / 8 + Pos.y / 32)) % 8 < 4)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_STATUS_REPAIR);
		Graphics()->SetColor(1, 1, 1, 1);

		Graphics()->QuadsSetRotation(0);

		RenderTools()->DrawSprite(Pos.x - 20, Pos.y - 30 - 70 * FlipY, 52);
		Graphics()->QuadsEnd();
	}

	// int f = CustomStuff()->m_SawbladeAngle * 100;

	if(Repair && frandom() < 0.15f)
		m_pClient->m_pEffects->Electrospark(vec2(Pos.x, Pos.y - (10 + frandom() * 30) * FlipY) +
												vec2(frandom() - frandom(), frandom() - frandom()) * 20.0f,
											20 + frandom() * 20,
											vec2(0, 0));

	const vec2 TeslaLightPos(pCurrent->m_X, pCurrent->m_Y - 80 * FlipY);
	m_pClient->m_pEffects->SimpleLight(TeslaLightPos, vec4(1.0f, 0.3f, 0.6f, 0.95f), vec2(440, 440));
	m_pClient->m_pEffects->SimpleLight(TeslaLightPos, vec4(1.0f, 0.3f, 0.6f, 0.5f), vec2(180, 180), false);
}

void CBuildings::RenderDestroyedReactor(const struct CNetObj_Building *pCurrent)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_REACTOR_DESTROYED);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y - 12, 183);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderStand(const CNetObj_Building *pCurrent, const CNetObj_Building *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

	int s = pCurrent->m_Status;
	bool Flip = s & (1 << BSTATUS_MIRROR);
	int FlipY = Flip ? -1 : 1;

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_STAND, Flip ? SPRITE_FLAG_FLIP_Y : 0);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	// RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y-24, 96*1.3f);

	float Scale = 0.8f;
	IGraphics::CQuadItem QuadItem(Pos.x, Pos.y - 25 * FlipY, 96 * Scale, 128 * Scale); // -37
	Graphics()->QuadsDraw(&QuadItem, 1);

	Graphics()->QuadsEnd();

	// skip tip if local player is undead
	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		int Flags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;

		if(Flags & GAMEFLAG_INFECTION && CustomStuff()->m_LocalTeam == TEAM_BLUE)
			return;
	}

	CResolvedWeaponProfile LocalWeaponProfile;
	const bool HasTurretWeapon = CWeaponCatalog::TryResolve(CustomStuff()->m_LocalWeapon, &LocalWeaponProfile) &&
								 LocalWeaponProfile.m_Combat.m_ValidForTurret;
	// render drop weapon tip for local player
	if(distance(CustomStuff()->m_LocalPos, vec2(Pos.x, Pos.y + 15)) < 60 && HasTurretWeapon)
	{
		char aDropKeys[64];
		m_pClient->m_pBinds->GetKeys("+dropweapon", aDropKeys, sizeof(aDropKeys));
		TextRender()->TextColor(0.2f, 0.8f, 0.2f, 1);
		TextRender()->Text(0, Pos.x + 22, Pos.y - 30 - 60 * FlipY, 32, aDropKeys[0] ? aDropKeys : "", -1);
		TextRender()->TextColor(1, 1, 1, 1);

		// render weapon cloning cost
		int Cost = CustomStuff()->m_LocalWeapon.m_Level + 1;

		char aBuf[8];
		str_format(aBuf, sizeof(aBuf), "%u", Cost);

		if(CustomStuff()->m_LocalKits >= Cost)
			TextRender()->TextColor(0.0f, 0.9f, 0.0f, 1);
		else
			TextRender()->TextColor(0.9f, 0.0f, 0.0f, 1);

		TextRender()->Text(0, Pos.x + 22 + 30, Pos.y - 21 - 60 * FlipY, 22, aBuf, -1);
		float tw = TextRender()->TextWidth(0, 22, aBuf, -1);
		TextRender()->TextColor(1, 1, 1, 1);

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0);
		RenderTools()->SelectSprite(SPRITE_PICKUP_KIT);
		RenderTools()->DrawSprite(Pos.x + 22 + 30 + 20 + tw, Pos.y - 5 - 60 * FlipY, 32);
		Graphics()->QuadsEnd();
		Graphics()->TextureSet(-1);
	}
}

void CBuildings::RenderLazer(const struct CNetObj_Building *pCurrent)
{
	// danger line
	Graphics()->ShaderBegin(SHADER_DEATHRAY, 0.3f);
	Graphics()->TextureSet(-1);
	RenderTools()->SelectSprite(-1);
	Graphics()->QuadsBegin();

	Graphics()->SetColor(0, 0, 0, 0.15f);

	vec2 p1 = vec2(pCurrent->m_X, pCurrent->m_Y + 8);
	// vec2 p2 = vec2(pCurrent->m_X, pCurrent->m_Y+1200);

	float y = 64.0f;
	bool Break = false;

	float s1 = 2.0f;

	while(y < 1200.0f && !Break)
	{
		vec2 p2 = vec2(pCurrent->m_X, pCurrent->m_Y + y);
		if(Collision()->IntersectLine(p1, p2, 0x0, &p2))
			Break = true;

		float s2 = frandom() * 6.0f;

		IGraphics::CFreeformItem FreeFormItem(p1.x - s1, p1.y, p1.x + s1, p1.y, p1.x - s2, p2.y, p1.x + s2, p2.y);

		Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);

		s1 = s2;
		p1 = p2;
		y += 64.0f;
	}

	/*
	Collision()->IntersectLine(p1, p2, 0x0, &p2);

	IGraphics::CFreeformItem FreeFormItem(
		p1.x-2, p1.y,
		p1.x+2, p1.y,
		p1.x-2, p2.y,
		p1.x+2, p2.y);

	Graphics()->QuadsDrawFreeform(&FreeFormItem, 1);
	*/
	Graphics()->QuadsEnd();
	Graphics()->ShaderEnd();

	// deathray
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();

	RenderTools()->SelectSprite(SPRITE_LAZER);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y + 15, 64);

	Graphics()->QuadsEnd();
}

void CBuildings::RenderTurret(const CNetObj_Turret *pCurrent, const CNetObj_Turret *pPrev)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());
	float Scale = 0.8f;

	int s = pCurrent->m_Status;
	bool Flip = s & (1 << BSTATUS_MIRROR);
	int FlipY = Flip ? -1 : 1;

	// stand
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_STAND, Flip ? SPRITE_FLAG_FLIP_Y : 0);

	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);

	// RenderTools()->DrawSprite(pCurrent->m_X, pCurrent->m_Y-24-9, 96*1.3f);

	IGraphics::CQuadItem Stand(Pos.x, Pos.y - 25 * FlipY, 96 * Scale, 128 * Scale); // -37
	Graphics()->QuadsDraw(&Stand, 1);
	Graphics()->QuadsEnd();

	CWeaponSpec WeaponSpec;
	CResolvedWeaponProfile WeaponProfile;
	if(!CWeaponCatalog::TryFromProtocol(pCurrent->m_WeaponDefinitionId, pCurrent->m_WeaponLevel, &WeaponSpec) ||
	   !CWeaponCatalog::TryResolve(WeaponSpec, &WeaponProfile))
		return;
	float Angle = (pCurrent->m_Angle + 90) / (180 / pi);
	vec2 p =
		Pos + vec2(cosf(Angle) * 12, sinf(Angle) * 12 + (-40 - 9) * FlipY); //+ vec2(cosf(Angle)*90, sinf(Angle)*90-71);
	vec2 Dir = GetDirection((int)(Angle * 256));

	// render weapon
	RenderTools()->SetShadersForWeapon(WeaponSpec);

	const int CustomHeldTexture = g_WeaponResources.HeldTexture(WeaponSpec);
	Graphics()->TextureSet(CustomHeldTexture >= 0 ? CustomHeldTexture : g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(Angle);

	RenderTools()->RenderWeapon(WeaponSpec, p, Dir, WEAPON_GAME_SIZE);

	Graphics()->QuadsEnd();

	// render muzzle
	if(WeaponProfile.m_Combat.m_FiringType != WFT_HOLD)
	{
		CustomStuff()->SetTurretMuzzle(ivec2(Pos.x, Pos.y), pCurrent->m_AttackTick, WeaponSpec);

		CTurretMuzzle Muzzle = CustomStuff()->GetTurretMuzzle(ivec2(Pos.x, Pos.y));

		if(Muzzle.m_Weapon.IsValid())
		{
			CResolvedWeaponProfile MuzzleProfile;
			if(CWeaponCatalog::TryResolve(Muzzle.m_Weapon, &MuzzleProfile))
			{
				const int CustomMuzzleTexture = g_WeaponResources.MuzzleTexture(Muzzle.m_Weapon);
				Graphics()->TextureSet(CustomMuzzleTexture >= 0 ? CustomMuzzleTexture
																: g_pData->m_aImages[IMAGE_MUZZLE].m_Id);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(Angle);
				vec2 Moff = MuzzleProfile.m_Visual.m_MuzzleOffset + vec2(-3, -6);
				if(CustomMuzzleTexture >= 0)
					Graphics()->QuadsSetSubset(0, 0, 1, 1);
				else
					RenderTools()->SelectSprite(SPRITE_MUZZLE1_1 + Muzzle.m_Muzzle * 4 + Muzzle.m_Time * 4,
												SPRITE_FLAG_FLIP_X);
				vec2 DirY(-Dir.y, Dir.x);
				vec2 MuzzlePos = p + Dir * Moff.x + DirY * Moff.y;
				if(CustomMuzzleTexture >= 0)
				{
					IGraphics::CQuadItem MuzzleQuad(MuzzlePos.x, MuzzlePos.y, 60, 40);
					Graphics()->QuadsDraw(&MuzzleQuad, 1);
				}
				else
					RenderTools()->DrawSprite(MuzzlePos.x, MuzzlePos.y, 60);
				Graphics()->QuadsEnd();
			}
		}
	}

	Graphics()->ShaderEnd();

	vec2 Offset = vec2(0, 0);

	// fastener
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SPRITE_TURRET_FASTENER, Dir.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);
	Graphics()->SetColor(1, 1, 1, 1);

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		int Flags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		int Team = pCurrent->m_Team;

		if((Flags & GAMEFLAG_TEAMS) && !(Flags & GAMEFLAG_INFECTION))
		{
			if(Team == TEAM_RED)
				Graphics()->SetColor(1.0f, 0.8f, 0.0f, 1.0f);
			else if(Team == TEAM_BLUE)
				Graphics()->SetColor(0.3f, 0.5f, 1.0f, 1.0f);
		}
	}
	// local player's turret
	else if(pCurrent->m_Team == TEAM_BLUE)
	{
		vec4 c = CustomStuff()->m_LocalColor;
		Graphics()->SetColor(c.r, c.g, c.b, 1);
	}

	Graphics()->QuadsSetRotation(Angle);

	RenderTools()->DrawSprite(Pos.x + Offset.x, Pos.y + (-40 - 9) * FlipY + Offset.y, 64 * 1.3f);
	Graphics()->QuadsEnd();

	// no ammo & low health status
	s = pCurrent->m_Status;
	bool Repair = s & (1 << BSTATUS_REPAIR);

	// s = pCurrent->m_Status;
	// bool NoAmmo = s & (1<<BSTATUS_NOPE);
	bool NoAmmo = false;

	if(Repair && int(CustomStuff()->LocalTick() / 12 + (Pos.x / 8 + Pos.y / 32)) % 8 < 4)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BUILDINGS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_STATUS_REPAIR);
		Graphics()->SetColor(1, 1, 1, 1);

		Graphics()->QuadsSetRotation(0);

		RenderTools()->DrawSprite(Pos.x - (NoAmmo ? 24 : 0), Pos.y - 50 - 52 * FlipY, 52);
		Graphics()->QuadsEnd();
	}
}

void CBuildings::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;

	int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

		if(Item.m_Type == NETOBJTYPE_TURRET)
		{
			const struct CNetObj_Turret *pTurret = (const CNetObj_Turret *)pData;
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);
			RenderTurret(pTurret, pPrev ? (const CNetObj_Turret *)pPrev : pTurret);
		}
		else if(Item.m_Type == NETOBJTYPE_POWERUPPER)
			RenderPowerupper((const CNetObj_Powerupper *)pData);
		else if(Item.m_Type == NETOBJTYPE_SHOP)
			RenderShop((const CNetObj_Shop *)pData);
		else if(Item.m_Type == NETOBJTYPE_BUILDING)
		{
			const struct CNetObj_Building *pBuilding = (const CNetObj_Building *)pData;
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);

			switch(pBuilding->m_Type)
			{
				case BUILDING_SAWBLADE:
					RenderSawblade(pBuilding);
					break;

				case BUILDING_MINE1:
					RenderMine(pBuilding);
					break;

				case BUILDING_MINE2:
					RenderElectromine(pBuilding);
					break;

				case BUILDING_BARREL:
					RenderBarrel(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding, 0);
					break;

				case BUILDING_BARREL2:
					RenderBarrel(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding, 1);
					break;

				case BUILDING_BARREL3:
					RenderBarrel(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding, 2);
					break;

				case BUILDING_POWERBARREL:
					RenderPowerBarrel(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding, 0);
					break;

				case BUILDING_POWERBARREL2:
					RenderPowerBarrel(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding, 1);
					break;

				case BUILDING_LAZER:
					RenderLazer(pBuilding);
					break;

				case BUILDING_FLAMETRAP:
					RenderFlametrap(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding);
					break;

				case BUILDING_BASE:
					RenderBase(pBuilding);
					break;

				case BUILDING_STAND:
					RenderStand(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding);
					break;

				case BUILDING_REACTOR:
					RenderReactor(pBuilding);
					break;

				case BUILDING_TESLACOIL:
					RenderTeslacoil(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding);
					break;

				case BUILDING_REACTOR_DESTROYED:
					RenderDestroyedReactor(pBuilding);
					break;

				case BUILDING_SWITCH:
					RenderSwitch(pBuilding);
					break;

				case BUILDING_DOOR1:
					RenderDoor1(pBuilding);
					break;

				case BUILDING_GENERATOR:
					RenderGenerator(pBuilding, pPrev ? (const CNetObj_Building *)pPrev : pBuilding);
					break;
				case BUILDING_PVE_SHIELD_NODE:
					RenderOverseerShieldNode(pBuilding);
					break;

				case BUILDING_SCREEN:
					RenderScreen(pBuilding);
					break;

					/*
					case BUILDING_SHOP:
						RenderShop(pBuilding);
						break;
						*/

				default:;
			};

			// m_pClient->m_pEffects->Light(vec2(pBuilding->m_X, pBuilding->m_Y), 512);
		}
	}
}
