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

void CDroids::RenderWalker(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

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
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

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
}

void CDroids::RenderCrawler(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());

	CDroidAnim *pDroidAnim = CustomStuff()->GetDroidAnim(ItemID);

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

	// check bone & slot positions
	RenderTools()->RenderCrawlerDroid(Pos,
									  Anim,
									  Time,
									  pCurrent->m_Dir * -1,
									  pDroidAnim->m_DisplayAngle * pCurrent->m_Dir,
									  pCurrent->m_Status,
									  pDroidAnim,
									  false);

	// render
	RenderTools()->RenderCrawlerLegs(pDroidAnim);
	RenderTools()->RenderCrawlerDroid(Pos,
									  Anim,
									  Time,
									  pCurrent->m_Dir * -1,
									  pDroidAnim->m_DisplayAngle * pCurrent->m_Dir,
									  pCurrent->m_Status,
									  pDroidAnim);
	RenderTools()->Graphics()->ShaderEnd();

	if((pCurrent->m_Type == DROIDTYPE_CRAWLER || pCurrent->m_Type == DROIDTYPE_LUMINOUS_PREDATOR) &&
		pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 90,
											32 + frandom() * 32,
											vec2(frandom() - frandom(), frandom() - frandom()) * 10.0f);

	if((pCurrent->m_Type == DROIDTYPE_BOSSCRAWLER || pCurrent->m_Type == DROIDTYPE_ABYSSAL_HEART) &&
		pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 140,
											64 + frandom() * 64,
											vec2(frandom() - frandom(), frandom() - frandom()) * 20.0f);

	if(pCurrent->m_Type == DROIDTYPE_BOSSSPLITTER && pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 120,
											48 + frandom() * 48,
											vec2(frandom() - frandom(), frandom() - frandom()) * 16.0f);

	vec4 DroidLight(0.5f, 1.0f, 1.0f, 0.5f);
	float DroidLightSize = 100.0f;
	if(pCurrent->m_Type == DROIDTYPE_LUMINOUS_PREDATOR)
	{
		DroidLight = vec4(0.22f, 0.95f, 0.78f, 0.72f);
		DroidLightSize = 128.0f;
	}
	else if(pCurrent->m_Type == DROIDTYPE_ABYSSAL_HEART)
	{
		DroidLight = vec4(0.35f, 0.65f, 1.0f, 0.86f);
		DroidLightSize = 220.0f;
	}
	m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -26), DroidLight, DroidLightSize);
}

void CDroids::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;

	auto RenderLostProtocol = [&](const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
	{
		int Atlas = ATLAS_LOST_PROTOCOL_BULWARK;
		float RenderYOffset = -20.0f;
		switch(pCurrent->m_Type)
		{
			case DROIDTYPE_BULWARK:
			case DROIDTYPE_REEF_SENTINEL:
				Atlas = ATLAS_LOST_PROTOCOL_BULWARK;
				RenderYOffset = -26.0f;
				break;
			case DROIDTYPE_ASSEMBLER:
				Atlas = ATLAS_LOST_PROTOCOL_ASSEMBLER;
				RenderYOffset = -24.0f;
				break;
			case DROIDTYPE_SABOTEUR:
				Atlas = ATLAS_LOST_PROTOCOL_SABOTEUR;
				RenderYOffset = -18.0f;
				break;
			case DROIDTYPE_RAILGUNNER:
				Atlas = ATLAS_LOST_PROTOCOL_RAILGUNNER;
				RenderYOffset = -24.0f;
				break;
			case DROIDTYPE_SIEGE_ENGINE:
				Atlas = ATLAS_LOST_PROTOCOL_SIEGE_ENGINE;
				RenderYOffset = -28.0f;
				break;
			case DROIDTYPE_OVERSEER_CORE:
				Atlas = ATLAS_LOST_PROTOCOL_OVERSEER_CORE;
				RenderYOffset = -22.0f;
				break;
			default:
				return;
		}

		const vec2 WorldPos =
			mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick());
		const vec2 Pos = WorldPos + vec2(0.0f, RenderYOffset);
		float AimDelta = (float)pCurrent->m_Angle - (float)pPrev->m_Angle;
		while(AimDelta > 180.0f)
			AimDelta -= 360.0f;
		while(AimDelta < -180.0f)
			AimDelta += 360.0f;
		const float TargetAim = (float)pPrev->m_Angle + AimDelta * Client()->IntraGameTick();
		CDroidAnim *pAnimState = CustomStuff()->GetDroidAnim(ItemID);
		const bool ResetAnim = !pAnimState->m_RenderInitialized || pAnimState->m_Type != pCurrent->m_Type ||
							   distance(pAnimState->m_Pos, WorldPos) > 256.0f;
		if(ResetAnim)
		{
			pAnimState->m_LocomotionTime = ItemID * 0.13f;
			pAnimState->m_SmoothedAimAngle = TargetAim;
			pAnimState->m_RenderInitialized = true;
		}
		else
		{
			const float MoveDelta = distance(pAnimState->m_Pos, WorldPos);
			if(pCurrent->m_Anim == 1 && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
				pAnimState->m_LocomotionTime += min(0.16f, MoveDelta * 0.0095f);
			else if(MoveDelta > 0.35f && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
				pAnimState->m_LocomotionTime += min(0.12f, MoveDelta * 0.0075f);
			float SmoothDelta = TargetAim - pAnimState->m_SmoothedAimAngle;
			while(SmoothDelta > 180.0f)
				SmoothDelta -= 360.0f;
			while(SmoothDelta < -180.0f)
				SmoothDelta += 360.0f;
			pAnimState->m_SmoothedAimAngle += SmoothDelta * 0.28f;
		}
		pAnimState->m_Dir = pCurrent->m_Dir * -1;
		pAnimState->m_Pos = WorldPos;
		pAnimState->m_Vel = vec2(pCurrent->m_X - pPrev->m_X, pCurrent->m_Y - pPrev->m_Y);
		pAnimState->m_Status = pCurrent->m_Status;
		pAnimState->m_Anim = pCurrent->m_Anim == 1 ? DROIDANIM_ATTACK : DROIDANIM_IDLE;
		pAnimState->m_Type = pCurrent->m_Type;
		const float AimAngle = pAnimState->m_SmoothedAimAngle;
		const float DeathAge = (Client()->PrevGameTick() - pCurrent->m_AttackTick + Client()->IntraGameTick()) /
							   (float)Client()->GameTickSpeed();
		const float AttackAge = (Client()->PrevGameTick() - pCurrent->m_AttackTick + Client()->IntraGameTick()) /
								(float)Client()->GameTickSpeed();
		const char *pBaseAnim = pCurrent->m_Anim == 1 ? "move" : (pCurrent->m_Anim == 2 ? "fly" : "idle");
		const float BaseTime = pCurrent->m_Anim == 1 ? pAnimState->m_LocomotionTime
													 : CustomStuff()->m_MonsterAnim * 0.28f + ItemID * 0.13f;
		const char *pOverlayAnim = 0;
		float OverlayTime = 0.0f;
		if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		{
			pOverlayAnim = "destroyed";
			// Spine timelines loop by default. Clamp before the final keyframe so a
			// dead unit cannot snap upright while its server body is settling.
			OverlayTime = clamp(DeathAge, 0.0f, 0.79f);
		}
		else if(pCurrent->m_Status == DROIDSTATUS_ELECTRIC)
		{
			pOverlayAnim = "emp";
			OverlayTime = CustomStuff()->m_MonsterAnim * 0.55f;
		}
		else if(pCurrent->m_Status == DROIDSTATUS_HURT)
		{
			pOverlayAnim = "hit";
			OverlayTime = max(0.0f, AttackAge);
		}
		else if(AttackAge >= 0.0f && AttackAge < 0.7f)
		{
			pOverlayAnim = "attack";
			OverlayTime = AttackAge;
		}

		if(pCurrent->m_Status != DROIDSTATUS_IDLE)
		{
			CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] = 1.0f;
			CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] = pCurrent->m_Status;
		}
		if(CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] > 0.0f)
		{
			if(CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] == DROIDSTATUS_ELECTRIC)
				Graphics()->ShaderBegin(SHADER_ELECTRIC, CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
			else
				Graphics()->ShaderBegin(SHADER_DAMAGE, CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS]);
		}

		// The crawler-style gait is the base animation. Sparse combat timelines
		// override only their authored bones, preserving continuous foot motion.
		if(pOverlayAnim)
			RenderTools()->RenderSkeleton(Pos,
										  Atlas,
										  pOverlayAnim,
										  OverlayTime,
										  vec2(1.0f, 1.0f),
										  -pCurrent->m_Dir,
										  AimAngle,
										  -1,
										  pBaseAnim,
										  BaseTime);
		else
			RenderTools()->RenderSkeleton(
				Pos, Atlas, pBaseAnim, BaseTime, vec2(1.0f, 1.0f), -pCurrent->m_Dir, AimAngle);
		Graphics()->ShaderEnd();
		if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		{
			const float Radius = pCurrent->m_Type == DROIDTYPE_SIEGE_ENGINE ? 130.0f : 80.0f;
			m_pClient->m_pEffects->Electrospark(WorldPos + vec2(frandom() - frandom(), frandom() - frandom()) *
															   frandom() * Radius,
												30.0f + frandom() * Radius * 0.35f,
												vec2(frandom() - frandom(), frandom() - frandom()) * 12.0f);
		}
	};

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
					RenderStar(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROIDTYPE_CRAWLER:
				case DROIDTYPE_LUMINOUS_PREDATOR:
				case DROIDTYPE_ABYSSAL_HEART:
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
				case DROIDTYPE_BULWARK:
				case DROIDTYPE_REEF_SENTINEL:
				case DROIDTYPE_ASSEMBLER:
				case DROIDTYPE_SABOTEUR:
				case DROIDTYPE_RAILGUNNER:
				case DROIDTYPE_SIEGE_ENGINE:
				case DROIDTYPE_OVERSEER_CORE:
					RenderLostProtocol(pDroidPrev, pDroid, Item.m_ID);
					break;
				default:;
			}
		}
	}
}
