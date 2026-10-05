#include <cmath>
#include <cstdio>
#include <game/client/droid_visual.h>
#include <game/industrial_boss_sockets.h>
#include <game/server/bosspool.h>
static int Fail;
#define CHECK(x)                                                                                                       \
	do                                                                                                                 \
	{                                                                                                                  \
		if(!(x))                                                                                                       \
		{                                                                                                              \
			fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                                            \
			++Fail;                                                                                                    \
		}                                                                                                              \
	} while(0)
int main()
{
	CHECK(DROIDTYPE_BOSSWARDEN == 17 && DROIDTYPE_BOSSANGLER == 18 && DROIDTYPE_BOSSRAIL == 19 && NUM_DROIDTYPES == 23);
	CHECK(DROIDANIM_TURN == 10);
	CHECK(IndustrialTurnSwitchTick() == 8 && IndustrialTurnEndTick() == 16);
	CHECK(!IndustrialTurnFacingSwitched(7) && IndustrialTurnFacingSwitched(8));
	CHECK(IndustrialTurnLocked(IB_TURN, 0) && IndustrialTurnLocked(IB_TURN, 15));
	CHECK(!IndustrialTurnLocked(IB_TURN, 16) && !IndustrialTurnLocked(IB_MOVE, 0));
	CHECK(IndustrialActFromSnap(DROIDANIM_TURN, DROIDSTATUS_IDLE) == IB_TURN);
	const int Expected[] = {17, 19, 18, 20, 21, 22, 5};
	for(int k = 0; k < 7; k++)
		CHECK(MilestoneBossForMapBand(k) == Expected[k]);
	CHECK(MilestoneBossForMapBand(-1) == 17);
	CHECK(MilestoneBossForMapBand(7) == 17);
	for(int k = 0; k < 4; k++)
	{
		int Type = DROIDTYPE_BOSSRAIL + k;
		CHECK(IsIndustrialBoss(Type));
		CHECK(IsBossDroidType(Type));
		// Slot 0 is the Skitter Matriarch (tests/skitter_matriarch.cpp); slots 1..3 are the v5 bosses (tests/boss_v5.cpp).
		CHECK(DroidVisual(Type).m_Draw == (k == 0 ? DROID_DRAW_MATRIARCH : DROID_DRAW_BOSSV5));
		CHECK(IsLegacyIndustrialBoss(Type) == (k != 0));
		CHECK(IndustrialProfile(k).m_Run >= 7.5f);
		CHECK(IndustrialProfile(k).m_Dash >= 22.f);
		CAttackSource Source{};
		Source.m_Kind = EAttackSourceKind::Droid;
		Source.m_Type = Type;
		CHECK(!BossIgnoresMapObject(Type, Source));
		for(int a = 0; a < NUM_IB_ACTS; a++)
		{
			const auto &C = IndustrialAct(a);
			CHECK(C.m_Duration > 0);
			CHECK(IndustrialActFromSnap(C.m_Anim, a == IB_DEATH ? DROIDSTATUS_TERMINATED : DROIDSTATUS_IDLE) == a);
			for(int t = -1; t <= 200; t++)
				for(int socket = 0; socket < 3; socket++)
				{
					vec2 P = IndustrialSocket(k, a, t, socket);
					CHECK(std::isfinite(P.x) && std::isfinite(P.y));
					CHECK(length(P) < 600);
				}
			if(C.m_Loop)
				CHECK(distance(IndustrialSocket(k, a, 0, 0), IndustrialSocket(k, a, WardenTicks(C.m_Duration), 0)) <
					  .0001f);
		}
	}
	CHECK(!IsIndustrialBoss(18));
	CHECK(!IsIndustrialBoss(23));
	for(int t = 0; t < 150; t++)
	{
		CHECK(IndustrialActive(IB_DASH, t) == (t >= 40 && t < 65));
		CHECK(IndustrialActive(IB_MELEE, t) == (t >= 36 && t < 50));
		CHECK(!IndustrialActive(IB_IDLE, t));
		CHECK(!IndustrialActive(IB_TURN, t));
		CHECK(!IndustrialExposed(IB_TURN, t));
		CHECK(!IndustrialActive(IB_DEATH, t));
		CHECK(IndustrialExposed(IB_DASH, t) == (t >= 82));
	}
	if(!Fail)
		puts("industrial bosses: IDs, environment mapping, profiles, animation "
			 "clocks, finite baked sockets and attack windows PASS");
	return Fail ? 1 : 0;
}
