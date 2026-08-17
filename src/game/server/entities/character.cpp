#include <new>
#include <engine/shared/config.h>
#include <engine/platform_events.h>
#include <game/challenge_variant.h>
#include <game/server/gamecontext.h>
#include <game/input_buffer.h>
#include <game/mapitems.h>

#include "character.h"
#include "weapon.h"
#include "building.h"
#include "turret.h"
#include "laser.h"
#include "projectile.h"
#include "droid.h"
#include "laserfail.h"
#include "staticlaser.h"

#include <game/weapons.h>
#include <game/buildables.h>
#include <game/forge.h>

#include <game/server/gamemodes/invasion.h>
#include <game/server/playerdata.h>
#include <game/server/pve_director.h>
#include <game/server/tutorial_director.h>
#include <game/server/player.h>
#include <game/server/ai.h>
#include <game/npc.h>
#include <game/pve_roguelite.h>
#include <game/questinfo.h>

inline vec2 RandomDir()
{
	return normalize(vec2(frandom() - 0.5f, frandom() - 0.5f));
}

namespace
{
int StaticType(const CWeapon *pWeapon)
{
	if(!pWeapon || pWeapon->GetWeaponProfile().m_Definition.m_Kind != EWeaponDefinitionKind::Static)
		return -1;
	return pWeapon->GetWeaponProfile().m_Definition.m_StaticType;
}

bool HasWeaponBehavior(const CWeapon *pWeapon, EWeaponBehaviorFlag Flag)
{
	return pWeapon && WeaponHasBehavior(pWeapon->GetWeaponProfile().m_Definition, Flag);
}

// The melee-only challenge excludes firearms, but still permits melee weapons
// and utility/throwable items (grenades, shields, upgrades, etc.). Modular
// BASE frames are the ranged firearm family; static GUN1/GUN2 and custom
// definitions carrying the compact-gun behavior are firearms as well.
bool IsFirearmWeapon(const CWeapon *pWeapon)
{
	if(!pWeapon)
		return false;

	const CWeaponDefinition &Definition = pWeapon->GetWeaponProfile().m_Definition;
	if(WeaponHasBehavior(Definition, WEAPON_BEHAVIOR_COMPACT_GUN_HANDS))
		return true;

	if(Definition.m_Kind == EWeaponDefinitionKind::Static)
		return Definition.m_StaticType == SW_GUN1 || Definition.m_StaticType == SW_GUN2;

	return Definition.m_Part1 >= PART1_BASE1 && Definition.m_Part1 <= PART1_BASE6;
}

} // namespace

int CCharacter::CurrentWeaponFiringType() const
{
	if(m_WeaponSlot < 0 || m_WeaponSlot >= NUM_SLOTS || !m_apWeapon[m_WeaponSlot])
		return WFT_NONE;
	return m_apWeapon[m_WeaponSlot]->GetWeaponProfile().m_Combat.m_FiringType;
}

#define RAD 0.017453292519943295769236907684886f

// input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

static CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i + 1) & INPUT_STATE_MASK;
		if(i & 1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CHARACTERS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld) : CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_Spawned = false;
	m_ProximityRadius = ms_PhysSize;
	m_HiddenHealth = 100;
	m_MaxHealth = 10;
	m_Health = 0;
	m_Armor = 0;
	m_Kits = 0;
	m_PainSoundTimer = 0;
	m_ElectroWallCooldown = 0;
	m_Silent = false;
	m_IgnoreCollision = false;
	m_SendInventoryTick = 0;
	m_ForceCoreSend = false;

	for(int i = 0; i < NUM_STATUSS; i++)
	{
		m_aStatus[i] = 0;
		m_aStatusSource[i] = CAttackSource::World(WEAPON_WORLD);
	}

	m_LastStatusEffect = 0;
	m_DeathrayTick = 0;

	m_Type = CCharacter::PLAYER;
	m_pPlayer = 0;
	m_pAI = 0;
	m_NpcSlot = -1;
	m_Team = 0;
	m_ToBeKicked = false;
}

CCharacter::~CCharacter()
{
	if(m_pAI)
	{
		delete m_pAI;
		m_pAI = 0;
	}
}

int CCharacter::GetCID() const
{
	return m_pPlayer ? m_pPlayer->GetCID() : -1;
}

int CCharacter::GetTeam()
{
	if(m_pPlayer)
		return m_pPlayer->GetTeam();
	if(GameServer()->m_pController && GameServer()->m_pController->IsCoop() && m_IsBot)
		return TEAM_BLUE;
	return m_Team;
}

int CCharacter::CoreIndex() const
{
	if(m_pPlayer)
		return GetCID();
	if(m_NpcSlot >= 0)
		return NpcCoreIndex(m_NpcSlot);
	return -1;
}

void CCharacter::MarkToBeKicked()
{
	m_ToBeKicked = true;
	if(m_pPlayer)
		m_pPlayer->m_ToBeKicked = true;
}

bool CCharacter::ToBeKicked() const
{
	if(m_pPlayer)
		return m_pPlayer->m_ToBeKicked;
	return m_ToBeKicked;
}

void CCharacter::SetAISkin()
{
	str_copy(m_TeeInfos.m_HeadName, m_AISkin.m_aHead, 24);
	str_copy(m_TeeInfos.m_BodyName, m_AISkin.m_aBody, 24);
	str_copy(m_TeeInfos.m_HandName, m_AISkin.m_aHand, 24);
	str_copy(m_TeeInfos.m_FootName, m_AISkin.m_aFoot, 24);
	str_copy(m_TeeInfos.m_TopperName, m_AISkin.m_aTopper, 24);
	str_copy(m_TeeInfos.m_EyeName, m_AISkin.m_aEye, 24);

	m_TeeInfos.m_ColorSkin = m_AISkin.m_ColorSkin;
	m_TeeInfos.m_ColorBody = m_AISkin.m_ColorBody;
	m_TeeInfos.m_ColorTopper = m_AISkin.m_ColorTopper;
	m_TeeInfos.m_ColorFeet = m_AISkin.m_ColorFoot;
	m_TeeInfos.m_BloodColor = m_AISkin.m_ColorBlood;
	m_TeeInfos.m_IsBot = true;
}

void CCharacter::SetCustomSkin(int)
{
}

void CCharacter::SetRandomSkin()
{
}

void CCharacter::InitBody(vec2 Pos)
{
	m_GrenadeGiveCooldown = 0;
	m_Spawned = true;
	m_Zombie = false;
	m_DamagedByPlayer = false;
	m_PickedWeaponSlot = 0;
	m_MaskEffectTick = 0;
	m_ToBeKicked = false;

	for(int i = 0; i < NUM_PLAYERITEMS; i++)
		m_aItem[i] = 0;

	for(int i = 0; i < NUM_STATUSS; i++)
		m_aStatus[i] = 0;

	for(int i = 0; i < NUM_SLOTS; i++)
		m_apWeapon[i] = 0;

	m_aStatus[STATUS_SPAWNING] = 0.7f * Server()->TickSpeed();

	m_SendInventoryTick = Server()->Tick() + Server()->TickSpeed() * 2.5f;

	m_ChangeDirTick = 0;
	m_LastDir = 0;
	m_DamageSoundTimer = 0;

	m_ShieldHealth = 0;
	m_ShieldRadius = 0;

	m_WeaponSlot = 0;
	m_WantedSlot = 0;

	m_AcidTimer = 0;

	m_Recoil = vec2(0, 0);

	m_SkipPickups = 0;

	m_CryTimer = 0;
	m_CryState = 0;

	m_ExplodeOnDeath = false;

	m_EmoteLockStop = 0;
	m_DeathTileTimer = 0;
	m_DelayedKill = false;
	m_WeaponPicked = false;
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastBlink = -1;
	m_LastNoAmmoSound = -1;
	m_ElectroWallCooldown = 0;
	m_PrevWeapon = WEAPON_HAMMER;
	m_QueuedCustomWeapon = -1;

	m_PainSoundTimer = 0;

	m_Pos = Pos;
	m_ChargeTick = 0;
	m_FireBufferEndTick = 0;
	m_SwitchBufferEndTick = 0;
	m_SpawnPos = Pos;
	m_LatestHitVel = vec2(0, 0);

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	InitBody(Pos);
	m_IsBot = false;
	m_pPlayer = pPlayer;
	m_NpcSlot = -1;
	m_Team = pPlayer->GetTeam();

	GameServer()->m_World.m_Core.m_apCharacters[pPlayer->GetCID()] = &m_Core;

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	if(pPlayer->m_pAI)
	{
		delete pPlayer->m_pAI;
		pPlayer->m_pAI = 0;
	}
	if(m_pAI)
	{
		delete m_pAI;
		m_pAI = 0;
	}

	GameServer()->m_pController->OnCharacterSpawn(this, pPlayer->m_IsBot);
	GameServer()->DispatchChallengeEvent(EChallengeScriptEvent::PlayerSpawn, pPlayer->GetCID(), 0);

	if(m_pAI)
	{
		m_IsBot = true;
		m_TeeInfos.m_IsBot = true;
		pPlayer->m_TeeInfos.m_IsBot = true;
		m_pAI->OnCharacterSpawn(this);
		pPlayer->m_IsBot = true;

		if(GameServer()->m_pController->IsCoop())
			m_Silent = true;
	}
	else if(pPlayer->m_pAI)
	{
		m_pAI = pPlayer->m_pAI;
		pPlayer->m_pAI = 0;
		m_IsBot = true;
		m_TeeInfos.m_IsBot = true;
		pPlayer->m_TeeInfos.m_IsBot = true;
		m_pAI->OnCharacterSpawn(this);
		pPlayer->m_IsBot = true;

		if(GameServer()->m_pController->IsCoop())
			m_Silent = true;
	}

	GiveStartWeapon();
	SendInventory();

	return true;
}

bool CCharacter::SpawnNpc(int Slot, int Team, vec2 Pos)
{
	InitBody(Pos);
	m_IsBot = true;
	m_pPlayer = 0;
	m_NpcSlot = Slot;
	m_Team = Team;
	m_Silent = GameServer()->m_pController && GameServer()->m_pController->IsCoop();
	m_TeeInfos.m_IsBot = true;

	if(m_pAI)
	{
		delete m_pAI;
		m_pAI = 0;
	}

	GameServer()->m_World.m_Core.m_apCharacters[NpcCoreIndex(Slot)] = &m_Core;

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	GameServer()->m_pController->OnCharacterSpawn(this, true);

	CGameContext::CNpcSlot *pSlot = &GameServer()->m_aNpcs[Slot];
	if(pSlot->m_AISkin.m_Valid)
	{
		m_AISkin = pSlot->m_AISkin;
		SetAISkin();
	}
	else if(m_AISkin.m_Valid)
		pSlot->m_AISkin = m_AISkin;

	if(m_pAI)
		m_pAI->OnCharacterSpawn(this);

	GiveStartWeapon();
	return true;
}

bool CCharacter::GiveBomb()
{
	int Slot = FreeSlot();
	if(Slot >= 0 && GetTeam() == TEAM_RED)
	{
		m_apWeapon[Slot] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_BOMB));
		SendInventory();
		return true;
	}

	return false;
}

int CCharacter::FreeSlot()
{
	for(int i = 0; i < 4; i++)
		if(!m_apWeapon[i])
			return i;

	return -1;
}

void CCharacter::RandomizeInventory()
{
	if(IsZombie())
		return;

	for(int x = 0; x < 16; x++)
	{
		int i = rand() % 4;
		int j = rand() % 12;

		if(i == j)
			continue;

		if(!m_apWeapon[j] || j == m_WeaponSlot)
			continue;

		bool CanSwitch = true;

		int wt1 = StaticType(m_apWeapon[i]);
		int wt2 = StaticType(m_apWeapon[j]);

		if((i > 0 && i <= 3) || (j > 0 && j <= 3))
		{
			if((wt1 >= SW_MASK1 && wt1 <= SW_MASK5) || (wt2 >= SW_MASK1 && wt2 <= SW_MASK5))
				continue;
		}

		if(i == 0 && ((wt2 >= SW_MASK1 && wt2 <= SW_MASK5)))
			continue;

		if((m_apWeapon[i] && !m_apWeapon[i]->CanSwitch()) || (m_apWeapon[j] && !m_apWeapon[j]->CanSwitch()))
			CanSwitch = false;

		if(CanSwitch)
		{
			CWeapon *pW1 = m_apWeapon[i];
			m_apWeapon[i] = m_apWeapon[j];
			m_apWeapon[j] = pW1;
		}
	}

	if(!m_IsBot)
		SendInventory();
}

void CCharacter::SaveData()
{
	if(g_Config.m_SvTutorialMode || m_IsBot || !m_Spawned || !GameServer()->m_pController->IsCoop())
		return;

	CPlayerData *pData = GameServer()->Server()->GetPlayerData(GetCID(), GetPlayer()->GetColorID());

	pData->m_Kits = m_Kits;
	pData->m_Armor = m_Armor;
	pData->m_Score = GetPlayer()->m_Score;
	pData->m_Gold = GetPlayer()->m_Gold;

	for(int i = 0; i < NUM_SLOTS; i++)
	{
		if(m_apWeapon[i] && !CWeaponCatalog::IsCustom(m_apWeapon[i]->GetWeaponSpec()))
		{
			const CWeaponSpec &Spec = m_apWeapon[i]->GetWeaponSpec();
			pData->m_aWeaponDefinitionId[i] = static_cast<int>(Spec.m_DefinitionId);
			pData->m_aWeaponLevel[i] = Spec.m_Level;
			pData->m_aWeaponAmmo[i] = m_apWeapon[i]->m_Ammo;
		}
		else
		{
			pData->m_aWeaponDefinitionId[i] = 0;
			pData->m_aWeaponLevel[i] = 0;
		}
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Data save - color=%d", GetPlayer()->GetColorID());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "Character", aBuf);
}

bool CCharacter::GiveWeapon(CWeapon *pWeapon)
{
	if(!pWeapon)
		return false;

	// Challenge variant: players cannot receive firearms, while melee weapons
	// and all non-firearm items remain available. Enemies keep their loadout.
	if(!m_IsBot && ChallengeVariantEnabled(g_Config.m_SvChallengeVariants, CHALLENGE_ONLY_MELEE) &&
	   IsFirearmWeapon(pWeapon))
		return false;

	if(m_WeaponSlot < 0 || m_WeaponSlot > NUM_SLOTS)
		return false;

	if(m_apWeapon[m_WeaponSlot])
	{
		for(int i = 0; i < NUM_SLOTS; i++)
		{
			if(!m_apWeapon[i])
			{
				m_apWeapon[i] = pWeapon;
				pWeapon->OnPlayerPick();

				if(m_IsBot && GameServer()->m_pController->IsCoop())
					pWeapon->m_InfiniteAmmo = true;

				// SendInventory();
				return true;
			}
		}
		return false;
	}

	m_apWeapon[m_WeaponSlot] = pWeapon;
	pWeapon->OnPlayerPick();

	if(m_IsBot && GameServer()->m_pController->IsCoop())
		pWeapon->m_InfiniteAmmo = true;

	// SendInventory();
	return true;
}

int CCharacter::GetWeaponPowerLevel(int WeaponSlot)
{
	if(WeaponSlot < 0)
		WeaponSlot = m_WeaponSlot;

	if(WeaponSlot < 0 || WeaponSlot > NUM_SLOTS)
		return 0;

	if(!m_apWeapon[WeaponSlot])
		return 0;

	return m_apWeapon[WeaponSlot]->GetPowerLevel();
}

bool CCharacter::SetLandmine()
{
	if(GameServer()->Collision()->GetCollisionAt(m_Pos.x - 16, m_Pos.y + 24) & CCollision::COLFLAG_SOLID &&
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x + 16, m_Pos.y + 24) & CCollision::COLFLAG_SOLID)
	{
		// new CLandmine(&GameServer()->m_World, m_Pos + vec2(0, 16), GetCID());
		CBuilding *b = new CBuilding(&GameServer()->m_World,
									 m_Pos + vec2(0, 6),
									 BUILDING_MINE1,
									 GameServer()->m_pController->IsTeamplay() ? GetTeam() : TEAM_NEUTRAL);
		b->m_DamageOwner = GetCID();
		GameServer()->CreateSound(m_Pos, SOUND_BODY_LAND);
		return true;
	}
	return false;
}

bool CCharacter::SetElectromine()
{
	if(GameServer()->Collision()->GetCollisionAt(m_Pos.x - 16, m_Pos.y + 24) & CCollision::COLFLAG_SOLID &&
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x + 16, m_Pos.y + 24) & CCollision::COLFLAG_SOLID)
	{
		// new CLandmine(&GameServer()->m_World, m_Pos + vec2(0, 16), GetCID());
		CBuilding *b = new CBuilding(&GameServer()->m_World,
									 m_Pos + vec2(0, 6),
									 BUILDING_MINE2,
									 GameServer()->m_pController->IsTeamplay() ? GetTeam() : TEAM_NEUTRAL);
		b->m_DamageOwner = GetCID();
		GameServer()->CreateSound(m_Pos, SOUND_BODY_LAND);
		return true;
	}
	return false;
}

void CCharacter::Teleport(vec2 Pos)
{
	m_Pos = Pos;
	m_Core.m_Pos = m_Pos;

	m_Core.Reset();

	m_Pos = Pos;
	m_Core.m_Pos = m_Pos;

	if(m_pAI)
		m_pAI->StandStill(15);
}

void CCharacter::Destroy()
{
	const int Index = CoreIndex();
	if(Index >= 0 && Index < MAX_CHARACTERS)
		GameServer()->m_World.m_Core.m_apCharacters[Index] = 0;
	m_Alive = false;
}

void CCharacter::SendInventory()
{
	if(m_IsBot || !m_pPlayer)
		return;

	CNetMsg_Sv_Inventory Msg;
	auto FillWeapon = [this](int Slot, int &DefinitionId, int &Level, int &Ammo)
	{
		const CWeapon *pWeapon = GetWeapon(Slot);
		DefinitionId = pWeapon ? static_cast<int>(pWeapon->GetWeaponSpec().m_DefinitionId) : 0;
		Level = pWeapon ? pWeapon->GetWeaponSpec().m_Level : 0;
		Ammo = pWeapon ? max(0, pWeapon->GetAmmo()) : 0;
	};
	FillWeapon(0, Msg.m_Item1DefinitionId, Msg.m_Item1Level, Msg.m_Item1Ammo);
	FillWeapon(1, Msg.m_Item2DefinitionId, Msg.m_Item2Level, Msg.m_Item2Ammo);
	FillWeapon(2, Msg.m_Item3DefinitionId, Msg.m_Item3Level, Msg.m_Item3Ammo);
	FillWeapon(3, Msg.m_Item4DefinitionId, Msg.m_Item4Level, Msg.m_Item4Ammo);
	FillWeapon(4, Msg.m_Item5DefinitionId, Msg.m_Item5Level, Msg.m_Item5Ammo);
	FillWeapon(5, Msg.m_Item6DefinitionId, Msg.m_Item6Level, Msg.m_Item6Ammo);
	FillWeapon(6, Msg.m_Item7DefinitionId, Msg.m_Item7Level, Msg.m_Item7Ammo);
	FillWeapon(7, Msg.m_Item8DefinitionId, Msg.m_Item8Level, Msg.m_Item8Ammo);
	FillWeapon(8, Msg.m_Item9DefinitionId, Msg.m_Item9Level, Msg.m_Item9Ammo);
	FillWeapon(9, Msg.m_Item10DefinitionId, Msg.m_Item10Level, Msg.m_Item10Ammo);
	FillWeapon(10, Msg.m_Item11DefinitionId, Msg.m_Item11Level, Msg.m_Item11Ammo);
	FillWeapon(11, Msg.m_Item12DefinitionId, Msg.m_Item12Level, Msg.m_Item12Ammo);
	Msg.m_Gold = GetPlayer()->GetGold();
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, GetCID());
}

void CCharacter::InventoryRoll(int Slot)
{
	if(Slot == -1)
		Slot = m_WeaponSlot;
	if(Slot < 0 || Slot >= NUM_SLOTS)
		return;

	if(IsZombie())
		return;

	int w1 = Slot;
	int w2 = (Slot + 4) % NUM_SLOTS;
	int w3 = (Slot + 8) % NUM_SLOTS;

	if(!m_apWeapon[w1] && !m_apWeapon[w2] && !m_apWeapon[w3])
		return;

	if((m_apWeapon[w1] && !m_apWeapon[w1]->CanSwitch()) || (m_apWeapon[w2] && !m_apWeapon[w2]->CanSwitch()) ||
	   (m_apWeapon[w3] && !m_apWeapon[w3]->CanSwitch()))
		return;

	CWeapon *pW1 = m_apWeapon[w1];
	m_apWeapon[w1] = m_apWeapon[w2];
	m_apWeapon[w2] = m_apWeapon[w3];
	m_apWeapon[w3] = pW1;

	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	SendInventory();
}

void CCharacter::DropItem(int Slot, vec2 Pos)
{
	if(IsZombie())
		return;

	if(Slot < 0 || Slot >= 12)
		return;

	if(UpgradeTurret(Pos, vec2(Pos.x > m_Pos.x ? -1 : 1, 0), Slot))
	{
		// m_apWeapon[Slot] = 0;
		// SendInventory();
		return;
	}

	if(m_apWeapon[Slot] && m_apWeapon[Slot]->Drop())
	{
		// vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
		vec2 Direction = normalize(Pos - m_Pos);

		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

		GameServer()->m_pController->DropWeapon(
			m_Pos + vec2(0, -16), m_Core.m_Vel / 1.7f + Direction * 10 + vec2(0, -3), m_apWeapon[Slot]);
		m_SkipPickups = 20;

		m_apWeapon[Slot] = 0;
		SendInventory();
		return;
	}
}

void CCharacter::SwapItem(int Item1, int Item2)
{
	if(IsZombie())
		return;

	if(Item1 < 0 || Item1 >= NUM_SLOTS || Item2 < 0 || Item2 >= NUM_SLOTS)
		return;

	if(Item1 == Item2)
		return;

	CWeapon *t = m_apWeapon[Item1];

	CWeapon *pWeapon1 = GetWeapon(Item1);
	CWeapon *pWeapon2 = GetWeapon(Item2);

	if(g_Config.m_SvForgeMode == 0 && HasWeaponBehavior(pWeapon1, WEAPON_BEHAVIOR_UPGRADE) && Item1 != Item2)
	{
		if(pWeapon2)
		{
			if(pWeapon1->GetWeaponSpec().m_Level >= WEAPON_UPGRADE_SUPERCHARGE_LEVEL && pWeapon2->Supercharge())
			{
				m_apWeapon[Item1]->m_DestructionTick = 1;
				m_apWeapon[Item1] = 0;

				// supercharge sound
				GameServer()->CreateSound(m_Pos, SOUND_UPGRADE);
			}
			else if(pWeapon1->GetWeaponSpec().m_Level >= WEAPON_UPGRADE_SUPERCHARGE_LEVEL &&
					HasWeaponBehavior(pWeapon2, WEAPON_BEHAVIOR_UPGRADE))
			{
				GameServer()->CreateSound(m_Pos, SOUND_NEGATIVE);
			}
			else if(pWeapon2->Overcharge())
			{
				m_apWeapon[Item1]->m_DestructionTick = 1;
				m_apWeapon[Item1] = 0;

				// overcharge sound
				GameServer()->CreateSound(m_Pos, SOUND_UPGRADE);
			}
			else
				GameServer()->CreateSound(m_Pos, SOUND_NEGATIVE);

			SendInventory();
			return;
		}
	}

	// combine melee
	if(g_Config.m_SvForgeMode == 0 && pWeapon1 && pWeapon2 &&
	   pWeapon1->GetWeaponProfile().m_Definition.m_Kind == EWeaponDefinitionKind::Modular &&
	   pWeapon2->GetWeaponProfile().m_Definition.m_Kind == EWeaponDefinitionKind::Modular &&
	   pWeapon1->GetWeaponProfile().m_Definition.m_Part1 == PART1_MELEE &&
	   pWeapon2->GetWeaponProfile().m_Definition.m_Part1 == PART1_MELEE &&
	   pWeapon1->GetWeaponProfile().m_Definition.m_Part2 == pWeapon2->GetWeaponProfile().m_Definition.m_Part2)
	{
		if(!m_apWeapon[Item1]->CanSwitch() || !m_apWeapon[Item2]->CanSwitch())
			return;

		m_apWeapon[Item1]->m_DestructionTick = 1;
		m_apWeapon[Item1] = 0;
		m_apWeapon[Item2]->m_DestructionTick = 1;
		m_apWeapon[Item2] = 0;
		m_apWeapon[Item2] = new CWeapon(
			&GameServer()->m_World,
			CWeaponCatalog::Modular(PART1_SPIN,
									pWeapon1->GetWeaponProfile().m_Definition.m_Part2,
									max(pWeapon1->GetWeaponSpec().m_Level, pWeapon2->GetWeaponSpec().m_Level)));

		GameServer()->CreateSound(m_Pos, SOUND_UPGRADE);
		SendInventory();
		return;
	}

	// swap slots
	if(Item2 >= 0)
	{
		m_apWeapon[Item1] = m_apWeapon[Item2];
		m_apWeapon[Item2] = t;
	}

	// confirm inventory to the client
	SendInventory();
}

void CCharacter::CombineItem(int Item1, int Item2, int Operation)
{
	if(!m_pPlayer)
		return;
	int ResultOperation = Operation;
	auto Reject = [this, &ResultOperation, Item1, Item2](int Result,
														 const CWeaponSpec &Product = CWeaponSpec(),
														 int Cost = 0,
														 int ProductAmmo = 0,
														 int ProductMaxAmmo = 0)
	{
		GetPlayer()->SendForgeResult(Result, ResultOperation, Item1, Item2, Cost, Product, ProductAmmo, ProductMaxAmmo);
		GameServer()->CreateSoundGlobal(SOUND_GUI_DENIED1, GetCID());
	};

	if(!IsAlive() || IsZombie() || GameServer()->m_World.m_Paused)
	{
		Reject(FORGERESULT_BUSY);
		return;
	}
	if(GetPlayer()->m_LastForgeRequestTick == Server()->Tick())
	{
		Reject(FORGERESULT_BUSY);
		return;
	}
	GetPlayer()->m_LastForgeRequestTick = Server()->Tick();
	if(Item1 < 0 || Item1 >= NUM_SLOTS || Item2 < 0 || Item2 >= NUM_SLOTS || Item1 == Item2)
	{
		Reject(FORGERESULT_INVALID_SLOT);
		return;
	}

	CWeapon *pTarget = m_apWeapon[Item1];
	CWeapon *pMaterial = m_apWeapon[Item2];
	CWeapon *pCurrent = GetWeapon();
	if((pTarget && !pTarget->CanSwitch()) || (pMaterial && !pMaterial->CanSwitch()) ||
	   (pCurrent && pCurrent != pTarget && pCurrent != pMaterial && !pCurrent->CanSwitch()))
	{
		Reject(FORGERESULT_BUSY);
		return;
	}
	if(g_Config.m_SvForgeMode == 0)
	{
		Reject(FORGERESULT_DISABLED);
		return;
	}
	// Mode 3: Upgrade materials can forge anywhere; other recipes still need a screen.
	if(g_Config.m_SvForgeMode == 2 ||
	   (g_Config.m_SvForgeMode == 3 && !HasWeaponBehavior(pMaterial, WEAPON_BEHAVIOR_UPGRADE)))
	{
		bool ScreenNear = false;
		for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING);
			pBuilding;
			pBuilding = (CBuilding *)pBuilding->TypeNext())
		{
			if(pBuilding->m_Type == BUILDING_SCREEN && distance(m_Pos, pBuilding->m_Pos) <= FORGE_SCREEN_RANGE)
			{
				ScreenNear = true;
				break;
			}
		}
		if(!ScreenNear)
		{
			Reject(FORGERESULT_TOO_FAR);
			return;
		}
	}
	if(Operation != FORGEOP_AUTO)
	{
		Reject(FORGERESULT_INVALID_RECIPE);
		return;
	}
	if(!pTarget || !pMaterial)
	{
		Reject(FORGERESULT_INVALID_RECIPE);
		return;
	}

	const CForgeRecipe Recipe = CForge::Resolve(pTarget->GetWeaponSpec(),
												pMaterial->GetWeaponSpec(),
												pTarget->GetAmmo(),
												g_Config.m_SvForgeBaseCost,
												g_Config.m_SvForgeLevelCost,
												pMaterial->GetAmmo());
	ResultOperation = Recipe.m_Operation;
	if(Recipe.m_Result != FORGERESULT_SUCCESS)
	{
		Reject(Recipe.m_Result, Recipe.m_Product);
		return;
	}
	const int Cost = CForge::EffectiveCost(Recipe, g_Config.m_SvForgeMode);
	if(GetPlayer()->GetGold() < Cost)
	{
		Reject(FORGERESULT_NOT_ENOUGH_GOLD,
			   Recipe.m_Product,
			   Cost,
			   Recipe.m_ProductAmmo,
			   Recipe.m_ProductMaxAmmo);
		return;
	}

	CWeapon *pProduct = GameServer()->NewWeapon(Recipe.m_Product);
	pProduct->m_Ammo = Recipe.m_ProductAmmo;
	pTarget->m_Disabled = true;
	pMaterial->m_Disabled = true;
	GameServer()->m_World.DestroyEntity(pTarget);
	GameServer()->m_World.DestroyEntity(pMaterial);
	m_apWeapon[Item1] = pProduct;
	m_apWeapon[Item2] = 0;
	GetPlayer()->ReduceGold(Cost);
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnGoldSpent(GetCID(), Cost);

	int SelectedSlot = -1;
	if(Item1 < 4 && m_apWeapon[Item1])
		SelectedSlot = Item1;
	else
		for(int Slot = 0; Slot < 4; ++Slot)
			if(m_apWeapon[Slot])
			{
				SelectedSlot = Slot;
				break;
			}
	if(SelectedSlot >= 0)
		m_WeaponSlot = m_WantedSlot = SelectedSlot;

	GameServer()->CreateSound(m_Pos, SOUND_UPGRADE);
	SendInventory();
	SaveData();
	GetPlayer()->SendForgeResult(FORGERESULT_SUCCESS,
								 Recipe.m_Operation,
								 Item1,
								 Item2,
								 Cost,
								 Recipe.m_Product,
								 Recipe.m_ProductAmmo,
								 Recipe.m_ProductMaxAmmo);
	if(GameServer()->m_pTutorialDirector)
		GameServer()->m_pTutorialDirector->OnGameplayProgress(GetCID(), TUTORIAL_EVENT_FORGE);
	Server()->SendPlatformEvent(GetCID(), PLATFORM_EVENT_FIRST_FORGE);
	Server()->DispatchModEvent(MOD_EVENT_FORGE, GetCID(), Recipe.m_Operation);
	GameServer()->DispatchChallengeEvent(EChallengeScriptEvent::Forge, GetCID(), Recipe.m_Operation);
}

bool CCharacter::TriggerWeapon(CWeapon *pWeapon)
{
	if(!pWeapon || GetWeapon() != pWeapon)
		return false;

	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_BOMB))
	{
		ReleaseWeapon();
		return true;
	}
	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_ACTIVATE_INVIS))
		return GiveBuff(PLAYERITEM_INVISIBILITY);
	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_ACTIVATE_SHIELD))
		return GiveBuff(PLAYERITEM_SHIELD);
	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_ACTIVATE_RESPAWNER) &&
	   (!GameServer()->m_pController->IsCoop() || !m_IsBot))
		return GameServer()->RespawnAlly(m_Pos, GetTeam(), GetCID());
	return false;
}

void CCharacter::ReleaseWeapon(CWeapon *pWeapon)
{
	if(!pWeapon)
	{
		if(!GetWeapon())
			return;

		GetWeapon()->Throw();
		m_apWeapon[GetWeaponSlot()] = 0;
	}
	else
	{
		for(int i = 0; i < NUM_SLOTS; i++)
		{
			if(m_apWeapon[i] == pWeapon)
			{
				m_apWeapon[i] = 0;
				break;
			}
		}
	}

	SendInventory();
}

bool CCharacter::IsBombCarrier()
{
	for(int i = 0; i < NUM_SLOTS; i++)
		if(HasWeaponBehavior(m_apWeapon[i], WEAPON_BEHAVIOR_BOMB))
			return true;

	return false;
}

bool CCharacter::PickWeapon(CWeapon *pWeapon)
{
	// cs | reactor defense
	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_BOMB) && GetTeam() != TEAM_RED)
		return false;

	// Challenge variant: players cannot pick up firearms, while melee weapons
	// and all non-firearm items remain available. Enemies keep their loadout.
	if(!m_IsBot && ChallengeVariantEnabled(g_Config.m_SvChallengeVariants, CHALLENGE_ONLY_MELEE) &&
	   IsFirearmWeapon(pWeapon))
		return false;

	if(IsZombie())
		return false;

	if(!GetWeapon())
	{
		pWeapon->SetOwner(CoreIndex());
		m_apWeapon[GetWeaponSlot()] = pWeapon;
		m_PickedWeaponSlot = GetWeaponSlot();
		SendInventory();
		return true;
	}

	bool Valid = true;

	if(pWeapon->GetWeaponProfile().m_Combat.m_AutoPick)
	{
		// check if weapon is lower level than currently held weapons overall
		if(pWeapon->GetWeaponProfile().m_Definition.m_MaxLevel > 1)
		{
			float Weapons = 0.0f;
			float WeaponLevel = 0.0f;

			for(int i = 0; i < 4; i++)
			{
				if(m_apWeapon[i] && m_apWeapon[i]->GetWeaponProfile().m_Definition.m_MaxLevel > 1)
				{
					Weapons += 1.0f;
					WeaponLevel += m_apWeapon[i]->GetWeaponSpec().m_Level;
				}

				if(Weapons > 1.0f)
					WeaponLevel /= Weapons;

				if(pWeapon->GetWeaponSpec().m_Level < WeaponLevel && Weapons > 0.0f &&
				   pWeapon->GetWeaponSpec().m_Level <= pWeapon->GetWeaponProfile().m_Definition.m_MaxLevel)
					Valid = false;
			}
		}

		for(int i = 0; i < NUM_SLOTS; i++)
		{
			if(m_apWeapon[i] &&
			   m_apWeapon[i]->GetWeaponSpec().m_DefinitionId == pWeapon->GetWeaponSpec().m_DefinitionId &&
			   m_apWeapon[i]->GetWeaponSpec().m_Level >= pWeapon->GetWeaponSpec().m_Level &&
			   !HasWeaponBehavior(m_apWeapon[i], WEAPON_BEHAVIOR_UPGRADE) &&
			   !HasWeaponBehavior(m_apWeapon[i], WEAPON_BEHAVIOR_ACTIVATE_RESPAWNER))
				Valid = false;
		}
	}
	else
		Valid = false;

	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_UPGRADE))
		Valid = true;

	if(HasWeaponBehavior(pWeapon, WEAPON_BEHAVIOR_ACTIVATE_RESPAWNER) &&
	   GameServer()->m_pController->CountPlayers(0) < 2)
		Valid = false;

	if(Valid)
	{
		for(int i = 0; i < 4; i++)
		{
			if(!m_apWeapon[i])
			{
				pWeapon->SetOwner(CoreIndex());
				m_apWeapon[i] = pWeapon;
				m_PickedWeaponSlot = i;
				SendInventory();
				return true;
			}
		}
	}

	return false;
}

bool CCharacter::UpgradeTurret(vec2 Pos, vec2 Dir, int Slot)
{
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "character", "Upgrade turret");

	if(Slot < 0)
		Slot = GetWeaponSlot();

	if(!GetWeapon(Slot))
		return false;

	if(!GetWeapon(Slot)->GetWeaponProfile().m_Combat.m_ValidForTurret)
		return false;

	// check if near upgradeable buildings
	float CheckRange = 48.0f;
	CBuilding *pNear = 0;
	CBuilding *apEnts[16];
	int Num = GameServer()->m_World.FindEntities(
		Pos + vec2(0, -20), 60, (CEntity **)apEnts, 16, CGameWorld::ENTTYPE_BUILDING);

	// check for turret stands
	for(int i = 0; i < Num; ++i)
	{
		CBuilding *pTarget = apEnts[i];

		if(pTarget->m_Type == BUILDING_STAND && distance(pTarget->m_Pos, Pos + vec2(0, -20)) < CheckRange)
		{
			pNear = pTarget;
			break;
		}
	}

	// transform stand to turret
	if(pNear)
	{
		int Cost = GetWeapon(Slot)->GetPowerLevel() + 1;
		if(GameServer()->m_pPveDirector)
			Cost = GameServer()->m_pPveDirector->ModifyBuildingCost(GetCID(), Cost);
		if(m_Kits < Cost)
			return false;

		m_Kits -= Cost;

		vec2 p = pNear->m_Pos;
		const int OriginalKitCost = pNear->m_PveKitCost;
		GameServer()->m_World.DestroyEntity(pNear);

		int Team = GetTeam();
		if(!GameServer()->m_pController->IsTeamplay())
			Team = GetCID();

		// clone the weapon in use and link it to turret
		CWeapon *pWeapon = GameServer()->NewWeapon(GetWeapon(Slot)->GetWeaponSpec());

		pWeapon->SetOwner(CoreIndex());
		CTurret *pTurret = new CTurret(&GameServer()->m_World, p, Team, pWeapon);
		pTurret->m_PveBuilder = GetCID();
		pTurret->m_PveKitCost = OriginalKitCost + Cost;
		pTurret->SetAngle(Dir);

		// sound
		GameServer()->CreateSound(Pos, SOUND_BUILD_TURRET);
		return true;
	}

	return false;
}

void CCharacter::DropWeapon()
{
	if(IsZombie())
		return;

	if(!GetWeapon())
		return;

	if(GetWeaponSlot() < 0 || GetWeaponSlot() >= NUM_SLOTS)
		return;

	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	if(m_HiddenHealth > 0)
	{
		if(UpgradeTurret(m_Pos, -Direction))
			return;
	}

	if(GetWeapon()->Drop())
	{
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

		if(HasWeaponBehavior(GetWeapon(), WEAPON_BEHAVIOR_BOMB))
			GameServer()->m_pController->DropWeapon(
				m_Pos + vec2(0, -16), (m_Core.m_Vel / 1.7f + Direction * 10 + vec2(0, -3)) * 0.75f, GetWeapon());
		else
			GameServer()->m_pController->DropWeapon(
				m_Pos + vec2(0, -16), m_Core.m_Vel / 1.7f + Direction * 10 + vec2(0, -3), GetWeapon());

		m_SkipPickups = 20;

		m_apWeapon[GetWeaponSlot()] = 0;
		SendInventory();
		return;
	}

	if(GetWeapon()->ReleaseCharge())
	{
		if(GetWeapon()->GetWeaponProfile().m_Combat.m_FiringType == WFT_THROW)
			ReleaseWeapon();

		m_apWeapon[GetWeaponSlot()] = 0;
		SendInventory();
		return;
	}
}

bool CCharacter::IsGrounded()
{

	if(GameServer()->Collision()->CheckPoint(m_Pos.x + m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x - m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5))
		return true;

	int c1 =
		GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5);
	int c2 =
		GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 2, m_Pos.y + m_ProximityRadius / 2 + 5);

	if(c1 & CCollision::COLFLAG_SOLID || c2 & CCollision::COLFLAG_SOLID)
		return true;

	return false;
}

void CCharacter::DoWeaponSwitch()
{
	if(m_aStatus[STATUS_DEATHRAY] > 0.0f || IsZombie())
		return;

	if(m_WantedSlot != m_WeaponSlot)
	{
		if(m_apWeapon[m_WeaponSlot] && !m_apWeapon[m_WeaponSlot]->CanSwitch())
			return;

		if(m_apWeapon[m_WantedSlot] && !m_apWeapon[m_WantedSlot]->CanSwitch())
			return;

		m_WeaponSlot = m_WantedSlot;
		m_FireBufferEndTick = 0;
		m_AttackTick = 0;
		if(GameServer()->m_pTutorialDirector && !m_IsBot)
			GameServer()->m_pTutorialDirector->OnGameplayProgress(GetCID(), TUTORIAL_EVENT_WEAPON_SWITCH);
	}
}

void CCharacter::HandleWeaponSwitch()
{
	const int StartSlot = m_WeaponSlot;
	const bool PendingRequest = m_SwitchBufferEndTick && Server()->Tick() <= m_SwitchBufferEndTick &&
		m_WantedSlot != m_WeaponSlot;
	int WantedSlot = PendingRequest ? m_WantedSlot : m_WeaponSlot;
	bool NewRequest = false;

	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;
	const int NextPresses = Next;
	const int PrevPresses = Prev;

	if(Next < 128)
	{
		while(Next) // Next Weapon selection
		{
			// WantedSlot = clamp(WantedSlot+1, 0, 3);
			if(++WantedSlot > 3)
				WantedSlot = 0;
			Next--;
			NewRequest = true;
		}
	}
	if(Prev < 128)
	{
		while(Prev) // Prev Weapon selection
		{
			// WantedSlot = clamp(WantedSlot-1, 0, 3);
			if(--WantedSlot < 0)
				WantedSlot = 3;
			Prev--;
			NewRequest = true;
		}
	}

	if(m_LatestInput.m_WantedWeapon && m_LatestInput.m_WantedWeapon != m_LatestPrevInput.m_WantedWeapon)
	{
		WantedSlot = clamp(m_LatestInput.m_WantedWeapon - 2, 0, 3);
		NewRequest = true;
	}

	if(NewRequest)
	{
		m_WantedSlot = WantedSlot;
		int ReloadTicks = 0;
		if(m_apWeapon[m_WeaponSlot])
			ReloadTicks = max(ReloadTicks, m_apWeapon[m_WeaponSlot]->ReloadTicksRemaining());
		if(m_apWeapon[m_WantedSlot])
			ReloadTicks = max(ReloadTicks, m_apWeapon[m_WantedSlot]->ReloadTicksRemaining());
		// Keep a request through the shot that is already in progress, but do
		// not allow an old selection to trigger much later in the fight.
		m_SwitchBufferEndTick = Server()->Tick() + SwitchInputBufferTicks(ReloadTicks, Server()->TickSpeed());
	}
	if(m_WantedSlot == m_WeaponSlot)
	{
		m_SwitchBufferEndTick = 0;
		return;
	}
	if(!m_SwitchBufferEndTick || Server()->Tick() > m_SwitchBufferEndTick)
	{
		m_WantedSlot = m_WeaponSlot;
		m_SwitchBufferEndTick = 0;
		return;
	}

	DoWeaponSwitch();
	if(g_Config.m_ClDebugWeaponWheel && (NextPresses > 0 || PrevPresses > 0))
	{
		char aBuf[256];
		str_format(aBuf,
				   sizeof(aBuf),
				   "server cid=%d tick=%d start=%d pending_start=%d next=%d prev=%d counters next=%d->%d prev=%d->%d wanted=%d result=%d buffer_end=%d",
				   GetCID(),
				   Server()->Tick(),
				   StartSlot,
				   PendingRequest ? 1 : 0,
				   NextPresses,
				   PrevPresses,
				   m_LatestPrevInput.m_NextWeapon,
				   m_LatestInput.m_NextWeapon,
				   m_LatestPrevInput.m_PrevWeapon,
				   m_LatestInput.m_PrevWeapon,
				   m_WantedSlot,
				   m_WeaponSlot,
				   m_SwitchBufferEndTick);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-wheel", aBuf);
	}
	if(m_WantedSlot == m_WeaponSlot)
		m_SwitchBufferEndTick = 0;
}

void CCharacter::Jumppad()
{
	/*
	m_Core.m_Vel.y = -9.0f;
	m_Core.m_Action = COREACTION_JUMPPAD;
	m_Core.m_ActionState = 0;
	*/

	m_Core.Jumppad();
}

int CCharacter::Reflect()
{
	// if (m_ScytheTick > Server()->Tick()-Server()->TickSpeed()*0.2f)
	//	return true;

	if(GetMask() == 3 && frandom() < 0.6f)
		return m_ProximityRadius;

	if(GetWeapon() && GetWeapon()->Reflect())
		return GetWeapon()->Reflect();

	return 0;
}

void CCharacter::FireWeapon()
{
	if(m_aStatus[STATUS_SPAWNING] > 0.0f || m_aStatus[STATUS_DEATHRAY] > 0.0f)
		return;

	if(!GetWeapon())
		return;

	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
	GetWeapon()->SetPos(m_Pos, m_Core.m_Vel, Direction, m_ProximityRadius);
	GetWeapon()->SetOwner(CoreIndex());

	const int FiringType = GetWeapon()->GetWeaponProfile().m_Combat.m_FiringType;
	if(FiringType == WFT_CHARGE || FiringType == WFT_THROW)
	{
		float Knockback = 0.0f;

		// charge
		if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses || m_LatestInput.m_Fire & 1)
		{
			if(GetWeapon()->Charge() && !m_ChargeTick)
			{
				m_ChargeTick = Server()->Tick();

				if(FiringType == WFT_THROW)
					m_AttackTick = Server()->Tick();
			}
		}
		// release
		else if(GetWeapon()->ReleaseCharge(&Knockback))
		{
			m_ChargeTick = 0;
			m_AttackTick = Server()->Tick();
			if(FiringType == WFT_THROW)
			{
				ReleaseWeapon();
				m_Core.m_ChargeLevel = -20;
				return;
			}
		}

		m_Core.m_ChargeLevel = GetWeapon()->GetCharge();
		return;
	}

	m_Core.m_ChargeLevel = 0;

	// Once a normal weapon switch is queued, let the current shot finish and
	// stop held full-auto fire from continuously restarting its cooldown.
	if(m_WantedSlot != m_WeaponSlot && m_SwitchBufferEndTick && Server()->Tick() <= m_SwitchBufferEndTick)
		return;

	// trigger finger
	bool FullAuto = m_IsBot ? true : GetWeapon()->FullAuto();

	bool WillFire = false;
	const bool Pressed = CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses != 0;
	if(Pressed)
	{
		if(!FullAuto && GetWeapon()->ReloadTicksRemaining() > 0 && GetWeapon()->ReloadTicksRemaining() <= 3 &&
		   (!GetWeapon()->UsesAmmo() || GetWeapon()->GetAmmo() > 0))
			m_FireBufferEndTick = QueueInputUntil(Server()->Tick(), GetWeapon()->ReloadTicksRemaining(), 3);
		else
			WillFire = true;
	}
	if(m_FireBufferEndTick)
	{
		const EInputBufferState BufferState = InputBufferState(Server()->Tick(), m_FireBufferEndTick, GetWeapon()->CanFireNow());
		if(BufferState == INPUT_BUFFER_EXPIRED)
			m_FireBufferEndTick = 0;
		else if(BufferState == INPUT_BUFFER_READY)
		{
			WillFire = true;
			m_FireBufferEndTick = 0;
		}
	}

	if(FullAuto && (m_LatestInput.m_Fire & 1))
		WillFire = true;

	if(m_Core.m_ChargeLevel < 0)
		WillFire = false;

	if(!WillFire)
		return;

	float Knockback = 0.0f;

	// fire
	if(GetWeapon()->Fire(&Knockback))
	{
		m_Recoil -= Direction * Knockback;
		m_AttackTick = Server()->Tick();
	}
	else
	{
	}
}

void CCharacter::HandleWeapons()
{
	/*
	if(m_ReloadTimer > 0)
	{
		m_ReloadTimer--;
		return;
	}
	*/

	HandleWeaponSwitch();
	// fire Weapon, if wanted
	FireWeapon();

	return;
}

void CCharacter::AutoWeaponChange()
{
	// todo
}

bool CCharacter::HasAmmo()
{
	if(!GetWeapon() || GetWeapon()->m_Ammo <= 0)
		return false;

	return true;
}

void CCharacter::GiveStartWeapon()
{
	if(IsZombie())
	{
		m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_CLAW));
		return;
	}

	// Challenge variant: melee-only — players start with MELEE1 and cannot
	// receive firearms (enemies keep their own loadout).
	if(!m_IsBot && ChallengeVariantEnabled(g_Config.m_SvChallengeVariants, CHALLENGE_ONLY_MELEE))
	{
		m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1, 1));
		return;
	}

	if(!m_IsBot && str_comp(g_Config.m_SvGametype, "base") == 0)
	{
		m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_TOOL));
	}

	if(str_comp(g_Config.m_SvGametype, "tutorial") == 0)
	{
		if(m_IsBot)
			return;
		m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1));
		m_apWeapon[1] = GameServer()->NewWeapon(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL2));
		GetPlayer()->m_Gold = max(GetPlayer()->m_Gold, 80);
		// Forge/build grant a starter pack so the build step is playable
		// even if kit pickups spawn far from the player.
		if(g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_FORGE ||
		   g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_BUILD)
			m_Kits = max(m_Kits, 10);
		return;
	}

	if(str_comp(g_Config.m_SvGametype, "coop") == 0)
	{
		if(m_IsBot)
			return;

		// load saved weapons
		CPlayerData *pData = GameServer()->Server()->GetPlayerData(GetCID(), GetPlayer()->GetColorID());
		if(pData->m_WeaponDataVersion != WEAPON_DATA_VERSION)
		{
			char aBuf[128];
			str_format(aBuf,
					   sizeof(aBuf),
					   "Reset weapon inventory: unsupported version %d (expected %d)",
					   pData->m_WeaponDataVersion,
					   WEAPON_DATA_VERSION);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-data", aBuf);
			pData->ResetWeapons();
		}
		for(int i = 0; i < NUM_SLOTS; ++i)
		{
			if(!pData->m_aWeaponDefinitionId[i])
				continue;
			CWeaponSpec Spec;
			CResolvedWeaponProfile Profile;
			if(!CWeaponCatalog::TryFromProtocol(pData->m_aWeaponDefinitionId[i], pData->m_aWeaponLevel[i], &Spec) ||
			   !CWeaponCatalog::TryResolve(Spec, &Profile) || pData->m_aWeaponAmmo[i] < 0 ||
			   pData->m_aWeaponAmmo[i] > Profile.m_Combat.m_MaxAmmo)
			{
				char aBuf[192];
				str_format(aBuf,
						   sizeof(aBuf),
						   "Reset weapon inventory: invalid slot %d definition=%d level=%d ammo=%d",
						   i,
						   pData->m_aWeaponDefinitionId[i],
						   pData->m_aWeaponLevel[i],
						   pData->m_aWeaponAmmo[i]);
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weapon-data", aBuf);
				pData->ResetWeapons();
				break;
			}
		}
		bool GotItems = false;

		for(int i = 0; i < NUM_SLOTS; i++)
		{
			if(pData->m_aWeaponDefinitionId[i])
			{
				CWeaponSpec Spec;
				if(CWeaponCatalog::TryFromProtocol(pData->m_aWeaponDefinitionId[i], pData->m_aWeaponLevel[i], &Spec))
				{
					m_apWeapon[i] = GameServer()->NewWeapon(Spec);
					m_apWeapon[i]->m_Ammo = pData->m_aWeaponAmmo[i];
				}
			}

			if(m_apWeapon[i])
				GotItems = true;
		}

		if(!GotItems)
			m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GUN1));

		m_Kits = pData->m_Kits;
		m_Armor = pData->m_Armor;
		GetPlayer()->m_Score = pData->m_Score;
		GetPlayer()->m_Gold = pData->m_Gold;

		if(CGameControllerInvasion *pInv = dynamic_cast<CGameControllerInvasion *>(GameServer()->m_pController))
		{
			if(pInv->RunBuffActive())
				m_Kits = max(m_Kits, 6);
		}

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Data load - color=%d", GetPlayer()->GetColorID());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "Character", aBuf);

		return;
	}

	// Horde / Extraction: fresh loadout each round (no invasion meta save)
	if(str_comp(g_Config.m_SvGametype, "horde") == 0 || str_comp(g_Config.m_SvGametype, "extract") == 0)
	{
		if(m_IsBot)
			return;

		m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GUN1));
		m_apWeapon[1] = GameServer()->NewWeapon(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1));
		if(frandom() < 0.5f)
			m_apWeapon[2] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE1));
		else
			m_apWeapon[2] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE2));
		m_Kits = max(m_Kits, 4);
		SetArmor(max(GetArmor(), 5));
		return;
	}

	// CS / reactor defense
	if(str_comp(g_Config.m_SvGametype, "def") == 0)
	{
		bool GotItems = false;

		for(int i = 0; i < NUM_SLOTS; i++)
			if(m_apWeapon[i])
				GotItems = true;

		if(!GotItems)
		{
			if(frandom() < 0.5f)
				m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GUN1));
			else
				m_apWeapon[0] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GUN2));
		}

		return;
	}

	// dm, tdm, ctf
	int w = 0;

	if(g_Config.m_SvRandomWeapons)
	{
		const CWeaponSpec RandomSpec = GameServer()->m_pController->GetRandomModularWeapon();
		if(RandomSpec.IsValid())
			m_apWeapon[w++] = GameServer()->NewWeapon(RandomSpec);

		// todo random item
		if(frandom() < 0.5f)
			m_apWeapon[w++] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE1));
		else
			m_apWeapon[w++] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE2));
	}
	if(g_Config.m_SvLaserWeapon)
	{
		m_apWeapon[w++] = GameServer()->NewWeapon(CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL3));
	}
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	// if (m_EmoteLockStop > Tick)
	//	return;

	if(m_EmoteLockStop > Server()->Tick())
		return;

	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::SetEmoteFor(int Emote, int Ticks, int LockEmote, bool UseTime)
{
	if(m_EmoteLockStop > Server()->Tick() && LockEmote == 0)
		return;

	m_EmoteType = Emote;

	if(UseTime)
	{
		m_EmoteStop = Server()->Tick() + Ticks * Server()->TickSpeed() / 1000;
		if(LockEmote > 0)
			m_EmoteLockStop = Server()->Tick() + LockEmote * Server()->TickSpeed() / 1000;
	}
	else
	{
		m_EmoteStop = Server()->Tick() + Ticks;
		if(LockEmote > 0)
			m_EmoteLockStop = Server()->Tick() + LockEmote;
	}
}

void CCharacter::SetDroidPawn(bool On)
{
	m_IgnoreCollision = On;
	const int Index = CoreIndex();
	if(Index >= 0 && Index < MAX_CHARACTERS)
		GameServer()->m_World.m_Core.m_apCharacters[Index] = On ? 0 : &m_Core;
	if(On)
		m_ReckoningTick = 0;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_FireBufferEndTick = 0;
	m_SwitchBufferEndTick = 0;
	m_WantedSlot = m_WeaponSlot;
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	m_Input.m_Down = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

bool CCharacter::Invisible()
{
	if(m_aStatus[STATUS_SPAWNING] > 0)
		return true;

	if(m_DamageTakenTick > Server()->Tick() - Server()->TickSpeed() * 1.0f && frandom() < 0.4f)
		return false;

	if(m_AttackTick > Server()->Tick() - Server()->TickSpeed() * 1.0f && frandom() < 0.4f)
		return false;

	if(m_aStatus[STATUS_INVISIBILITY] > 0 && m_aStatus[STATUS_SHIELD] <= 0)
		return true;

	return false;
}

void CCharacter::UseKit(int Kit, vec2 Pos)
{
	if(!m_pPlayer || Kit < 0 || Kit >= NUM_BUILDABLES)
		return;

	int Cost = BuildableCost[Kit];
	if(GameServer()->m_pPveDirector)
		Cost = GameServer()->m_pPveDirector->ModifyBuildingCost(GetCID(), Cost);
	if(m_Kits >= Cost)
	{
		if(GameServer()->AddBuilding(Kit, Pos, GetCID(), Cost))
		{
			m_Kits -= Cost;
			GameServer()->CreateSound(Pos, SOUND_BUILD);
			Server()->SendPlatformEvent(GetCID(), PLATFORM_EVENT_FIRST_BUILD);
			Server()->DispatchModEvent(MOD_EVENT_BUILD, GetCID(), Kit);
			GameServer()->DispatchChallengeEvent(EChallengeScriptEvent::Build, GetCID(), Kit);
			if(GameServer()->m_pTutorialDirector)
				GameServer()->m_pTutorialDirector->OnGameplayProgress(GetCID(), TUTORIAL_EVENT_BUILD);
		}
	}
}

void CCharacter::SelectItem(int Item)
{
	if(m_aItem[Item] <= 0)
		return;

	if(Item == PLAYERITEM_RAGE && m_aStatus[STATUS_DASH] <= 0)
	{
		m_aStatus[STATUS_DASH] = Server()->TickSpeed() * 20.0f;
		m_aItem[Item]--;
	}

	if(Item == PLAYERITEM_LANDMINE)
	{
		if(SetLandmine())
			m_aItem[Item]--;
	}

	if(Item == PLAYERITEM_ELECTROMINE)
	{
		if(SetElectromine())
			m_aItem[Item]--;
	}

	if(Item == PLAYERITEM_SHIELD && m_aStatus[STATUS_SHIELD] <= 0)
	{
		m_aStatus[STATUS_SHIELD] = Server()->TickSpeed() * 20.0f;
		m_ShieldHealth = 100;
		m_ShieldRadius = 16;
		m_aItem[Item]--;
	}

	if(Item == PLAYERITEM_INVISIBILITY && m_aStatus[STATUS_INVISIBILITY] <= 0)
	{
		m_aStatus[STATUS_INVISIBILITY] = Server()->TickSpeed() * 20.0f;
		m_aItem[Item]--;
	}
}

bool CCharacter::UpgradeWeapon()
{
	if(GetWeapon() && GetWeapon()->Upgrade())
	{
		GameServer()->CreateSound(m_Pos, SOUND_UPGRADE);
		SendInventory();
		return true;
	}

	return false;
}

bool CCharacter::GiveBuff(int Item)
{
	if(Item < 0)
		return false;

	if(Item == PLAYERITEM_UPGRADE)
		return UpgradeWeapon();

	if(Item == PLAYERITEM_SHIELD)
	{
		if(m_aStatus[STATUS_SHIELD] > 0)
			return false;

		m_aStatus[STATUS_SHIELD] = Server()->TickSpeed() * 20.0f;
		m_ShieldHealth = 100;
		m_ShieldRadius = 16;
		return true;
	}

	if(Item == PLAYERITEM_INVISIBILITY)
	{
		if(m_aStatus[STATUS_INVISIBILITY] > 0)
			return false;

		m_aStatus[STATUS_INVISIBILITY] = Server()->TickSpeed() * 15.0f;
		return true;
	}

	return false;
}

void CCharacter::GiveRandomBuff()
{
	// disabled
	return;

	int Buff = -1;

	while(Buff < 0 || Buff == PLAYERITEM_FILL || Buff == PLAYERITEM_LANDMINE || Buff == PLAYERITEM_ELECTROMINE ||
		  (Buff == PLAYERITEM_FUEL && g_Config.m_SvUnlimitedTurbo))
		Buff = rand() % NUM_PLAYERITEMS;

	GiveBuff(Buff);
}

int CCharacter::GetMask()
{
	for(int i = 0; i < 12; i++)
	{
		int w = StaticType(m_apWeapon[i]);
		if(w >= SW_MASK1 && w <= SW_MASK5 && GetWeaponSlot() != i)
			return w - (SW_MASK1 - 1);
	}

	return 0;
}

void CCharacter::UpdateCoreStatus()
{
	m_Core.m_Health = m_HiddenHealth;

	m_Core.m_Status = 0;

	if(GameServer()->m_pController->IsCoop() && m_IsBot)
		m_aStatus[STATUS_SLOWMOVING] = 9999;

	// end shield effect when needed
	if(m_ShieldHealth <= 0 || m_aStatus[STATUS_SHIELD] <= 0)
	{
		m_ShieldHealth = 0;
		m_aStatus[STATUS_SHIELD] = 0;
		m_ShieldRadius = 0;
	}

	// check if carrying bomb (reactor defense)
	bool m_BombCarrier = false;

	for(int w = 0; w < NUM_SLOTS; w++)
	{
		if(HasWeaponBehavior(m_apWeapon[w], WEAPON_BEHAVIOR_BOMB) && GetWeaponSlot() != w)
		{
			m_BombCarrier = true;
			break;
		}
	}

	if(m_BombCarrier)
		m_aStatus[STATUS_BOMBCARRIER] = 100;
	else
		m_aStatus[STATUS_BOMBCARRIER] = 0;

	/*
	m_aStatus[STATUS_MASK1] = 10;
	m_aStatus[STATUS_MASK2] = 10;
	*/

	// pack statuses
	for(int i = 0; i < NUM_STATUSS; i++)
	{
		if(m_aStatus[i] > 0)
		{
			m_Core.m_Status |= 1 << i;
			m_aStatus[i]--;
		}
	}

	// store mask to status
	m_Core.m_Status |= GetMask() << STATUS_MASK1;

	if(g_Config.m_SvUnlimitedTurbo)
		m_Core.m_JetpackPower = 200;

	if(m_LastStatusEffect + Server()->TickSpeed() / 3 <= Server()->Tick())
	{
		m_LastStatusEffect = Server()->Tick();

		// flame damage
		if(m_aStatus[STATUS_AFLAME] > 0)
		{
			CAttackSource StatusSource = m_aStatusSource[STATUS_AFLAME];
			StatusSource.m_HitFeedback = false;
			TakeDamage(StatusSource, 2, vec2(0, 0), vec2(0, 0));
		}
	}

	// rolling stops flames a bit faster
	if(m_Core.m_Roll > 0 && m_aStatus[STATUS_AFLAME] > 0)
		m_aStatus[STATUS_AFLAME]--;
}

void CCharacter::TickNpcAI()
{
	if(!m_pAI)
		return;

	m_pAI->Tick();
	CNetObj_PlayerInput Input;
	mem_zero(&Input, sizeof(Input));
	m_pAI->UpdateInput((int *)&Input);
	OnDirectInput(&Input);
	OnPredictedInput(&Input);
}

void CCharacter::Tick()
{
	if(m_pPlayer && m_pPlayer->GetDroid())
		return;

	TickNpcAI();

	if(m_IsBot && !GameServer()->Collision()->IsMapPath() && m_SnapTick &&
	   m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 15.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
			MarkToBeKicked();
	}

	if(m_ElectroWallCooldown > 0)
		m_ElectroWallCooldown--;

	if(m_PainSoundTimer > 0)
		m_PainSoundTimer--;

	if(m_SkipPickups > 0)
		m_SkipPickups--;

	if(m_DamageSoundTimer > 0)
		m_DamageSoundTimer--;

	if(m_SendInventoryTick && m_SendInventoryTick < Server()->Tick())
	{
		SendInventory();
		m_SendInventoryTick = 0;
	}

	if(IsBombCarrier())
	{
		GameServer()->m_pController->m_BombPos = m_Pos;
		GameServer()->m_pController->m_BombStatus = BOMB_CARRIED;
	}

	if(GetMask() == 1)
	{
		if(!m_MaskEffectTick || m_MaskEffectTick < Server()->Tick())
		{
			IncreaseHealth(1);
			m_MaskEffectTick = Server()->Tick() + Server()->TickSpeed() * 0.5f;
		}
	}
	else
		m_MaskEffectTick = 0;

	if(g_Config.m_SvInfiniteGrenades && m_GrenadeGiveCooldown-- <= 0)
	{
		bool GotGrenade = false;

		int Slot = GetWeaponSlot();

		if(!GetWeapon(Slot))
		{
			m_apWeapon[Slot] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE1));
			SendInventory();
			m_GrenadeGiveCooldown = 100;
		}
		else
		{
			for(int w = 0; w < 4; w++)
			{
				if(HasWeaponBehavior(m_apWeapon[w], WEAPON_BEHAVIOR_GRENADE_TIMED))
					GotGrenade = true;
			}

			if(!GotGrenade)
			{
				int Slot = rand() % 4;
				if(!GetWeapon(Slot) && Slot != GetWeaponSlot())
				{
					m_apWeapon[Slot] = GameServer()->NewWeapon(CWeaponCatalog::Static(SW_GRENADE1));
					SendInventory();
					m_GrenadeGiveCooldown = 100;
				}
			}
		}
	}

	/*
	if(m_pPlayer->m_ForceBalanced)
	{
		char Buf[128];
		str_format(Buf, sizeof(Buf), "You were moved to %s due to team balancing",
	GameServer()->m_pController->GetTeamName(GetTeam())); GameServer()->SendBroadcast(Buf,
	GetCID());

		m_pPlayer->m_ForceBalanced = false;
	}
	*/

	UpdateCoreStatus();

	if(m_aStatus[STATUS_SPAWNING] > 0.0f)
		return;

	m_Core.m_Input = m_Input;
	m_Core.m_MoveSpeedMultiplier = 1.0f;
	if(GameServer()->m_pPveDirector)
	{
		if(m_IsBot)
			m_Core.m_MoveSpeedMultiplier = GameServer()->m_pPveDirector->EnemySpeedMultiplier();
		else
			m_Core.m_MoveSpeedMultiplier = GameServer()->m_pPveDirector->MovementMultiplier(GetCID());
	}

	float RecoilCap = 17.5f;

	if((m_Core.m_Vel.x < RecoilCap && m_Recoil.x > 0) || (m_Core.m_Vel.x > -RecoilCap && m_Recoil.x < 0))
		m_Core.m_Vel.x += m_Recoil.x * 0.7f;

	if((m_Core.m_Vel.y < RecoilCap && m_Recoil.y > 0) || (m_Core.m_Vel.y > -RecoilCap && m_Recoil.y < 0))
		m_Core.m_Vel.y += m_Recoil.y * 0.7f;

	m_Recoil *= 0.6f;

	if(m_Core.m_KickDamage >= 0 && m_Core.m_KickDamage < MAX_CHARACTERS)
	{
		GameServer()->CreateSound(m_Pos, SOUND_KICKHIT);
	}

	m_Core.m_ClientID = CoreIndex();
	m_Core.Tick(true);

	if((m_Core.m_BallHitVel.x != 0.0f || m_Core.m_BallHitVel.y != 0.0f) && CoreIndex() >= 0)
		GameServer()->m_pController->m_LastBallToucher = CoreIndex();

	// anti head stuck
	if(GameServer()->Collision()->CheckPoint(m_Pos.x, m_Pos.y - m_ProximityRadius / 3.f - 42) &&
	   (!m_Core.IsGrounded() && m_Core.m_Slide == 0))
	{
		m_Pos.y += 1.0f;
		m_Core.m_Pos.y += 1.0f;
		m_Core.m_Vel.y = 0.0f;
	}

	if(m_Core.m_FluidDamage && m_AcidTimer <= 0)
	{
		TakeDamage(CAttackSource::World(WEAPON_ACID), 2, normalize(m_Core.m_Vel), vec2(0, 0));
		m_AcidTimer = 4;
	}

	if(m_AcidTimer > 0)
		m_AcidTimer--;

	if(m_Core.m_KickDamage >= 0 && m_Core.m_KickDamage < MAX_CHARACTERS)
	{
		GameServer()->CreateSound(m_Pos, SOUND_KICKHIT);
	}

	if(m_CryTimer > 0)
		m_CryTimer--;

	// handle death-tiles
	if(GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f,
												 m_Pos.y - m_ProximityRadius / 3.f - 24) &
		   CCollision::COLFLAG_DEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) &
		   CCollision::COLFLAG_DEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f,
												 m_Pos.y - m_ProximityRadius / 3.f - 24) &
		   CCollision::COLFLAG_DEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) &
		   CCollision::COLFLAG_DEATH)
	{
		m_DeathTileTimer = 10;
		TakeDeathtileDamage();
	}

	// handle insta death-tiles
	if(GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f,
												 m_Pos.y - m_ProximityRadius / 3.f - 24) &
		   CCollision::COLFLAG_INSTADEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x + m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) &
		   CCollision::COLFLAG_INSTADEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f,
												 m_Pos.y - m_ProximityRadius / 3.f - 24) &
		   CCollision::COLFLAG_INSTADEATH ||
	   GameServer()->Collision()->GetCollisionAt(m_Pos.x - m_ProximityRadius / 3.f, m_Pos.y + m_ProximityRadius / 3.f) &
		   CCollision::COLFLAG_INSTADEATH)
	{
		Die(CAttackSource::World(DEATHTYPE_SPIKE, GetCID()));
	}

	// leaving gamelayer (ignore going right)
	if(GameLayerClipped(vec2(min(0.0f, m_Pos.x), m_Pos.y)))
		Die(CAttackSource::World(DEATHTYPE_SPIKE, GetCID()));

	// delayed death ray
	if(m_DeathrayTick > 0 && m_DeathrayTick <= Server()->Tick())
		TakeDeathrayDamage();

	if(m_DelayedKill)
	{
		Die(CAttackSource::World(WEAPON_WORLD, GetCID()));
		m_LatestHitVel = vec2(0, 0);
	}

	if(m_DeathTileTimer > 0)
		m_DeathTileTimer--;

	// GameServer()->CreateDeath(m_Pos+vec2(frandom()*100, frandom()*100) - vec2(frandom()*100, frandom()*100), -1);

	// handle Weapons
	HandleWeapons();

	// Previnput
	m_PrevInput = m_Input;

	// GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "debug", "Tick end");

	if(!m_IsBot || GameServer()->Collision()->IsMapPath())
		GameServer()->ActivateBlockEntities(m_Pos.x);

	return;
}

void CCharacter::TickDefered()
{
	if(m_pPlayer && m_pPlayer->GetDroid())
	{
		CDroid *pDroid = m_pPlayer->GetDroid();
		m_Pos = pDroid->m_Pos;
		m_Core.m_Pos = m_Pos;
		m_Core.m_Vel = vec2(0, 0);
		m_SendCore = m_Core;
		m_ReckoningCore = m_Core;
		m_ReckoningTick = 0;
		return;
	}

	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	// lastsentcore
	//  vec2 StartPos = m_Core.m_Pos;
	//  vec2 StartVel = m_Core.m_Vel;
	// bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.Move();
	// bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	// bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	/*
	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
	*/

	int Events = m_Core.m_TriggeredEvents;
	int64 Mask = GetCID() >= 0 ? CmaskAllExceptOne(GetCID()) : CmaskAll();

	if(Events & COREEVENT_HOOK_ATTACH_PLAYER)
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, Mask);
	if(Events & COREEVENT_HOOK_ATTACH_GROUND)
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Mask);
	if(Events & COREEVENT_HOOK_HIT_NOHOOK)
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Mask);

	if(Events & COREEVENT_GROUND_JUMP)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Mask);

	if(GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ForceCoreSend || m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() ||
		   mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ForceCoreSend = false;
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_ReckoningTick;
	if(m_LastBlink != -1)
		++m_LastBlink;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

void CCharacter::SetArmor(int Armor)
{
	m_Armor = Armor;
}

void CCharacter::SetHealth(int Health)
{
	if(m_IsBot && GameServer()->m_pPveDirector)
		Health = max(1, (int)(Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f));
	m_MaxHealth = Health;
	m_HiddenHealth = Health;
}

void CCharacter::RefillHealth()
{
	m_HiddenHealth = m_MaxHealth;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_HiddenHealth >= m_MaxHealth)
		return false;

	if(GetMask() == 4)
		Amount *= 2;
	if(GameServer()->m_pPveDirector && !m_IsBot)
		Amount = GameServer()->m_pPveDirector->ModifyRecovery(GetCID(), Amount, true);

	m_HiddenHealth = clamp(m_HiddenHealth + Amount, 0, m_MaxHealth);

	// GetPlayer()->m_InterestPoints += 40;

	return true;
}

bool CCharacter::AddKits(int Amount)
{
	if(GameServer()->m_pController->IsInfection() && GetTeam() == TEAM_BLUE)
		return false;

	if(m_Kits < 99)
	{
		m_Kits = min(m_Kits + Amount, 99);
		return true;
	}

	return false;
}

bool CCharacter::AddKit()
{
	if(GameServer()->m_pController->IsInfection() && GetTeam() == TEAM_BLUE)
		return false;

	if(m_Kits < 99)
	{
		m_Kits = min(m_Kits + 5, 99);
		return true;
	}

	return false;
}

bool CCharacter::AddClip()
{
	if(GetWeapon() && GetWeapon()->AddClip())
		return true;

	for(int i = 0; i < NUM_SLOTS; i++)
		if(m_apWeapon[i] && m_apWeapon[i]->AddClip())
			return true;

	return false;
}

bool CCharacter::IncreaseAmmo(int Amount)
{
	if(GetMask() == 4)
	{
		if(AddClip())
		{
			AddClip();
			return true;
		}

		return false;
	}

	return AddClip();
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 100)
		return false;

	if(GetMask() == 4)
		Amount *= 2;
	if(GameServer()->m_pPveDirector && !m_IsBot)
		Amount = GameServer()->m_pPveDirector->ModifyRecovery(GetCID(), Amount, false);

	m_Armor = clamp(m_Armor + Amount, 0, 100);
	return true;
}

void CCharacter::ReleaseWeapons()
{
	m_ForceCoreSend = true;

	for(int i = 0; i < NUM_SLOTS; i++)
		if(m_apWeapon[i])
		{
			if(HasWeaponBehavior(m_apWeapon[i], WEAPON_BEHAVIOR_BOMB))
			{
				GameServer()->m_pController->DropWeapon(
					m_Pos + vec2(0, -16), (m_Core.m_Vel / 1.7f + vec2(0, -3)) * 0.75f, m_apWeapon[i]);
			}
			else
			{
				m_apWeapon[i]->OnOwnerDeath(i == m_WeaponSlot);
			}

			m_apWeapon[i] = 0;
		}
}

void CCharacter::Die(const CAttackSource &Source, bool SkipKillMessage, bool IsTurret1)
{
	int Killer = Source.m_Owner;
	const bool GameSource = Source.m_Kind == EAttackSourceKind::World && Source.m_Type == WEAPON_GAME;
	// we got to wait 0.5 secs before respawning
	// m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;

	SaveData();

	if(m_pPlayer)
	{
		m_pPlayer->m_DeathTick = Server()->Tick();

		if(g_Config.m_SvSurvivalMode)
			m_pPlayer->m_RespawnTick = Server()->Tick();
		else
			m_pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvRespawnDelay;
	}

	if(Killer == NEUTRAL_BASE)
		Killer = GetCID();

	if(!SkipKillMessage && Killer >= 0 && Killer < MAX_CLIENTS)
	{
		int ModeSpecial =
			GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Source);

		if(!m_IsBot && GetCID() >= 0)
		{
			char aBuf[256];
			str_format(aBuf,
					   sizeof(aBuf),
					   "kill killer='%d:%s' victim='%d:%s' source=%d:%d special=%d",
					   Killer,
					   Server()->ClientName(Killer),
					   GetCID(),
					   Server()->ClientName(GetCID()),
					   static_cast<int>(Source.m_Kind),
					   Source.m_Type,
					   ModeSpecial);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		}

		// send the kill message
		if(!GameSource && GetCID() >= 0)
		{
			CNetMsg_Sv_KillMsg Msg;
			Msg.m_Killer = Killer;
			Msg.m_Victim = GetCID();
			Msg.m_SourceKind = static_cast<int>(Source.m_Kind);
			Msg.m_SourceType = Source.m_Type;
			Msg.m_WeaponDefinitionId = static_cast<int>(Source.m_Weapon.m_DefinitionId);
			Msg.m_WeaponLevel = Source.m_Weapon.m_Level;
			Msg.m_ModeSpecial = ModeSpecial;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
		}
	}
	else
		GameServer()->m_pController->OnCharacterDeath(this, 0, Source);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	if(m_pPlayer)
		m_pPlayer->m_DieTick = Server()->Tick();

	ReleaseWeapons();

	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	const int Index = CoreIndex();
	if(Index >= 0 && Index < MAX_CHARACTERS)
		GameServer()->m_World.m_Core.m_apCharacters[Index] = 0;

	if(CoreIndex() >= 0 && ((Killer >= 0 && !GameSource) || !m_IsBot))
		GameServer()->CreateDeath(m_Pos, CoreIndex());

	if(Killer >= 0 && Killer < MAX_CLIENTS && !GameSource && Source.m_Kind != EAttackSourceKind::Building)
		GameServer()->CreateSoundGlobal(SOUND_KILL, Killer);

	if(GetCID() >= 0)
		GameServer()->CreateSoundGlobal(SOUND_DEATH, GetCID());
}

void CCharacter::Cry()
{
	if(m_CryTimer <= 0)
	{
		m_CryTimer = 50;
		if(m_CryState == 0 || m_CryState == 2)
		{
			GameServer()->CreateSound(m_Pos, SOUND_TEE_CRY);
			m_CryState++;
		}
		else if(m_CryState == 1)
		{
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);
			m_CryState++;
			m_CryTimer = 30;
		}
		else if(m_CryState == 3)
		{
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
			m_CryState = 0;
		}
	}
}
#define RAD 0.017453292519943295769236907684886f

void CCharacter::Warp()
{
	GameServer()->CreateEffect(FX_MONSTERSPAWN, m_Pos);
	m_aStatus[STATUS_SPAWNING] = 5.5f * Server()->TickSpeed();
	// m_aStatus[STATUS_INVISIBILITY] = Server()->TickSpeed() * 20.0f;
	// m_IgnoreCollision = true;
}

void CCharacter::Deathray(bool Kill)
{
	if(m_DeathrayTick > 0)
		return;

	if(Kill)
	{
		m_DeathrayTick = Server()->Tick() + Server()->TickSpeed() * 0.2f;
		m_aStatus[STATUS_DEATHRAY] = 10.0f * Server()->TickSpeed();
	}
	else
	{
		m_aStatus[STATUS_DEATHRAY] = 0.25f * Server()->TickSpeed();
	}
}

void CCharacter::Electrocute(float Duration)
{
	if(m_aStatus[STATUS_ELECTRIC] < Duration * Server()->TickSpeed())
		m_aStatus[STATUS_ELECTRIC] = Duration * Server()->TickSpeed();
}

void CCharacter::Slow(float Duration)
{
	const int Ticks = max(1, (int)(Duration * Server()->TickSpeed()));
	m_aStatus[STATUS_SLOWMOVING] = max(m_aStatus[STATUS_SLOWMOVING], Ticks);
}

void CCharacter::SetAflame(float Duration, const CAttackSource &Source)
{
	if(IgnoreCollision())
		return;

	if(CoreIndex() >= 0 && GameServer()->m_pController->IsFriendlyFire(CoreIndex(), Source.m_Owner) &&
	   !g_Config.m_SvTeamdamage)
		return;

	if(m_aStatus[STATUS_AFLAME] < Duration * Server()->TickSpeed())
	{
		m_aStatusSource[STATUS_AFLAME] = Source;
		m_aStatus[STATUS_AFLAME] = Duration * Server()->TickSpeed();
	}
}

bool CCharacter::TakeDamage(const CAttackSource &Source, int Dmg, vec2 Force, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	const bool HasCombatProfile = CWeaponCatalog::TryResolveAttack(Source, &Combat);
	uint32_t SourceBehavior = 0;
	if(Source.m_Kind == EAttackSourceKind::PlayerWeapon)
	{
		CWeaponDefinition Definition;
		if(CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &Definition))
			SourceBehavior = Definition.m_BehaviorFlags;
	}
	// skip everything while spawning
	if(m_aStatus[STATUS_SPAWNING] > 0.0f)
		return false;
	if(m_IgnoreCollision)
		return false;

	if((SourceBehavior & WEAPON_BEHAVIOR_ELECTROWALL) && m_ElectroWallCooldown <= 0)
	{
		Force = RandomDir() * 0.1f;
		m_aStatus[STATUS_DEATHRAY] = 0.15f * Server()->TickSpeed();
		m_ElectroWallCooldown = 0.17f * Server()->TickSpeed();
		Dmg = 2;
		Pos = m_Pos - vec2(0, frandom() * 18.0f);
	}

	if(!Dmg)
		return false;

	if(m_ShieldHealth <= 0)
		m_Recoil += Force;
	else
		m_Recoil += Force / 2;

	// signal AI
	if(Dmg > 0 && m_pAI && HasCombatProfile)
		m_pAI->ReceiveDamage(From, Dmg);

	if(CoreIndex() >= 0 && GameServer()->m_pController->IsFriendlyFire(CoreIndex(), From) && !g_Config.m_SvTeamdamage)
		return false;

	float Flame = Combat.m_FlameAmount;
	float Electro = Combat.m_ElectroAmount;

	// damage reduction for invasion
	if(Dmg > 0 && GameServer()->m_pController->IsCoop() && !m_IsBot)
		Dmg = max(1, Dmg / 2);

	if(Dmg > 0 && GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDamage(Source, m_IsBot ? -2 : GetCID(), Dmg, this);

	if(CoreIndex() >= 0 && From == CoreIndex())
	{
		if(GameServer()->m_pController->IsCoop())
			Dmg = max(1, Dmg / 4);
		else
			Dmg = max(1, Dmg / 4);
	}

	if(From >= 0)
		m_DamagedByPlayer = true;
	GameServer()->DispatchChallengeEvent(EChallengeScriptEvent::Damage, GetCID(), Dmg);

	// disable self damage if weapon is forced
	// if (g_Config.m_SvForceWeapon && From == GetCID())
	//	return false;

	m_DamageTaken++;

	// damage / projectile end position
	vec2 DmgPos = m_Pos + vec2(0, -12);
	if(Pos.x != 0 && Pos.y != 0)
		DmgPos = Pos;

	if(Flame > 0.0f && Dmg > 2)
		SetAflame(Flame, Source);

	// create healthmod indicator
	if(m_ShieldHealth <= 0)
	{
		// if (Type == DAMAGETYPE_NORMAL)
		if(Flame == 0.0f && Electro == 0.0f &&
		   !(Source.m_Kind == EAttackSourceKind::World && Source.m_Type == WEAPON_ACID) &&
		   !(SourceBehavior & WEAPON_BEHAVIOR_ELECTROWALL))
		{
			if(Server()->Tick() < m_DamageTakenTick + 25)
				GameServer()->CreateDamageInd(
					DmgPos, GetAngle(-Force), Dmg * (m_Type == CCharacter::ROBOT ? -1 : 1), CoreIndex());
			else
			{
				m_DamageTaken = 0;
				GameServer()->CreateDamageInd(
					DmgPos, GetAngle(-Force), Dmg * (m_Type == CCharacter::ROBOT ? -1 : 1), CoreIndex());
			}

			if(m_Type == CCharacter::ROBOT && m_DamageSoundTimer <= 0)
				GameServer()->CreateBuildingHit(DmgPos);
		}
		else
		{
			GameServer()->CreateDamageInd(DmgPos, GetAngle(-Force), -Dmg, CoreIndex());
		}

		if(SourceBehavior & WEAPON_BEHAVIOR_CHAINSAW)
			m_Core.m_Vel *= 0.9f;

		if(Electro > 0.0f)
			m_aStatus[STATUS_ELECTRIC] = max(1.0f * m_aStatus[STATUS_ELECTRIC], Electro * Server()->TickSpeed());

		/*
		else
		{
			if (Type == DAMAGETYPE_ELECTRIC)
			{
				//GameServer()->SendEffect(GetCID(), EFFECT_ELECTRODAMAGE);
				m_aStatus[STATUS_ELECTRIC] = 1.0f*Server()->TickSpeed();
			}

			// damage indicator but no blood
			if (Type != DAMAGETYPE_FLAME)
				GameServer()->CreateDamageInd(DmgPos, GetAngle(-Force), -Dmg, GetCID());
		}
		*/
	}

	if(Dmg)
	{
		// if (m_ShieldHealth > 0 && Type != DAMAGETYPE_FLAME)
		if(m_ShieldHealth > 0 && Flame == 0.0f)
		{
			GameServer()->CreateEffect(FX_SHIELDHIT, DmgPos);
			const int ShieldDamage = min(Dmg + (g_Config.m_SvOneHitKill ? 1000 : 0), m_ShieldHealth);
			m_ShieldHealth -= Dmg + (g_Config.m_SvOneHitKill ? 1000 : 0);
			if(From != CoreIndex())
				GameServer()->CreateHitConfirm(DmgPos, Source, ShieldDamage, HIT_TARGET_SHIELD, false);

			return false;
		}
		else
		{
			// block damage with armor
			if(m_Armor > 0 && !g_Config.m_SvOneHitKill)
			{
				int ArmorDmg = min(Dmg / 2, m_Armor);
				m_Armor -= ArmorDmg;
				Dmg -= ArmorDmg;
			}
			if(Dmg >= m_HiddenHealth && !m_IsBot && GameServer()->m_pPveDirector &&
			   GameServer()->m_pPveDirector->UseLastStand(GetCID()))
				Dmg = max(0, m_HiddenHealth - 1);

			const int HealthBefore = m_HiddenHealth;
			m_HiddenHealth -= Dmg + (g_Config.m_SvOneHitKill ? 1000 : 0);
			if(GameServer()->m_pTutorialDirector && m_IsBot)
			{
				CPlayer *pFromPlayer = GameServer()->GetClientPlayer(From);
				if(pFromPlayer && !pFromPlayer->m_IsBot)
					GameServer()->m_pTutorialDirector->OnGameplayProgress(From, TUTORIAL_EVENT_TARGET_HIT);
			}
			const int TargetType = m_Type == CCharacter::ROBOT ? HIT_TARGET_METAL : HIT_TARGET_FLESH;
			if(From != CoreIndex())
				GameServer()->CreateHitConfirm(DmgPos, Source, min(Dmg, HealthBefore), TargetType, m_HiddenHealth <= 0);

			// if (Type == DAMAGETYPE_NORMAL)

			m_LatestHitVel = Force;

			if(Flame > 0.0f)
				GameServer()->CreateDamageInd(DmgPos, GetAngle(-Force), -Dmg, CoreIndex());

			m_Core.m_DamageTick = Server()->Tick();
		}
	}

	if(m_pPlayer)
	{
		m_pPlayer->m_ActionTimer = 0;
		m_pPlayer->m_InterestPoints += Dmg * 4;
	}

	m_DamageTakenTick = Server()->Tick();

	// do damage Hit sound
	if(!(Source.m_Kind == EAttackSourceKind::Building && Source.m_Type == BUILDING_TESLACOIL) &&
	   m_DamageSoundTimer <= 0)
	{
		CPlayer *pFromPlayer = GameServer()->GetClientPlayer(From);
		if(pFromPlayer && From != CoreIndex())
		{
			m_DamageSoundTimer = 2;
			pFromPlayer->m_InterestPoints += Dmg * 5;

			int64 Mask = CmaskOne(From);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS &&
				   GameServer()->m_apPlayers[i]->m_SpectatorID == From)
					Mask |= CmaskOne(i);
			}
			// GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
		}
	}

	// check for death
	if(m_HiddenHealth <= 0)
	{
		Die(Source, false, false);

		// set attacker's face to happy (taunt!)
		if(From >= 0 && From != CoreIndex())
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(From);
			if(pChr)
			{
				pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
				// pChr->m_EmoteType = EMOTE_HAPPY;
				// pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
			}
		}

		return false;
	}

	if(m_PainSoundTimer <= 0 && !m_Silent)
	{
		// if (Dmg > 10 || frandom()*10 < 3)
		//	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
		// else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);
		m_PainSoundTimer = 2;
	}

	SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);
	// m_EmoteType = EMOTE_PAIN;
	// m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	return true;
}

void CCharacter::TakeSawbladeDamage(vec2 SawbladePos)
{
	if(IgnoreCollision())
		return;

	if(m_ShieldHealth > 0)
	{
		GameServer()->CreateEffect(FX_SHIELDHIT, (m_Pos + SawbladePos) / 2.0f);
		m_ShieldHealth -= 5 + (g_Config.m_SvOneHitKill ? 1000 : 0);
		return;
	}

	m_DamageTaken++;

	m_Core.m_DamageTick = Server()->Tick();

	GameServer()->CreateDamageInd((m_Pos + SawbladePos) / 2.0f,
								  GetAngle(normalize(vec2(frandom() - 0.5f, frandom() - 0.5f))),
								  3,
								  CoreIndex());

	m_Core.m_Vel += normalize(m_Pos - SawbladePos) * 2.0f;

	if(m_Armor > 0)
	{
		m_Armor = max(m_Armor - 3, 0);
		m_HiddenHealth -= 2 + (g_Config.m_SvOneHitKill ? 1000 : 0);
	}
	else
		m_HiddenHealth -= 5 + (g_Config.m_SvOneHitKill ? 1000 : 0);

	m_DamageTakenTick = Server()->Tick();

	// check for death
	if(m_HiddenHealth <= 0)
	{
		Die(CAttackSource::World(DEATHTYPE_SAWBLADE, GetCID()));
		return;
	}

	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);

	if(m_pAI)
		m_pAI->ReceiveDamage(-1, 5);
}

void CCharacter::TakeDeathrayDamage()
{
	m_DamageTaken++;
	Die(CAttackSource::World(DEATHTYPE_DEATHRAY, GetCID()));
}

void CCharacter::TakeDeathtileDamage()
{
	m_DamageTaken++;

	int top = GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y - 32);
	int bot = GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y + 32);
	int left = GameServer()->Collision()->GetCollisionAt(m_Pos.x - 32, m_Pos.y);
	int right = GameServer()->Collision()->GetCollisionAt(m_Pos.x + 32, m_Pos.y);

	m_Core.m_Jumped = 0;

	if(!top && bot)
		m_Core.m_Vel.y = -5.0f;
	if(!bot && top)
		m_Core.m_Vel.y = +5.0f;
	if(!left && right)
		m_Core.m_Vel.x = -5.0f;
	if(!right && left)
		m_Core.m_Vel.x = +5.0f;

	m_LatestHitVel = GetVel();

	// create healthmod indicator
	if(Server()->Tick() < m_DamageTakenTick + 25)
	{
		// make sure that the damage indicators doesn't group together
		GameServer()->CreateDamageInd(
			m_Pos, GetAngle(normalize(vec2(frandom() - 0.5f, frandom() - 0.5f))), 3, CoreIndex());
	}
	else
	{
		GameServer()->CreateDamageInd(
			m_Pos, GetAngle(normalize(vec2(frandom() - 0.5f, frandom() - 0.5f))), 3, CoreIndex());
	}

	m_HiddenHealth -= 10;

	m_DamageTakenTick = Server()->Tick();

	// check for death
	if(m_HiddenHealth <= 0)
	{
		Die(CAttackSource::World(DEATHTYPE_SPIKE, GetCID()));
		return;
	}

	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);

	if(m_pAI)
		m_pAI->ReceiveDamage(-1, 10);
}

void CCharacter::FillCharacterSnap(CNetObj_Character *pCharacter, int SnappingClient)
{
	m_SnapTick = Server()->Tick();

	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	if(m_Core.m_DashTimer > 0)
		pCharacter->m_Movement = m_Core.m_DashTimer | m_Core.m_DashAngle << 6;
	else
		pCharacter->m_Movement = 0;

	pCharacter->m_Emote = m_EmoteType;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	if(GetWeapon())
	{
		pCharacter->m_WeaponDefinitionId = static_cast<int>(GetWeapon()->GetWeaponSpec().m_DefinitionId);
		pCharacter->m_WeaponLevel = GetWeapon()->GetWeaponSpec().m_Level;
	}
	else
	{
		pCharacter->m_WeaponDefinitionId = 0;
		pCharacter->m_WeaponLevel = 0;
	}

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Health = m_HiddenHealth;
	pCharacter->m_PlayerFlags = m_pPlayer ? m_pPlayer->m_PlayerFlags : PLAYERFLAG_PLAYING;
	if(m_pPlayer && m_pPlayer->GetDroid())
	{
		CDroid *pDroid = m_pPlayer->GetDroid();
		pCharacter->m_PlayerFlags |= PLAYERFLAG_DROID;
		if(pDroid->m_MaxHealth > 0)
			pCharacter->m_Health = clamp((pDroid->m_Health * 100) / pDroid->m_MaxHealth, 0, 100);
	}

	const bool ShowPrivate = GetCID() >= 0 &&
							 (GetCID() == SnappingClient || SnappingClient == -1 ||
							  (!g_Config.m_SvStrictSpectateMode && SnappingClient >= 0 &&
							   GameServer()->m_apPlayers[SnappingClient] &&
							   GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID));
	if(ShowPrivate)
	{
		pCharacter->m_Armor = m_Armor;
		pCharacter->m_AmmoCount = GetWeapon() ? GetWeapon()->GetAmmo() : 0;
	}

	if(m_LastBlink < Server()->Tick())
	{
		if(m_LastBlink + 5 < Server()->Tick())
			m_LastBlink = Server()->Tick() + Server()->TickSpeed() * (frandom() * 15.0f);

		if(pCharacter->m_Emote == EMOTE_NORMAL)
			pCharacter->m_Emote = EMOTE_BLINK;
	}
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(IsNpc())
	{
		CNetObj_Npc *pNpc = static_cast<CNetObj_Npc *>(
			Server()->SnapNewItem(NETOBJTYPE_NPC, m_NpcSlot, sizeof(CNetObj_Npc)));
		if(!pNpc)
			return;

		FillCharacterSnap(pNpc, SnappingClient);
		pNpc->m_Team = GetTeam();
		StrToInts(&pNpc->m_Topper0, 6, m_TeeInfos.m_TopperName);
		StrToInts(&pNpc->m_Eye0, 6, m_TeeInfos.m_EyeName);
		StrToInts(&pNpc->m_Head0, 6, m_TeeInfos.m_HeadName);
		StrToInts(&pNpc->m_Body0, 6, m_TeeInfos.m_BodyName);
		StrToInts(&pNpc->m_Hand0, 6, m_TeeInfos.m_HandName);
		StrToInts(&pNpc->m_Foot0, 6, m_TeeInfos.m_FootName);
		pNpc->m_ColorBody = m_TeeInfos.m_ColorBody;
		pNpc->m_ColorFeet = m_TeeInfos.m_ColorFeet;
		pNpc->m_ColorTopper = m_TeeInfos.m_ColorTopper;
		pNpc->m_ColorSkin = m_TeeInfos.m_ColorSkin;
		pNpc->m_BloodColor = m_TeeInfos.m_BloodColor;
		return;
	}

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(
		Server()->SnapNewItem(NETOBJTYPE_CHARACTER, GetCID(), sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	FillCharacterSnap(pCharacter, SnappingClient);

	if(GetWeapon() && GetCID() >= 0)
	{
		CNetObj_WeaponRuntime *pRuntime = static_cast<CNetObj_WeaponRuntime *>(
			Server()->SnapNewItem(NETOBJTYPE_WEAPONRUNTIME, GetCID(), sizeof(CNetObj_WeaponRuntime)));
		if(pRuntime)
		{
			pRuntime->m_Owner = GetCID();
			pRuntime->m_WeaponDefinitionId = static_cast<int>(GetWeapon()->GetWeaponSpec().m_DefinitionId);
			pRuntime->m_WeaponLevel = GetWeapon()->GetWeaponSpec().m_Level;
			pRuntime->m_RandomState = static_cast<int>(GetWeapon()->ScriptRandomState());
			pRuntime->m_State0 = GetWeapon()->ScriptStateGet(0);
			pRuntime->m_State1 = GetWeapon()->ScriptStateGet(1);
			pRuntime->m_State2 = GetWeapon()->ScriptStateGet(2);
			pRuntime->m_State3 = GetWeapon()->ScriptStateGet(3);
			pRuntime->m_State4 = GetWeapon()->ScriptStateGet(4);
			pRuntime->m_State5 = GetWeapon()->ScriptStateGet(5);
			pRuntime->m_State6 = GetWeapon()->ScriptStateGet(6);
			pRuntime->m_State7 = GetWeapon()->ScriptStateGet(7);
		}
	}
}
