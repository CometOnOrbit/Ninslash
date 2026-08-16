#include <cassert>

#include <game/droid_control.h>

int main()
{
	vec2 aPos[4] = {vec2(0, 0), vec2(100, 0), vec2(30, 0), vec2(10, 0)};
	int aHealth[4] = {100, 100, 0, 100};
	int aController[4] = {-1, -1, -1, 2};

	assert(DroidPickNearest(aPos, aHealth, aController, 4, vec2(0, 0), 80.0f) == 0);
	assert(DroidPickNearest(aPos, aHealth, aController, 4, vec2(0, 0), 120.0f) == 0);
	aHealth[0] = 0;
	assert(DroidPickNearest(aPos, aHealth, aController, 4, vec2(0, 0), 80.0f) == -1);
	assert(DroidPickNearest(aPos, aHealth, aController, 4, vec2(0, 0), 150.0f) == 1);

	aHealth[0] = 100;
	aController[0] = 0;
	assert(DroidPickNearest(aPos, aHealth, aController, 4, vec2(0, 0), 150.0f) == 1);

	vec2 Ground = vec2(0, 0);
	DroidControlVel(&Ground, 1, 1, 0, DROIDCONTROL_GROUND, 1);
	assert(Ground.x == 6.0f);
	assert(Ground.y == -9.0f);

	vec2 Coast = vec2(10, 0);
	DroidControlVel(&Coast, 0, 0, 0, DROIDCONTROL_GROUND, 1);
	assert(Coast.x < 10.0f && Coast.x > 5.0f);
	assert(Coast.y == 0.8f);

	vec2 Fly = vec2(0, 0);
	DroidControlVel(&Fly, 1, 1, 0, DROIDCONTROL_FLY, 0);
	assert(Fly.x > 0.0f);
	assert(Fly.y < 0.0f);

	assert(DroidWalkerFace(1, -10, 0, -1) == 1);
	assert(DroidWalkerFace(0, -10, 1, 1) == -1);
	assert(DroidWalkerFace(0, 0, 0, -1) == -1);
	assert(DroidWalkerAnim(0) == 0);
	assert(DroidWalkerAnim(-1) == 1);
	assert(DroidWalkerCanStep(0, 0, 1) == 0);
	assert(DroidWalkerCanStep(1, 1, 1) == 0);
	assert(DroidWalkerCanStep(1, 0, 0) == 0);
	assert(DroidWalkerCanStep(1, 0, 1) == 1);
	assert(DroidWalkerCanStep(-1, 0, 1) == 1);

	vec2 Placed = DroidWalkerPlaceOnFloor(vec2(10, 0), vec2(10, 100), 1);
	assert(Placed.x == 10.0f);
	assert(Placed.y == 82.0f);
	Placed = DroidWalkerPlaceOnFloor(vec2(10, 0), vec2(0, 0), 0);
	assert(Placed.y == 16.0f);
	assert(DroidWalkerFall(1, 8.0f) == 0.0f);
	assert(DroidWalkerFall(0, 8.0f) == 8.0f);

	assert(DroidCrawlerAnim(0, 0) == DROIDCRAWLER_ANIM_IDLE);
	assert(DroidCrawlerAnim(1, 0) == DROIDCRAWLER_ANIM_ATTACK);
	assert(DroidCrawlerAnim(1, 1) == DROIDCRAWLER_ANIM_JUMPATTACK);
	assert(DroidCrawlerAccel(0) == 0.9f);
	assert(DroidCrawlerAccel(1) == 1.8f);
	assert(DroidCrawlerSpeedCap(0) == 8.0f);
	assert(DroidCrawlerCanJump(1, 1, 0) == 1);
	assert(DroidCrawlerCanJump(1, 1, 1) == 0);
	assert(DroidCrawlerCanJump(1, 0, 0) == 0);

	CDroidCrawlerControl Normal = DroidCrawlerControlNormal();
	assert(Normal.m_BoxX == 60.0f);
	assert(Normal.m_OffYJump == 50);
	assert(DroidCrawlerAccelOf(Normal, 0) == 0.9f);
	assert(DroidCrawlerSpeedCapOf(Normal, 1) == 15.0f);

	CDroidCrawlerControl Boss = DroidCrawlerControlBoss();
	assert(Boss.m_BoxX == 90.0f);
	assert(Boss.m_BoxY == 100.0f);
	assert(Boss.m_OffYJump == 90);
	assert(Boss.m_OffYGround == 160);
	assert(Boss.m_JumpForce == -9.0f);
	assert(Boss.m_ProjX == 84.0f);
	assert(Boss.m_ProjY == -54.0f);
	assert(DroidCrawlerAccelOf(Boss, 0) == 1.2f);
	assert(DroidCrawlerSpeedCapOf(Boss, 0) == 16.0f);
	return 0;
}
