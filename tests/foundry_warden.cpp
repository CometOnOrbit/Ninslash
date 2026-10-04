#include <cstdio>
#include <game/foundry_warden.h>
#include <game/server/bosspool.h>

static int Fail(const char *pWhat)
{
	fprintf(stderr, "foundry warden: %s\n", pWhat);
	return 1;
}

int main()
{
	if(DROIDTYPE_BOSSWARDEN != 17)
		return Fail("type id");
	if(DROIDANIM_MORTAR != 4 || DROIDANIM_STAGGER != 7)
		return Fail("anim ids");
	if(WardenTicks(0.775f) != 39 || WardenClampImpactTick() != 39)
		return Fail("clamp impact");
	if(WardenMortarShotTick(0) != 36 || WardenMortarShotTick(1) != 54 || WardenMortarShotTick(2) != 72)
		return Fail("mortar shots");
	if(WardenSlamImpactTick() != 56)
		return Fail("slam impact");
	if(WardenTicks(0.52f) != 26 || WardenTicks(2.55f) != 128)
		return Fail("vent window");
	if(DROIDANIM_GRAB != 8 || WardenActFromSnap(DROIDANIM_GRAB, DROIDSTATUS_IDLE) != WARDEN_ACT_GRAB)
		return Fail("grab anim");
	if(DROIDANIM_CHARGE != 9 || WardenActFromSnap(DROIDANIM_CHARGE, DROIDSTATUS_IDLE) != WARDEN_ACT_CHARGE ||
	   WardenChargeLaunchTick() != 20 || WardenChargeEndTick() != 55 ||
	   WardenChargeEndTick() >= WardenTicks(WardenAct(WARDEN_ACT_CHARGE).m_Duration))
		return Fail("charge timing");
	if(WardenPhase(2801, 4200) != 0 || WardenPhase(2800, 4200) != 1 || WardenPhase(1401, 4200) != 1 ||
	   WardenPhase(1400, 4200) != 2)
		return Fail("phases");
	if(!WardenCoreExposed(WARDEN_ACT_VENT, 26) || WardenCoreExposed(WARDEN_ACT_VENT, 128) ||
	   WardenCoreExposed(WARDEN_ACT_CLAMP, 40))
		return Fail("core window");
	if(WardenIncomingDamage(10, WARDEN_PART_CORE, true) != 16 || WardenIncomingDamage(10, WARDEN_PART_CORE, false) != 7 ||
	   WardenIncomingDamage(10, WARDEN_PART_ARMS, true) != 10 || WardenIncomingDamage(1, WARDEN_PART_CORE, false) != 1)
		return Fail("part damage");
	if(WardenPartAt(0.0f, -52.0f) != WARDEN_PART_CORE || WardenPartAt(-180.0f, -52.0f) != WARDEN_PART_ARMS ||
	   WardenPartAt(180.0f, -52.0f) != WARDEN_PART_ARMS || WardenPartAt(-110.0f, 45.0f) != WARDEN_PART_LEGS ||
	   WardenPartAt(60.0f, -165.0f) != WARDEN_PART_MORTAR || WardenPartAt(0.0f, 0.0f) != WARDEN_PART_CORE ||
	   WardenPartAt(5000.0f, 5000.0f) != WARDEN_PART_CORE)
		return Fail("part routing");
	if(WardenActFromSnap(DROIDANIM_MORTAR, DROIDSTATUS_IDLE) != WARDEN_ACT_MORTAR)
		return Fail("snap mortar");
	if(WardenActFromSnap(DROIDANIM_IDLE, DROIDSTATUS_TERMINATED) != WARDEN_ACT_DEATH)
		return Fail("snap death");
	if(WardenClipTime(WARDEN_ACT_IDLE, 0, WARDEN_TICK_SPEED, 0.0f) < 0.99f ||
	   WardenClipTime(WARDEN_ACT_IDLE, 0, WARDEN_TICK_SPEED, 0.0f) > 1.01f)
		return Fail("idle loop");
	if(WardenClipTime(WARDEN_ACT_CLAMP, 0, WARDEN_TICK_SPEED * 10, 0.0f) != WardenAct(WARDEN_ACT_CLAMP).m_Duration)
		return Fail("clamp clamp");

	for(int i = 0; i < 4; i++)
	{
		const float FootY = -WARDEN_FOOT_ABOVE_GROUND;
		const float Away = s_aWardenFootX[i] < s_aWardenHipX[i] ? -WARDEN_STEP_DIST : WARDEN_STEP_DIST;
		const float dx = s_aWardenFootX[i] + Away - s_aWardenHipX[i];
		const float dy = FootY - s_aWardenHipY[i];
		const float Reach = s_aWardenUpper[i] + s_aWardenLower[i];
		if(sqrtf(dx * dx + dy * dy) > Reach - 1.0f)
			return Fail("trailing foot reach");
		float Hip, Knee;
		WardenSolveLegUp(s_aWardenHipX[i], s_aWardenHipY[i], s_aWardenFootX[i], FootY, s_aWardenUpper[i], s_aWardenLower[i], &Hip, &Knee);
		const float kx = s_aWardenHipX[i] + cosf(Hip) * s_aWardenUpper[i];
		const float ky = s_aWardenHipY[i] + sinf(Hip) * s_aWardenUpper[i];
		const float fx = kx + cosf(Hip + Knee) * s_aWardenLower[i];
		const float fy = ky + sinf(Hip + Knee) * s_aWardenLower[i];
		if(fabsf(fx - s_aWardenFootX[i]) > 0.2f || fabsf(fy - FootY) > 0.2f)
			return Fail("ik plant");
		if(ky > s_aWardenHipY[i] + 1.0f && ky > FootY)
			return Fail("knee up");
		if((kx - s_aWardenHipX[i]) * s_aWardenFootX[i] < 0.0f)
			return Fail("knee outward");
	}

	CAttackSource Saw;
	Saw.m_Kind = EAttackSourceKind::Building;
	Saw.m_Type = BUILDING_SAWBLADE;
	CAttackSource Barrel;
	Barrel.m_Kind = EAttackSourceKind::Building;
	Barrel.m_Type = BUILDING_BARREL;
	CAttackSource Turret;
	Turret.m_Kind = EAttackSourceKind::Building;
	Turret.m_Type = BUILDING_TURRET;
	CAttackSource Shot;
	Shot.m_Kind = EAttackSourceKind::PlayerWeapon;
	CAttackSource Acid;
	Acid.m_Kind = EAttackSourceKind::World;
	Acid.m_Type = DAMAGETYPE_FLUID;
	if(!BossIgnoresMapObject(DROIDTYPE_BOSSWARDEN, Saw) || !BossIgnoresMapObject(DROIDTYPE_BOSSANGLER, Barrel) ||
	   !BossIgnoresMapObject(DROIDTYPE_BOSSANGLER, Acid) || BossIgnoresMapObject(DROIDTYPE_BOSSWARDEN, Turret) ||
	   BossIgnoresMapObject(DROIDTYPE_BOSSWARDEN, Shot) || BossIgnoresMapObject(DROIDTYPE_CRAWLER, Saw))
		return Fail("map object immunity");
	return 0;
}
