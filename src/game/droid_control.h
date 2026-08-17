#ifndef GAME_DROID_CONTROL_H
#define GAME_DROID_CONTROL_H

#include <base/vmath.h>

enum
{
	DROIDCONTROL_GROUND = 0,
	DROIDCONTROL_FLY,
};

inline int DroidWalkerFace(int StepDir, int AimX, int Firing, int PrevDir)
{
	if(Firing && AimX)
		return AimX < 0 ? -1 : 1;
	if(StepDir)
		return StepDir < 0 ? -1 : 1;
	if(AimX)
		return AimX < 0 ? -1 : 1;
	return PrevDir < 0 ? -1 : 1;
}

inline int DroidWalkerCanStep(int Dir, int WallSolid, int FloorSolid)
{
	if(Dir != -1 && Dir != 1)
		return 0;
	return !WallSolid && FloorSolid ? 1 : 0;
}

inline int DroidWalkerAnim(int StepDir)
{
	return StepDir ? 1 : 0;
}

enum
{
	DROIDWALKER_FLOOR = 18,
};

inline vec2 DroidWalkerPlaceOnFloor(vec2 From, vec2 Hit, int HitFound)
{
	if(HitFound)
		return vec2(From.x, Hit.y - DROIDWALKER_FLOOR);
	return From + vec2(0, 16);
}

inline float DroidWalkerFall(int FloorSolid, float Step)
{
	return FloorSolid ? 0.0f : Step;
}

enum
{
	DROIDCRAWLER_ANIM_IDLE = 0,
	DROIDCRAWLER_ANIM_MOVE = 1,
	DROIDCRAWLER_ANIM_ATTACK = 2,
	DROIDCRAWLER_ANIM_JUMPATTACK = 3,
};

inline int DroidCrawlerAnim(int Firing, int Jumping)
{
	if(Jumping)
		return DROIDCRAWLER_ANIM_JUMPATTACK;
	if(Firing)
		return DROIDCRAWLER_ANIM_ATTACK;
	return DROIDCRAWLER_ANIM_IDLE;
}

inline int DroidCrawlerCanJump(int Jump, int Grounded, int Jumping)
{
	return Jump && Grounded && !Jumping ? 1 : 0;
}

struct CDroidCrawlerControl
{
	float m_BoxX;
	float m_BoxY;
	int m_OffYJump;
	int m_OffYGround;
	float m_JumpForce;
	float m_ProjX;
	float m_ProjY;
	float m_Hook;
	int m_CoreRad;
	float m_Friction;
	float m_Accel;
	float m_AccelFire;
	float m_Cap;
	float m_CapFire;
};

inline CDroidCrawlerControl DroidCrawlerControlNormal()
{
	CDroidCrawlerControl C = {60.0f, 60.0f, 50, 80, -7.0f, 54.0f, -20.0f, 0.5f, 30, 0.8f, 0.9f, 1.8f, 8.0f, 15.0f};
	return C;
}

inline CDroidCrawlerControl DroidCrawlerControlBoss()
{
	CDroidCrawlerControl C = {90.0f, 100.0f, 90, 160, -9.0f, 84.0f, -54.0f, 0.1f, 60, 0.9f, 1.2f, 1.8f, 16.0f, 20.0f};
	return C;
}

inline float DroidCrawlerAccelOf(const CDroidCrawlerControl &C, int Firing)
{
	return Firing ? C.m_AccelFire : C.m_Accel;
}

inline float DroidCrawlerSpeedCapOf(const CDroidCrawlerControl &C, int Firing)
{
	return Firing ? C.m_CapFire : C.m_Cap;
}

inline float DroidCrawlerAccel(int Firing)
{
	return DroidCrawlerAccelOf(DroidCrawlerControlNormal(), Firing);
}

inline float DroidCrawlerSpeedCap(int Firing)
{
	return DroidCrawlerSpeedCapOf(DroidCrawlerControlNormal(), Firing);
}

inline void DroidControlVel(vec2 *pVel, int Direction, int Jump, int Down, int Kind, int Grounded)
{
	if(!pVel)
		return;
	if(Kind == DROIDCONTROL_FLY)
	{
		pVel->x += Direction * 0.75f;
		if(Jump)
			pVel->y -= 0.75f;
		if(Down)
			pVel->y += 0.75f;
		*pVel *= 0.90f;
		float Speed = length(*pVel);
		if(Speed > 20.0f)
			*pVel = normalize(*pVel) * 20.0f;
	}
	else
	{
		pVel->y += 0.8f;
		if(Direction)
			pVel->x = Direction * 6.0f;
		else
			pVel->x *= 0.6f;
		if(Jump && Grounded)
			pVel->y = -9.0f;
	}
}

inline int DroidPickNearest(const vec2 *pPos,
							const int *pHealth,
							const int *pController,
							int Count,
							vec2 From,
							float MaxDist)
{
	int Best = -1;
	float BestSq = MaxDist * MaxDist;
	for(int i = 0; i < Count; i++)
	{
		if(pHealth[i] <= 0 || pController[i] >= 0)
			continue;
		float dx = pPos[i].x - From.x;
		float dy = pPos[i].y - From.y;
		float Sq = dx * dx + dy * dy;
		if(Sq < BestSq)
		{
			BestSq = Sq;
			Best = i;
		}
	}
	return Best;
}

#endif
