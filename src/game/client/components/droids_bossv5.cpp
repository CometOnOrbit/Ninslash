// Rendering of the v5 bosses (Bastion Strider, Storm Seraph, Siege Monolith) and their shots.
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skelebank.h>
#include <game/client/customstuff.h>
#include <game/client/components/effects.h>
#include <game/client/droid_visual.h>
#include <game/foundry_warden.h> // BossClipTime
#include <game/bastion_strider.h>
#include <game/storm_seraph.h>
#include <game/siege_monolith.h>

#include "boss_v5_rig.h"
#include "droids.h"

static_assert((int)SKELETON_BASTION_STRIDER == (int)ATLAS_BASTION_STRIDER, "strider rig index");
static_assert((int)SKELETON_STORM_SERAPH == (int)ATLAS_STORM_SERAPH, "seraph rig index");
static_assert((int)SKELETON_SIEGE_MONOLITH == (int)ATLAS_SIEGE_MONOLITH, "monolith rig index");

static CStriderRig s_aStriderRig[MAX_DROIDS];
static CSeraphRig s_aSeraphRig[MAX_DROIDS];
static CMonolithRig s_aMonolithRig[MAX_DROIDS];

// Tesla nodes are separate shots; the lattice between them is drawn once all are known.
static struct
{
	vec2 m_aPos[SERAPH_NODES];
	bool m_aValid[SERAPH_NODES];
	int m_State;
} s_Nodes;

void CDroids::BeginBossV5Frame()
{
	for(int i = 0; i < SERAPH_NODES; i++)
		s_Nodes.m_aValid[i] = false;
	s_Nodes.m_State = -1;
}

void CDroids::EndBossV5Frame()
{
	if(s_Nodes.m_State != SERAPH_NODE_ARMED && s_Nodes.m_State != SERAPH_NODE_LIVE)
		return;
	const bool Live = s_Nodes.m_State == SERAPH_NODE_LIVE;
	// Diagonals only exist in the last phase; the server only hurts with the pairs it uses,
	// and the client draws what it can infer: edges always, diagonals when the boss bar says phase 3.
	int Phase = 0;
	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);
		if(Item.m_Type == NETOBJTYPE_BOSSSTATUS)
			Phase = max(Phase, ((const CNetObj_BossStatus *)pData)->m_Phase);
	}
	static const int s_aPair[6][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 2}, {1, 3}};
	const int Pairs = Phase >= 2 ? 6 : 4;
	const int Seed = Client()->GameTick() / 2;
	for(int p = 0; p < Pairs; p++)
	{
		const int a = s_aPair[p][0], b = s_aPair[p][1];
		if(!s_Nodes.m_aValid[a] || !s_Nodes.m_aValid[b])
			continue;
		if(Live)
		{
			BossV5DrawBolt(Graphics(), s_Nodes.m_aPos[a], s_Nodes.m_aPos[b], 7.0f, vec4(0.45f, 0.8f, 1.0f, 1.0f), Seed * 7 + p, 16.0f);
			m_pClient->m_pEffects->SimpleLight(mix(s_Nodes.m_aPos[a], s_Nodes.m_aPos[b], 0.5f), vec4(0.4f, 0.75f, 1.0f, 0.6f), 160.0f);
		}
		else
		{
			// Telegraph: thin flickering guide lines.
			const float Alpha = 0.35f + 0.25f * sinf(Client()->LocalTime() * 30.0f);
			BossV5DrawBolt(Graphics(), s_Nodes.m_aPos[a], s_Nodes.m_aPos[b], 1.6f, vec4(0.5f, 0.8f, 1.0f, Alpha), p, 0.0f);
		}
	}
}

void CDroids::RenderBossV5(const CNetObj_Droid *pPrev, const CNetObj_Droid *pCurrent, int ItemID)
{
	const vec2 Pos = MixPos(pPrev, pCurrent);
	const int Slot = ItemID % MAX_DROIDS;
	const int Type = pCurrent->m_Type;

	if(pCurrent->m_Status != DROIDSTATUS_IDLE && pCurrent->m_Status != DROIDSTATUS_TERMINATED)
	{
		CustomStuff()->m_DroidDamageIntensity[Slot] = 1.0f;
		CustomStuff()->m_DroidDamageType[Slot] = pCurrent->m_Status;
	}
	const CNetObj_BossStatus *pStatus =
		(const CNetObj_BossStatus *)Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_BOSSSTATUS, ItemID);
	const CNetObj_BossStatus *pPrevStatus =
		(const CNetObj_BossStatus *)Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_BOSSSTATUS, ItemID);

	CBossV5RigInput In;
	In.m_Pos = Pos;
	In.m_Vel = vec2(pCurrent->m_X - pPrev->m_X, pCurrent->m_Y - pPrev->m_Y);
	In.m_Dir = pCurrent->m_Dir >= 0 ? 1 : -1;
	In.m_Flags = pCurrent->m_Angle;
	CBossV5Act Clip;
	if(Type == DROIDTYPE_BOSSBULKHEAD)
	{
		In.m_Act = StriderActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
		Clip = StriderAct(In.m_Act);
	}
	else if(Type == DROIDTYPE_BOSSARC)
	{
		In.m_Act = SeraphActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
		Clip = SeraphAct(In.m_Act);
	}
	else
	{
		In.m_Act = MonolithActFromSnap(pCurrent->m_Anim, pCurrent->m_Status);
		Clip = MonolithAct(In.m_Act);
	}
	In.m_Time = BossClipTime(Clip.m_Duration, Clip.m_Loop, pCurrent->m_AttackTick, Client()->GameTick(), Client()->IntraGameTick());
	In.m_Aim = pStatus ? vec2(pStatus->m_ArmX, pStatus->m_ArmY) : Pos;
	if(pStatus && pPrevStatus)
		In.m_Aim = mix(vec2(pPrevStatus->m_ArmX, pPrevStatus->m_ArmY), In.m_Aim, Client()->IntraGameTick());
	In.m_Phase = pStatus ? pStatus->m_Phase : 0;
	In.m_aBroken[0] = false;
	In.m_aBroken[1] = pStatus && pStatus->m_Part1 <= 0;
	In.m_aBroken[2] = pStatus && pStatus->m_Part2 <= 0;
	In.m_aBroken[3] = pStatus && pStatus->m_Part3 <= 0;
	In.m_Hurt = CustomStuff()->m_DroidDamageIntensity[Slot] * 0.55f;

	auto BeginHurt = [&]()
	{
		if(In.m_Hurt <= 0.0f)
			return;
		if(CustomStuff()->m_DroidDamageType[Slot] == DROIDSTATUS_ELECTRIC)
			RenderTools()->Graphics()->ShaderBegin(SHADER_ELECTRIC, In.m_Hurt);
		else
			RenderTools()->Graphics()->ShaderBegin(SHADER_DAMAGE, In.m_Hurt);
	};
	auto Fresh = [&](CBossV5RigBase &Rig, auto ResetFn)
	{
		if(Rig.m_ItemID != ItemID || Client()->GameTick() - Rig.m_LastTick > 25)
		{
			ResetFn();
			Rig.m_ItemID = ItemID;
		}
		Rig.m_LastTick = Client()->GameTick();
	};
	CEffects *pFx = m_pClient->m_pEffects;
	const float Dt = Client()->RenderFrameTime();
	const float Now = Client()->LocalTime();
	const bool Dead = pCurrent->m_Status == DROIDSTATUS_TERMINATED;

	if(Type == DROIDTYPE_BOSSBULKHEAD)
	{
		CStriderRig &Rig = s_aStriderRig[Slot];
		Fresh(Rig, [&]() { Rig.Reset(); });
		Rig.Update(m_pClient->Collision(), In, Dt);
		BeginHurt();
		Rig.Render(RenderTools(), ATLAS_BASTION_STRIDER, In);
		RenderTools()->Graphics()->ShaderEnd();
		for(; Rig.m_Landed > 0; Rig.m_Landed--)
			pFx->SpriteSmoke(Rig.FootPos(rand() % 4) + vec2(frandom() - 0.5f, -0.3f) * 24.0f, 22.0f + frandom() * 14.0f,
				vec4(0.45f, 0.42f, 0.38f, 0.35f));
		if(In.m_Flags & BOSSV5_FLAG_THRUST)
		{
			pFx->SmokeTrail(Rig.ThrusterPos(), vec2(-In.m_Dir * 90.0f, -10.0f));
			pFx->SimpleLight(Rig.ThrusterPos(), vec4(1.0f, 0.6f, 0.2f, 0.9f), 150.0f);
			pFx->SimpleLight(Rig.ShieldPos(), vec4(1.0f, 0.7f, 0.3f, 0.55f), 130.0f);
		}
		else if(In.m_Act == STRIDER_ACT_CHARGE)
		{
			// Charge windup: the thrusters spool up.
			pFx->SimpleLight(Rig.ThrusterPos(), vec4(1.0f, 0.5f, 0.15f, 0.4f + 0.4f * sinf(Now * 30.0f)), 110.0f);
			if(frandom() < 0.4f)
				pFx->SpriteSmoke(Rig.ThrusterPos(), 16.0f, vec4(0.5f, 0.45f, 0.4f, 0.4f));
		}
		if(In.m_Flags & BOSSV5_FLAG_EXPOSED)
		{
			pFx->SimpleLight(Rig.CorePos(), vec4(1.0f, 0.45f, 0.1f, 0.6f + 0.25f * sinf(Now * 16.0f)), 190.0f);
			if(frandom() < 0.35f)
				pFx->SpriteSmoke(Rig.CorePos() + vec2((frandom() - 0.5f) * 120.0f, -50.0f), 18.0f, vec4(0.6f, 0.55f, 0.5f, 0.4f));
		}
		if(In.m_Act == STRIDER_ACT_STOMP && In.m_Time < StriderStompTick() / (float)BOSSV5_TICK_SPEED)
			pFx->SimpleLight(Rig.FootPos(3), vec4(1.0f, 0.5f, 0.15f, 0.7f), 120.0f);
		if(In.m_Act == STRIDER_ACT_MORTAR && In.m_Time < 0.5f)
			pFx->SimpleLight(Rig.MortarPos(), vec4(1.0f, 0.6f, 0.2f, 0.6f), 90.0f);
		if(In.m_aBroken[STRIDER_PART_MORTAR] && frandom() < 0.15f)
			pFx->SpriteSmoke(Rig.MortarPos(), 18.0f, vec4(0.25f, 0.25f, 0.25f, 0.4f));
		if(Dead)
			pFx->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 140.0f, 40.0f + frandom() * 40.0f,
				vec2(frandom() - frandom(), frandom() - frandom()) * 18.0f);
	}
	else if(Type == DROIDTYPE_BOSSARC)
	{
		CSeraphRig &Rig = s_aSeraphRig[Slot];
		Fresh(Rig, [&]() { Rig.Reset(); });
		Rig.Update(m_pClient->Collision(), In, Dt);
		const bool Exposed = In.m_Flags & BOSSV5_FLAG_EXPOSED;
		// Plunge telegraph: a column of light down to the impact point.
		if(In.m_Act == SERAPH_ACT_PLUNGE && !(In.m_Flags & BOSSV5_FLAG_THRUST) && In.m_Time > SeraphPlungeRise() / (float)BOSSV5_TICK_SPEED)
		{
			vec2 Floor = Pos + vec2(0, 600.0f);
			m_pClient->Collision()->IntersectLine(Pos, Floor, &Floor, 0);
			const float a = 0.3f + 0.25f * sinf(Now * 24.0f);
			BossV5DrawBolt(Graphics(), Pos + vec2(0, 60.0f), Floor, 3.0f, vec4(1.0f, 0.9f, 0.4f, a), 3, 0.0f);
			pFx->SimpleLight(Floor, vec4(1.0f, 0.85f, 0.3f, 0.7f), vec2(SERAPH_PLUNGE_RADIUS * 2.0f, 60.0f));
		}
		BeginHurt();
		Rig.Render(RenderTools(), ATLAS_STORM_SERAPH, In);
		RenderTools()->Graphics()->ShaderEnd();
		pFx->SimpleLight(Rig.CorePos(), vec4(0.35f, 0.75f, 1.0f, Exposed ? 0.95f : 0.5f), Exposed ? 210.0f : 110.0f);
		pFx->SimpleLight(Rig.ThrusterPos(), vec4(0.3f, 0.7f, 1.0f, 0.5f), 80.0f);
		if(!In.m_aBroken[SERAPH_PART_HALO])
			pFx->SimpleLight(Rig.HaloPos(), vec4(0.6f, 0.85f, 1.0f, In.m_Act == SERAPH_ACT_ORBS ? 0.9f : 0.4f), 90.0f);
		if(Exposed && frandom() < 0.35f)
			pFx->Electrospark(Rig.CorePos() + vec2(frandom() - 0.5f, frandom() - 0.5f) * 60.0f, 24.0f, vec2(0, -4.0f));
		if(In.m_Flags & BOSSV5_FLAG_THRUST)
			pFx->SmokeTrail(Rig.ThrusterPos(), -In.m_Vel * 6.0f);
		if(In.m_Act == SERAPH_ACT_ZAP)
		{
			const float Fire = SeraphZapTick() / (float)BOSSV5_TICK_SPEED;
			if(In.m_Time < Fire)
				BossV5DrawBolt(Graphics(), Rig.CorePos(), In.m_Aim, 1.5f, vec4(0.6f, 0.85f, 1.0f, 0.3f + In.m_Time / Fire * 0.5f), 1, 0.0f);
			else if(In.m_Time < Fire + 0.18f)
			{
				BossV5DrawBolt(Graphics(), Rig.CorePos(), In.m_Aim, 10.0f, vec4(0.55f, 0.85f, 1.0f, 1.0f), Client()->GameTick(), 22.0f);
				pFx->SimpleLight(In.m_Aim, vec4(0.5f, 0.8f, 1.0f, 0.9f), 160.0f);
			}
		}
		if(In.m_Act == SERAPH_ACT_ROAR && In.m_Time > SeraphRoarTick() / (float)BOSSV5_TICK_SPEED && In.m_Time < 0.8f)
			for(int k = 0; k < 6; k++)
			{
				const float a = k * pi / 3.0f + Now;
				BossV5DrawBolt(Graphics(), Rig.CorePos(), Rig.CorePos() + vec2(cosf(a), sinf(a)) * SERAPH_ROAR_RADIUS, 5.0f,
					vec4(0.5f, 0.8f, 1.0f, 0.9f), Client()->GameTick() + k, 18.0f);
			}
		if(Dead)
			pFx->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 100.0f, 40.0f + frandom() * 40.0f,
				vec2(frandom() - frandom(), frandom() - frandom()) * 18.0f);
	}
	else
	{
		CMonolithRig &Rig = s_aMonolithRig[Slot];
		Fresh(Rig, [&]() { Rig.Reset(); });
		Rig.Update(m_pClient->Collision(), In, Dt);
		const bool Air = In.m_Flags & BOSSV5_FLAG_AIR;
		const bool Beam = In.m_Flags & BOSSV5_FLAG_BEAM;
		// Slam telegraph: a red pool where it is going to land.
		if(In.m_Act == MONOLITH_ACT_SLAM && !(In.m_Flags & BOSSV5_FLAG_EXPOSED))
		{
			vec2 Floor = Pos + vec2(0, 700.0f);
			m_pClient->Collision()->IntersectLine(Pos, Floor, &Floor, 0);
			pFx->SimpleLight(Floor, vec4(1.0f, 0.2f, 0.1f, 0.5f + 0.3f * sinf(Now * 26.0f)), vec2(MONOLITH_POUND_RADIUS * 2.0f, 70.0f));
		}
		BeginHurt();
		Rig.Render(RenderTools(), ATLAS_SIEGE_MONOLITH, In);
		RenderTools()->Graphics()->ShaderEnd();
		for(; Rig.m_Landed > 0; Rig.m_Landed--)
			pFx->SpriteSmoke(Pos + vec2((frandom() - 0.5f) * 200.0f, MONOLITH_HOVER - 10.0f), 26.0f + frandom() * 14.0f,
				vec4(0.45f, 0.42f, 0.38f, 0.4f));
		if(!In.m_aBroken[MONOLITH_PART_THRUSTERS])
		{
			if(Air || (In.m_Flags & BOSSV5_FLAG_THRUST))
			{
				pFx->SimpleLight(Rig.ThrusterPos(), vec4(1.0f, 0.35f, 0.2f, 0.85f), 170.0f);
				if(frandom() < 0.6f)
					pFx->SmokeTrail(Rig.ThrusterPos(), vec2((frandom() - 0.5f) * 30.0f, 120.0f));
			}
			else
				pFx->SimpleLight(Rig.ThrusterPos(), vec4(1.0f, 0.3f, 0.2f, 0.25f), 80.0f);
		}
		else if(frandom() < 0.25f)
			pFx->SpriteSmoke(Rig.ThrusterPos(), 22.0f, vec4(0.2f, 0.2f, 0.2f, 0.45f));
		if(In.m_Flags & BOSSV5_FLAG_EXPOSED)
			for(int v = 0; v < 2; v++)
			{
				pFx->SimpleLight(Rig.VentPos(v), vec4(1.0f, 0.5f, 0.15f, 0.8f), 120.0f);
				if(frandom() < 0.5f)
					pFx->SpriteSmoke(Rig.VentPos(v) + vec2(In.m_Dir * 20.0f, 0), 16.0f + frandom() * 10.0f, vec4(0.85f, 0.8f, 0.75f, 0.4f));
			}
		if(In.m_Act == MONOLITH_ACT_SWEEP && !In.m_aBroken[MONOLITH_PART_TURRET])
		{
			if(Beam)
			{
				BossV5DrawBolt(Graphics(), Rig.Muzzle(), In.m_Aim, MONOLITH_BEAM_RADIUS * 0.7f, vec4(1.0f, 0.25f, 0.15f, 1.0f), Client()->GameTick(), 3.0f);
				pFx->SimpleLight(In.m_Aim, vec4(1.0f, 0.3f, 0.15f, 0.9f), 150.0f);
				pFx->SimpleLight(Rig.Muzzle(), vec4(1.0f, 0.3f, 0.15f, 0.9f), 110.0f);
				if(frandom() < 0.5f)
					pFx->Spark(In.m_Aim);
			}
			else if(In.m_Time < MonolithSweepStart() / (float)BOSSV5_TICK_SPEED)
			{
				const float a = 0.25f + 0.3f * (In.m_Time / (MonolithSweepStart() / (float)BOSSV5_TICK_SPEED));
				BossV5DrawBolt(Graphics(), Rig.Muzzle(), In.m_Aim, 1.4f, vec4(1.0f, 0.2f, 0.15f, a), 0, 0.0f);
				pFx->SimpleLight(Rig.Muzzle(), vec4(1.0f, 0.25f, 0.15f, a), 70.0f);
			}
		}
		if(Dead)
			pFx->Electrospark(Pos + vec2(frandom() - frandom(), frandom() - frandom()) * frandom() * 150.0f, 40.0f + frandom() * 40.0f,
				vec2(frandom() - frandom(), frandom() - frandom()) * 18.0f);
	}

	const CDroidVisual &Visual = DroidVisual(Type);
	m_pClient->m_pEffects->SimpleLight(Pos, Visual.m_Light, Visual.m_LightSize);
}

void CDroids::RenderBossV5Shot(const CNetObj_BossShot *pShot, vec2 Pos, int ItemID)
{
	const vec2 Vel = vec2(pShot->m_VelX, pShot->m_VelY) / 100.0f;
	const vec4 White(1.0f, 1.0f, 1.0f, 1.0f);
	CEffects *pFx = m_pClient->m_pEffects;
	const float Now = Client()->LocalTime();
	switch(pShot->m_Kind)
	{
	case STRIDER_SHOT_WAVE:
	{
		const float Dir = Vel.x >= 0.0f ? 1.0f : -1.0f;
		BossV5DrawBolt(Graphics(), Pos + vec2(Dir * 14.0f, 0), Pos + vec2(-Dir * 8.0f, -STRIDER_WAVE_HEIGHT), 9.0f, vec4(1.0f, 0.55f, 0.15f, 0.9f),
			ItemID + Client()->GameTick() / 3, 8.0f);
		if(frandom() < 0.6f)
			pFx->SpriteSmoke(Pos + vec2(-Dir * 10.0f, -10.0f), 20.0f + frandom() * 10.0f, vec4(0.55f, 0.48f, 0.4f, 0.45f));
		pFx->SimpleLight(Pos + vec2(0, -20.0f), vec4(1.0f, 0.5f, 0.15f, 0.8f), 110.0f);
		break;
	}
	case STRIDER_SHOT_SHELL:
		RenderTools()->RenderAtlasSpriteEx(ATLAS_BASTION_STRIDER, "shell", Pos, vec2(34.0f, 32.0f), atan2f(Vel.y, Vel.x) + pi * 0.25f, false,
			false, White);
		pFx->SmokeTrail(Pos - normalize(Vel + vec2(0.001f, 0)) * 16.0f, Vel * -2.0f);
		pFx->SimpleLight(Pos, vec4(1.0f, 0.55f, 0.15f, 0.85f), 80.0f);
		break;
	case SERAPH_SHOT_ORB:
	{
		const float Wobble = 1.0f + 0.1f * sinf(Now * 26.0f + ItemID);
		RenderTools()->RenderAtlasSpriteEx(ATLAS_STORM_SERAPH, "orb", Pos, vec2(52.0f * Wobble, 34.0f / Wobble), atan2f(Vel.y, Vel.x), false,
			false, White);
		if(frandom() < 0.25f)
			pFx->Electrospark(Pos, 18.0f, -Vel);
		pFx->SimpleLight(Pos, vec4(0.4f, 0.75f, 1.0f, 0.85f), 90.0f);
		break;
	}
	case SERAPH_SHOT_NODE:
	{
		const int State = pShot->m_VelX;
		const int Index = pShot->m_VelY;
		if(State == SERAPH_NODE_DOCKED)
			break;
		if(Index >= 0 && Index < SERAPH_NODES)
		{
			s_Nodes.m_aPos[Index] = Pos;
			s_Nodes.m_aValid[Index] = true;
			s_Nodes.m_State = State;
		}
		const float Bob = sinf(Now * 5.0f + Index) * 3.0f;
		const bool Hot = State == SERAPH_NODE_LIVE || State == SERAPH_NODE_ARMED;
		RenderTools()->RenderAtlasSpriteEx(ATLAS_STORM_SERAPH, "node", Pos + vec2(0, Bob), vec2(34.0f, 47.0f), 0.0f, false, false,
			Hot ? vec4(1.25f, 1.35f, 1.5f, 1.0f) : White);
		pFx->SimpleLight(Pos, vec4(0.4f, 0.75f, 1.0f, Hot ? 0.9f : 0.5f), Hot ? 110.0f : 70.0f);
		break;
	}
	case SERAPH_SHOT_SPARK:
		BossV5DrawBolt(Graphics(), Pos, Pos + vec2((frandom() - 0.5f) * 20.0f, -60.0f), 5.0f, vec4(0.5f, 0.8f, 1.0f, 0.9f),
			ItemID + Client()->GameTick(), 12.0f);
		if(frandom() < 0.4f)
			pFx->Electrospark(Pos + vec2(0, -10.0f), 22.0f, vec2(0, -6.0f));
		pFx->SimpleLight(Pos + vec2(0, -20.0f), vec4(0.4f, 0.75f, 1.0f, 0.8f), 100.0f);
		break;
	case MONOLITH_SHOT_BOMB:
	{
		RenderTools()->RenderAtlasSpriteEx(ATLAS_SIEGE_MONOLITH, "bomb", Pos, vec2(40.0f, 25.0f), atan2f(Vel.y, Vel.x), false, false, White);
		const bool Blink = fmodf(Now * 8.0f + ItemID, 1.0f) < 0.5f;
		pFx->SimpleLight(Pos, vec4(1.0f, 0.15f, 0.1f, Blink ? 0.9f : 0.3f), 70.0f);
		// Landing marker on the floor below.
		vec2 Floor = Pos + vec2(Vel.x * 10.0f, 500.0f);
		if(m_pClient->Collision()->IntersectLine(Pos, Floor, &Floor, 0))
			pFx->SimpleLight(Floor, vec4(1.0f, 0.2f, 0.1f, 0.35f), vec2(MONOLITH_BOMB_RADIUS * 1.6f, 40.0f));
		break;
	}
	case MONOLITH_SHOT_SHELL:
		RenderTools()->RenderAtlasSpriteEx(ATLAS_SIEGE_MONOLITH, "bomb", Pos, vec2(34.0f, 21.0f), atan2f(Vel.y, Vel.x), false, false, White);
		pFx->SmokeTrail(Pos - normalize(Vel + vec2(0.001f, 0)) * 14.0f, Vel * -2.0f);
		pFx->SimpleLight(Pos, vec4(1.0f, 0.35f, 0.15f, 0.8f), 70.0f);
		break;
	case MONOLITH_SHOT_BLAST:
	{
		const float Age = clamp(pShot->m_VelY / 20.0f, 0.0f, 1.0f);
		const float Radius = 30.0f + Age * 90.0f;
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(1.0f, 0.55f, 0.2f, 1.0f - Age);
		for(int n = 0; n < 32; n++)
		{
			const float A = n * 2 * pi / 32, B = (n + 1) * 2 * pi / 32;
			IGraphics::CLineItem Line(Pos.x + cosf(A) * Radius, Pos.y + sinf(A) * Radius * 0.35f, Pos.x + cosf(B) * Radius,
				Pos.y + sinf(B) * Radius * 0.35f);
			Graphics()->LinesDraw(&Line, 1);
		}
		Graphics()->LinesEnd();
		break;
	}
	default:
		break;
	}
}
