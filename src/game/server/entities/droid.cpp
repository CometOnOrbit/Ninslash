#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/droid_control.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>
#include <game/server/tutorial_director.h>
#include "droid.h"

static int DroidControlKindForType(int Type)
{
	if(Type == DROIDTYPE_STAR || Type == DROIDTYPE_FLY || Type == DROIDTYPE_BOSSSTAR ||
	   Type == DROIDTYPE_TEMPESTSTAR || Type == DROIDTYPE_KAMIKAZESTAR || Type == DROIDTYPE_RAILSTAR ||
	   Type == DROIDTYPE_TESLASTAR)
		return DROIDCONTROL_FLY;
	return DROIDCONTROL_GROUND;
}

static vec2 DroidControlBox(int Type, float Radius)
{
	if(Type == DROIDTYPE_WALKER || Type == DROIDTYPE_BOSSWALKER)
		return vec2(78.0f, 64.0f);
	if(DroidControlKindForType(Type) == DROIDCONTROL_FLY)
		return vec2(96.0f, 128.0f);
	if(Type == DROIDTYPE_BOSSCRAWLER)
		return vec2(90.0f, 100.0f);
	float Size = max(Radius, 60.0f);
	return vec2(Size, Size);
}

CDroid::CDroid(CGameWorld *pGameWorld, vec2 Pos, int Type) : CEntity(pGameWorld, CGameWorld::ENTTYPE_DROID)
{
	m_ProximityRadius = DroidPhysSize;

	m_StartPos = Pos;
	m_Type = Type;

	Reset();
	// GameWorld()->InsertEntity(this);
}

void CDroid::Reset()
{
	m_Center = vec2(0, -50);
	m_Health = 100;
	m_MaxHealth = m_Health;
	m_Pos = m_StartPos;
	m_Status = 0;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(0);
	m_TargetIndex = -1;
	m_ReloadTimer = 0;
	m_AttackTick = 0;
	m_TargetTimer = 0;
	m_Target = vec2(0, 0);
	m_NewTarget = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_FlyTargetTick = 0;
	m_Mode = 0;
	m_ProximityRadius = DroidPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_Controller = -1;
}

void CDroid::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0)
		return;
	// skip everything while spawning
	// if (m_aStatus[STATUS_SPAWNING] > 0.0f)
	//	return false;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
	{
		const bool Boss = m_Type == DROIDTYPE_BOSSCRAWLER || m_Type == DROIDTYPE_BOSSSTAR ||
						  m_Type == DROIDTYPE_BOSSWALKER || m_Type == DROIDTYPE_BOSSSPLITTER;
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, Boss, this);
	}

	vec2 DmgPos = m_Pos + m_Center;

	// create damage indicator
	if(Combat.m_ElectroAmount > 0.0f)
		m_Status = DROIDSTATUS_ELECTRIC;
	else if(Combat.m_FlameAmount > 0.0f)
		m_Status = DROIDSTATUS_HURT;
	else
	{
		if(Pos.x != 0 && Pos.y != 0)
			DmgPos = Pos;

		GameServer()->CreateBuildingHit(DmgPos);
		m_Status = DROIDSTATUS_HURT;
	}

	GameServer()->CreateDamageInd(DmgPos, GetAngle(-Force), -Dmg, -1);

	m_Vel += Force * 0.75f;

	const int HealthBefore = m_Health;
	m_Health -= Dmg;
	if(GameServer()->m_pTutorialDirector)
	{
		CPlayer *pFromPlayer = GameServer()->GetClientPlayer(From);
		if(pFromPlayer && !pFromPlayer->m_IsBot)
			GameServer()->m_pTutorialDirector->OnGameplayProgress(From, TUTORIAL_EVENT_TARGET_HIT);
	}
	GameServer()->CreateHitConfirm(DmgPos, Source, min(Dmg, HealthBefore), HIT_TARGET_METAL, m_Health <= 0);

	// check for death
	if(m_Health <= 0)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		// set attacker's face to happy (taunt!)
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

		GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true));
		m_DeathTick = Server()->Tick();

		// random pickup drop
		if(frandom() * 10 < 4)
			GameServer()->m_pController->DropPickup(
				m_Pos + vec2(0, -42),
				POWERUP_AMMO,
				Force + vec2(frandom() * 6.0 - frandom() * 6.0, frandom() * 6.0 - frandom() * 6.0),
				0);
		else if(frandom() * 10 < 4)
			GameServer()->m_pController->DropPickup(
				m_Pos + vec2(0, -42),
				POWERUP_HEALTH,
				Force + vec2(frandom() * 6.0 - frandom() * 6.0, frandom() * 6.0 - frandom() * 6.0),
				0);
		else if(frandom() * 10 < 4)
			GameServer()->m_pController->DropPickup(
				m_Pos + vec2(0, -42),
				POWERUP_ARMOR,
				Force + vec2(frandom() * 6.0 - frandom() * 6.0, frandom() * 6.0 - frandom() * 6.0),
				0);
		else
			GameServer()->m_pController->DropPickup(
				m_Pos + vec2(0, -42),
				POWERUP_KIT,
				Force + vec2(frandom() * 6.0 - frandom() * 6.0, frandom() * 6.0 - frandom() * 6.0),
				0);

		return;
	}

	m_DamageTakenTick = Server()->Tick();
}

CDroid::~CDroid()
{
	DropController();
}

void CDroid::DropController()
{
	if(m_Controller < 0)
		return;
	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	m_Controller = -1;
	if(pPlayer && pPlayer->GetDroid() == this)
		pPlayer->ReleaseDroid();
}

CAttackSource CDroid::ShotSource() const
{
	return CAttackSource::Droid(m_Controller >= 0 ? m_Controller : NEUTRAL_BASE, m_Type);
}

bool CDroid::TakeControl()
{
	if(m_Controller < 0)
		return false;

	if(m_Health <= 0)
	{
		DropController();
		return false;
	}

	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		DropController();
		return false;
	}

	return true;
}

bool CDroid::TickWalkerControl(int CoreRad)
{
	if(!TakeControl())
		return false;

	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	const CNetObj_PlayerInput *pIn = &pPlayer->m_DroidInput;
	vec2 Aim = vec2(pIn->m_TargetX, pIn->m_TargetY);
	if(Aim.x == 0 && Aim.y == 0)
		Aim.y = -1;

	const int StepDir = pIn->m_Direction;
	const int Firing = pIn->m_Fire & 1;
	m_Dir = DroidWalkerFace(StepDir, (int)Aim.x, Firing, m_Dir);
	m_Anim = DroidWalkerAnim(StepDir);
	m_State = StepDir ? MOVE : IDLE;
	m_NextState = m_State;

	const int OnFloor = GameServer()->Collision()->IsTileSolid(m_Pos.x, m_Pos.y + DROIDWALKER_FLOOR);
	m_Pos.y += DroidWalkerFall(OnFloor, 8.0f);

	if(StepDir == -1 || StepDir == 1)
	{
		const int Wall = GameServer()->Collision()->IsTileSolid(m_Pos.x + StepDir * 46, m_Pos.y - 8);
		const int Floor = GameServer()->Collision()->IsTileSolid(m_Pos.x + StepDir * 55, m_Pos.y + 18);
		if(DroidWalkerCanStep(StepDir, Wall, Floor))
			m_Pos.x += StepDir * 6.0f;
	}

	if(Firing)
	{
		m_NewTarget = Aim * -1.0f;
		m_Target += (m_NewTarget - m_Target) / 6.0f;
		if(--m_FireDelay < 0)
		{
			m_FireDelay = 0;
			Fire();
		}
	}
	else
	{
		m_FireDelay = min(m_FireDelay + 2, 20);
		m_Target += (vec2(m_Dir * 50, 0) - m_Target) / 6.0f;
	}

	if(GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, 0), 2, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	if(m_Health <= 0)
	{
		DropController();
		return false;
	}

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;

	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, CoreRad);
	return true;
}

bool CDroid::TickCrawlerControl(const CDroidCrawlerControl &Control,
							   int *pMove,
							   int *pJumpTick,
							   float *pJumpForce,
							   int *pAttackCount)
{
	if(!TakeControl() || !pMove || !pJumpTick || !pJumpForce || !pAttackCount)
		return false;

	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	const CNetObj_PlayerInput *pIn = &pPlayer->m_DroidInput;
	vec2 Aim = vec2(pIn->m_TargetX, pIn->m_TargetY);
	if(Aim.x == 0 && Aim.y == 0)
		Aim.y = -1;

	const int Firing = pIn->m_Fire & 1;
	*pMove = pIn->m_Direction;
	m_Dir = DroidWalkerFace(*pMove, (int)Aim.x, Firing, m_Dir);
	m_Target = Aim;

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * Control.m_Hook;
	m_Vel.y += 0.8f;
	m_Vel *= 0.99f;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(Control.m_BoxX, Control.m_BoxY), 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, Control.m_CoreRad);

	const int Jumping = *pJumpForce < -0.1f ? 1 : 0;
	const int OffY = Jumping ? Control.m_OffYJump : Control.m_OffYGround;
	vec2 To = m_Pos + vec2(0, OffY);
	const int Grounded = GameServer()->Collision()->IntersectLine(m_Pos, To, 0x0, &To, false, true) ? 1 : 0;

	if(Grounded)
	{
		float VelY = m_Pos.y - (To.y - OffY) * 0.0002f;
		if(VelY > 0.0f && !Jumping)
		{
			m_Vel.y -= min(1.4f, VelY);
			m_Vel.y *= 0.99f;
		}

		m_Vel.x *= Control.m_Friction;
		const float Cap = DroidCrawlerSpeedCapOf(Control, Firing);
		if(abs(m_Vel.x) < Cap)
			m_Vel.x += *pMove * DroidCrawlerAccelOf(Control, Firing);

		if(DroidCrawlerCanJump(pIn->m_Jump, Grounded, Jumping))
			*pJumpForce = Control.m_JumpForce;

		m_Vel.y += *pJumpForce;
		m_Vel.x -= *pJumpForce * *pMove * 0.25f;
	}
	else if(*pJumpTick && *pJumpTick < Server()->Tick())
		*pJumpTick = 0;

	m_Vel.x -= *pJumpForce * *pMove * 0.1f;
	*pJumpForce *= 0.9f;
	m_Anim = DroidCrawlerAnim(Firing, *pJumpForce < -0.1f ? 1 : 0);

	if(Firing && (m_Anim == DROIDCRAWLER_ANIM_JUMPATTACK || m_Anim == DROIDCRAWLER_ANIM_ATTACK))
	{
		if((*pAttackCount)++ > 3)
		{
			*pAttackCount = 0;
			const int ShotDir = *pMove ? *pMove : m_Dir;
			vec2 ProjPos = To + vec2(ShotDir * Control.m_ProjX, Control.m_ProjY);
			GameServer()->CreateProjectile(ShotSource(), 0, ProjPos, normalize(m_Pos - ProjPos), m_Pos);
			m_AttackTick = Server()->Tick();
		}
	}
	else
		*pAttackCount = 0;

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;

	return true;
}

bool CDroid::TickFlyerControl(vec2 Box, int CoreRad)
{
	if(!TakeControl())
		return false;

	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	const CNetObj_PlayerInput *pIn = &pPlayer->m_DroidInput;
	vec2 Aim = vec2(pIn->m_TargetX, pIn->m_TargetY);
	if(Aim.x == 0 && Aim.y == 0)
		Aim.y = -1;

	const int Firing = pIn->m_Fire & 1;
	m_Dir = DroidWalkerFace(pIn->m_Direction, (int)Aim.x, Firing, m_Dir);
	m_Anim = pIn->m_Direction || pIn->m_Jump || pIn->m_Down ? 1 : 0;

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID);
	DroidControlVel(&m_Vel, pIn->m_Direction, pIn->m_Jump, pIn->m_Down, DROIDCONTROL_FLY, 0);
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, Box, 0, false, true);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, CoreRad);

	if(GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, -0.5f), 2, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	if(m_Health <= 0)
	{
		DropController();
		return false;
	}

	if(Firing)
	{
		m_NewTarget = Aim * -1.0f;
		m_Target += (m_NewTarget - m_Target) / 6.0f;
		if(--m_FireDelay < 0)
		{
			m_FireDelay = 0;
			Fire();
		}
	}
	else
	{
		m_FireDelay = min(m_FireDelay + 2, 20);
		m_Target += (vec2(m_Dir * 50, 0) - m_Target) / 6.0f;
	}

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;

	return true;
}

bool CDroid::TickControlled()
{
	if(m_Type == DROIDTYPE_BOSSWALKER)
		return TickWalkerControl(80);
	if(m_Type == DROIDTYPE_STAR || m_Type == DROIDTYPE_BOSSSTAR)
		return TickFlyerControl(vec2(96.0f, 128.0f), 40);

	if(!TakeControl())
		return false;

	CPlayer *pPlayer = GameServer()->GetClientPlayer(m_Controller);
	const CNetObj_PlayerInput *pIn = &pPlayer->m_DroidInput;
	vec2 Aim = vec2(pIn->m_TargetX, pIn->m_TargetY);
	if(Aim.x == 0 && Aim.y == 0)
		Aim.y = -1;
	m_Target = Aim * -1.0f;
	m_NewTarget = m_Target;
	m_Dir = pIn->m_Direction ? pIn->m_Direction : (Aim.x < 0 ? -1 : 1);
	m_Anim = pIn->m_Direction ? 1 : 0;

	const int Kind = DroidControlKindForType(m_Type);
	const vec2 Box = DroidControlBox(m_Type, m_ProximityRadius);
	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID);
	const bool Grounded = GameServer()->Collision()->CheckPoint(m_Pos.x, m_Pos.y + Box.y * 0.5f + 5);
	DroidControlVel(&m_Vel, pIn->m_Direction, pIn->m_Jump, pIn->m_Down, Kind, Grounded ? 1 : 0);
	if(Kind == DROIDCONTROL_FLY)
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, Box, 0, false, true);
	else
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, Box, 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 40);

	if(GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, 0), 2, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	if(m_Health <= 0)
	{
		DropController();
		return false;
	}

	if(pIn->m_Fire & 1)
		Fire();

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;

	return true;
}

void CDroid::Fire()
{
}

void CDroid::Tick()
{
}

bool CDroid::Target()
{
	vec2 TurretPos = m_Pos + m_Center;

	if(m_TargetIndex >= 0 && m_TargetIndex < MAX_CLIENTS)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[m_TargetIndex];
		if(!pPlayer)
			return false;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter)
			return false;

		if(!pCharacter->IsAlive())
			return false;

		if((m_Dir < 0 && pCharacter->m_Pos.x > m_Pos.x) || (m_Dir > 0 && pCharacter->m_Pos.x < m_Pos.x))
		{
			m_Dir *= -1;
			m_State = CDroid::IDLE;
			m_StateChangeTick = Server()->Tick() + Server()->TickSpeed() * (1 + frandom());
		}

		int Distance = distance(pCharacter->m_Pos, TurretPos);
		if(Distance < 700 && !GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), TurretPos))
		{
			vec2 r = vec2(sin(Server()->Tick() * 0.075f), cos(Server()->Tick() * 0.075f)) * Distance * 0.3f;
			m_NewTarget = r + TurretPos - ((pCharacter->m_Pos + vec2(0, -24)) + pCharacter->GetCore().m_Vel * 2.0f);
			return true;
		}
		else
			return false;
	}

	return false;
}

bool CDroid::FindTarget()
{
	m_TargetIndex = -1;
	CCharacter *pClosestCharacter = 0;
	int ClosestDistance = 0;
	vec2 TurretPos = m_Pos + vec2(0, -67);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;

		// if (pPlayer->GetTeam() == m_Team && GameServer()->m_pController->IsTeamplay())
		//	continue;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter)
			continue;

		if(!pCharacter->IsAlive())
			continue;

		if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
			continue;

		if((m_Dir < 0 && pCharacter->m_Pos.x > m_Pos.x) || (m_Dir > 0 && pCharacter->m_Pos.x < m_Pos.x))
			continue;

		int Distance = distance(pCharacter->m_Pos, TurretPos);
		if(Distance < 800 && !GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), TurretPos))
		{
			if(!pClosestCharacter || Distance < ClosestDistance)
			{
				pClosestCharacter = pCharacter;
				ClosestDistance = Distance;
				m_TargetIndex = i;
			}
		}
	}

	if(pClosestCharacter)
		return true;

	return false;
}

void CDroid::SetState(int State)
{
	m_State = State;
	m_NextState = State;
	m_StateChangeTick = Server()->Tick();
}

void CDroid::TickPaused()
{
}

void CDroid::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	m_SnapTick = Server()->Tick();

	CNetObj_Droid *pP =
		static_cast<CNetObj_Droid *>(Server()->SnapNewItem(NETOBJTYPE_DROID, m_ID, sizeof(CNetObj_Droid)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	pP->m_Status = m_Status;
	pP->m_AttackTick = m_Health <= 0 ? m_DeathTick : m_AttackTick;
	pP->m_Anim = m_Anim;
	pP->m_Dir = m_Dir;
	pP->m_Angle = GetAngle(vec2(abs(m_Target.x), -m_Target.y)) * (180 / pi);
}
