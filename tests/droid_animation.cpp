#include <cstdio>
#include <initializer_list>

#include <base/system.h>
#include <base/vmath.h>
#include <generated/protocol.h>

// Exercise the real animation tick without a graphics client. Collision is an
// empty world so the test can isolate whether leg targets follow the body.
class CCollision
{
  public:
	void MoveBox(vec2 *pPos, vec2 *pVel, vec2, float, bool) { *pPos += *pVel; }
	int IntersectLine(vec2, vec2, vec2 *, vec2 *, bool = false, bool = false, bool = true) { return 0; }
};

class CGameClient
{
	CCollision m_Collision;

  public:
	CCollision *Collision() { return &m_Collision; }
};

#define GAME_CLIENT_GAMECLIENT_H
#include "../src/game/client/customstuff/droidanim.cpp"

int main()
{
	CGameClient Client;
	const int aTypes[] = {
		DROIDTYPE_MENDERCRAWLER,
		DROIDTYPE_CYCLONECRAWLER,
		DROIDTYPE_CRAWLER,
		DROIDTYPE_BOSSCRAWLER,
		DROIDTYPE_BOSSSPLITTER,
		DROIDTYPE_SIEGEBREAKERCRAWLER,
		DROIDTYPE_SPLITCRAWLER,
		DROIDTYPE_STALKERCRAWLER};
	for(int Type : aTypes)
	{
		for(int Animation : {DROIDANIM_IDLE, DROIDANIM_MOVE, DROIDANIM_ATTACK, DROIDANIM_JUMPATTACK})
		{
			CDroidAnim Anim(&Client);
			Anim.m_Type = Type;
			Anim.m_Anim = Animation;
			Anim.m_Pos = vec2(1000.0f, 500.0f);
			Anim.m_Vel = vec2(3.0f, 0.0f);
			Anim.Tick();
			const vec2 Offset(240.0f, 80.0f);
			vec2 aPrevious[4];
			for(int Leg = 0; Leg < 4; ++Leg)
				aPrevious[Leg] = Anim.m_aLegTargetPos[Leg];
			Anim.m_Pos += Offset;
			Anim.Tick();
			for(int Leg = 0; Leg < 4; ++Leg)
			{
				const vec2 Movement = Anim.m_aLegTargetPos[Leg] - aPrevious[Leg];
				if(distance(Movement, Offset) > 20.0f)
				{
					fprintf(stderr,
						"crawler type=%d animation=%d leg=%d did not follow body: (%.1f, %.1f)\n",
						Type,
						Animation,
						Leg,
						Movement.x,
						Movement.y);
					return 1;
				}
			}
		}
	}
	return 0;
}
