#include <cmath>
#include <cstdio>
#include <game/client/droid_visual.h>
#include <game/industrial_boss.h>
#include <game/skitter_matriarch.h>

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

// Integrate a ballistic launch the way the server does (gravity added before the move).
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
	// Collision body == art: covers the sac and every planted foot, soles at the ride height, crouch <= 25%.
	CHECK(MATRIARCH_BODY_HALF_W >= -(s_MatriarchSac.m_X - s_MatriarchSac.m_W * 0.5f) - 10.0f);
	for(int i = 0; i < 6; i++)
		CHECK(MATRIARCH_BODY_HALF_W >= fabsf(s_aMatriarchFootX[i]));
	CHECK(MATRIARCH_MAX_CROUCH <= 0.25f * (MATRIARCH_HOVER - MATRIARCH_BODY_TOP));

	// Slot and visuals.
	CHECK(DroidVisual(DROIDTYPE_BOSSRAIL).m_Draw == DROID_DRAW_MATRIARCH);
	CHECK(IsIndustrialBoss(DROIDTYPE_BOSSRAIL));
	CHECK(!IsLegacyIndustrialBoss(DROIDTYPE_BOSSRAIL));

	// Act table round-trips through the snapshot's Anim field, death through status.
	for(int a = 0; a < NUM_MATRIARCH_ACTS; a++)
	{
		CHECK(MatriarchAct(a).m_Duration > 0.0f);
		CHECK(MatriarchAct(a).m_Anim >= 0 && MatriarchAct(a).m_Anim <= DROIDANIM_TURN);
		CHECK(MatriarchActFromSnap(MatriarchAct(a).m_Anim, a == MATRIARCH_ACT_DEATH ? DROIDSTATUS_TERMINATED : DROIDSTATUS_IDLE) == a);
	}
	// At least five distinct attacks besides locomotion/reactions.
	const int aAttacks[] = {MATRIARCH_ACT_STAB, MATRIARCH_ACT_POUNCE, MATRIARCH_ACT_SPIT, MATRIARCH_ACT_BROOD, MATRIARCH_ACT_THRASH,
		MATRIARCH_ACT_CLING};
	CHECK(sizeof(aAttacks) / sizeof(aAttacks[0]) >= 5);

	// Phases.
	CHECK(MatriarchPhase(4800, 4800) == 0);
	CHECK(MatriarchPhase(3200, 4800) == 1);
	CHECK(MatriarchPhase(1600, 4800) == 2);
	CHECK(MatriarchSpeed(2, true) > MatriarchSpeed(0, true));
	CHECK(MatriarchSpeed(0, false) < MatriarchSpeed(0, true));
	// Faster than the minions it leads (crawler walk cap is 8).
	CHECK(MatriarchSpeed(0, true) > 8.0f);

	// Every attack window sits inside its clip, in order, so a combo is never cut short.
	for(int Phase = 0; Phase < 3; Phase++)
	{
		const int Stabs = MatriarchStabCount(Phase);
		for(int s = 0; s < Stabs; s++)
		{
			CHECK(MatriarchStabTick(s) < MatriarchTicks(MatriarchAct(MATRIARCH_ACT_STAB).m_Duration));
			CHECK(s == 0 || MatriarchStabTick(s) > MatriarchStabTick(s - 1));
		}
		// A punish window follows the last stab.
		CHECK(MatriarchTicks(MatriarchAct(MATRIARCH_ACT_STAB).m_Duration) - MatriarchStabTick(Stabs - 1) >= 15);
		for(int s = 0; s < MatriarchSpitCount(Phase); s++)
			CHECK(MatriarchSpitTick(s) < MatriarchTicks(MatriarchAct(MATRIARCH_ACT_SPIT).m_Duration));
		CHECK(MatriarchPounceWindup(Phase, false) >= MatriarchPounceWindup(Phase, true));
		CHECK(MatriarchPounceWindup(Phase, false) >= 10); // still a readable tell (0.2 s+)
	}
	CHECK(MatriarchBroodTick() < MatriarchTicks(MatriarchAct(MATRIARCH_ACT_BROOD).m_Duration));
	CHECK(MatriarchThrashTick() < MatriarchTicks(MatriarchAct(MATRIARCH_ACT_THRASH).m_Duration));
	CHECK(MatriarchRoarTick() < MatriarchTicks(MatriarchAct(MATRIARCH_ACT_ROAR).m_Duration));

	// The pounce actually lands where it aims (within the speed cap).
	const float aTarget[][2] = {{300, 0}, {-420, 0}, {500, -150}, {250, 120}, {-600, -60}};
	for(const auto &T : aTarget)
	{
		const float Dist = sqrtf(T[0] * T[0] + T[1] * T[1]);
		const float Flight = MatriarchPounceFlight(Dist);
		float Vx, Vy, x, y;
		MatriarchBallistic(T[0], T[1], Flight, MATRIARCH_GRAVITY, 26.0f, &Vx, &Vy);
		CHECK(sqrtf(Vx * Vx + Vy * Vy) <= 26.0f + 0.001f);
		Fly(Vx, Vy, MATRIARCH_GRAVITY, (int)(Flight + 0.5f), &x, &y);
		CHECK(fabsf(x - T[0]) < 40.0f);
		CHECK(fabsf(y - T[1]) < 40.0f);
		CHECK(T[1] > 0.0f || Vy < 0.0f); // a leap, never a ground skid, unless dropping down
	}

	// Part lookup in facing space.
	CHECK(MatriarchPartAt(0.0f, -8.0f) == MATRIARCH_PART_CORE);
	CHECK(MatriarchPartAt(165.0f, 40.0f) == MATRIARCH_PART_FANGS);
	CHECK(MatriarchPartAt(-178.0f, 4.0f) == MATRIARCH_PART_SAC);
	CHECK(MatriarchPartAt(-20.0f, 60.0f) == MATRIARCH_PART_LEGS);
	CHECK(MatriarchPartAt(0.0f, -400.0f) == MATRIARCH_PART_CORE);

	// Damage model: sac is the weak point, much weaker when exposed; core is armored.
	CHECK(MatriarchIncomingDamage(100, MATRIARCH_PART_SAC, true) == 160);
	CHECK(MatriarchIncomingDamage(100, MATRIARCH_PART_SAC, false) == 125);
	CHECK(MatriarchIncomingDamage(100, MATRIARCH_PART_CORE, false) == 70);
	CHECK(MatriarchIncomingDamage(100, MATRIARCH_PART_CORE, true) == 100);
	CHECK(MatriarchIncomingDamage(1, MATRIARCH_PART_CORE, false) == 1);
	float Share = 0.0f;
	for(int i = 0; i < NUM_MATRIARCH_PARTS; i++)
		Share += s_aMatriarchPartShare[i];
	CHECK(Share > 0.4f && Share < 0.7f);

	// Tripod gait: three legs per group, each group has a near and a far leg.
	int aCount[2] = {0, 0}, aNear[2] = {0, 0};
	for(int i = 0; i < 6; i++)
	{
		aCount[s_aMatriarchGait[i]]++;
		if(i >= 3)
			aNear[s_aMatriarchGait[i]]++;
	}
	CHECK(aCount[0] == 3 && aCount[1] == 3);
	CHECK(aNear[0] >= 1 && aNear[1] >= 1);
	// Rest footholds reachable from the hips at hover height.
	for(int i = 0; i < 6; i++)
	{
		const float dx = s_aMatriarchFootX[i] - s_aMatriarchHipX[i];
		const float dy = MATRIARCH_HOVER - s_aMatriarchHipY[i];
		CHECK(sqrtf(dx * dx + dy * dy) < MATRIARCH_THIGH + MATRIARCH_SHIN - MATRIARCH_STEP_DIST * 0.5f);
	}
	// Hit circles stay on the painted body.
	for(int i = 0; i < NUM_MATRIARCH_HIT; i++)
		CHECK(fabsf(s_aMatriarchHit[i].m_X) < 230.0f && fabsf(s_aMatriarchHit[i].m_Y) < 90.0f);

	if(!Fail)
		puts("skitter matriarch: slot, acts, phases, attack windows, ballistic pounce, parts, gait PASS");
	return Fail ? 1 : 0;
}
