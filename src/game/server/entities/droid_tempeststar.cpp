#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_tempeststar.h"

CTempestStar::CTempestStar(CGameWorld *pGameWorld, vec2 Pos) : CStar(pGameWorld, Pos)
{
	m_Type = DROIDTYPE_TEMPESTSTAR;
	Reset();
}

void CTempestStar::Reset()
{
	CStar::Reset();
	m_Health = 300;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_ProximityRadius = TempestStarPhysSize;
}

void CTempestStar::Fire()
{
	if(m_ReloadTimer > 0)
	{
		--m_ReloadTimer;
		return;
	}

	const vec2 Aim = m_Target * -1.0f;
	if(length(Aim) <= 0.0001f)
		return;

	const vec2 CenterDirection = normalize(Aim);
	const float CenterAngle = GetAngle(CenterDirection);
	const float SpreadStep = 10.0f * pi / 180.0f;
	const vec2 TurretPos = m_Pos + vec2(m_Dir * 16, m_Center.y - 20);

	m_ReloadTimer = max(1, 900 * Server()->TickSpeed() / 1000);
	m_Vel -= CenterDirection * 6.0f;
	GameServer()->CreateSound(m_Pos, SOUND_STAR_FIRE);

	for(int Projectile = -2; Projectile <= 2; ++Projectile)
	{
		const float Angle = CenterAngle + Projectile * SpreadStep;
		const vec2 Direction = vec2(cosf(Angle), sinf(Angle));
		GameServer()->CreateProjectile(
			CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0, TurretPos + Direction * 30.0f, Direction, TurretPos);
	}

	m_AttackTick = Server()->Tick();
	if(++m_FireCount >= 3)
	{
		m_FireCount = 0;
		m_FireDelay = 30;
	}
}
