#include <engine/graphics.h>
#include <engine/demo.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/gamecore.h> // get_angle
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include <game/client/skelebank.h>

#include <game/client/customstuff.h>

#include <game/client/components/effects.h>

#include "droids.h"

void CDroids::OnReset()
{
}

vec2 CDroids::MixPos(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent)
{
	return mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());
}

void CDroids::RenderWalker(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = MixPos(pPrev, pCurrent);

	if(pCurrent->m_Status != DROIDSTATUS_IDLE)
	{
		CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] = 1.0f;
		CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] = pCurrent->m_Status;
	}

	if(CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] > 0.0f)
	{
		if(CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] == DROIDSTATUS_ELECTRIC)
			RenderTools()->Graphics()->ShaderBegin(SHADER_ELECTRIC,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
		else
			RenderTools()->Graphics()->ShaderBegin(SHADER_DAMAGE,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
	}

	RenderTools()->RenderWalker(Pos,
								pCurrent->m_Anim,
								CustomStuff()->m_MonsterAnim + ItemID * 0.3f,
								pCurrent->m_Dir * -1,
								pCurrent->m_Angle / 2,
								pCurrent->m_Status,
								pCurrent->m_Type);
	RenderTools()->Graphics()->ShaderEnd();

	int Dir = pCurrent->m_Dir;

	// muzzle
	// if (Client()->GameTick() > pCurrent->m_AttackTick + 100)
	{
		float Angle = 0.0f;

		if(Dir > 0)
			Angle = pCurrent->m_Angle / (180 / pi);
		else
			Angle = (180 - pCurrent->m_Angle) / (180 / pi);

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_MUZZLE].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(Angle);
		Graphics()->SetColor(1, 1, 1, 1);

		// muzzle
		float IntraTick = Client()->IntraGameTick();

		int MuzzleDuration = 10;

		// check if we're firing stuff
		{
			// vec2 Dir = GetDirection((int)(Angle*256));

			int Sprite = SPRITE_MUZZLE5_1;

			float Alpha = 0.0f;
			int Phase1Tick = (Client()->GameTick() - pCurrent->m_AttackTick);
			if(Phase1Tick < MuzzleDuration) // duration
			{
				float t = ((((float)Phase1Tick) + IntraTick) / (float)MuzzleDuration);
				Alpha = mix(2.0f, 0.0f, min(1.0f, max(0.0f, t)));
			}

			Sprite += Phase1Tick / 2;
			if(Sprite > SPRITE_MUZZLE5_1 + 3)
				Sprite = SPRITE_MUZZLE5_1 + 3;

			if(Alpha > 0.0f)
			{
				RenderTools()->SelectSprite(Sprite, SPRITE_FLAG_FLIP_X | (Dir < 0 ? SPRITE_FLAG_FLIP_Y : 0));

				float OffsetY = pCurrent->m_Anim < 3 ? -57 : 7;

				vec2 p = Pos + vec2(Dir * 14, OffsetY + 2) + vec2(cosf(Angle), sinf(Angle)) * 37;
				RenderTools()->DrawSprite(p.x, p.y, 54);

				p = Pos + vec2(Dir * 28, OffsetY - 4) + vec2(cosf(Angle), sinf(Angle)) * 49;
				p += vec2(sinf(Angle), 0) * 6 * Dir;
				RenderTools()->DrawSprite(p.x, p.y, 54);
			}
		}

		Graphics()->QuadsEnd();
	}
}

void CDroids::RenderStar(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = MixPos(pPrev, pCurrent);

	CDroidAnim DroidAnim;

	if(pCurrent->m_Status != DROIDSTATUS_IDLE)
	{
		CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] = 1.0f;
		CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] = pCurrent->m_Status;
	}

	if(CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] > 0.0f)
	{
		if(CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] == DROIDSTATUS_ELECTRIC)
			RenderTools()->Graphics()->ShaderBegin(SHADER_ELECTRIC,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
		else
			RenderTools()->Graphics()->ShaderBegin(SHADER_DAMAGE,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
	}

	static float s_LastGameTickTime = Client()->GameTickTime();
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime();
	float Ct = (Client()->PrevGameTick() - pCurrent->m_AttackTick) / (float)SERVER_TICK_SPEED + s_LastGameTickTime;

	DroidAnim.m_aValue[CDroidAnim::VEL_X] = pCurrent->m_Dir * (pCurrent->m_X - pPrev->m_X) / 24.0f;
	DroidAnim.m_aValue[CDroidAnim::BODY_ANGLE] = pCurrent->m_Dir * (pCurrent->m_X - pPrev->m_X) / 64.0f;
	DroidAnim.m_aValue[CDroidAnim::TURRET_ANGLE] = pCurrent->m_Angle;
	DroidAnim.m_Type = pCurrent->m_Type;

	int Anim = 0;
	float Time = CustomStuff()->m_MonsterAnim * 0.3f + ItemID * 0.3f;

	if(Ct > 0.01f && Ct < 0.2f)
	{
		Anim = 1;
		Time = Ct * 1.5f;
	}

	RenderTools()->RenderStarDroid(
		Pos, Anim, Time, pCurrent->m_Dir * -1, pCurrent->m_Angle, pCurrent->m_Status, &DroidAnim);
	RenderTools()->Graphics()->ShaderEnd();

	// effects
	if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 90,
											32 + frandom() * 32,
											vec2(frandom() - frandom(), frandom() - frandom()) * 10.0f);

	m_pClient->m_pEffects->SmokeTrail(DroidAnim.m_aVectorValue[CDroidAnim::THRUST1_POS],
									  DroidAnim.m_aVectorValue[CDroidAnim::THRUST1_VEL] * 600);
	m_pClient->m_pEffects->SmokeTrail(DroidAnim.m_aVectorValue[CDroidAnim::THRUST2_POS],
									  DroidAnim.m_aVectorValue[CDroidAnim::THRUST2_VEL] * 600);

	vec4 DroidLight;
	float DroidLightSize = 0.0f;
	switch(pCurrent->m_Type)
	{
		case DROIDTYPE_TEMPESTSTAR:
			DroidLight = vec4(0.72f, 0.25f, 1.0f, 0.72f);
			DroidLightSize = 125.0f;
			break;
		case DROIDTYPE_KAMIKAZESTAR:
			DroidLight = vec4(1.0f, 0.08f, 0.04f, 0.85f);
			DroidLightSize = 135.0f;
			break;
		case DROIDTYPE_RAILSTAR:
			DroidLight = vec4(0.55f, 1.0f, 1.0f, 0.78f);
			DroidLightSize = 140.0f;
			break;
		case DROIDTYPE_TESLASTAR:
			DroidLight = vec4(0.12f, 0.55f, 1.0f, 0.88f);
			DroidLightSize = 145.0f;
			break;
		default:
			break;
	}
	if(DroidLightSize > 0.0f)
		m_pClient->m_pEffects->SimpleLight(Pos, DroidLight, DroidLightSize);
}

void CDroids::RenderCrawler(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = MixPos(pPrev, pCurrent);

	CDroidAnim *pDroidAnim = CustomStuff()->GetDroidAnim(ItemID);

	if(pCurrent->m_Status != DROIDSTATUS_IDLE && pCurrent->m_Status != DROIDSTATUS_STEALTH)
	{
		CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] = 1.0f;
		CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] = pCurrent->m_Status;
	}

	const bool Stalker = pCurrent->m_Type == DROIDTYPE_STALKERCRAWLER;
	const float StealthAlpha = mix(
		Stalker && pPrev->m_Status == DROIDSTATUS_STEALTH ? 0.35f : 1.0f,
		Stalker && pCurrent->m_Status == DROIDSTATUS_STEALTH ? 0.35f : 1.0f,
		Client()->IntraGameTick());
	if(StealthAlpha < 1.0f)
		RenderTools()->Graphics()->PlayerShaderBegin(0.0f, 0.0f, 0.0f, StealthAlpha, 0.0f, 0.0f, 0.0f);
	else if(CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] > 0.0f)
	{
		if(CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] == DROIDSTATUS_ELECTRIC)
			RenderTools()->Graphics()->ShaderBegin(SHADER_ELECTRIC,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
		else
			RenderTools()->Graphics()->ShaderBegin(SHADER_DAMAGE,
												   CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
	}

	/*
	static float s_LastGameTickTime = Client()->GameTickTime();
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime();
	*/

	// float Ct = (Client()->PrevGameTick()-pCurrent->m_AttackTick)/(float)SERVER_TICK_SPEED + s_LastGameTickTime;

	int Anim = 0;
	float Time = 0.0f;

	pDroidAnim->m_Dir = pCurrent->m_Dir * -1;
	pDroidAnim->m_Pos = Pos;
	pDroidAnim->m_Vel = vec2(pPrev->m_X - pCurrent->m_X, pPrev->m_Y - pCurrent->m_Y);
	pDroidAnim->m_Status = pCurrent->m_Status;
	pDroidAnim->m_Anim = pCurrent->m_Anim;
	pDroidAnim->m_Type = pCurrent->m_Type;
	const bool CycloneAirborne = pCurrent->m_Type == DROIDTYPE_CYCLONECRAWLER &&
		pCurrent->m_Anim == DROIDANIM_JUMPATTACK;
	const float RenderAngle = CycloneAirborne ?
		mix((float)pPrev->m_Angle, (float)pCurrent->m_Angle, Client()->IntraGameTick()) / (180.0f / pi) :
		pDroidAnim->m_DisplayAngle * pCurrent->m_Dir;

	// check bone & slot positions
	RenderTools()->RenderCrawlerDroid(Pos,
									  Anim,
									  Time,
									  pCurrent->m_Dir * -1,
									  RenderAngle,
									  pCurrent->m_Status,
									  pDroidAnim,
									  false);

	// render
	if(!CycloneAirborne)
		RenderTools()->RenderCrawlerLegs(pDroidAnim);
	RenderTools()->RenderCrawlerDroid(Pos,
									  Anim,
									  Time,
									  pCurrent->m_Dir * -1,
									  RenderAngle,
									  pCurrent->m_Status,
									  pDroidAnim);
	RenderTools()->Graphics()->ShaderEnd();

	if((pCurrent->m_Type == DROIDTYPE_CRAWLER ||
		pCurrent->m_Type == DROIDTYPE_SIEGEBREAKERCRAWLER ||
		pCurrent->m_Type == DROIDTYPE_SPLITCRAWLER ||
		pCurrent->m_Type == DROIDTYPE_MENDERCRAWLER ||
		pCurrent->m_Type == DROIDTYPE_STALKERCRAWLER ||
		pCurrent->m_Type == DROIDTYPE_CYCLONECRAWLER) &&
		pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 90,
											32 + frandom() * 32,
											vec2(frandom() - frandom(), frandom() - frandom()) * 10.0f);

	if(pCurrent->m_Type == DROIDTYPE_BOSSCRAWLER && pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 140,
											64 + frandom() * 64,
											vec2(frandom() - frandom(), frandom() - frandom()) * 20.0f);

	if(pCurrent->m_Type == DROIDTYPE_BOSSSPLITTER && pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 120,
											48 + frandom() * 48,
											vec2(frandom() - frandom(), frandom() - frandom()) * 16.0f);

	vec4 DroidLight(0.5f, 1.0f, 1.0f, 0.5f);
	float DroidLightSize = 100.0f;
	switch(pCurrent->m_Type)
	{
		case DROIDTYPE_SIEGEBREAKERCRAWLER:
			DroidLight = vec4(1.0f, 0.28f, 0.08f, 0.75f);
			DroidLightSize = 145.0f;
			break;
		case DROIDTYPE_SPLITCRAWLER:
			DroidLight = vec4(0.72f, 1.0f, 0.18f, 0.65f);
			DroidLightSize = 110.0f;
			break;
		case DROIDTYPE_MENDERCRAWLER:
			DroidLight = vec4(0.2f, 1.0f, 0.35f, 0.7f);
			DroidLightSize = 120.0f;
			break;
		case DROIDTYPE_STALKERCRAWLER:
			DroidLight = vec4(0.38f, 0.12f, 0.65f, 0.42f * StealthAlpha);
			DroidLightSize = 105.0f;
			break;
		case DROIDTYPE_CYCLONECRAWLER:
			DroidLight = vec4(1.0f, 0.62f, 0.08f, CycloneAirborne ? 0.95f : 0.78f);
			DroidLightSize = CycloneAirborne ? 155.0f : 135.0f;
			break;
		default:
			break;
	}
	m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -26), DroidLight, DroidLightSize);
}

void CDroids::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;

	int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

		if(Item.m_Type == NETOBJTYPE_DROID)
		{
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);
			const struct CNetObj_Droid *pDroid = (const CNetObj_Droid *)pData;
			const CNetObj_Droid *pDroidPrev = pPrev ? (const CNetObj_Droid *)pPrev : pDroid;

			switch(pDroid->m_Type)
			{
				case DROIDTYPE_WALKER:
					RenderWalker(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_STAR:
				case DROIDTYPE_TEMPESTSTAR:
				case DROIDTYPE_KAMIKAZESTAR:
				case DROIDTYPE_RAILSTAR:
				case DROIDTYPE_TESLASTAR:
					RenderStar(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_CRAWLER:
				case DROIDTYPE_SIEGEBREAKERCRAWLER:
				case DROIDTYPE_SPLITCRAWLER:
				case DROIDTYPE_MENDERCRAWLER:
				case DROIDTYPE_STALKERCRAWLER:
				case DROIDTYPE_CYCLONECRAWLER:
					RenderCrawler(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_BOSSCRAWLER:
				case DROIDTYPE_BOSSSPLITTER:
					RenderCrawler(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_BOSSSTAR:
					RenderStar(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_BOSSWALKER:
					RenderWalker(pDroidPrev, pDroid, Item.m_ID);
					break;
				default:;
			}
		}
	}
}
