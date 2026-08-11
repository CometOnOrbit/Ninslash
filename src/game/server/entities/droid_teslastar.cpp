#include <cmath>

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_teslastar.h"
#include "lightning.h"

static constexpr float TESLASTAR_PRIMARY_RANGE = 500.0f;
static constexpr float TESLASTAR_HOP_RANGE = 280.0f;
static constexpr int TESLASTAR_MAX_TARGETS = 4;
static constexpr int TESLASTAR_COOLDOWN_MS = 3000;
static constexpr int TESLASTAR_MAX_PROFILE_DAMAGE = 1000;

CTeslaStar::CTeslaStar(CGameWorld *pGameWorld, vec2 Pos) : CStar(pGameWorld, Pos)
{
	m_Type = DROIDTYPE_TESLASTAR;
	Reset();
}

void CTeslaStar::Reset()
{
	CStar::Reset();
	m_Health = 260;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_ProximityRadius = TeslaStarPhysSize;
	m_NextCastTick = 0;
}

CCharacter *CTeslaStar::ValidTarget(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return nullptr;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return nullptr;

	CCharacter *pCharacter = pPlayer->GetCharacter();
	if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
		return nullptr;

	if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
		return nullptr;

	return pCharacter;
}

int CTeslaStar::FindNextTarget(const bool *pHit, const vec2 &From)
{
	int BestClientID = -1;
	float BestDistanceSquared = TESLASTAR_HOP_RANGE * TESLASTAR_HOP_RANGE;

	for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
	{
		if(pHit[ClientID])
			continue;

		CCharacter *pCharacter = ValidTarget(ClientID);
		if(!pCharacter)
			continue;

		const vec2 TargetPos = pCharacter->m_Pos;
		const vec2 Delta = TargetPos - From;
		const float DistanceSquared = dot(Delta, Delta);
		if(DistanceSquared > BestDistanceSquared || GameServer()->Collision()->FastIntersectLine(From, TargetPos))
			continue;

		if(BestClientID < 0 || DistanceSquared < BestDistanceSquared)
		{
			BestClientID = ClientID;
			BestDistanceSquared = DistanceSquared;
		}
	}

	return BestClientID;
}

void CTeslaStar::Fire()
{
	if(Server()->Tick() < m_NextCastTick)
		return;

	CCharacter *pPrimary = ValidTarget(m_TargetIndex);
	if(!pPrimary)
		return;

	const vec2 Origin = m_Pos + vec2(m_Dir * 16.0f, m_Center.y - 20.0f);
	const vec2 PrimaryPos = pPrimary->m_Pos;
	if(distance(Origin, PrimaryPos) > TESLASTAR_PRIMARY_RANGE ||
	   GameServer()->Collision()->FastIntersectLine(Origin, PrimaryPos))
		return;

	const CAttackSource Source = CAttackSource::Droid(NEUTRAL_BASE, m_Type);
	CWeaponCombatProfile Combat{};
	if(!CWeaponCatalog::TryResolveAttack(Source, &Combat) || !std::isfinite(Combat.m_ProjectileDamage) ||
	   Combat.m_ProjectileDamage <= 0.0f)
		return;

	const int BaseDamage = max(1, (int)(min(Combat.m_ProjectileDamage, (float)TESLASTAR_MAX_PROFILE_DAMAGE) + 0.5f));
	bool aHit[MAX_CLIENTS] = {false};
	int ClientID = m_TargetIndex;
	int Damage = BaseDamage;
	vec2 From = Origin;
	int HitCount = 0;

	for(int Hop = 0; Hop < TESLASTAR_MAX_TARGETS; ++Hop)
	{
		CCharacter *pTarget = ValidTarget(ClientID);
		if(!pTarget || aHit[ClientID])
			break;

		const vec2 TargetPos = pTarget->m_Pos;
		new CLightning(GameWorld(), TargetPos, From);
		GameServer()->CreateEffect(FX_ELECTROHIT, TargetPos);
		aHit[ClientID] = true;
		pTarget->TakeDamage(Source, Damage, vec2(0, 0), TargetPos);
		++HitCount;
		From = TargetPos;
		Damage = max(1, Damage * 3 / 4);
		ClientID = FindNextTarget(aHit, From);
		if(ClientID < 0)
			break;
	}

	if(HitCount <= 0)
		return;

	GameServer()->CreateSound(Origin, SOUND_TESLACOIL_FIRE);
	m_AttackTick = Server()->Tick();
	m_NextCastTick = Server()->Tick() + max(1, TESLASTAR_COOLDOWN_MS * Server()->TickSpeed() / 1000);
}

void CTeslaStar::TickPaused()
{
	CStar::TickPaused();
	if(m_NextCastTick > 0)
		++m_NextCastTick;
}
