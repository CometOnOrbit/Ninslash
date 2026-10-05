// Bastion Strider / Storm Seraph / Siege Monolith (v5 bosses in DROIDTYPE slots 20..22):
// snapshot act mapping, attack windows inside their clips, phases, part/damage rules, ballistic aim.
#include <cmath>
#include <cstdio>
#include <game/client/droid_visual.h>
#include <game/industrial_boss.h>
#include <game/bastion_strider.h>
#include <game/storm_seraph.h>
#include <game/siege_monolith.h>

static int Fail;
#define CHECK(x) \
	do \
	{ \
		if(!(x)) \
		{ \
			fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
			++Fail; \
		} \
	} while(0)

static float Secs(int Ticks) { return Ticks / (float)BOSSV5_TICK_SPEED; }

template<typename F, typename G>
static void CheckActs(int Num, int DeathAct, F Act, G FromSnap)
{
	for(int a = 0; a < Num; a++)
	{
		CHECK(Act(a).m_Duration > 0.0f);
		CHECK(Act(a).m_Anim >= 0 && Act(a).m_Anim <= DROIDANIM_TURN);
		CHECK(FromSnap(Act(a).m_Anim, a == DeathAct ? DROIDSTATUS_TERMINATED : DROIDSTATUS_IDLE) == a);
		// Only idle/move loop: everything else has a clock.
		CHECK(Act(a).m_Loop == (a <= 1));
	}
}

static void CheckHits(const CBossV5Hit *pHits, int Num, const float *pShare)
{
	float Sum = 0.0f;
	for(int p = 0; p < BOSSV5_NUM_PARTS; p++)
		Sum += pShare[p];
	CHECK(pShare[0] == 0.0f && Sum > 0.3f && Sum < 0.8f);
	bool aSeen[BOSSV5_NUM_PARTS] = {false, false, false, false};
	for(int i = 0; i < Num; i++)
	{
		CHECK(pHits[i].m_Part >= 0 && pHits[i].m_Part < BOSSV5_NUM_PARTS);
		CHECK(pHits[i].m_R > 10.0f && pHits[i].m_R < 80.0f);
		aSeen[pHits[i].m_Part] = true;
		// A shot dead on a hit circle resolves to that circle's part.
		CHECK(BossV5PartAt(pHits, Num, pHits[i].m_X, pHits[i].m_Y) == pHits[i].m_Part || pHits[i].m_Part == 0);
	}
	CHECK(aSeen[0]);
}

static void Fly(float Vx, float Vy, float Gravity, int Ticks, float *pX, float *pY)
{
	float x = 0, y = 0;
	for(int i = 0; i < Ticks; i++)
	{
		Vy += Gravity;
		x += Vx;
		y += Vy;
	}
	*pX = x;
	*pY = y;
}

int main()
{
	// ---- slots / visuals ----
	CHECK(DROIDTYPE_BOSSBULKHEAD == 20 && DROIDTYPE_BOSSARC == 21 && DROIDTYPE_BOSSVAULT == 22);
	for(int t = DROIDTYPE_BOSSBULKHEAD; t <= DROIDTYPE_BOSSVAULT; t++)
	{
		CHECK(DroidVisual(t).m_Draw == DROID_DRAW_BOSSV5);
		CHECK(IsIndustrialBoss(t));
	}

	// ---- collision bodies == art (full silhouette, soles at the rig's ride height, crouch <= 25%) ----
	CHECK(STRIDER_BODY_BOTTOM == STRIDER_HOVER && STRIDER_BODY_TOP <= s_StriderMortar.m_Y - s_StriderMortar.m_H * 0.5f + 20.0f);
	CHECK(STRIDER_MAX_CROUCH <= 0.25f * (STRIDER_BODY_BOTTOM - STRIDER_BODY_TOP) && STRIDER_STEP_UP < STRIDER_MAX_HOP);
	CHECK(STRIDER_BODY_HALF_W >= s_StriderShield.m_X + s_StriderShield.m_W * 0.5f - 20.0f);
	CHECK(MONOLITH_MAX_CROUCH <= 0.25f * (MONOLITH_HOVER - MONOLITH_BODY_TOP) && MONOLITH_AIR_HALF_W < MONOLITH_BODY_HALF_W);
	CHECK(MONOLITH_AIR_BOTTOM <= MONOLITH_HOVER && MONOLITH_STEP_UP < MONOLITH_MAX_HOP);
	CHECK(SERAPH_BODY_BOTTOM >= s_SeraphThruster.m_Y + s_SeraphThruster.m_H * 0.5f - 5.0f);
	CHECK(SERAPH_BODY_TOP <= s_SeraphHalo.m_Y - s_SeraphHalo.m_H * 0.5f + 2.0f);

	// ---- act tables ----
	CheckActs(NUM_STRIDER_ACTS, STRIDER_ACT_DEATH, StriderAct, StriderActFromSnap);
	CheckActs(NUM_SERAPH_ACTS, SERAPH_ACT_DEATH, SeraphAct, SeraphActFromSnap);
	CheckActs(NUM_MONOLITH_ACTS, MONOLITH_ACT_DEATH, MonolithAct, MonolithActFromSnap);
	// At least five attacks each (not counting locomotion, roar, stagger, death).
	const int aStriderAttacks[] = {STRIDER_ACT_BASH, STRIDER_ACT_CHARGE, STRIDER_ACT_STOMP, STRIDER_ACT_MORTAR, STRIDER_ACT_LEAP};
	const int aSeraphAttacks[] = {SERAPH_ACT_DIVE, SERAPH_ACT_LATTICE, SERAPH_ACT_ORBS, SERAPH_ACT_PLUNGE, SERAPH_ACT_ZAP};
	const int aMonolithAttacks[] = {MONOLITH_ACT_SWEEP, MONOLITH_ACT_MORTAR, MONOLITH_ACT_BOMB, MONOLITH_ACT_DRONES, MONOLITH_ACT_SLAM};
	CHECK(sizeof(aStriderAttacks) / sizeof(int) >= 5 && sizeof(aSeraphAttacks) / sizeof(int) >= 5 && sizeof(aMonolithAttacks) / sizeof(int) >= 5);

	// ---- phases ----
	CHECK(BossV5Phase(6000, 6000) == 0);
	CHECK(BossV5Phase(4000, 6000) == 1);
	CHECK(BossV5Phase(2000, 6000) == 2);
	CHECK(BossV5Phase(1, 6000) == 2);

	// ---- mobility: none of them is slower than the minions they fight beside ----
	CHECK(StriderSpeed(1, true) >= 8.0f);
	CHECK(StriderSpeed(0, false) < StriderSpeed(0, true));
	CHECK(StriderChargeSpeed(0) > 18.0f);
	CHECK(SeraphSpeed(0, true) > 8.0f && SeraphSpeed(0, false) < SeraphSpeed(0, true));
	CHECK(SeraphDiveSpeed(true) > 20.0f);
	CHECK(MonolithSpeed(0, true) >= 7.0f);
	CHECK(MonolithSpeed(2, false) > MonolithSpeed(0, false));

	// ---- attack windows fit inside their clips (telegraph first, never cut short) ----
	for(int p = 0; p < 3; p++)
	{
		CHECK(Secs(StriderBashTick()) < StriderAct(STRIDER_ACT_BASH).m_Duration);
		CHECK(Secs(StriderChargeWindup(p) + StriderChargeTicks()) < StriderAct(STRIDER_ACT_CHARGE).m_Duration);
		CHECK(Secs(StriderStompTick()) < StriderAct(STRIDER_ACT_STOMP).m_Duration);
		CHECK(Secs(StriderShellTick(StriderShellCount(p) - 1)) < StriderAct(STRIDER_ACT_MORTAR).m_Duration);
		CHECK(Secs(StriderLeapWindup(p)) + StriderLeapFlight(900.0f) / BOSSV5_TICK_SPEED < StriderAct(STRIDER_ACT_LEAP).m_Duration);
		CHECK(StriderChargeWindup(p) >= BossV5Ticks(0.28f)); // still a readable telegraph
		CHECK(Secs(StriderBashTick() + (StriderBashCount(p) - 1) * StriderBashGap()) < StriderAct(STRIDER_ACT_BASH).m_Duration);

		const int Dives = SeraphDiveCount(p);
		CHECK(Secs(SeraphDiveWindup(p) + SeraphDiveTicks() + (Dives - 1) * (BossV5Ticks(0.2f) + SeraphDiveTicks())) <
			  SeraphAct(SERAPH_ACT_DIVE).m_Duration);
		CHECK(SeraphLatticeLive(p) < SeraphLatticeEnd(p));
		CHECK(Secs(SeraphLatticeEnd(p)) < SeraphAct(SERAPH_ACT_LATTICE).m_Duration);
		CHECK(Secs(SeraphLatticeLive(p)) >= 1.0f); // lines are visible a full second before they hurt
		for(int s = 0; s < SeraphOrbCount(p, true); s++)
			CHECK(Secs(SeraphOrbTick(s)) < SeraphAct(SERAPH_ACT_ORBS).m_Duration);
		CHECK(Secs(SeraphPlungeRise() + SeraphPlungeHold(p)) + 60.0f / BOSSV5_TICK_SPEED <= SeraphAct(SERAPH_ACT_PLUNGE).m_Duration);
		CHECK(Secs(SeraphZapTick()) < SeraphAct(SERAPH_ACT_ZAP).m_Duration);

		CHECK(MonolithSweepStart() < MonolithSweepEnd(p));
		CHECK(Secs(MonolithSweepEnd(p)) < MonolithAct(MONOLITH_ACT_SWEEP).m_Duration);
		CHECK(Secs(MonolithShellTick(MonolithShellCount(p) - 1)) < MonolithAct(MONOLITH_ACT_MORTAR).m_Duration);
		CHECK(0.4f + Secs(MonolithBombTicks()) < MonolithAct(MONOLITH_ACT_BOMB).m_Duration);
		CHECK(Secs(MonolithDroneTick()) < MonolithAct(MONOLITH_ACT_DRONES).m_Duration);
		CHECK(Secs(MonolithSlamRise()) + 0.35f + 70.0f / BOSSV5_TICK_SPEED < MonolithAct(MONOLITH_ACT_SLAM).m_Duration);
		CHECK(MonolithDroneCount(p) <= MONOLITH_MAX_DRONES);
	}

	// ---- difficulty scaling ----
	CHECK(BossV5DepthHealth(10) == 1.0f && BossV5DepthHealth(40) > 1.3f && BossV5DepthHealth(200) == 2.0f);
	CHECK(BossV5DepthDamage(20) == 1.0f && BossV5DepthDamage(60) > 1.3f && BossV5DepthDamage(200) <= 1.6f);
	CHECK(BossV5PhaseDamage(2) > BossV5PhaseDamage(1) && BossV5PhaseDamage(1) > BossV5PhaseDamage(0));
	CHECK(BossV5Armor(0) < 1.0f && BossV5Armor(2) <= BossV5Armor(0));
	CHECK(STRIDER_HEALTH >= 8000 && SERAPH_HEALTH >= 6000 && MONOLITH_HEALTH >= 8800);
	CHECK(SeraphSpeed(0, true) >= 13.0f && MonolithSpeed(0, false) >= 6.5f && StriderSpeed(0, true) >= 8.5f);
	// Legs reach the floor at rest with room to spare for slopes.
	CHECK(STRIDER_THIGH + STRIDER_SHIN > (STRIDER_HOVER - STRIDER_ANKLE - s_aStriderHipY[2]) * 2.0f);

	// ---- parts and damage rules ----
	CheckHits(s_aStriderHit, NUM_STRIDER_HIT, s_aStriderPartShare);
	CheckHits(s_aSeraphHit, NUM_SERAPH_HIT, s_aSeraphPartShare);
	CheckHits(s_aMonolithHit, NUM_MONOLITH_HIT, s_aMonolithPartShare);
	{
		int Shield = 0;
		// Shield soaks the front: the hull only gets the bleed, the shield gets it all.
		CHECK(StriderIncomingDamage(100, STRIDER_PART_SHIELD, false, false, &Shield) == 15 && Shield == 100);
		// Flanking beats the frontal armour; opened vents from behind are the best spot.
		const int Front = StriderIncomingDamage(100, STRIDER_PART_CORE, false, false, &Shield);
		const int Back = StriderIncomingDamage(100, STRIDER_PART_CORE, true, false, &Shield);
		const int Vent = StriderIncomingDamage(100, STRIDER_PART_CORE, true, true, &Shield);
		CHECK(Front < Back && Back < Vent && Shield == 0);
		CHECK(StriderIncomingDamage(1, STRIDER_PART_SHIELD, false, false, &Shield) == 1); // never zero
	}
	{
		const int Shielded = SeraphIncomingDamage(100, SERAPH_PART_CORE, false, true);
		const int Open = SeraphIncomingDamage(100, SERAPH_PART_CORE, false, false);
		const int Exposed = SeraphIncomingDamage(100, SERAPH_PART_CORE, true, false);
		CHECK(Shielded < Open && Open < Exposed);
		CHECK(Exposed >= 150);
	}
	{
		const int Armour = MonolithIncomingDamage(100, MONOLITH_PART_CORE, false);
		const int Vented = MonolithIncomingDamage(100, MONOLITH_PART_CORE, true);
		const int Vent = MonolithIncomingDamage(100, MONOLITH_PART_VENTS, true);
		CHECK(Armour < 100 && Vented > 100 && Vent > Vented);
	}
	CHECK(SeraphNodeAngle(1, 0) - SeraphNodeAngle(0, 0) > 1.5f);

	// ---- ballistic aim lands on target (shells / mortars) ----
	const float aTarget[][2] = {{300, 0}, {600, 80}, {-450, -120}, {200, 200}};
	for(const auto &t : aTarget)
	{
		const float T = 50.0f;
		float Vx, Vy, x, y;
		BossV5Ballistic(t[0], t[1], T, STRIDER_SHELL_GRAVITY, 100.0f, &Vx, &Vy);
		Fly(Vx, Vy, STRIDER_SHELL_GRAVITY, (int)T, &x, &y);
		CHECK(fabsf(x - t[0]) < 2.0f && fabsf(y - t[1]) < 15.0f);
		BossV5Ballistic(t[0], t[1], T, MONOLITH_SHELL_GRAVITY, 100.0f, &Vx, &Vy);
		Fly(Vx, Vy, MONOLITH_SHELL_GRAVITY, (int)T, &x, &y);
		CHECK(fabsf(x - t[0]) < 2.0f && fabsf(y - t[1]) < 15.0f);
	}
	{
		float Vx, Vy;
		BossV5Ballistic(5000, 0, 10, 0.5f, 24.0f, &Vx, &Vy);
		CHECK(sqrtf(Vx * Vx + Vy * Vy) <= 24.01f);
	}

	// ---- client layout sanity ----
	CHECK(STRIDER_THIGH + STRIDER_SHIN + STRIDER_ANKLE > STRIDER_HOVER - s_aStriderHipY[0] - 10.0f);
	CHECK(MonolithMuzzleLocal(0.0f).x > MONOLITH_TURRET_X + 100.0f);

	if(!Fail)
		puts("boss v5: slots, act snapshots, attack windows, phases, mobility, part damage and ballistic aim PASS");
	return Fail ? 1 : 0;
}
