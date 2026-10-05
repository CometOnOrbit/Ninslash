#include <game/industrial_boss.h>
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
#include <game/client/droid_visual.h>
#include <game/foundry_warden.h>
#include <game/abyss_angler.h>
#include <game/skitter_matriarch.h>
#include "matriarch_rig.h"

static_assert((int)SKELETON_FOUNDRY_WARDEN == (int)ATLAS_FOUNDRY_WARDEN, "warden skeleton/atlas index");
static_assert((int)SKELETON_ABYSS_ANGLER == (int)ATLAS_ABYSS_ANGLER, "angler skeleton/atlas index");

static_assert((int)SKELETON_SKITTER_MATRIARCH == (int)ATLAS_SKITTER_MATRIARCH, "matriarch rig index");

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

	const CDroidVisual &StarVisual = DroidVisual(pCurrent->m_Type);
	if(StarVisual.m_LightSize > 0.0f)
		m_pClient->m_pEffects->SimpleLight(Pos, StarVisual.m_Light, StarVisual.m_LightSize);
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

	const CDroidVisual &CrawlerVisual = DroidVisual(pCurrent->m_Type);
	const bool Stalker = CrawlerVisual.m_Stealth;
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
	const bool CycloneAirborne = CrawlerVisual.m_AirSpin &&
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

	vec4 DroidLight = CrawlerVisual.m_Light;
	float DroidLightSize = CrawlerVisual.m_LightSize;
	if(CrawlerVisual.m_Stealth)
	{
		DroidLight = vec4(0.38f, 0.12f, 0.65f, 0.42f * StealthAlpha);
		DroidLightSize = 105.0f;
	}
	else if(CrawlerVisual.m_AirSpin)
	{
		DroidLight = vec4(1.0f, 0.62f, 0.08f, CycloneAirborne ? 0.95f : 0.78f);
		DroidLightSize = CycloneAirborne ? 155.0f : 135.0f;
	}
	m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -26), DroidLight, DroidLightSize);
}

void CDroids::RenderWarden(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = MixPos(pPrev, pCurrent);
	if(pCurrent->m_Status != DROIDSTATUS_IDLE && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
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

	CDroidAnim *pDroidAnim = CustomStuff()->GetDroidAnim(ItemID);
	pDroidAnim->m_Dir = pCurrent->m_Dir * -1;
	pDroidAnim->m_Pos = Pos;
	pDroidAnim->m_Vel = vec2(pCurrent->m_X - pPrev->m_X, pCurrent->m_Y - pPrev->m_Y);
	pDroidAnim->m_Status = pCurrent->m_Status;
	pDroidAnim->m_Anim = pCurrent->m_Anim;
	pDroidAnim->m_Type = pCurrent->m_Type;

	const int Act = WardenActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
	const float Time = WardenClipTime(Act, pCurrent->m_AttackTick, Client()->GameTick(), Client()->IntraGameTick());
	pDroidAnim->OnWardenClip(Act, Time);
	const float Scale = DroidVisual(pCurrent->m_Type).m_Scale;
	RenderTools()->RenderSkeleton(Pos + vec2(0, 64),
								  ATLAS_FOUNDRY_WARDEN,
								  WardenAct(Act).m_pClip,
								  Time,
								  vec2(Scale, Scale),
								  pCurrent->m_Dir * -1,
								  0.0f,
								  -1,
								  0,
								  0.0f,
								  pDroidAnim);
	RenderTools()->Graphics()->ShaderEnd();

	const float JetSpeed = length(pDroidAnim->m_Vel);
	const vec2 Jet = JetSpeed > 6.0f ? -pDroidAnim->m_Vel / JetSpeed : vec2(0.0f, 1.0f);
	for(; pDroidAnim->m_JetPuffs > 0; pDroidAnim->m_JetPuffs--)
		m_pClient->m_pEffects->Flame(pDroidAnim->m_aLegPos[rand() % 4], Jet * (500.0f + frandom() * 300.0f), 0.9f, true);
	if(Act == WARDEN_ACT_CHARGE)
		for(int i = 0; i < 4; i++)
			m_pClient->m_pEffects->SimpleLight(pDroidAnim->m_aLegPos[i], vec4(1.0f, 0.55f, 0.15f, 0.7f), 70.0f);

	const CNetObj_BossStatus *pStatus = (const CNetObj_BossStatus *)Client()->SnapFindItem(
		IClient::SNAP_CURRENT, NETOBJTYPE_BOSSSTATUS, ItemID);
	if(pStatus && pStatus->m_ArmOut)
	{
		const CNetObj_BossStatus *pPrevStatus = (const CNetObj_BossStatus *)Client()->SnapFindItem(
			IClient::SNAP_PREV, NETOBJTYPE_BOSSSTATUS, ItemID);
		vec2 Tip = vec2(pStatus->m_ArmX, pStatus->m_ArmY);
		if(pPrevStatus && pPrevStatus->m_ArmOut)
			Tip = mix(vec2(pPrevStatus->m_ArmX, pPrevStatus->m_ArmY), Tip, Client()->IntraGameTick());
		RenderTools()->RenderWardenArm(Pos + vec2(pCurrent->m_Dir * 80.0f, -72.0f), Tip, Scale);
	}

	if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 140,
											64 + frandom() * 64,
											vec2(frandom() - frandom(), frandom() - frandom()) * 20.0f);

	const CDroidVisual &Visual = DroidVisual(pCurrent->m_Type);
	m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -20), Visual.m_Light, Visual.m_LightSize);
}

void CDroids::RenderAngler(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	vec2 Pos = MixPos(pPrev, pCurrent);
	if(pCurrent->m_Status != DROIDSTATUS_IDLE && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
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

	CDroidAnim *pDroidAnim = CustomStuff()->GetDroidAnim(ItemID);
	pDroidAnim->m_Dir = pCurrent->m_Dir * -1;
	pDroidAnim->m_Pos = Pos;
	pDroidAnim->m_Vel = vec2(pCurrent->m_X - pPrev->m_X, pCurrent->m_Y - pPrev->m_Y);
	pDroidAnim->m_Status = pCurrent->m_Status;
	pDroidAnim->m_Anim = pCurrent->m_Anim;
	pDroidAnim->m_Type = pCurrent->m_Type;

	const int Act = AnglerActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
	const float Time = AnglerClipTime(Act, pCurrent->m_AttackTick, Client()->GameTick(), Client()->IntraGameTick());
	pDroidAnim->OnAnglerClip(Act, Time);
	const float Scale = DroidVisual(pCurrent->m_Type).m_Scale;
	const vec2 Origin = Pos;
	const vec2 DrawScale(Scale, Scale);
	RenderTools()->RenderSkeleton(Origin,
								  ATLAS_ABYSS_ANGLER,
								  AnglerAct(Act).m_pClip,
								  Time,
								  DrawScale,
								  pCurrent->m_Dir * -1,
								  0.0f,
								  -1,
								  0,
								  0.0f,
								  pDroidAnim);
	RenderTools()->Graphics()->ShaderEnd();

	const vec2 Lure = RenderTools()->SkeletonBonePos(
		ATLAS_ABYSS_ANGLER, "lure_tip", Origin, DrawScale, pCurrent->m_Dir * -1, pDroidAnim->m_BodyTilt);
	const float Glow = Act == ANGLER_ACT_LURE || Act == ANGLER_ACT_ROAR ? 1.0f : 0.55f;
	m_pClient->m_pEffects->SimpleLight(Lure, vec4(0.45f, 0.95f, 1.0f, Glow), 80.0f + Glow * 70.0f);

	const CNetObj_BossStatus *pStatus = (const CNetObj_BossStatus *)Client()->SnapFindItem(
		IClient::SNAP_CURRENT, NETOBJTYPE_BOSSSTATUS, ItemID);
	if(pStatus && pStatus->m_ArmOut)
		m_pClient->m_pEffects->SpriteSmoke(vec2(pStatus->m_ArmX, pStatus->m_ArmY) +
											   vec2(frandom() - frandom(), frandom() - frandom()) * 18.0f,
										   18.0f + frandom() * 10.0f,
										   vec4(0.55f, 0.95f, 1.0f, 0.35f));

	if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 120,
											48 + frandom() * 48,
											vec2(frandom() - frandom(), frandom() - frandom()) * 16.0f);

	const CDroidVisual &Visual = DroidVisual(pCurrent->m_Type);
	m_pClient->m_pEffects->SimpleLight(Pos, Visual.m_Light, Visual.m_LightSize);
}

static CMatriarchRig s_aMatriarchRig[MAX_DROIDS];

void CDroids::RenderMatriarch(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	const vec2 Pos = MixPos(pPrev, pCurrent);
	CMatriarchRig &Rig = s_aMatriarchRig[ItemID % MAX_DROIDS];
	if(Rig.m_ItemID != ItemID || Client()->GameTick() - Rig.m_LastTick > 25)
	{
		Rig.Reset();
		Rig.m_ItemID = ItemID;
	}
	Rig.m_LastTick = Client()->GameTick();

	if(pCurrent->m_Status != DROIDSTATUS_IDLE && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
	{
		CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] = 1.0f;
		CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] = pCurrent->m_Status;
	}

	const CNetObj_BossStatus *pStatus = (const CNetObj_BossStatus *)Client()->SnapFindItem(
		IClient::SNAP_CURRENT, NETOBJTYPE_BOSSSTATUS, ItemID);
	const CNetObj_BossStatus *pPrevStatus = (const CNetObj_BossStatus *)Client()->SnapFindItem(
		IClient::SNAP_PREV, NETOBJTYPE_BOSSSTATUS, ItemID);

	CMatriarchRig::CInput In;
	In.m_Pos = Pos;
	In.m_Vel = vec2(pCurrent->m_X - pPrev->m_X, pCurrent->m_Y - pPrev->m_Y);
	In.m_Dir = pCurrent->m_Dir >= 0 ? 1 : -1;
	In.m_Act = MatriarchActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
	const CMatriarchAct &Clip = MatriarchAct(In.m_Act);
	In.m_Time = BossClipTime(Clip.m_Duration, Clip.m_Loop, pCurrent->m_AttackTick, Client()->GameTick(), Client()->IntraGameTick());
	In.m_Exposed = pStatus && pStatus->m_ArmOut;
	In.m_Aim = pStatus ? vec2(pStatus->m_ArmX, pStatus->m_ArmY) : Pos;
	if(pStatus && pPrevStatus)
		In.m_Aim = mix(vec2(pPrevStatus->m_ArmX, pPrevStatus->m_ArmY), In.m_Aim, Client()->IntraGameTick());
	In.m_Phase = pStatus ? pStatus->m_Phase : 0;
	In.m_FangsBroken = pStatus && pStatus->m_Part1 <= 0;
	In.m_LegsBroken = pStatus && pStatus->m_Part2 <= 0;
	In.m_SacBroken = pStatus && pStatus->m_Part3 <= 0;
	// Bosses soak sustained fire; keep the flash readable instead of painting the whole rig red.
	In.m_Hurt = CustomStuff()->m_DroidDamageIntensity[ItemID % MAX_DROIDS] * 0.55f;

	Rig.Update(m_pClient->Collision(), In, Client()->RenderFrameTime());

	if(In.m_Hurt > 0.0f)
	{
		if(CustomStuff()->m_DroidDamageType[ItemID % MAX_DROIDS] == DROIDSTATUS_ELECTRIC)
			RenderTools()->Graphics()->ShaderBegin(SHADER_ELECTRIC, In.m_Hurt);
		else
			RenderTools()->Graphics()->ShaderBegin(SHADER_DAMAGE, In.m_Hurt);
	}
	Rig.Render(RenderTools(), Graphics(), ATLAS_SKITTER_MATRIARCH, In);
	RenderTools()->Graphics()->ShaderEnd();

	// Footfalls and landings kick up dust.
	for(; Rig.m_Landed > 0; Rig.m_Landed--)
		m_pClient->m_pEffects->SpriteSmoke(Rig.FootPos(rand() % 6) + vec2(frandom() - 0.5f, -0.2f) * 20.0f, 20.0f + frandom() * 14.0f,
			vec4(0.45f, 0.42f, 0.38f, 0.35f));

	const float Eye = In.m_Act == MATRIARCH_ACT_ROAR || In.m_Act == MATRIARCH_ACT_POUNCE ? 0.95f : 0.6f;
	m_pClient->m_pEffects->SimpleLight(Rig.HeadPos(), vec4(0.55f, 1.0f, 0.3f, Eye), 70.0f + Eye * 40.0f);
	if(!In.m_SacBroken)
	{
		const float Glow = In.m_Exposed ? 0.75f + 0.25f * sinf(Client()->LocalTime() * 14.0f) : 0.35f;
		m_pClient->m_pEffects->SimpleLight(Rig.SacPos(), vec4(0.5f, 1.0f, 0.2f, Glow), In.m_Exposed ? 170.0f : 90.0f);
		if(In.m_Exposed && frandom() < 0.3f)
			m_pClient->m_pEffects->SpriteSmoke(Rig.SacPos() + vec2(frandom() - 0.5f, frandom() - 0.5f) * 30.0f, 14.0f, vec4(0.5f, 1.0f, 0.3f, 0.4f));
	}
	else if(frandom() < 0.2f)
		m_pClient->m_pEffects->SpriteSmoke(Rig.SacPos(), 18.0f, vec4(0.25f, 0.25f, 0.25f, 0.35f));
	if(In.m_Act == MATRIARCH_ACT_SPIT && frandom() < 0.5f)
		m_pClient->m_pEffects->SpriteSmoke(Rig.MouthPos(), 10.0f + frandom() * 8.0f, vec4(0.55f, 1.0f, 0.25f, 0.5f));
	if(In.m_Act == MATRIARCH_ACT_POUNCE && In.m_Time < 0.42f)
		m_pClient->m_pEffects->SimpleLight(Rig.HeadPos(), vec4(1.0f, 0.95f, 0.3f, 0.6f), 120.0f);

	if(pCurrent->m_Status == DROIDSTATUS_TERMINATED)
		m_pClient->m_pEffects->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 150,
			48 + frandom() * 48, vec2(frandom() - frandom(), frandom() - frandom()) * 18.0f);

	const CDroidVisual &Visual = DroidVisual(pCurrent->m_Type);
	m_pClient->m_pEffects->SimpleLight(Pos, Visual.m_Light, Visual.m_LightSize);
}

void CDroids::RenderMatriarchShot(const CNetObj_BossShot *pShot, vec2 Pos, int ItemID)
{
	const vec2 Vel = vec2(pShot->m_VelX, pShot->m_VelY) / 100.0f;
	const vec4 White(1.0f, 1.0f, 1.0f, 1.0f);
	if(pShot->m_Kind == MATRIARCH_SHOT_GLOB)
	{
		const float Wobble = 1.0f + 0.12f * sinf(Client()->LocalTime() * 30.0f + ItemID);
		RenderTools()->RenderAtlasSpriteEx(ATLAS_SKITTER_MATRIARCH, "glob", Pos, vec2(34.0f * Wobble, 32.0f / Wobble),
			atan2f(Vel.y, Vel.x), false, false, White);
		if(frandom() < 0.5f)
			m_pClient->m_pEffects->SpriteSmoke(Pos - Vel * 1.5f, 10.0f, vec4(0.55f, 1.0f, 0.25f, 0.45f));
		m_pClient->m_pEffects->SimpleLight(Pos, vec4(0.5f, 1.0f, 0.2f, 0.75f), 70.0f);
	}
	else if(pShot->m_Kind == MATRIARCH_SHOT_PUDDLE)
	{
		const float Life = (float)pShot->m_VelX;
		const float Alpha = clamp(Life / 30.0f, 0.0f, 1.0f);
		const float Grow = clamp((MATRIARCH_PUDDLE_LIFE - Life) / 8.0f, 0.3f, 1.0f);
		const float W = (MATRIARCH_PUDDLE_RADIUS * 2.0f + 24.0f) * Grow;
		RenderTools()->RenderAtlasSpriteEx(ATLAS_SKITTER_MATRIARCH, "puddle", Pos + vec2(0, -5.0f), vec2(W, 20.0f), 0.0f, false,
			false, vec4(1.0f, 1.0f, 1.0f, Alpha));
		if(frandom() < 0.12f * Alpha)
			m_pClient->m_pEffects->SpriteSmoke(Pos + vec2((frandom() - 0.5f) * W, -10.0f), 12.0f, vec4(0.5f, 1.0f, 0.25f, 0.3f));
		m_pClient->m_pEffects->SimpleLight(Pos + vec2(0, -10.0f), vec4(0.45f, 1.0f, 0.2f, 0.45f * Alpha), vec2(W * 1.4f, 60.0f));
	}
	else if(pShot->m_Kind == MATRIARCH_SHOT_EGG)
	{
		const float Life = (float)pShot->m_VelY;
		const float Urgency = clamp(Life / MATRIARCH_EGG_HATCH, 0.0f, 1.0f);
		const float Pulse = 1.0f + 0.08f * sinf(Client()->LocalTime() * (8.0f + Urgency * 30.0f));
		const float Tilt = sinf(Client()->LocalTime() * (6.0f + Urgency * 24.0f)) * 0.15f * Urgency;
		RenderTools()->RenderAtlasSpriteEx(ATLAS_SKITTER_MATRIARCH, "egg", Pos, vec2(36.0f * Pulse, 43.0f * Pulse), Tilt, false,
			false, White);
		m_pClient->m_pEffects->SimpleLight(Pos, vec4(0.5f, 1.0f, 0.25f, 0.4f + Urgency * 0.5f), 60.0f + Urgency * 40.0f);
	}
}

void CDroids::OnRender()
{
	if(!Client()->IsGameWorldActive())
		return;

	BeginBossV5Frame();
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

			switch(DroidVisual(pDroid->m_Type).m_Draw)
			{
				case DROID_DRAW_WALKER:
					RenderWalker(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROID_DRAW_STAR:
					RenderStar(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROID_DRAW_CRAWLER:
					RenderCrawler(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROID_DRAW_WARDEN:
					RenderWarden(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROID_DRAW_BOSSV5:
					RenderBossV5(pDroidPrev, pDroid, Item.m_ID);
					break;
				case DROID_DRAW_MATRIARCH:
					RenderMatriarch(pDroidPrev, pDroid, Item.m_ID);
					break;
                case DROID_DRAW_ANGLER:
					RenderAngler(pDroidPrev, pDroid, Item.m_ID);
					break;
				default:;
			}
		}
		else if(Item.m_Type == NETOBJTYPE_BOSSSHOT)
		{
			const CNetObj_BossShot *pShot = (const CNetObj_BossShot *)pData;
			const CNetObj_BossShot *pPrev = (const CNetObj_BossShot *)Client()->SnapFindItem(
				IClient::SNAP_PREV, Item.m_Type, Item.m_ID);
			vec2 Pos = vec2(pShot->m_X, pShot->m_Y);
			if(pPrev)
				Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), Pos, Client()->IntraGameTick());
			const vec2 Vel = vec2(pShot->m_VelX, pShot->m_VelY) / 100.0f;
			if(pShot->m_Kind >= MATRIARCH_SHOT_GLOB && pShot->m_Kind <= MATRIARCH_SHOT_EGG)
				RenderMatriarchShot(pShot, Pos, Item.m_ID);
			else if(pShot->m_Kind >= 2)
				RenderBossV5Shot(pShot, Pos, Item.m_ID);
			else if(pShot->m_Kind == 1)
			{
				const float Size = 36.0f + 6.0f * sinf(Client()->GameTick() * 0.2f + Item.m_ID);
				RenderTools()->RenderAtlasSprite(ATLAS_ABYSS_ANGLER, "bubble", Pos, vec2(Size, Size), 0.0f);
				m_pClient->m_pEffects->SimpleLight(Pos, vec4(0.4f, 0.9f, 1.0f, 0.7f), 70.0f);
			}
			else
			{
				RenderTools()->RenderAtlasSprite(ATLAS_FOUNDRY_WARDEN, "barrel", Pos, vec2(46.0f, 26.0f), atan2f(Vel.y, Vel.x));
				m_pClient->m_pEffects->SmokeTrail(Pos - normalize(Vel) * 20.0f, Vel * -2.0f);
				m_pClient->m_pEffects->SimpleLight(Pos, vec4(1.0f, 0.5f, 0.1f, 0.9f), 90.0f);
			}
		}
	}
	EndBossV5Frame();
}
