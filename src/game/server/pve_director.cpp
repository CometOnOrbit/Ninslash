#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/platform_events.h>
#include <generated/protocol.h>

#include <game/weapons.h>
#include <game/server/entities/character.h>
#include <game/server/entities/droid.h>
#include <game/server/entities/lightning.h>
#include <game/server/entities/pve_drone.h>
#include <game/server/entities/pve_drone_pulse.h>
#include <game/server/entities/weapon.h>
#include <game/server/entities/building.h>
#include <game/server/entities/radar.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/extract.h>
#include <game/server/gamemodes/horde.h>
#include <game/server/gamemodes/invasion.h>
#include <game/server/player.h>
#include <game/server/playerdata.h>
#include <game/server/tutorial_director.h>

#include "pve_director.h"

static bool IsHeavyWeaponAttack(const CAttackSource &Source)
{
	CWeaponDefinition Definition;
	return Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
		   CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &Definition) &&
		   Source.m_Weapon.m_Level >= max(2, (Definition.m_MaxLevel + 1) / 2);
}

void CPveDirector::CPlayerRun::Reset()
{
	m_Connected = false;
	m_ProgressSynced = false;
	m_ChoicePending = false;
	m_LastStandUsed = false;
	m_ResearchPoints = 0;
	m_ResearchMask = CPveResearchMask();
	m_HighestInvasion = 0;
	m_PreferredCheckpoint = 1;
	m_Choices = 0;
	m_ChoiceNonce = 0;
	m_LastChoiceNonce = 0;
	for(int i = 0; i < 3; i++)
		m_aOffered[i] = -1;
	for(int i = 0; i < NUM_PVE_CARDS; i++)
		m_aStacks[i] = 0;
	m_ContractVote = -1;
	m_LastContractNonce = 0;
	m_LastResearchNonce = 0;
	m_LegendaryCard = -1;
	for(int i = 0; i < 4; i++)
		m_aWeaponResources[i] = 0;
	m_Barrier = 0;
	m_StageKills = 0;
	m_SecondWindTriggers = 0;
	m_SalvageKits = 0;
	m_DirectHits = 0;
	m_EmpoweredBlasts = 0;
	m_FullVoltageReleases = 0;
	m_LastEmpoweredSpecialization = PVE_SPECIALIZATION_NONE;
	m_PerfectSequenceShots = 0;
	m_PerfectSequencePending = false;
	m_TacticalReloadShots = 0;
	m_DeathlessFloors = 0;
	m_AvatarEndTick = 0;
	m_LastDamageTick = 0;
	m_LastBarrierRefitTick = 0;
	m_AegisLoopUsed = false;
	m_ObjectiveCacheUsed = false;
	m_CleanExitUsed = false;
	m_DiedThisStage = false;
	m_pDrone = 0;
	m_pDroneTarget = 0;
	m_DroneModule = PVE_DRONE_NONE;
	m_DroneSwitchReadyTick = 0;
	m_DroneActionTick = 0;
	m_LastDroneNonce = 0;
	m_LastBuildStateTick = 0;
	for(int i = 0; i < 11; i++)
		m_aLastBuildState[i] = -1;
	m_KillChainStacks = 0;
	m_KillChainEndTick = 0;
	m_SustainedHits = 0;
	m_SustainedEndTick = 0;
	m_CapacitorHits = 0;
	m_ReaperKills = 0;
	m_ReaperChainEndTick = 0;
	m_ReaperEndTick = 0;
	m_ShockwaveEndTick = 0;
	m_InvasionFloorsCompleted = 0;
	m_StageSuppliesApplied = false;
	m_EmergencyPlatingUsed = false;
	m_PendingArmor = 0;
	m_PendingKits = 0;
	m_PendingAmmo = false;
}

CPveDirector::CPveDirector(CGameContext *pGameServer)
	: m_pGameServer(pGameServer), m_TutorialSandbox(str_comp(g_Config.m_SvGametype, "tutorial") == 0)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPlayers[i].Reset();
	if(m_TutorialSandbox)
		m_Mode = PVE_MODE_INVASION; // reuse card mechanics, never Invasion flow/progress
	else if(str_comp(g_Config.m_SvGametype, "coop") == 0)
		m_Mode = PVE_MODE_INVASION;
	else if(str_comp(g_Config.m_SvGametype, "horde") == 0)
		m_Mode = PVE_MODE_HORDE;
	else if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		m_Mode = PVE_MODE_EXTRACTION;
	else
		m_Mode = PVE_MODE_ANY;
	m_IntermissionState = PVE_INTERMISSION_NONE;
	m_EndTick = 0;
	m_LastIntermissionSyncTick = 0;
	m_PerkTargetChoices = 0;
	m_NextNonce = 1 + rand();
	m_WasWorldPaused = false;
	m_PerkAfterContract = false;
	m_aContractOptions[0] = -1;
	m_aContractOptions[1] = -1;
	m_ContractNonce = 0;
	m_UsedContracts = 0;
	m_ActiveContract = -1;
	m_ContractState = PVE_CONTRACT_STATE_NONE;
	m_ContractStartTick = 0;
	m_ContractEndTick = 0;
	m_ContractProgress = 0;
	m_ContractTarget = 0;
	m_ContractParticipants = 0;
	m_pEliteContractBoss = 0;
	mem_zero(m_apEliteContractGuards, sizeof(m_apEliteContractGuards));
	m_NumEliteContractGuards = 0;
	m_pBlackBoxRadar = 0;
	m_BlackBoxPos = vec2(0, 0);
	m_BlackBoxHoldTicks = 0;
	m_ApplyingSecondaryEffect = false;
	mem_zero(m_aTargetStatus, sizeof(m_aTargetStatus));
	m_TargetStatusCount = 0;
	m_TargetSummaryTick = -1;
	m_VulnerableTargetCount = 0;
	m_BleedingTargetCount = 0;
	m_DeathlessHordeWaves = 0;
	m_AnyStageDeath = false;
	mem_zero(m_aPendingBlasts, sizeof(m_aPendingBlasts));
	m_PendingBlastCount = 0;
	m_EnvironmentBiome = clamp(g_Config.m_SvPveBiome, 0, 1);
	m_EnvironmentPhase = PVE_ENV_PHASE_CALM;
	m_EnvironmentBossPhase = PVE_ENV_BOSS_PHASE_NONE;
	m_EnvironmentPhaseEndTick = 0;
	m_EnvironmentLevel = max(0, g_Config.m_SvMapGenLevel);

	char aError[128];
	if(!PveValidateDefinitions(aError, sizeof(aError)))
		dbg_msg("pve", "definition validation failed: %s", aError);
}

CPveDirector::~CPveDirector()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		DestroyDrone(i);
	if(InIntermission())
		m_pGameServer->m_World.m_Paused = m_WasWorldPaused;
}

bool CPveDirector::Enabled() const
{
	return g_Config.m_SvPveRoguelite && m_Mode != PVE_MODE_ANY;
}

bool CPveDirector::TogglePauseAfterIntermission()
{
	if(!InIntermission())
		return false;
	m_WasWorldPaused = !m_WasWorldPaused;
	// The intermission owns the live pause. The command changes only the state
	// that will be restored when voting finishes.
	m_pGameServer->m_World.m_Paused = true;
	return true;
}

bool CPveDirector::IsEligiblePlayer(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientID];
	return pPlayer && !pPlayer->m_IsBot && pPlayer->GetTeam() != TEAM_SPECTATORS && m_aPlayers[ClientID].m_Connected;
}

int CPveDirector::EligiblePlayerCount() const
{
	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i))
			Count++;
	return Count;
}

int CPveDirector::CurrentWeaponSpecialization(int ClientID) const
{
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	if(!pChr || !pChr->GetWeapon())
		return PVE_SPECIALIZATION_NONE;
	return WeaponSpecialization(pChr->GetWeapon()->GetWeaponSpec());
}

int CPveDirector::WeaponSpecialization(const CWeaponSpec &Weapon) const
{
	CResolvedWeaponProfile Profile;
	if(!CWeaponCatalog::TryResolve(Weapon, &Profile))
		return PVE_SPECIALIZATION_NONE;
	if(Profile.m_Combat.m_ExplosiveProjectile)
		return PVE_SPECIALIZATION_EXPLOSIVE;
	if(Profile.m_Combat.m_ElectroAmount > 0.0f || Profile.m_Combat.m_LaserWeapon)
		return PVE_SPECIALIZATION_ELECTRIC;
	const int RenderType = Profile.m_Visual.m_RenderType;
	if(Profile.m_Combat.m_FiringType == WFT_MELEE || RenderType == WRT_MELEE || RenderType == WRT_MELEESMALL ||
	   RenderType == WRT_SPIN)
		return PVE_SPECIALIZATION_MELEE;
	return PVE_SPECIALIZATION_FIREARM;
}

bool CPveDirector::CardEligible(int ClientID, int CardID) const
{
	const CPveCardDef *pDef = PveCardDef(CardID);
	if(!pDef || !IsEligiblePlayer(ClientID))
		return false;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	// Drone modules/upgrades are useless without a chassis in this run. Chassis
	// itself and Hold the Line (also a defense buff) stay available.
	if((pDef->m_Keywords & PVE_KEYWORD_DRONE) && CardID != PVE_CARD_DRONE_CHASSIS && CardID != PVE_CARD_HOLD_THE_LINE &&
	   !Run.m_aStacks[PVE_CARD_DRONE_CHASSIS])
		return false;
	return PveCardIsUnlocked(CardID, Run.m_ResearchMask) && (pDef->m_Mode == PVE_MODE_ANY || pDef->m_Mode == m_Mode) &&
		   (!pDef->m_Legendary || Run.m_LegendaryCard < 0) && Run.m_aStacks[CardID] < pDef->m_MaxStacks;
}

int CPveDirector::DrawCard(int ClientID, const bool *pExcluded, int RequiredSpecialization, bool CommonOnly) const
{
	int aaEligible[4][NUM_PVE_CARDS];
	int aCount[4] = {0, 0, 0, 0};
	for(int ID = 0; ID < NUM_PVE_CARDS; ID++)
	{
		const CPveCardDef *pDef = PveCardDef(ID);
		if(!CardEligible(ClientID, ID) || (pExcluded && pExcluded[ID]))
			continue;
		if(RequiredSpecialization != PVE_SPECIALIZATION_NONE && pDef->m_Specialization != RequiredSpecialization)
			continue;
		if(CommonOnly && pDef->m_Rarity != PVE_RARITY_COMMON)
			continue;
		aaEligible[pDef->m_Rarity][aCount[pDef->m_Rarity]++] = ID;
	}
	const int aRarityWeights[4] = {55, 30, 10, 5};
	int WeightTotal = 0;
	for(int Rarity = 0; Rarity < 4; Rarity++)
		if(aCount[Rarity] > 0)
			WeightTotal += aRarityWeights[Rarity];
	if(WeightTotal <= 0)
		return -1;
	int Pick = rand() % WeightTotal;
	for(int Rarity = 0; Rarity < 4; Rarity++)
	{
		if(aCount[Rarity] <= 0)
			continue;
		if(Pick < aRarityWeights[Rarity])
			return aaEligible[Rarity][rand() % aCount[Rarity]];
		Pick -= aRarityWeights[Rarity];
	}
	return -1;
}

void CPveDirector::GenerateChoices(int ClientID)
{
	bool aExcluded[NUM_PVE_CARDS] = {false};
	const int Specialization = CurrentWeaponSpecialization(ClientID);
	int First = DrawCard(ClientID, aExcluded, Specialization, false);
	if(First < 0)
		First = DrawCard(ClientID, aExcluded, PVE_SPECIALIZATION_NONE, false);
	if(First >= 0)
		aExcluded[First] = true;
	m_aPlayers[ClientID].m_aOffered[0] = First;
	for(int Slot = 1; Slot < 3; Slot++)
	{
		int ID = DrawCard(ClientID, aExcluded, PVE_SPECIALIZATION_NONE, false);
		if(ID >= 0)
			aExcluded[ID] = true;
		m_aPlayers[ClientID].m_aOffered[Slot] = ID;
	}
	const int aFallbacks[3] = {PVE_SUPPLY_ARMOR, PVE_SUPPLY_AMMO, PVE_SUPPLY_KITS};
	for(int Slot = 0; Slot < 3; Slot++)
		if(m_aPlayers[ClientID].m_aOffered[Slot] < 0)
			m_aPlayers[ClientID].m_aOffered[Slot] = aFallbacks[Slot];
}

void CPveDirector::SendChoice(int ClientID)
{
	if(!IsEligiblePlayer(ClientID) || !m_aPlayers[ClientID].m_ChoicePending)
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	CNetMsg_Sv_PveChoice Msg;
	Msg.m_Nonce = Run.m_ChoiceNonce;
	Msg.m_EndTick = m_EndTick;
	Msg.m_ChoiceSequence = min(99, Run.m_Choices + 1);
	Msg.m_Card0 = Run.m_aOffered[0];
	Msg.m_Card1 = Run.m_aOffered[1];
	Msg.m_Card2 = Run.m_aOffered[2];
	Msg.m_Stack0 = Msg.m_Card0 < NUM_PVE_CARDS ? Run.m_aStacks[Msg.m_Card0] : 0;
	Msg.m_Stack1 = Msg.m_Card1 < NUM_PVE_CARDS ? Run.m_aStacks[Msg.m_Card1] : 0;
	Msg.m_Stack2 = Msg.m_Card2 < NUM_PVE_CARDS ? Run.m_aStacks[Msg.m_Card2] : 0;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	if(g_Config.m_Debug)
	{
		char aBuf[192];
		str_format(aBuf,
				   sizeof(aBuf),
				   "offer client=%d sequence=%d nonce=%d cards=%d/%d/%d end=%d",
				   ClientID,
				   Msg.m_ChoiceSequence,
				   Msg.m_Nonce,
				   Msg.m_Card0,
				   Msg.m_Card1,
				   Msg.m_Card2,
				   Msg.m_EndTick);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "pve", aBuf);
	}
}

void CPveDirector::SendContractVote(int ClientID)
{
	CNetMsg_Sv_PveContractVote Msg;
	Msg.m_Nonce = m_ContractNonce;
	Msg.m_EndTick = m_EndTick;
	Msg.m_Contract0 = m_aContractOptions[0];
	Msg.m_Contract1 = m_aContractOptions[1];
	Msg.m_Votes0 = 0;
	Msg.m_Votes1 = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsEligiblePlayer(i))
			continue;
		if(m_aPlayers[i].m_ContractVote == m_aContractOptions[0])
			Msg.m_Votes0++;
		else if(m_aPlayers[i].m_ContractVote == m_aContractOptions[1])
			Msg.m_Votes1++;
	}
	if(ClientID < 0)
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	else if(IsEligiblePlayer(ClientID))
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CPveDirector::SendContractStatus(int ClientID)
{
	CNetMsg_Sv_PveContractStatus Msg;
	Msg.m_Contract = m_ActiveContract;
	Msg.m_State = m_ContractState;
	Msg.m_Progress = m_ContractProgress;
	Msg.m_Target = m_ContractTarget;
	Msg.m_EndTick = m_ContractEndTick;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CPveDirector::SendProgress(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	CNetMsg_Sv_PveProgress Msg;
	Msg.m_Version = 2;
	Msg.m_ResearchPoints = clamp(Run.m_ResearchPoints, 0, 999);
	Msg.m_ResearchMask0 = (int)(Run.m_ResearchMask.m_aWords[0] & 0xffffffffULL);
	Msg.m_ResearchMask1 = (int)((Run.m_ResearchMask.m_aWords[0] >> 32) & 0xffffffffULL);
	Msg.m_ResearchMask2 = (int)(Run.m_ResearchMask.m_aWords[1] & 0xffffffffULL);
	Msg.m_ResearchMask3 = (int)((Run.m_ResearchMask.m_aWords[1] >> 32) & 0xffffffffULL);
	Msg.m_HighestInvasion = clamp(Run.m_HighestInvasion, 0, 9999);
	Msg.m_PreferredCheckpoint = max(1, Run.m_PreferredCheckpoint);
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

int CPveDirector::EnvironmentBrightness() const
{
	if(m_EnvironmentBiome != PVE_BIOME_BLUE_PLANET)
		return 255;
	switch(m_EnvironmentPhase)
	{
		case PVE_ENV_PHASE_WARNING: return 128;
		case PVE_ENV_PHASE_DARK: return 0;
		case PVE_ENV_PHASE_RECOVERY: return 192;
		default: return 255;
	}
}

void CPveDirector::UpdateEnvironment()
{
	const int Now = m_pGameServer->Server()->Tick();
	const int NewLevel = max(0, g_Config.m_SvMapGenLevel);
	const int NewBiome = clamp(g_Config.m_SvPveBiome, 0, 1);
	if(!Enabled() || m_TutorialSandbox || NewBiome != PVE_BIOME_BLUE_PLANET)
	{
		m_EnvironmentBiome = NewBiome;
		m_EnvironmentLevel = NewLevel;
		m_EnvironmentPhase = PVE_ENV_PHASE_CALM;
		m_EnvironmentBossPhase = PVE_ENV_BOSS_PHASE_NONE;
		m_EnvironmentPhaseEndTick = Now + m_pGameServer->Server()->TickSpeed();
		return;
	}
	if(m_EnvironmentBiome != NewBiome || m_EnvironmentLevel != NewLevel || m_EnvironmentPhaseEndTick <= 0)
	{
		m_EnvironmentBiome = NewBiome;
		m_EnvironmentLevel = NewLevel;
		m_EnvironmentPhase = PVE_ENV_PHASE_CALM;
		m_EnvironmentPhaseEndTick = Now + m_pGameServer->Server()->TickSpeed() * 12;
	}
	static const int s_aPhaseDuration[] = {12, 7, 9, 8};
	while(Now >= m_EnvironmentPhaseEndTick)
	{
		m_EnvironmentPhase = (m_EnvironmentPhase + 1) % 4;
		m_EnvironmentPhaseEndTick += m_pGameServer->Server()->TickSpeed() * s_aPhaseDuration[m_EnvironmentPhase];
	}
	if(m_EnvironmentLevel >= 30)
	{
		const int CycleTick = max(0, Now - (m_EnvironmentPhaseEndTick - m_pGameServer->Server()->TickSpeed() *
			s_aPhaseDuration[m_EnvironmentPhase]));
		const int Cycle = max(0, m_pGameServer->Server()->TickSpeed() * 28);
		m_EnvironmentBossPhase = CycleTick < Cycle / 3 ? PVE_ENV_BOSS_PHASE_ONE :
			(CycleTick < Cycle * 2 / 3 ? PVE_ENV_BOSS_PHASE_TWO : PVE_ENV_BOSS_PHASE_THREE);
	}
	else
		m_EnvironmentBossPhase = PVE_ENV_BOSS_PHASE_NONE;
}

void CPveDirector::Snap(int SnappingClient)
{
	if(!Enabled())
		return;
	CNetObj_PveEnvironmentStatus *pEnvironment = static_cast<CNetObj_PveEnvironmentStatus *>(
		m_pGameServer->Server()->SnapNewItem(NETOBJTYPE_PVEENVIRONMENTSTATUS, 0, sizeof(CNetObj_PveEnvironmentStatus)));
	if(pEnvironment)
	{
		pEnvironment->m_Biome = m_EnvironmentBiome;
		pEnvironment->m_Phase = m_EnvironmentPhase;
		pEnvironment->m_PhaseEndTick = m_EnvironmentPhaseEndTick;
		pEnvironment->m_BossPhase = m_EnvironmentBossPhase;
		pEnvironment->m_Level = m_EnvironmentLevel;
	}
	(void)SnappingClient;
}

void CPveDirector::SendValidation(int ClientID, int Code)
{
	CNetMsg_Sv_PveValidation Msg;
	Msg.m_Code = Code;
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	if(Code == PVE_VALIDATION_EXPIRED)
		m_pGameServer->SendChatTarget(ClientID, "PvE selection rejected: expired choice");
	else if(Code == PVE_VALIDATION_DUPLICATE)
		m_pGameServer->SendChatTarget(ClientID, "PvE selection rejected: duplicate choice");
	else if(Code == PVE_VALIDATION_RANGE)
		m_pGameServer->SendChatTarget(ClientID, "PvE selection rejected: out-of-range value");
	else if(Code == PVE_VALIDATION_NOT_OFFERED)
		m_pGameServer->SendChatTarget(ClientID, "PvE selection rejected: choice was not offered");
	else
		m_pGameServer->SendChatTarget(ClientID, "PvE selection rejected: invalid progression");
}

void CPveDirector::BeginContractVote(bool PerkAfterContract)
{
	int aPool[NUM_PVE_CONTRACTS];
	int Count = 0;
	for(int ID = 0; ID < NUM_PVE_CONTRACTS; ID++)
		if(!(m_UsedContracts & (1 << ID)) && PveContractAvailableInMode(ID, m_Mode))
			aPool[Count++] = ID;
	if(Count < 2)
	{
		if(PerkAfterContract)
			BeginPerkChoice();
		else
			FinishIntermission();
		return;
	}
	const int Pick0 = rand() % Count;
	m_aContractOptions[0] = aPool[Pick0];
	aPool[Pick0] = aPool[--Count];
	m_aContractOptions[1] = aPool[rand() % Count];
	m_UsedContracts |= (1 << m_aContractOptions[0]) | (1 << m_aContractOptions[1]);
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i))
		{
			CPlayerData *pData = m_pGameServer->Server()->GetPlayerData(i, m_pGameServer->m_apPlayers[i]->GetColorID());
			if(pData && m_Mode != PVE_MODE_ANY)
				pData->m_PveUsedContracts = m_UsedContracts;
		}
	m_ContractNonce = ++m_NextNonce;
	m_PerkAfterContract = PerkAfterContract;
	m_IntermissionState = PVE_INTERMISSION_CONTRACT;
	m_EndTick =
		m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * g_Config.m_SvPveContractVoteTime;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aPlayers[i].m_ContractVote = -1;
		m_aPlayers[i].m_LastContractNonce = 0;
	}
	SendContractVote();
	m_LastIntermissionSyncTick = m_pGameServer->Server()->Tick();
}

void CPveDirector::BeginPerkChoice()
{
	m_IntermissionState = PVE_INTERMISSION_PERK;
	m_EndTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * g_Config.m_SvPveChoiceTime;
	m_PerkTargetChoices = 1;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i))
			m_PerkTargetChoices = max(m_PerkTargetChoices, m_aPlayers[i].m_Choices + 1);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsEligiblePlayer(i))
			continue;
		CPlayerRun &Run = m_aPlayers[i];
		Run.m_ChoicePending = Run.m_Choices < m_PerkTargetChoices;
		if(!Run.m_ChoicePending)
			continue;
		Run.m_ChoiceNonce = ++m_NextNonce;
		Run.m_LastChoiceNonce = 0;
		GenerateChoices(i);
		SendChoice(i);
	}
	m_LastIntermissionSyncTick = m_pGameServer->Server()->Tick();
}

void CPveDirector::FinishContractVote()
{
	int aVotes[2] = {0, 0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsEligiblePlayer(i))
			continue;
		if(m_aPlayers[i].m_ContractVote == m_aContractOptions[0])
			aVotes[0]++;
		else if(m_aPlayers[i].m_ContractVote == m_aContractOptions[1])
			aVotes[1]++;
	}
	const int Winner = aVotes[0] == aVotes[1] ? rand() % 2 : (aVotes[1] > aVotes[0]);
	m_ActiveContract = m_aContractOptions[Winner];
	m_ContractState = PVE_CONTRACT_STATE_ACTIVE;
	m_ContractStartTick = 0;
	m_ContractProgress = 0;
	m_ContractTarget = m_Mode == PVE_MODE_HORDE ? 4 : 1;
	m_ContractEndTick = 0;
	m_ContractParticipants = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pGameServer->m_apPlayers[i] && !m_pGameServer->m_apPlayers[i]->m_IsBot)
		{
			CPlayerData *pData = m_pGameServer->Server()->GetPlayerData(i, m_pGameServer->m_apPlayers[i]->GetColorID());
			if(pData)
			{
				pData->m_PveContractParticipant = IsEligiblePlayer(i);
				pData->m_PveContractNonce = IsEligiblePlayer(i) ? m_ContractNonce : 0;
			}
		}
		if(IsEligiblePlayer(i))
			m_ContractParticipants |= 1ULL << i;
	}
	if(m_ActiveContract == PVE_CONTRACT_BLACK_BOX)
	{
		m_ContractTarget = 3;
		m_BlackBoxPos = m_pGameServer->GetFarHumanSpawnPos(true);
		m_BlackBoxHoldTicks = 0;
		m_pBlackBoxRadar = new CServerRadar(&m_pGameServer->m_World, RADAR_REACTOR);
		m_pBlackBoxRadar->Activate(m_BlackBoxPos);
	}
	SendContractStatus();
	m_pGameServer->SendChatTarget(-1, "Team contract started");
	if(m_PerkAfterContract)
		BeginPerkChoice();
	else
		FinishIntermission();
}

void CPveDirector::FinishIntermission()
{
	m_IntermissionState = PVE_INTERMISSION_NONE;
	m_EndTick = 0;
	m_LastIntermissionSyncTick = 0;
	m_PerkTargetChoices = 0;
	m_PerkAfterContract = false;
	m_pGameServer->m_World.m_Paused = m_WasWorldPaused;
	if(m_ContractState == PVE_CONTRACT_STATE_ACTIVE && m_ContractStartTick == 0)
	{
		m_ContractStartTick = m_pGameServer->Server()->Tick();
		if(m_ActiveContract == PVE_CONTRACT_SPEED_CLEAR)
			m_ContractEndTick = m_ContractStartTick + m_pGameServer->Server()->TickSpeed() * 150;
		SendContractStatus();
	}
}

bool CPveDirector::AllChoicesComplete() const
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i) && m_aPlayers[i].m_ChoicePending)
			return false;
	return true;
}

bool CPveDirector::AllContractVotesComplete() const
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i) && m_aPlayers[i].m_ContractVote < 0)
			return false;
	return true;
}

void CPveDirector::ApplyChoice(int ClientID, int CardID, bool Catchup)
{
	(void)Catchup;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(CardID >= 0 && CardID < NUM_PVE_CARDS)
	{
		const CPveCardDef *pDef = PveCardDef(CardID);
		if(pDef && (!pDef->m_Legendary || Run.m_LegendaryCard < 0) && Run.m_aStacks[CardID] < pDef->m_MaxStacks)
		{
			Run.m_aStacks[CardID]++;
			if(pDef->m_Legendary)
				Run.m_LegendaryCard = CardID;
			// Modules only equip when a chassis is already in this run.
			if(Run.m_DroneModule == PVE_DRONE_NONE && Run.m_aStacks[PVE_CARD_DRONE_CHASSIS])
			{
				if(CardID == PVE_CARD_ASSAULT_MODULE)
					Run.m_DroneModule = PVE_DRONE_ASSAULT;
				else if(CardID == PVE_CARD_GUARDIAN_MODULE)
					Run.m_DroneModule = PVE_DRONE_GUARDIAN;
				else if(CardID == PVE_CARD_REPAIR_MODULE)
					Run.m_DroneModule = PVE_DRONE_REPAIR;
			}
		}
		CNetMsg_Sv_PvePerk Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Card = CardID;
		Msg.m_Stacks = Run.m_aStacks[CardID];
		Msg.m_Choices = min(99, Run.m_Choices + 1);
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr && pChr->IsAlive())
		{
			if(CardID == PVE_SUPPLY_ARMOR)
				pChr->SetArmor(min(100, pChr->GetArmor() + 25));
			else if(CardID == PVE_SUPPLY_AMMO)
			{
				for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
					if(pChr->GetWeapon(Slot))
						pChr->GetWeapon(Slot)->IncreaseAmmo(999);
			}
			else if(CardID == PVE_SUPPLY_KITS)
				pChr->AddKits(5);
		}
		else
		{
			if(CardID == PVE_SUPPLY_ARMOR)
				Run.m_PendingArmor += 25;
			else if(CardID == PVE_SUPPLY_AMMO)
				Run.m_PendingAmmo = true;
			else if(CardID == PVE_SUPPLY_KITS)
				Run.m_PendingKits += 5;
		}
		CNetMsg_Sv_PvePerk Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Card = CardID;
		Msg.m_Stacks = 0;
		Msg.m_Choices = min(99, Run.m_Choices + 1);
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	Run.m_Choices++;
	SendBuildState(ClientID, true);
	if(m_Mode != PVE_MODE_ANY && IsEligiblePlayer(ClientID))
	{
		CPlayerData *pData =
			m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
		if(pData)
		{
			for(int i = 0; i < NUM_PVE_CARDS; i++)
				pData->m_aPvePerks[i] = Run.m_aStacks[i];
			pData->m_PveChoices = Run.m_Choices;
			pData->m_PveRunMode = m_Mode;
			pData->m_PveUsedContracts = m_UsedContracts;
			pData->m_PveInvasionFloors = Run.m_InvasionFloorsCompleted;
			pData->m_PvePendingArmor = Run.m_PendingArmor;
			pData->m_PvePendingKits = Run.m_PendingKits;
			pData->m_PvePendingAmmo = Run.m_PendingAmmo;
		}
	}
}

void CPveDirector::GrantCatchup(int ClientID)
{
	if(!IsEligiblePlayer(ClientID))
		return;
	int MaxChoices = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i) && i != ClientID)
			MaxChoices = max(MaxChoices, m_aPlayers[i].m_Choices);
	CPlayerRun &Run = m_aPlayers[ClientID];
	const int TargetChoices = m_IntermissionState == PVE_INTERMISSION_PERK ? m_PerkTargetChoices : MaxChoices;
	const int AutoChoices = max(0, TargetChoices - Run.m_Choices - 1);
	const int aFallbacks[3] = {PVE_SUPPLY_ARMOR, PVE_SUPPLY_AMMO, PVE_SUPPLY_KITS};
	for(int n = 0; n < AutoChoices; n++)
	{
		bool aExcluded[NUM_PVE_CARDS] = {false};
		int CardID = DrawCard(ClientID, aExcluded, CurrentWeaponSpecialization(ClientID), true);
		if(CardID < 0)
			CardID = DrawCard(ClientID, aExcluded, PVE_SPECIALIZATION_NONE, true);
		if(CardID < 0)
			CardID = aFallbacks[n % 3];
		ApplyChoice(ClientID, CardID, true);
	}
	if(m_IntermissionState == PVE_INTERMISSION_PERK && Run.m_Choices < m_PerkTargetChoices &&
	   (AutoChoices > 0 || !Run.m_ChoicePending))
	{
		Run.m_ChoicePending = true;
		Run.m_ChoiceNonce = ++m_NextNonce;
		Run.m_LastChoiceNonce = 0;
		GenerateChoices(ClientID);
		SendChoice(ClientID);
	}
}

void CPveDirector::StartIntermission(bool ContractVote, bool PerkChoice)
{
	if(!Enabled() || InIntermission() || EligiblePlayerCount() <= 0 || (!ContractVote && !PerkChoice))
		return;
	ContractVote = ContractVote && Enabled() && !m_TutorialSandbox;
	PerkChoice = PerkChoice && Enabled();
	if(!ContractVote && !PerkChoice)
		return;
	m_WasWorldPaused = m_pGameServer->m_World.m_Paused;
	if(!m_TutorialSandbox)
		m_pGameServer->m_World.m_Paused = true;
	if(ContractVote && g_Config.m_SvPveContracts)
		BeginContractVote(PerkChoice);
	else if(PerkChoice)
		BeginPerkChoice();
	else
		FinishIntermission();
}

void CPveDirector::Tick()
{
	UpdateEnvironment();
	if(!Enabled())
	{
		const bool RogueliteIntermission =
			m_IntermissionState == PVE_INTERMISSION_CONTRACT || m_IntermissionState == PVE_INTERMISSION_PERK;
		const bool HadState = RogueliteIntermission || m_ContractState != PVE_CONTRACT_STATE_NONE || m_pBlackBoxRadar;
		if(RogueliteIntermission)
		{
			for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
				if(IsEligiblePlayer(ClientID) && m_aPlayers[ClientID].m_ChoicePending)
				{
					CNetMsg_Sv_PvePerk Msg;
					Msg.m_ClientID = ClientID;
					Msg.m_Card = PVE_SUPPLY_ARMOR;
					Msg.m_Stacks = 0;
					Msg.m_Choices = min(99, m_aPlayers[ClientID].m_Choices);
					m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
					m_aPlayers[ClientID].m_ChoicePending = false;
				}
			m_pGameServer->m_World.m_Paused = m_WasWorldPaused;
		}
		if(m_pBlackBoxRadar)
		{
			m_pGameServer->m_World.DestroyEntity(m_pBlackBoxRadar);
			m_pBlackBoxRadar = 0;
		}
		if(RogueliteIntermission)
			m_IntermissionState = PVE_INTERMISSION_NONE;
		m_LastIntermissionSyncTick = 0;
		m_PerkTargetChoices = 0;
		m_ActiveContract = -1;
		m_ContractState = PVE_CONTRACT_STATE_NONE;
		m_ContractProgress = 0;
		m_ContractTarget = 0;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			DestroyDrone(ClientID);
		mem_zero(m_aTargetStatus, sizeof(m_aTargetStatus));
		m_TargetStatusCount = 0;
		m_TargetSummaryTick = -1;
		m_VulnerableTargetCount = 0;
		m_BleedingTargetCount = 0;
		if(HadState)
			SendContractStatus();
		return;
	}
	if(Enabled() && !InIntermission() && m_ContractState == PVE_CONTRACT_STATE_ACTIVE &&
	   m_ActiveContract == PVE_CONTRACT_BLACK_BOX)
		TickBlackBox();
	if(Enabled())
	{
		TickTargetStatuses();
		TickPendingBlasts();
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(IsEligiblePlayer(ClientID))
				TickPlayerState(ClientID);
	}
	if(!InIntermission())
		return;
	const bool Expired = m_pGameServer->Server()->Tick() >= m_EndTick;
	// Map loading and component resets can happen after the first reliable
	// offer was queued. Reassert the authoritative intermission state once per
	// second so a ready client always reconstructs the overlay.
	if(!Expired && m_pGameServer->Server()->Tick() >= m_LastIntermissionSyncTick + m_pGameServer->Server()->TickSpeed())
	{
		if(m_IntermissionState == PVE_INTERMISSION_CONTRACT)
			SendContractVote();
		else if(m_IntermissionState == PVE_INTERMISSION_PERK)
			for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
				if(IsEligiblePlayer(ClientID) && m_aPlayers[ClientID].m_ChoicePending)
					SendChoice(ClientID);
		m_LastIntermissionSyncTick = m_pGameServer->Server()->Tick();
	}
	if(m_IntermissionState == PVE_INTERMISSION_CONTRACT && (Expired || AllContractVotesComplete()))
	{
		if(Expired)
			m_pGameServer->SendChatTarget(-1, "Contract vote timed out; a valid option was selected");
		FinishContractVote();
		return;
	}
	if(m_IntermissionState == PVE_INTERMISSION_PERK && Expired)
	{
		m_pGameServer->SendChatTarget(-1, "Perk choice timed out; displayed options were selected at random");
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!IsEligiblePlayer(i) || !m_aPlayers[i].m_ChoicePending)
				continue;
			while(m_aPlayers[i].m_Choices < m_PerkTargetChoices)
			{
				const int Slot = rand() % 3;
				ApplyChoice(i, m_aPlayers[i].m_aOffered[Slot]);
				if(m_aPlayers[i].m_Choices < m_PerkTargetChoices)
				{
					m_aPlayers[i].m_ChoiceNonce = ++m_NextNonce;
					GenerateChoices(i);
					SendChoice(i);
				}
			}
			m_aPlayers[i].m_ChoicePending = false;
		}
	}
	if(m_IntermissionState == PVE_INTERMISSION_PERK && AllChoicesComplete())
		FinishIntermission();
}

void CPveDirector::OnClientEnter(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_pGameServer->IsBot(ClientID))
		return;
	CPlayerData *pData =
		m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
	if(m_Mode == PVE_MODE_ANY || !Enabled())
		return;
	DestroyDrone(ClientID);
	m_aPlayers[ClientID].Reset();
	m_aPlayers[ClientID].m_Connected = true;
	if(!m_TutorialSandbox && Enabled() && pData && pData->m_PveRunMode == m_Mode)
	{
		for(int i = 0; i < NUM_PVE_CARDS; i++)
			m_aPlayers[ClientID].m_aStacks[i] = pData->m_aPvePerks[i];
		m_aPlayers[ClientID].m_Choices = pData->m_PveChoices;
		m_aPlayers[ClientID].m_InvasionFloorsCompleted = pData->m_PveInvasionFloors;
		m_aPlayers[ClientID].m_StageSuppliesApplied = pData->m_PveStageSuppliesApplied;
		m_aPlayers[ClientID].m_LastStandUsed = pData->m_PveLastStandUsed;
		m_aPlayers[ClientID].m_EmergencyPlatingUsed = pData->m_PveEmergencyPlatingUsed;
		m_aPlayers[ClientID].m_PendingArmor = pData->m_PvePendingArmor;
		m_aPlayers[ClientID].m_PendingKits = pData->m_PvePendingKits;
		m_aPlayers[ClientID].m_PendingAmmo = pData->m_PvePendingAmmo;
		m_aPlayers[ClientID].m_LegendaryCard = pData->m_PveLegendaryCard;
		for(int i = 0; i < 4; i++)
			m_aPlayers[ClientID].m_aWeaponResources[i] = clamp(pData->m_aPveWeaponResources[i], 0, 10);
		m_aPlayers[ClientID].m_Barrier = clamp(pData->m_PveBarrier, 0, 30);
		m_aPlayers[ClientID].m_DroneModule = clamp(pData->m_PveDroneModule, (int)PVE_DRONE_NONE, (int)PVE_DRONE_REPAIR);
		m_aPlayers[ClientID].m_DroneSwitchReadyTick = pData->m_PveDroneSwitchReadyTick;
		m_aPlayers[ClientID].m_DeathlessFloors = clamp(pData->m_PveDeathlessFloors, 0, 5);
		if(pData->m_PveContractParticipant && pData->m_PveContractNonce == m_ContractNonce &&
		   m_ContractState == PVE_CONTRACT_STATE_ACTIVE)
			m_ContractParticipants |= 1ULL << ClientID;
		else if(m_ContractState != PVE_CONTRACT_STATE_ACTIVE)
		{
			pData->m_PveContractParticipant = false;
			pData->m_PveContractNonce = 0;
		}
		m_UsedContracts |= pData->m_PveUsedContracts;
	}
	else if(!m_TutorialSandbox && Enabled() && pData)
		pData->Reset();
	if(!m_TutorialSandbox && Enabled() && pData)
		pData->m_PveRunMode = m_Mode;
	if(Enabled() && m_pGameServer->GetPlayerChar(ClientID))
		OnPlayerSpawn(ClientID);
	if(Enabled())
		SendProgress(ClientID);
	if(Enabled() && m_ContractState != PVE_CONTRACT_STATE_NONE)
		SendContractStatus(ClientID);
	if(m_IntermissionState == PVE_INTERMISSION_CONTRACT)
		SendContractVote(ClientID);
}

void CPveDirector::OnClientDrop(int ClientID)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		// Status effects keep non-owning entity pointers so they can apply their
		// periodic damage. Bots are removed immediately after OnClientDrop; clear
		// every reference while their character is still alive as a C++ object.
		// Otherwise an environmental kill (which has no eligible player source)
		// can leave a status pointing at freed character memory for the next tick.
		CCharacter *pCharacter = m_pGameServer->GetPlayerChar(ClientID);
		if(pCharacter)
		{
			ClearTargetStatus(pCharacter);
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(m_aPlayers[i].m_pDroneTarget == pCharacter)
					m_aPlayers[i].m_pDroneTarget = 0;
		}
		SendBuildState(ClientID, true);
		DestroyDrone(ClientID);
		m_aPlayers[ClientID].m_Connected = false;
	}
}

void CPveDirector::OnProgress(int ClientID,
							  int Version,
							  int Points,
							  int Mask0,
							  int Mask1,
							  int Mask2,
							  int Mask3,
							  int HighestInvasion,
							  int PreferredCheckpoint)
{
	if(!IsEligiblePlayer(ClientID))
		return;
	if(g_Config.m_SvTutorialMode)
	{
		Version = 2;
		Points = 99;
		Mask0 = Mask1 = Mask2 = Mask3 = 0;
		HighestInvasion = 0;
		PreferredCheckpoint = 1;
	}
	CPlayerRun &Run = m_aPlayers[ClientID];
	const unsigned long long Low = (unsigned int)Mask0 | ((unsigned long long)(unsigned int)Mask1 << 32);
	const unsigned long long High =
		Version >= 2 ? ((unsigned int)Mask2 | ((unsigned long long)(unsigned int)Mask3 << 32)) : 0;
	Run.m_ResearchMask = PveSanitizeResearchMask(CPveResearchMask(Low, High));
	Run.m_ResearchPoints = clamp(Points, 0, 999);
	Run.m_HighestInvasion = clamp(HighestInvasion, 0, 9999);
	const int MaxCheckpoint = Run.m_HighestInvasion >= 10 ? (Run.m_HighestInvasion / 10) * 10 + 1 : 1;
	Run.m_PreferredCheckpoint = clamp(PreferredCheckpoint, 1, MaxCheckpoint);
	if((Run.m_PreferredCheckpoint - 1) % 10 != 0)
		Run.m_PreferredCheckpoint = 1;
	Run.m_ProgressSynced = Version == 1 || Version == 2;
	if(g_Config.m_Debug)
	{
		char aBuf[192];
		str_format(aBuf,
				   sizeof(aBuf),
				   "progress client=%d version=%d points=%d mask=%016llX%016llX checkpoint=%d",
				   ClientID,
				   Version,
				   Run.m_ResearchPoints,
				   Run.m_ResearchMask.m_aWords[1],
				   Run.m_ResearchMask.m_aWords[0],
				   Run.m_PreferredCheckpoint);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "pve", aBuf);
	}
	GrantCatchup(ClientID);
	SendProgress(ClientID);
	SendBuildState(ClientID, true);
	for(int CardID = 0; CardID < NUM_PVE_CARDS; CardID++)
		if(Run.m_aStacks[CardID] > 0)
		{
			CNetMsg_Sv_PvePerk Msg;
			Msg.m_ClientID = ClientID;
			Msg.m_Card = CardID;
			Msg.m_Stacks = Run.m_aStacks[CardID];
			Msg.m_Choices = min(99, Run.m_Choices);
			m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		}
}

void CPveDirector::OnResearchBuy(int ClientID, int Nonce, int CardID)
{
	if(!IsEligiblePlayer(ClientID) || CardID < 0 || CardID >= NUM_PVE_CARDS || PveCardIsBase(CardID))
	{
		SendValidation(ClientID, PVE_VALIDATION_RANGE);
		return;
	}
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Nonce <= Run.m_LastResearchNonce)
	{
		SendValidation(ClientID, PVE_VALIDATION_DUPLICATE);
		return;
	}
	const CPveCardDef *pDef = PveCardDef(CardID);
	if(!pDef || PveCardIsUnlocked(CardID, Run.m_ResearchMask) || Run.m_ResearchPoints < pDef->m_ResearchCost ||
	   !Run.m_ResearchMask.PrerequisitesMet(CardID))
	{
		SendValidation(ClientID, PVE_VALIDATION_PROGRESS);
		return;
	}
	(void)Nonce;
	Run.m_ResearchPoints -= pDef->m_ResearchCost;
	Run.m_ResearchMask.Set(CardID);
	Run.m_LastResearchNonce = Nonce;
	if(g_Config.m_Debug)
	{
		char aBuf[160];
		str_format(aBuf,
				   sizeof(aBuf),
				   "research purchase client=%d card=%d points=%d mask=%016llX%016llX",
				   ClientID,
				   CardID,
				   Run.m_ResearchPoints,
				   Run.m_ResearchMask.m_aWords[1],
				   Run.m_ResearchMask.m_aWords[0]);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "pve", aBuf);
	}
	SendProgress(ClientID);
	if(m_pGameServer->m_pTutorialDirector)
		m_pGameServer->m_pTutorialDirector->OnGameplayProgress(ClientID, TUTORIAL_EVENT_RESEARCH);
}

void CPveDirector::OnChoice(int ClientID, int Nonce, int CardID)
{
	if(!IsEligiblePlayer(ClientID) || CardID < 0 || CardID >= NUM_PVE_CHOICES)
	{
		SendValidation(ClientID, PVE_VALIDATION_RANGE);
		return;
	}
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(!Run.m_ChoicePending)
	{
		SendValidation(ClientID, Nonce == Run.m_LastChoiceNonce ? PVE_VALIDATION_DUPLICATE : PVE_VALIDATION_EXPIRED);
		return;
	}
	if(Nonce != Run.m_ChoiceNonce || m_IntermissionState != PVE_INTERMISSION_PERK ||
	   m_pGameServer->Server()->Tick() > m_EndTick)
	{
		SendValidation(ClientID, PVE_VALIDATION_EXPIRED);
		return;
	}
	bool Offered = false;
	for(int i = 0; i < 3; i++)
		Offered |= Run.m_aOffered[i] == CardID;
	if(!Offered)
	{
		SendValidation(ClientID, PVE_VALIDATION_NOT_OFFERED);
		return;
	}
	ApplyChoice(ClientID, CardID);
	if(m_TutorialSandbox)
		GrantTutorialBuildLoadout(ClientID);
	if(m_pGameServer->m_pTutorialDirector)
		m_pGameServer->m_pTutorialDirector->OnGameplayProgress(ClientID, TUTORIAL_EVENT_PERK);
	Run.m_LastChoiceNonce = Nonce;
	if(Run.m_Choices < m_PerkTargetChoices)
	{
		Run.m_ChoicePending = true;
		Run.m_ChoiceNonce = ++m_NextNonce;
		GenerateChoices(ClientID);
		SendChoice(ClientID);
	}
	else
		Run.m_ChoicePending = false;
	if(AllChoicesComplete())
		FinishIntermission();
}

void CPveDirector::GrantTutorialBuildLoadout(int ClientID)
{
	if(!m_TutorialSandbox || !IsEligiblePlayer(ClientID))
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	const int aCards[] = {PVE_CARD_DRONE_CHASSIS,
						  PVE_CARD_SERVO_LINK,
						  PVE_CARD_ASSAULT_MODULE,
						  PVE_CARD_GUARDIAN_MODULE,
						  PVE_CARD_REPAIR_MODULE};
	for(unsigned i = 0; i < sizeof(aCards) / sizeof(aCards[0]); i++)
	{
		const int CardID = aCards[i];
		Run.m_aStacks[CardID] = max(1, Run.m_aStacks[CardID]);
		CNetMsg_Sv_PvePerk Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Card = CardID;
		Msg.m_Stacks = Run.m_aStacks[CardID];
		Msg.m_Choices = min(99, Run.m_Choices + 1);
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	Run.m_DroneModule = PVE_DRONE_ASSAULT;
	Run.m_DroneSwitchReadyTick = 0;
	SendBuildState(ClientID, true);
	dbg_msg("tutorial", "granted sandbox drone chassis and three modules to player %d", ClientID);
}

void CPveDirector::OnContractVote(int ClientID, int Nonce, int ContractID)
{
	if(!IsEligiblePlayer(ClientID) || ContractID < 0 || ContractID >= NUM_PVE_CONTRACTS)
	{
		SendValidation(ClientID, PVE_VALIDATION_RANGE);
		return;
	}
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Nonce != m_ContractNonce || m_IntermissionState != PVE_INTERMISSION_CONTRACT ||
	   m_pGameServer->Server()->Tick() > m_EndTick)
	{
		SendValidation(ClientID, Nonce == Run.m_LastContractNonce ? PVE_VALIDATION_DUPLICATE : PVE_VALIDATION_EXPIRED);
		return;
	}
	if(ContractID != m_aContractOptions[0] && ContractID != m_aContractOptions[1])
	{
		SendValidation(ClientID, PVE_VALIDATION_NOT_OFFERED);
		return;
	}
	if(Run.m_ContractVote >= 0)
	{
		SendValidation(ClientID, PVE_VALIDATION_DUPLICATE);
		return;
	}
	Run.m_ContractVote = ContractID;
	Run.m_LastContractNonce = Nonce;
	SendContractVote();
	if(AllContractVotesComplete())
		FinishContractVote();
}

void CPveDirector::ApplyStageSupplies(int ClientID)
{
	if(!IsEligiblePlayer(ClientID))
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Run.m_StageSuppliesApplied)
		return;
	if(ActiveContract() == PVE_CONTRACT_SEALED_SUPPLIES)
	{
		Run.m_StageSuppliesApplied = true;
		return;
	}
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	if(!pChr || !pChr->IsAlive())
		return;
	Run.m_StageSuppliesApplied = true;
	CPlayerData *pData =
		m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
	if(pData)
		pData->m_PveStageSuppliesApplied = true;
	const int SupplyBonus = m_Mode == PVE_MODE_INVASION ? Run.m_aStacks[PVE_CARD_DELVER] : 0;
	const float SupplyScale = 1.0f + SupplyBonus * 0.25f;
	pChr->IncreaseArmor((int)(Run.m_aStacks[PVE_CARD_REINFORCED_PLATES] * 8 * SupplyScale));
	pChr->AddKits((int)(Run.m_aStacks[PVE_CARD_FIELD_SUPPLIES] * 2 * SupplyScale + 0.5f));
	const float AmmoFraction = min(1.0f, Run.m_aStacks[PVE_CARD_AMMO_RESERVE] * 0.15f * SupplyScale);
	for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
		if(pChr->GetWeapon(Slot))
			pChr->GetWeapon(Slot)->IncreaseAmmo((int)(pChr->GetWeapon(Slot)->m_MaxAmmo * AmmoFraction));
	if(m_Mode == PVE_MODE_HORDE && Run.m_aStacks[PVE_CARD_BREATHING_ROOM])
	{
		const float RestoreFraction = min(0.60f, Run.m_aStacks[PVE_CARD_BREATHING_ROOM] * 0.20f);
		pChr->IncreaseHealth((int)(pChr->m_MaxHealth * RestoreFraction));
		for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
			if(pChr->GetWeapon(Slot))
				pChr->GetWeapon(Slot)->IncreaseAmmo((int)(pChr->GetWeapon(Slot)->m_MaxAmmo * RestoreFraction));
	}
}

void CPveDirector::OnStageStart()
{
	if(!Enabled())
		return;
	mem_zero(m_aTargetStatus, sizeof(m_aTargetStatus));
	m_TargetStatusCount = 0;
	m_TargetSummaryTick = -1;
	m_VulnerableTargetCount = 0;
	m_BleedingTargetCount = 0;
	m_AnyStageDeath = false;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(!IsEligiblePlayer(ClientID))
			continue;
		CPlayerRun &Run = m_aPlayers[ClientID];
		Run.m_LastStandUsed = false;
		Run.m_EmergencyPlatingUsed = false;
		Run.m_StageSuppliesApplied = false;
		Run.m_StageKills = 0;
		Run.m_SecondWindTriggers = 0;
		Run.m_SalvageKits = 0;
		Run.m_AegisLoopUsed = false;
		Run.m_ObjectiveCacheUsed = false;
		Run.m_CleanExitUsed = false;
		Run.m_DiedThisStage = false;
		CPlayerData *pData =
			m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
		if(pData)
		{
			pData->m_PveLastStandUsed = false;
			pData->m_PveEmergencyPlatingUsed = false;
			pData->m_PveStageSuppliesApplied = false;
		}
		ApplyStageSupplies(ClientID);
		AddBarrier(ClientID, Run.m_aStacks[PVE_CARD_BARRIER_CELL] * 6);
	}
	if(m_Mode == PVE_MODE_HORDE)
	{
		int FortifiedStacks = 0;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(IsEligiblePlayer(ClientID))
				FortifiedStacks = max(FortifiedStacks, m_aPlayers[ClientID].m_aStacks[PVE_CARD_FORTIFIED_CYCLE]);
		if(FortifiedStacks > 0)
			for(CBuilding *pBuilding = (CBuilding *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING);
				pBuilding;
				pBuilding = (CBuilding *)pBuilding->TypeNext())
				if(pBuilding->m_MaxLife > 0 && pBuilding->m_Life > 0)
					pBuilding->Repair(max(1, pBuilding->m_MaxLife * FortifiedStacks * 5 / 100));
	}
}

void CPveDirector::OnPlayerSpawn(int ClientID)
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return;
	ApplyStageSupplies(ClientID);
	CPlayerRun &Run = m_aPlayers[ClientID];
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	if(!pChr)
		return;
	if(Run.m_PendingArmor > 0)
		pChr->SetArmor(min(100, pChr->GetArmor() + Run.m_PendingArmor));
	if(Run.m_PendingKits > 0)
		pChr->AddKits(Run.m_PendingKits);
	if(Run.m_PendingAmmo)
		for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
			if(pChr->GetWeapon(Slot))
				pChr->GetWeapon(Slot)->IncreaseAmmo(999);
	Run.m_PendingArmor = 0;
	Run.m_PendingKits = 0;
	Run.m_PendingAmmo = false;
	CPlayerData *pData =
		m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
	if(pData)
	{
		pData->m_PvePendingArmor = 0;
		pData->m_PvePendingKits = 0;
		pData->m_PvePendingAmmo = false;
	}
	SendBuildState(ClientID, true);
}

void CPveDirector::OnStageComplete(bool Success)
{
	if(!Enabled())
		return;
	if(Success)
	{
		OnObjectiveComplete();
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		{
			if(!IsEligiblePlayer(ClientID))
				continue;
			CPlayerRun &Run = m_aPlayers[ClientID];
			CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientID];
			if(pPlayer && Run.m_aStacks[PVE_CARD_RESERVE_FUND])
				pPlayer->m_Gold =
					min(999,
						pPlayer->m_Gold + min(30, pPlayer->GetGold() * Run.m_aStacks[PVE_CARD_RESERVE_FUND] * 5 / 100));
			if(m_Mode == PVE_MODE_INVASION && !m_AnyStageDeath && Run.m_aStacks[PVE_CARD_FLOOR_MEMORY])
				Run.m_DeathlessFloors = min(5, Run.m_DeathlessFloors + 1);
			if(m_Mode == PVE_MODE_INVASION && Run.m_aStacks[PVE_CARD_DEEP_SOVEREIGN])
				AddBarrier(ClientID, 20);
			if(m_Mode == PVE_MODE_HORDE && !m_AnyStageDeath)
			{
				if(Run.m_aStacks[PVE_CARD_WAVE_DIVIDEND] && pPlayer)
					pPlayer->m_Gold = min(999, pPlayer->m_Gold + 8);
			}
		}
		if(m_Mode == PVE_MODE_HORDE)
			m_DeathlessHordeWaves = m_AnyStageDeath ? 0 : min(5, m_DeathlessHordeWaves + 1);
	}
	if(Success && m_Mode == PVE_MODE_INVASION)
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		{
			if(!IsEligiblePlayer(ClientID))
				continue;
			CPlayerRun &Run = m_aPlayers[ClientID];
			Run.m_InvasionFloorsCompleted++;
			CPlayerData *pData =
				m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
			if(pData)
				pData->m_PveInvasionFloors = Run.m_InvasionFloorsCompleted;
		}
	if(m_ContractState != PVE_CONTRACT_STATE_ACTIVE)
		return;
	if(m_Mode == PVE_MODE_HORDE)
	{
		m_ContractProgress = min(m_ContractTarget, m_ContractProgress + 1);
		SendContractStatus();
		if(m_ContractProgress < m_ContractTarget)
			return;
		if(m_ActiveContract == PVE_CONTRACT_ELITE_HUNT && m_pEliteContractBoss)
			Success = false;
		CompleteContract(Success);
		return;
	}
	if(m_ActiveContract == PVE_CONTRACT_SPEED_CLEAR && m_pGameServer->Server()->Tick() > m_ContractEndTick)
		Success = false;
	if((m_ActiveContract == PVE_CONTRACT_BLACK_BOX || m_ActiveContract == PVE_CONTRACT_ELITE_HUNT ||
		m_ActiveContract == PVE_CONTRACT_ELITE_GUARD || m_ActiveContract == PVE_CONTRACT_HEAVY_CARGO) &&
	   m_ContractProgress < m_ContractTarget)
		Success = false;
	CompleteContract(Success);
}

void CPveDirector::OnPlayerDeath(int ClientID)
{
	if(IsEligiblePlayer(ClientID))
	{
		m_aPlayers[ClientID].m_DiedThisStage = true;
		m_AnyStageDeath = true;
		m_DeathlessHordeWaves = 0;
		DestroyDrone(ClientID);
		SendBuildState(ClientID, true);
	}
	if(m_ContractState == PVE_CONTRACT_STATE_ACTIVE && m_ActiveContract == PVE_CONTRACT_FLAWLESS &&
	   IsEligiblePlayer(ClientID))
		CompleteContract(false);
}

void CPveDirector::RegisterEliteContractBoss(CDroid *pBoss)
{
	if(!pBoss || m_ContractState != PVE_CONTRACT_STATE_ACTIVE)
		return;
	if(m_ActiveContract == PVE_CONTRACT_ELITE_HUNT)
		m_pEliteContractBoss = pBoss;
	else if(m_ActiveContract == PVE_CONTRACT_ELITE_GUARD &&
			m_NumEliteContractGuards < (int)(sizeof(m_apEliteContractGuards) / sizeof(m_apEliteContractGuards[0])))
	{
		if(m_NumEliteContractGuards == 0)
		{
			m_ContractProgress = 0;
			m_ContractTarget = 0;
		}
		m_apEliteContractGuards[m_NumEliteContractGuards++] = pBoss;
		m_ContractTarget++;
		SendContractStatus();
	}
}

void CPveDirector::OnBossKilled(bool ContractBoss)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_pGameServer->m_apPlayers[i] && !m_pGameServer->m_apPlayers[i]->m_IsBot)
			m_pGameServer->Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_BOSS);
	if(ContractBoss && m_ContractState == PVE_CONTRACT_STATE_ACTIVE && m_ActiveContract == PVE_CONTRACT_ELITE_HUNT)
	{
		m_pEliteContractBoss = 0;
		m_ContractProgress = 1;
		CompleteContract(true);
	}
	else if(ContractBoss && m_ContractState == PVE_CONTRACT_STATE_ACTIVE &&
			m_ActiveContract == PVE_CONTRACT_ELITE_GUARD)
	{
		m_pEliteContractBoss = 0;
		m_ContractProgress = 1;
		SendContractStatus();
	}
}

void CPveDirector::OnEnemyKilled(const CAttackSource &Source, vec2 Pos, CEntity *pTarget)
{
	const int ClientID = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponVisualProfile Visual{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat, &Visual);
	const int Specialization = Source.m_Kind == EAttackSourceKind::PlayerWeapon ? WeaponSpecialization(Source.m_Weapon)
																				: PVE_SPECIALIZATION_NONE;
	if(m_ApplyingSecondaryEffect)
	{
		ClearTargetStatus(pTarget);
		return;
	}
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	CTargetStatus *pKilledStatus = TargetStatus(pTarget, false);
	if(pKilledStatus && pKilledStatus->m_BleedStacks > 0 && Run.m_aStacks[PVE_CARD_BLOOD_TEMPER])
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->IncreaseHealth(5);
	}
	Run.m_StageKills++;
	if(Run.m_aStacks[PVE_CARD_SECOND_WIND] && Run.m_StageKills % 5 == 0 && Run.m_SecondWindTriggers < 3)
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->IncreaseHealth(Run.m_aStacks[PVE_CARD_SECOND_WIND] * 2);
		Run.m_SecondWindTriggers++;
	}
	if(Run.m_aStacks[PVE_CARD_SALVAGE_INSTINCT] && Run.m_StageKills % 8 == 0 && Run.m_SalvageKits < 6)
	{
		const int Kits = min(6 - Run.m_SalvageKits, Run.m_aStacks[PVE_CARD_SALVAGE_INSTINCT]);
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->AddKits(Kits);
		Run.m_SalvageKits += Kits;
	}
	if(Specialization == PVE_SPECIALIZATION_ELECTRIC && Run.m_aStacks[PVE_CARD_FEEDBACK])
		Run.m_aWeaponResources[PVE_SPECIALIZATION_ELECTRIC - 1] =
			min(10, Run.m_aWeaponResources[PVE_SPECIALIZATION_ELECTRIC - 1] + Run.m_aStacks[PVE_CARD_FEEDBACK] * 2);
	const int Now = m_pGameServer->Server()->Tick();
	if(Run.m_aStacks[PVE_CARD_KILL_CHAIN])
	{
		if(Now > Run.m_KillChainEndTick)
			Run.m_KillChainStacks = 0;
		Run.m_KillChainStacks = min(5, Run.m_KillChainStacks + 1);
		Run.m_KillChainEndTick = Now + m_pGameServer->Server()->TickSpeed() * 5;
	}
	if(m_Mode == PVE_MODE_HORDE && Run.m_aStacks[PVE_CARD_REAPER])
	{
		if(Now > Run.m_ReaperChainEndTick)
			Run.m_ReaperKills = 0;
		Run.m_ReaperKills++;
		Run.m_ReaperChainEndTick = Now + m_pGameServer->Server()->TickSpeed() * 2;
		if(Run.m_ReaperKills >= 3)
			Run.m_ReaperEndTick = Now + m_pGameServer->Server()->TickSpeed() * 5;
	}
	const int RenderType = Visual.m_RenderType;
	const bool Melee = Combat.m_FiringType == WFT_MELEE || RenderType == WRT_MELEE || RenderType == WRT_MELEESMALL ||
					   RenderType == WRT_SPIN;
	if(Melee && Run.m_aStacks[PVE_CARD_BLOOD_DRIVE])
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->IncreaseHealth(4);
	}
	if(Combat.m_ExplosiveProjectile && Run.m_aStacks[PVE_CARD_CHAIN_REACTION] && frandom() < 0.25f)
		m_pGameServer->CreateExplosion(Pos, Source);
	ClearTargetStatus(pTarget);
}

void CPveDirector::OnDroidKilled(CDroid *pDroid, const CAttackSource &Source)
{
	const int ClientID = Source.m_Owner;
	if(!pDroid)
		return;
	CTargetStatus *pStatus = TargetStatus(pDroid, false);
	if(!m_ApplyingSecondaryEffect && IsEligiblePlayer(ClientID) && pStatus && pStatus->m_BleedStacks > 0 &&
	   m_aPlayers[ClientID].m_aStacks[PVE_CARD_BLOOD_TEMPER])
	{
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(pChr)
			pChr->IncreaseHealth(5);
	}
	if(m_Mode == PVE_MODE_EXTRACTION)
	{
		CGameControllerExtract *pExtract = dynamic_cast<CGameControllerExtract *>(m_pGameServer->m_pController);
		if(pExtract)
			pExtract->OnDroidKilled(pDroid);
	}
	OnEnemyKilled(Source, pDroid->m_Pos + pDroid->m_Center);
	ClearTargetStatus(pDroid);
	if(pDroid == m_pEliteContractBoss)
		OnBossKilled(true);
	for(int i = 0; i < m_NumEliteContractGuards; i++)
		if(pDroid == m_apEliteContractGuards[i])
		{
			m_apEliteContractGuards[i] = 0;
			if(m_ContractState == PVE_CONTRACT_STATE_ACTIVE && m_ActiveContract == PVE_CONTRACT_ELITE_GUARD)
			{
				m_ContractProgress = min(m_ContractTarget, m_ContractProgress + 1);
				SendContractStatus();
			}
			break;
		}
}

void CPveDirector::OnSwitchTriggered()
{
}

void CPveDirector::OnObjectiveComplete()
{
	if(!Enabled())
		return;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(!IsEligiblePlayer(ClientID))
			continue;
		CPlayerRun &Run = m_aPlayers[ClientID];
		if(m_Mode == PVE_MODE_INVASION && Run.m_aStacks[PVE_CARD_RELIC_SCANNER] && m_pGameServer->m_apPlayers[ClientID])
			m_pGameServer->m_apPlayers[ClientID]->m_Gold =
				min(999, m_pGameServer->m_apPlayers[ClientID]->m_Gold + Run.m_aStacks[PVE_CARD_RELIC_SCANNER] * 4);
		if(Run.m_ObjectiveCacheUsed || !Run.m_aStacks[PVE_CARD_OBJECTIVE_CACHE])
			continue;
		Run.m_ObjectiveCacheUsed = true;
		CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
		if(!pChr)
			continue;
		pChr->IncreaseArmor(Run.m_aStacks[PVE_CARD_OBJECTIVE_CACHE] * 3);
		const float Fraction = Run.m_aStacks[PVE_CARD_OBJECTIVE_CACHE] * 0.10f;
		for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
			if(pChr->GetWeapon(Slot))
				pChr->GetWeapon(Slot)->IncreaseAmmo((int)(pChr->GetWeapon(Slot)->m_MaxAmmo * Fraction));
	}
}

void CPveDirector::OnGoldSpent(int ClientID, int Amount)
{
	if(!Enabled() || !IsEligiblePlayer(ClientID) || Amount <= 0 ||
	   !m_aPlayers[ClientID].m_aStacks[PVE_CARD_WAR_ECONOMY])
		return;
	AddBarrier(ClientID, min(15, (Amount / 25) * 5));
}

void CPveDirector::OnFullReload(int ClientID)
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return;
	if(m_aPlayers[ClientID].m_aStacks[PVE_CARD_TACTICAL_RELOAD])
		m_aPlayers[ClientID].m_TacticalReloadShots = 5;
	if(m_aPlayers[ClientID].m_PerfectSequencePending)
	{
		m_aPlayers[ClientID].m_PerfectSequencePending = false;
		m_aPlayers[ClientID].m_PerfectSequenceShots = 6;
	}
}

void CPveDirector::OnEvacuationStarted()
{
	// Clean Exit fires when a player enters the evacuation zone, not when the door opens.
}

void CPveDirector::OnEvacuationZoneEntered(int ClientID)
{
	if(!Enabled() || m_Mode != PVE_MODE_EXTRACTION || !IsEligiblePlayer(ClientID))
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(!Run.m_aStacks[PVE_CARD_CLEAN_EXIT] || Run.m_CleanExitUsed)
		return;
	Run.m_CleanExitUsed = true;
	AddBarrier(ClientID, 20);
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	if(pChr)
		for(int Slot = 0; Slot < NUM_SLOTS; Slot++)
			if(pChr->GetWeapon(Slot))
				pChr->GetWeapon(Slot)->IncreaseAmmo((int)(pChr->GetWeapon(Slot)->m_MaxAmmo * 0.30f));
}

void CPveDirector::OnCargoDelivered()
{
	if(ActiveContract() != PVE_CONTRACT_HEAVY_CARGO)
		return;
	m_ContractProgress = 1;
	SendContractStatus();
}

void CPveDirector::TickBlackBox()
{
	bool Occupied = false;
	float InteractionScale = 1.0f;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsEligiblePlayer(i))
			continue;
		CCharacter *pChr = m_pGameServer->GetPlayerChar(i);
		if(pChr && pChr->IsAlive() && distance(pChr->m_Pos, m_BlackBoxPos) < 80.0f)
		{
			Occupied = true;
			InteractionScale = max(InteractionScale, InteractionSpeedBonus(i));
		}
	}
	if(Occupied)
		m_BlackBoxHoldTicks += max(1, (int)(InteractionScale + frandom()));
	else
		m_BlackBoxHoldTicks = 0;
	const int NewProgress = m_BlackBoxHoldTicks / m_pGameServer->Server()->TickSpeed();
	if(NewProgress != m_ContractProgress)
	{
		m_ContractProgress = min(m_ContractTarget, NewProgress);
		SendContractStatus();
	}
	if(m_ContractProgress >= m_ContractTarget)
		CompleteContract(true);
}

void CPveDirector::CompleteContract(bool Success)
{
	if(m_ContractState != PVE_CONTRACT_STATE_ACTIVE)
		return;
	const int CompletedContract = m_ActiveContract;
	m_ContractState = Success ? PVE_CONTRACT_STATE_SUCCESS : PVE_CONTRACT_STATE_FAILED;
	m_pEliteContractBoss = 0;
	mem_zero(m_apEliteContractGuards, sizeof(m_apEliteContractGuards));
	m_NumEliteContractGuards = 0;
	if(m_pBlackBoxRadar)
	{
		m_pGameServer->m_World.DestroyEntity(m_pBlackBoxRadar);
		m_pBlackBoxRadar = 0;
	}
	if(Success)
		m_ContractProgress = max(m_ContractProgress, m_ContractTarget);
	SendContractStatus();
	if(Success)
	{
		m_pGameServer->SendChatTarget(-1, "Contract completed: +1 research point");
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		{
			if(!IsEligiblePlayer(ClientID) || !(m_ContractParticipants & (1ULL << ClientID)))
				continue;
			CPlayerRun &Run = m_aPlayers[ClientID];
			Run.m_ResearchPoints = clamp(Run.m_ResearchPoints + 1, 0, 999);
			const int Checkpoint = Run.m_HighestInvasion >= 10 ? (Run.m_HighestInvasion / 10) * 10 + 1 : 1;
			CNetMsg_Sv_PveResearchReward Msg;
			Msg.m_Amount = 1;
			Msg.m_Reason = PVE_REWARD_CONTRACT;
			Msg.m_HighestInvasion = Run.m_HighestInvasion;
			Msg.m_UnlockedCheckpoint = Checkpoint;
			m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		}
	}
	else
		m_pGameServer->SendChatTarget(-1, "Contract failed");
	if(CompletedContract == PVE_CONTRACT_NO_RESPAWN)
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(IsEligiblePlayer(ClientID) && !m_pGameServer->GetPlayerChar(ClientID))
				m_pGameServer->m_apPlayers[ClientID]->m_RespawnTick =
					m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * g_Config.m_SvRespawnDelay;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		if(m_pGameServer->m_apPlayers[ClientID] && !m_pGameServer->m_apPlayers[ClientID]->m_IsBot)
		{
			CPlayerData *pData =
				m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
			if(pData)
			{
				pData->m_PveContractParticipant = false;
				pData->m_PveContractNonce = 0;
			}
		}
}

void CPveDirector::RewardResearch(int Amount, int Reason, int HighestInvasion)
{
	if(!Enabled() || Amount < 0 || g_Config.m_SvTutorialMode)
		return;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(!IsEligiblePlayer(ClientID))
			continue;
		CPlayerRun &Run = m_aPlayers[ClientID];
		Run.m_ResearchPoints = clamp(Run.m_ResearchPoints + Amount, 0, 999);
		if(HighestInvasion > 0)
			Run.m_HighestInvasion = max(Run.m_HighestInvasion, HighestInvasion);
		const int Checkpoint = Run.m_HighestInvasion >= 10 ? (Run.m_HighestInvasion / 10) * 10 + 1 : 1;
		CNetMsg_Sv_PveResearchReward Msg;
		Msg.m_Amount = Amount;
		Msg.m_Reason = Reason;
		Msg.m_HighestInvasion = Run.m_HighestInvasion;
		Msg.m_UnlockedCheckpoint = Checkpoint;
		m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		SendProgress(ClientID);
	}
	if(Amount > 0)
		m_pGameServer->SendChatTarget(-1, "Research reward: +%d point(s)", Amount);
}

int CPveDirector::PerkStacks(int ClientID, int CardID) const
{
	return Enabled() && ClientID >= 0 && ClientID < MAX_CLIENTS && CardID >= 0 && CardID < NUM_PVE_CARDS
			   ? m_aPlayers[ClientID].m_aStacks[CardID]
			   : 0;
}

int CPveDirector::DroneModule(int ClientID) const
{
	return Enabled() && ClientID >= 0 && ClientID < MAX_CLIENTS ? m_aPlayers[ClientID].m_DroneModule : PVE_DRONE_NONE;
}

CPveDirector::CTargetStatus *CPveDirector::TargetStatus(CEntity *pTarget, bool Create)
{
	if(!pTarget)
		return 0;
	CTargetStatus *pFree = 0;
	for(int i = 0; i < (int)(sizeof(m_aTargetStatus) / sizeof(m_aTargetStatus[0])); i++)
	{
		if(m_aTargetStatus[i].m_pTarget == pTarget)
			return &m_aTargetStatus[i];
		if(!pFree && !m_aTargetStatus[i].m_pTarget)
			pFree = &m_aTargetStatus[i];
	}
	if(!Create || !pFree)
		return 0;
	mem_zero(pFree, sizeof(*pFree));
	pFree->m_pTarget = pTarget;
	pFree->m_BleedSource = CAttackSource::World(WEAPON_WORLD);
	m_TargetStatusCount++;
	m_TargetSummaryTick = -1;
	return pFree;
}

void CPveDirector::ClearTargetStatus(CEntity *pTarget)
{
	CTargetStatus *pStatus = TargetStatus(pTarget, false);
	if(pStatus)
	{
		mem_zero(pStatus, sizeof(*pStatus));
		m_TargetStatusCount = max(0, m_TargetStatusCount - 1);
		m_TargetSummaryTick = -1;
	}
}

void CPveDirector::ApplyVulnerable(CEntity *pTarget, int Percent, int Seconds)
{
	CTargetStatus *pStatus = TargetStatus(pTarget, true);
	if(!pStatus || Percent <= 0)
		return;
	pStatus->m_VulnerablePercent = min(25, pStatus->m_VulnerablePercent + Percent);
	pStatus->m_VulnerableEndTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * Seconds;
	m_TargetSummaryTick = -1;
}

void CPveDirector::ApplyBleed(CEntity *pTarget, int Stacks, const CAttackSource &Source)
{
	CTargetStatus *pStatus = TargetStatus(pTarget, true);
	if(!pStatus || Stacks <= 0)
		return;
	const bool WasInactive = pStatus->m_BleedStacks <= 0;
	pStatus->m_BleedStacks = min(5, pStatus->m_BleedStacks + Stacks);
	pStatus->m_BleedEndTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 5;
	if(WasInactive)
		pStatus->m_BleedNextTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed();
	pStatus->m_BleedSource = Source;
	m_TargetSummaryTick = -1;
}

int CPveDirector::VulnerablePercent(CEntity *pTarget)
{
	CTargetStatus *pStatus = TargetStatus(pTarget, false);
	if(!pStatus || pStatus->m_VulnerableEndTick < m_pGameServer->Server()->Tick())
		return 0;
	return pStatus->m_VulnerablePercent;
}

void CPveDirector::ProcessHit(int ClientID, CEntity *pTarget, int Damage, bool Direct)
{
	if(!Direct || m_ApplyingSecondaryEffect || !IsEligiblePlayer(ClientID) || !pTarget || Damage <= 0)
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Run.m_aStacks[PVE_CARD_MARKING_ROUNDS])
	{
		CTargetStatus *pStatus = TargetStatus(pTarget, true);
		if(!pStatus)
			return;
		pStatus->m_aMarkingHits[ClientID]++;
		if(pStatus->m_aMarkingHits[ClientID] >= 6)
		{
			pStatus->m_aMarkingHits[ClientID] = 0;
			ApplyVulnerable(pTarget, Run.m_aStacks[PVE_CARD_MARKING_ROUNDS] * 3, 4);
		}
	}
}

void CPveDirector::TickTargetStatuses()
{
	if(m_TargetStatusCount <= 0)
		return;
	const int Now = m_pGameServer->Server()->Tick();
	m_TargetSummaryTick = -1;
	for(int i = 0; i < (int)(sizeof(m_aTargetStatus) / sizeof(m_aTargetStatus[0])); i++)
	{
		CTargetStatus &Status = m_aTargetStatus[i];
		if(!Status.m_pTarget)
			continue;
		if(Status.m_VulnerableEndTick < Now)
			Status.m_VulnerablePercent = 0;
		if(Status.m_BleedEndTick < Now)
			Status.m_BleedStacks = 0;
		if(Status.m_BleedStacks <= 0 || Status.m_BleedNextTick > Now)
			continue;
		Status.m_BleedNextTick += m_pGameServer->Server()->TickSpeed();
		const int Damage = Status.m_BleedStacks * 2;
		m_ApplyingSecondaryEffect = true;
		if(Status.m_pTarget->GetType() == CGameWorld::ENTTYPE_DROID)
		{
			CDroid *pDroid = static_cast<CDroid *>(Status.m_pTarget);
			if(pDroid->m_Health > 0)
			{
				CAttackSource StatusSource = Status.m_BleedSource;
				StatusSource.m_HitFeedback = false;
				pDroid->TakeDamage(vec2(0, 0), Damage, StatusSource, pDroid->m_Pos);
			}
		}
		else if(Status.m_pTarget->GetType() == CGameWorld::ENTTYPE_CHARACTER)
		{
			CCharacter *pCharacter = static_cast<CCharacter *>(Status.m_pTarget);
			if(pCharacter->m_IsBot && pCharacter->IsAlive())
			{
				CAttackSource StatusSource = Status.m_BleedSource;
				StatusSource.m_HitFeedback = false;
				pCharacter->TakeDamage(StatusSource, Damage, vec2(0, 0), pCharacter->m_Pos);
			}
		}
		m_ApplyingSecondaryEffect = false;
	}
}

void CPveDirector::UpdateTargetSummary()
{
	const int Now = m_pGameServer->Server()->Tick();
	if(m_TargetSummaryTick == Now)
		return;
	m_TargetSummaryTick = Now;
	m_VulnerableTargetCount = 0;
	m_BleedingTargetCount = 0;
	if(m_TargetStatusCount <= 0)
		return;
	for(int i = 0; i < (int)(sizeof(m_aTargetStatus) / sizeof(m_aTargetStatus[0])); i++)
	{
		const CTargetStatus &Status = m_aTargetStatus[i];
		if(!Status.m_pTarget)
			continue;
		m_VulnerableTargetCount += Status.m_VulnerablePercent > 0 && Status.m_VulnerableEndTick >= Now;
		m_BleedingTargetCount += Status.m_BleedStacks > 0 && Status.m_BleedEndTick >= Now;
	}
}

void CPveDirector::ScheduleSecondaryBlast(const CAttackSource &Source, vec2 Pos, int Damage)
{
	for(int i = 0; i < (int)(sizeof(m_aPendingBlasts) / sizeof(m_aPendingBlasts[0])); i++)
		if(m_aPendingBlasts[i].m_Tick == 0)
		{
			m_aPendingBlasts[i].m_Pos = Pos;
			m_aPendingBlasts[i].m_Source = Source;
			m_aPendingBlasts[i].m_Damage = max(1, Damage * 60 / 100);
			m_aPendingBlasts[i].m_Tick =
				m_pGameServer->Server()->Tick() + max(1, m_pGameServer->Server()->TickSpeed() * 35 / 100);
			m_PendingBlastCount++;
			return;
		}
}

void CPveDirector::TickPendingBlasts()
{
	if(m_PendingBlastCount <= 0)
		return;
	const int Now = m_pGameServer->Server()->Tick();
	for(int i = 0; i < (int)(sizeof(m_aPendingBlasts) / sizeof(m_aPendingBlasts[0])); i++)
	{
		CPendingBlast &Blast = m_aPendingBlasts[i];
		if(Blast.m_Tick == 0 || Blast.m_Tick > Now)
			continue;
		m_pGameServer->CreateEffect(FX_EXPLOSION1, Blast.m_Pos);
		m_ApplyingSecondaryEffect = true;
		for(CCharacter *pCharacter = (CCharacter *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
			pCharacter;
			pCharacter = (CCharacter *)pCharacter->TypeNext())
			if(pCharacter->m_IsBot && pCharacter->IsAlive() && distance(Blast.m_Pos, pCharacter->m_Pos) <= 170.0f)
				pCharacter->TakeDamage(Blast.m_Source, Blast.m_Damage, vec2(0, 0), pCharacter->m_Pos);
		for(CDroid *pDroid = (CDroid *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); pDroid;
			pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid->m_Health > 0 && distance(Blast.m_Pos, pDroid->m_Pos + pDroid->m_Center) <= 170.0f)
				pDroid->TakeDamage(vec2(0, 0), Blast.m_Damage, Blast.m_Source, pDroid->m_Pos);
		m_ApplyingSecondaryEffect = false;
		Blast.m_Tick = 0;
		m_PendingBlastCount = max(0, m_PendingBlastCount - 1);
	}
}

void CPveDirector::ApplyThunderhead(const CAttackSource &Source, CEntity *pTarget, int Damage)
{
	if(!pTarget || Damage <= 0)
		return;
	const vec2 Pos =
		pTarget->m_Pos +
		(pTarget->GetType() == CGameWorld::ENTTYPE_DROID ? static_cast<CDroid *>(pTarget)->m_Center : vec2(0, 0));
	new CLightning(&m_pGameServer->m_World, Pos, Pos + vec2(0, -320.0f));
	m_ApplyingSecondaryEffect = true;
	if(pTarget->GetType() == CGameWorld::ENTTYPE_DROID)
		static_cast<CDroid *>(pTarget)->TakeDamage(vec2(0, 0), Damage, Source, Pos);
	else if(pTarget->GetType() == CGameWorld::ENTTYPE_CHARACTER)
		static_cast<CCharacter *>(pTarget)->TakeDamage(Source, Damage, vec2(0, 0), Pos);
	m_ApplyingSecondaryEffect = false;
}

int CPveDirector::AddBarrier(int ClientID, int Amount)
{
	if(!Enabled() || !IsEligiblePlayer(ClientID) || Amount <= 0)
		return 0;
	float RecoveryScale = 1.0f;
	CCharacter *pTarget = m_pGameServer->GetPlayerChar(ClientID);
	if(pTarget)
		for(int Ally = 0; Ally < MAX_CLIENTS; Ally++)
		{
			CCharacter *pAlly = IsEligiblePlayer(Ally) ? m_pGameServer->GetPlayerChar(Ally) : 0;
			if(pAlly && m_aPlayers[Ally].m_aStacks[PVE_CARD_FIELD_RELAY] &&
			   distance(pAlly->m_Pos, pTarget->m_Pos) <= 350.0f)
				RecoveryScale += m_aPlayers[Ally].m_aStacks[PVE_CARD_FIELD_RELAY] * 0.06f;
		}
	if(ActiveContract() == PVE_CONTRACT_ATTRITION)
		RecoveryScale *= 0.5f;
	Amount = max(1, (int)(Amount * RecoveryScale + 0.5f));
	CPlayerRun &Run = m_aPlayers[ClientID];
	const int Old = Run.m_Barrier;
	Run.m_Barrier = min(30, Run.m_Barrier + Amount);
	if(Run.m_Barrier != Old)
		SendBuildState(ClientID, true);
	return Run.m_Barrier - Old;
}

int CPveDirector::ModifyRecovery(int ClientID, int Amount, bool Health) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID) || Amount <= 0)
		return Amount;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	if(Health && m_Mode == PVE_MODE_EXTRACTION && Run.m_aStacks[PVE_CARD_FINAL_DEPARTURE])
	{
		const CGameControllerExtract *pExtract =
			dynamic_cast<const CGameControllerExtract *>(m_pGameServer->m_pController);
		if(pExtract && pExtract->Evacuating())
			return 0;
	}
	float Multiplier = 1.0f + Run.m_aStacks[PVE_CARD_FIRST_AID] * 0.20f;
	CCharacter *pTarget = m_pGameServer->GetPlayerChar(ClientID);
	if(pTarget)
		for(int Ally = 0; Ally < MAX_CLIENTS; Ally++)
		{
			if(!IsEligiblePlayer(Ally) || !m_aPlayers[Ally].m_aStacks[PVE_CARD_FIELD_RELAY])
				continue;
			CCharacter *pAlly = m_pGameServer->GetPlayerChar(Ally);
			if(pAlly && distance(pAlly->m_Pos, pTarget->m_Pos) <= 350.0f)
				Multiplier += m_aPlayers[Ally].m_aStacks[PVE_CARD_FIELD_RELAY] * 0.06f;
		}
	if(ActiveContract() == PVE_CONTRACT_ATTRITION)
		Multiplier *= 0.5f;
	return max(0, (int)(Amount * Multiplier + 0.5f));
}

void CPveDirector::DestroyDrone(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Run.m_pDrone)
	{
		m_pGameServer->m_World.DestroyEntity(Run.m_pDrone);
		Run.m_pDrone = 0;
	}
	Run.m_pDroneTarget = 0;
}

int CPveDirector::DroneSwitchReadyTick(int ClientID) const
{
	return Enabled() && ClientID >= 0 && ClientID < MAX_CLIENTS ? m_aPlayers[ClientID].m_DroneSwitchReadyTick : 0;
}

bool CPveDirector::DamageDrone(int ClientID, int Damage)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_aPlayers[ClientID].m_pDrone)
		return false;
	CPlayerRun &Run = m_aPlayers[ClientID];
	return Run.m_pDrone->TakeDamage(Damage);
}

void CPveDirector::ApplyDroneEmp(int ClientID, int Seconds)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aPlayers[ClientID].m_pDrone)
		m_aPlayers[ClientID].m_pDrone->ApplyEmp(m_pGameServer->Server()->TickSpeed() * max(0, Seconds));
}

float CPveDirector::DroneEfficiency(int ClientID) const
{
	if(!IsEligiblePlayer(ClientID))
		return 1.0f;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	float Efficiency = 1.0f + min(0.30f, Run.m_aStacks[PVE_CARD_DRONE_CHASSIS] * 0.10f);
	if(Run.m_aStacks[PVE_CARD_COORDINATED_FIRMWARE])
		Efficiency += 0.25f;
	if(Run.m_aStacks[PVE_CARD_AUTONOMOUS_CORE])
		Efficiency += 0.50f;
	if(Run.m_aStacks[PVE_CARD_GRID_LINK] && Run.m_aWeaponResources[PVE_SPECIALIZATION_ELECTRIC - 1] > 5)
		Efficiency += 0.35f;
	if(Run.m_aStacks[PVE_CARD_HOLD_THE_LINE] && InHordeDefenseArea(ClientID))
		Efficiency += 0.25f;
	return Efficiency;
}

bool CPveDirector::InHordeDefenseArea(int ClientID) const
{
	if(m_Mode != PVE_MODE_HORDE || ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	const CCharacter *pCharacter = m_pGameServer->GetPlayerChar(ClientID);
	const CGameControllerHorde *pHorde = dynamic_cast<const CGameControllerHorde *>(m_pGameServer->m_pController);
	return pCharacter && pCharacter->IsAlive() && pHorde && pHorde->InDefenseArea(pCharacter->m_Pos);
}

void CPveDirector::TickDrone(int ClientID)
{
	CPlayerRun &Run = m_aPlayers[ClientID];
	CCharacter *pOwner = m_pGameServer->GetPlayerChar(ClientID);
	if(!Run.m_aStacks[PVE_CARD_DRONE_CHASSIS] || !pOwner || !pOwner->IsAlive())
	{
		DestroyDrone(ClientID);
		return;
	}
	if(!Run.m_pDrone)
		Run.m_pDrone = new CPveDrone(&m_pGameServer->m_World, ClientID);
	if(!Run.m_pDrone || !Run.m_pDrone->Active() || Run.m_DroneModule == PVE_DRONE_NONE ||
	   Run.m_DroneActionTick > m_pGameServer->Server()->Tick())
		return;
	const float Efficiency = DroneEfficiency(ClientID);
	const float CooldownReduction = min(0.30f, Run.m_aStacks[PVE_CARD_SERVO_LINK] * 0.08f);
	if(Run.m_DroneModule == PVE_DRONE_ASSAULT)
	{
		CCharacter *pBestCharacter = 0;
		CDroid *pBestDroid = 0;
		float BestDistanceSquared = 700.0f * 700.0f;
		for(CCharacter *pCharacter = (CCharacter *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
			pCharacter;
			pCharacter = (CCharacter *)pCharacter->TypeNext())
			if(pCharacter->m_IsBot && pCharacter->IsAlive())
			{
				const vec2 Delta = Run.m_pDrone->m_Pos - pCharacter->m_Pos;
				const float DistanceSquared = dot(Delta, Delta);
				if(DistanceSquared >= BestDistanceSquared)
					continue;
				BestDistanceSquared = DistanceSquared;
				pBestCharacter = pCharacter;
				pBestDroid = 0;
			}
		for(CDroid *pDroid = (CDroid *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); pDroid;
			pDroid = (CDroid *)pDroid->TypeNext())
			if(pDroid->m_Health > 0)
			{
				const vec2 Delta = Run.m_pDrone->m_Pos - (pDroid->m_Pos + pDroid->m_Center);
				const float DistanceSquared = dot(Delta, Delta);
				if(DistanceSquared >= BestDistanceSquared)
					continue;
				BestDistanceSquared = DistanceSquared;
				pBestCharacter = 0;
				pBestDroid = pDroid;
			}
		if(pBestCharacter || pBestDroid)
		{
			CWeapon *pWeapon = pOwner->GetWeapon();
			const CAttackSource Source = pWeapon ? CAttackSource::PlayerWeapon(ClientID, pWeapon->GetWeaponSpec())
												 : CAttackSource::World(WEAPON_GAME, ClientID);
			float BaseDamage = pWeapon ? max(pWeapon->GetWeaponProfile().m_Combat.m_ProjectileDamage,
											 pWeapon->GetWeaponProfile().m_Combat.m_ExplosionDamage)
									   : 0.0f;
			if(BaseDamage <= 0.0f)
				BaseDamage = 10.0f;
			float DamageScale = 0.45f * Efficiency;
			if(Run.m_aStacks[PVE_CARD_CROSSFIRE])
				DamageScale *= 1.50f;
			const int Damage = max(1, (int)(BaseDamage * DamageScale + 0.5f));
			const vec2 TargetPos = pBestCharacter ? pBestCharacter->m_Pos : pBestDroid->m_Pos + pBestDroid->m_Center;
			Run.m_pDroneTarget = pBestCharacter ? (CEntity *)pBestCharacter : (CEntity *)pBestDroid;
			Run.m_pDrone->SetAction(TargetPos,
									m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() / 5);
			// Match droid_star projectile sprite/trace/fx; damage stays hitscan so Efficiency/Crossfire still apply.
			new CPveDronePulse(&m_pGameServer->m_World,
							   Run.m_pDrone->m_Pos,
							   TargetPos,
							   CAttackSource::Droid(ClientID, DROIDTYPE_STAR));
			m_pGameServer->CreateSound(Run.m_pDrone->m_Pos, SOUND_STAR_FIRE);
			m_ApplyingSecondaryEffect = true;
			if(pBestCharacter)
				pBestCharacter->TakeDamage(Source, Damage, vec2(0, 0), TargetPos);
			else
				pBestDroid->TakeDamage(vec2(0, 0), Damage, Source, TargetPos);
			m_ApplyingSecondaryEffect = false;
		}
		Run.m_DroneActionTick =
			m_pGameServer->Server()->Tick() +
			max(1, (int)(m_pGameServer->Server()->TickSpeed() * 0.55f * (1.0f - CooldownReduction)));
	}
	else if(Run.m_DroneModule == PVE_DRONE_REPAIR)
	{
		CCharacter *pBest = 0;
		int LowestArmor = 101;
		for(int Ally = 0; Ally < MAX_CLIENTS; Ally++)
		{
			CCharacter *pCharacter = IsEligiblePlayer(Ally) ? m_pGameServer->GetPlayerChar(Ally) : 0;
			if(pCharacter && pCharacter->IsAlive() && distance(Run.m_pDrone->m_Pos, pCharacter->m_Pos) <= 600.0f &&
			   pCharacter->GetArmor() < LowestArmor)
			{
				pBest = pCharacter;
				LowestArmor = pCharacter->GetArmor();
			}
		}
		const int Repair = max(1, (int)(5.0f * Efficiency + 0.5f));
		if(pBest && LowestArmor < 100)
		{
			Run.m_pDroneTarget = pBest;
			Run.m_pDrone->SetAction(pBest->m_Pos,
									m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() / 3);
			pBest->IncreaseArmor(Repair);
		}
		else
		{
			CBuilding *pBestBuilding = 0;
			float BestFraction = 1.0f;
			for(CBuilding *pBuilding = (CBuilding *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING);
				pBuilding;
				pBuilding = (CBuilding *)pBuilding->TypeNext())
				if(pBuilding->m_MaxLife > 0 && pBuilding->m_Life > 0 && pBuilding->m_Life < pBuilding->m_MaxLife &&
				   distance(Run.m_pDrone->m_Pos, pBuilding->m_Pos) <= 600.0f &&
				   pBuilding->m_Life / (float)pBuilding->m_MaxLife < BestFraction)
				{
					BestFraction = pBuilding->m_Life / (float)pBuilding->m_MaxLife;
					pBestBuilding = pBuilding;
				}
			if(pBestBuilding)
			{
				Run.m_pDroneTarget = pBestBuilding;
				Run.m_pDrone->SetAction(pBestBuilding->m_Pos,
										m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() / 3);
				pBestBuilding->Repair(Repair);
			}
		}
		Run.m_DroneActionTick =
			m_pGameServer->Server()->Tick() +
			max(1, (int)(m_pGameServer->Server()->TickSpeed() * 0.45f * (1.0f - CooldownReduction)));
	}
}

void CPveDirector::SendBuildState(int ClientID, bool Force)
{
	if(!IsEligiblePlayer(ClientID))
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(!Force && Run.m_LastBuildStateTick + m_pGameServer->Server()->TickSpeed() / 5 > m_pGameServer->Server()->Tick())
		return;
	Run.m_LastBuildStateTick = m_pGameServer->Server()->Tick();
	UpdateTargetSummary();
	CNetMsg_Sv_PveBuildState Msg;
	Msg.m_Focus = Run.m_aWeaponResources[0];
	Msg.m_BlastCharge = Run.m_aWeaponResources[1];
	Msg.m_Voltage = Run.m_aWeaponResources[2];
	Msg.m_Fury = Run.m_aWeaponResources[3];
	Msg.m_Barrier = Run.m_Barrier;
	Msg.m_VulnerableTargets = min(99, m_VulnerableTargetCount);
	Msg.m_BleedingTargets = min(99, m_BleedingTargetCount);
	Msg.m_LegendaryCard = Run.m_LegendaryCard;
	Msg.m_DroneModule = Run.m_DroneModule;
	Msg.m_DroneSwitchReadyTick = Run.m_DroneSwitchReadyTick;
	const int aState[11] = {Msg.m_Focus,
							Msg.m_BlastCharge,
							Msg.m_Voltage,
							Msg.m_Fury,
							Msg.m_Barrier,
							Msg.m_VulnerableTargets,
							Msg.m_BleedingTargets,
							Msg.m_LegendaryCard,
							Msg.m_DroneModule,
							Msg.m_DroneSwitchReadyTick,
							Run.m_DeathlessFloors};
	if(!Force && mem_comp(aState, Run.m_aLastBuildState, sizeof(aState)) == 0)
		return;
	mem_copy(Run.m_aLastBuildState, aState, sizeof(aState));
	m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	if(m_pGameServer->m_apPlayers[ClientID])
	{
		CPlayerData *pData =
			m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
		if(pData)
		{
			pData->m_PveLegendaryCard = Run.m_LegendaryCard;
			for(int i = 0; i < 4; i++)
				pData->m_aPveWeaponResources[i] = Run.m_aWeaponResources[i];
			pData->m_PveBarrier = Run.m_Barrier;
			pData->m_PveDroneModule = Run.m_DroneModule;
			pData->m_PveDroneSwitchReadyTick = Run.m_DroneSwitchReadyTick;
			pData->m_PveDeathlessFloors = Run.m_DeathlessFloors;
		}
	}
}

void CPveDirector::TickPlayerState(int ClientID)
{
	CPlayerRun &Run = m_aPlayers[ClientID];
	const int Now = m_pGameServer->Server()->Tick();
	if(Run.m_aStacks[PVE_CARD_BARRIER_REFIT] &&
	   Now >= Run.m_LastDamageTick + m_pGameServer->Server()->TickSpeed() * 5 &&
	   Now >= Run.m_LastBarrierRefitTick + m_pGameServer->Server()->TickSpeed())
	{
		Run.m_LastBarrierRefitTick = Now;
		AddBarrier(ClientID, 2);
	}
	TickDrone(ClientID);
	SendBuildState(ClientID);
}

void CPveDirector::OnDroneModule(int ClientID, int Nonce, int Module)
{
	if(!IsEligiblePlayer(ClientID) || Module < PVE_DRONE_ASSAULT || Module > PVE_DRONE_REPAIR)
	{
		SendValidation(ClientID, PVE_VALIDATION_RANGE);
		return;
	}
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(Nonce <= Run.m_LastDroneNonce)
	{
		SendValidation(ClientID, PVE_VALIDATION_DUPLICATE);
		return;
	}
	const int Card = Module == PVE_DRONE_ASSAULT
						 ? PVE_CARD_ASSAULT_MODULE
						 : (Module == PVE_DRONE_GUARDIAN ? PVE_CARD_GUARDIAN_MODULE : PVE_CARD_REPAIR_MODULE);
	if(!Run.m_aStacks[PVE_CARD_DRONE_CHASSIS] || !Run.m_aStacks[Card])
	{
		SendValidation(ClientID, PVE_VALIDATION_MODULE_LOCKED);
		return;
	}
	if(m_pGameServer->Server()->Tick() < Run.m_DroneSwitchReadyTick)
	{
		SendValidation(ClientID, PVE_VALIDATION_MODULE_COOLDOWN);
		return;
	}
	Run.m_LastDroneNonce = Nonce;
	Run.m_DroneModule = Module;
	Run.m_DroneSwitchReadyTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 8;
	Run.m_DroneActionTick = m_pGameServer->Server()->Tick();
	SendBuildState(ClientID, true);
	if(m_pGameServer->m_pTutorialDirector)
		m_pGameServer->m_pTutorialDirector->OnGameplayProgress(ClientID, TUTORIAL_EVENT_DRONE);
}

void CPveDirector::ApplyArcConductor(const CAttackSource &Source, CEntity *pOriginalTarget, vec2 Origin, int Damage)
{
	const int ClientID = Source.m_Owner;
	if(m_ApplyingSecondaryEffect || !IsEligiblePlayer(ClientID) || Damage <= 0)
		return;
	CPlayerRun &Run = m_aPlayers[ClientID];
	int ArcCount = Run.m_aStacks[PVE_CARD_ARC_CONDUCTOR] ? 1 : 0;
	if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_ELECTRIC)
		ArcCount += Run.m_aStacks[PVE_CARD_VOLTAGE_BANK];
	CEntity *apHit[4] = {0, 0, 0, 0};
	for(int Arc = 0; Arc < min(4, ArcCount); Arc++)
	{
		CCharacter *pBestCharacter = 0;
		CDroid *pBestDroid = 0;
		float BestDistanceSquared = 320.0f * 320.0f;
		for(CCharacter *pCharacter = (CCharacter *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
			pCharacter;
			pCharacter = (CCharacter *)pCharacter->TypeNext())
		{
			bool Excluded = pCharacter == pOriginalTarget;
			for(int i = 0; i < Arc; i++)
				Excluded |= apHit[i] == pCharacter;
			if(Excluded || !pCharacter->m_IsBot || !pCharacter->IsAlive())
				continue;
			const vec2 Delta = Origin - pCharacter->m_Pos;
			const float DistanceSquared = dot(Delta, Delta);
			if(DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				pBestCharacter = pCharacter;
				pBestDroid = 0;
			}
		}
		for(CDroid *pDroid = (CDroid *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); pDroid;
			pDroid = (CDroid *)pDroid->TypeNext())
		{
			bool Excluded = pDroid == pOriginalTarget;
			for(int i = 0; i < Arc; i++)
				Excluded |= apHit[i] == pDroid;
			if(Excluded || pDroid->m_Health <= 0)
				continue;
			const vec2 Delta = Origin - (pDroid->m_Pos + pDroid->m_Center);
			const float DistanceSquared = dot(Delta, Delta);
			if(DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				pBestCharacter = 0;
				pBestDroid = pDroid;
			}
		}
		if(!pBestCharacter && !pBestDroid)
			break;
		CEntity *pTarget = pBestCharacter ? (CEntity *)pBestCharacter : (CEntity *)pBestDroid;
		apHit[Arc] = pTarget;
		int ArcDamage = max(1, (int)(Damage * 0.35f + 0.5f));
		CTargetStatus *pStatus = TargetStatus(pTarget, false);
		if(pStatus && pStatus->m_ConductiveEndTick >= m_pGameServer->Server()->Tick())
			ArcDamage = max(1, ArcDamage * 125 / 100);
		const vec2 TargetPos = pBestCharacter ? pBestCharacter->m_Pos : pBestDroid->m_Pos + pBestDroid->m_Center;
		new CLightning(&m_pGameServer->m_World, TargetPos, Origin);
		m_ApplyingSecondaryEffect = true;
		if(pBestCharacter)
			pBestCharacter->TakeDamage(Source, ArcDamage, vec2(0, 0), TargetPos);
		else
			pBestDroid->TakeDamage(vec2(0, 0), ArcDamage, Source, TargetPos);
		m_ApplyingSecondaryEffect = false;
	}
}

void CPveDirector::OnMeleeAttack(const CAttackSource &Source, vec2 Pos, int Damage)
{
	const int ClientID = Source.m_Owner;
	if(!Enabled() || !IsEligiblePlayer(ClientID) || !m_aPlayers[ClientID].m_aStacks[PVE_CARD_SHOCKWAVE] || Damage <= 0)
		return;
	CWeaponDefinition Definition;
	if(Source.m_Kind != EAttackSourceKind::PlayerWeapon ||
	   !CWeaponCatalog::TryGetDefinition(Source.m_Weapon.m_DefinitionId, &Definition))
		return;
	const int Charge = Source.m_Weapon.m_Level;
	const int HeavyThreshold = max(2, (Definition.m_MaxLevel + 1) / 2);
	if(Charge < HeavyThreshold || m_aPlayers[ClientID].m_ShockwaveEndTick > m_pGameServer->Server()->Tick())
		return;
	m_aPlayers[ClientID].m_ShockwaveEndTick =
		m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 2 / 5;
	m_pGameServer->CreateEffect(FX_EXPLOSION1, Pos);
	m_ApplyingSecondaryEffect = true;
	int HitCount = 0;
	CCharacter *pCharacter = (CCharacter *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for(; pCharacter; pCharacter = (CCharacter *)pCharacter->TypeNext())
		if(pCharacter->m_IsBot && pCharacter->IsAlive() && distance(Pos, pCharacter->m_Pos) <= 190.0f)
		{
			pCharacter->TakeDamage(
				Source, max(1, Damage / 2), normalize(pCharacter->m_Pos - Pos) * 4.0f, pCharacter->m_Pos);
			HitCount++;
		}
	CDroid *pDroid = (CDroid *)m_pGameServer->m_World.FindFirst(CGameWorld::ENTTYPE_DROID);
	for(; pDroid; pDroid = (CDroid *)pDroid->TypeNext())
		if(pDroid->m_Health > 0 && distance(Pos, pDroid->m_Pos + pDroid->m_Center) <= 190.0f)
		{
			pDroid->TakeDamage(
				normalize(pDroid->m_Pos + pDroid->m_Center - Pos) * 4.0f, max(1, Damage / 2), Source, pDroid->m_Pos);
			HitCount++;
		}
	m_ApplyingSecondaryEffect = false;
	if(HitCount > 0 && m_aPlayers[ClientID].m_aStacks[PVE_CARD_KINETIC_RETURN])
	{
		m_aPlayers[ClientID].m_aWeaponResources[PVE_SPECIALIZATION_MELEE - 1] =
			min(10, m_aPlayers[ClientID].m_aWeaponResources[PVE_SPECIALIZATION_MELEE - 1] + HitCount);
		AddBarrier(ClientID, HitCount * 2);
	}
}

int CPveDirector::ModifyDamage(const CAttackSource &Source, int To, int Damage)
{
	const int From = Source.m_Owner;
	if(!Enabled() || Damage <= 0 || m_ApplyingSecondaryEffect)
		return Damage;
	float Multiplier = 1.0f;
	CCharacter *pOutgoingTarget = 0;
	if(IsEligiblePlayer(From) && (To == -2 || m_pGameServer->IsBot(To)))
	{
		CPlayerRun &Run = m_aPlayers[From];
		const int Specialization = Source.m_Kind == EAttackSourceKind::PlayerWeapon
									   ? WeaponSpecialization(Source.m_Weapon)
									   : PVE_SPECIALIZATION_NONE;
		Run.m_LastEmpoweredSpecialization = PVE_SPECIALIZATION_NONE;
		pOutgoingTarget = To >= 0 ? m_pGameServer->GetPlayerChar(To) : 0;
		Multiplier += Run.m_aStacks[PVE_CARD_COMBAT_TRAINING] * 0.08f;
		if(pOutgoingTarget && pOutgoingTarget->m_MaxHealth > 0 &&
		   pOutgoingTarget->m_HiddenHealth * 100 <= pOutgoingTarget->m_MaxHealth * 30)
			Multiplier += Run.m_aStacks[PVE_CARD_FINISHER] * 0.20f;
		const int Vulnerable = VulnerablePercent(pOutgoingTarget);
		if(Vulnerable > 0)
		{
			Multiplier *= 1.0f + Vulnerable / 100.0f;
			if(Run.m_aStacks[PVE_CARD_PREDATOR])
				Multiplier += 0.15f;
			if(Specialization == PVE_SPECIALIZATION_FIREARM)
				Multiplier += Run.m_aStacks[PVE_CARD_ARMOR_PIERCER] * 0.08f;
		}
		if(pOutgoingTarget && Run.m_aStacks[PVE_CARD_CROSSFIRE] && Run.m_pDroneTarget == pOutgoingTarget)
			Multiplier += 0.15f;

		if(Specialization == PVE_SPECIALIZATION_EXPLOSIVE)
			Multiplier += Run.m_aStacks[PVE_CARD_DEMOLITION] * 0.12f;
		else if(Specialization == PVE_SPECIALIZATION_ELECTRIC)
			Multiplier += Run.m_aStacks[PVE_CARD_OVERCHARGE] * 0.12f;
		else if(Specialization == PVE_SPECIALIZATION_MELEE)
			Multiplier += Run.m_aStacks[PVE_CARD_BERSERKER] * 0.12f;
		else
		{
			Multiplier += Run.m_aStacks[PVE_CARD_HOLLOW_POINT] * 0.12f;
			if(Run.m_aStacks[PVE_CARD_SUSTAINED_FIRE])
			{
				const int Now = m_pGameServer->Server()->Tick();
				if(Now > Run.m_SustainedEndTick)
					Run.m_SustainedHits = 0;
				Run.m_SustainedHits = min(5, Run.m_SustainedHits + 1);
				Run.m_SustainedEndTick = Now + m_pGameServer->Server()->TickSpeed();
				Multiplier += Run.m_SustainedHits * 0.05f;
			}
			if(Run.m_PerfectSequenceShots > 0)
			{
				Multiplier += 0.35f;
				Run.m_PerfectSequenceShots--;
			}
			if(Run.m_TacticalReloadShots > 0)
			{
				Multiplier += 0.15f;
				Run.m_TacticalReloadShots--;
			}
		}

		int Threshold = 10;
		int Gain = 1;
		bool ResourceEnabled = false;
		if(Specialization == PVE_SPECIALIZATION_FIREARM && Run.m_aStacks[PVE_CARD_FOCUS_DRILL])
		{
			Threshold = max(7, 10 - Run.m_aStacks[PVE_CARD_CALIBRATION]);
			ResourceEnabled = true;
		}
		else if(Specialization == PVE_SPECIALIZATION_EXPLOSIVE && Run.m_aStacks[PVE_CARD_BLAST_BATTERY])
		{
			Threshold = max(3, 5 - Run.m_aStacks[PVE_CARD_PACKED_CHARGE]);
			ResourceEnabled = true;
		}
		else if(Specialization == PVE_SPECIALIZATION_ELECTRIC && Run.m_aStacks[PVE_CARD_VOLTAGE_BANK])
		{
			Gain = min(3, 1 + Run.m_aStacks[PVE_CARD_CHARGE_COIL]);
			ResourceEnabled = true;
		}
		else if(Specialization == PVE_SPECIALIZATION_MELEE && Run.m_aStacks[PVE_CARD_FURY_METER])
		{
			Gain = min(4, 1 + Run.m_aStacks[PVE_CARD_FURY_ENGINE]);
			ResourceEnabled = true;
		}
		// Non-weapon player-owned damage (for example a turret) intentionally has
		// no specialization. Never form an out-of-bounds resources[-1] reference
		// for it, even if none of the specialization branches would use it.
		int *pResource = Specialization >= PVE_SPECIALIZATION_FIREARM && Specialization <= PVE_SPECIALIZATION_MELEE
							 ? &Run.m_aWeaponResources[Specialization - 1]
							 : 0;
		if(ResourceEnabled && pResource && *pResource >= Threshold)
		{
			*pResource = 0;
			Run.m_LastEmpoweredSpecialization = Specialization;
			if(Specialization == PVE_SPECIALIZATION_FIREARM)
			{
				Multiplier += Run.m_aStacks[PVE_CARD_FOCUS_DRILL] * 0.15f;
				if(Run.m_aStacks[PVE_CARD_PERFECT_SEQUENCE])
					Run.m_PerfectSequencePending = true;
			}
			else if(Specialization == PVE_SPECIALIZATION_EXPLOSIVE)
			{
				Multiplier += Run.m_aStacks[PVE_CARD_BLAST_BATTERY] * 0.08f;
				Run.m_EmpoweredBlasts++;
			}
			else if(Specialization == PVE_SPECIALIZATION_ELECTRIC)
			{
				Run.m_FullVoltageReleases++;
				if(Run.m_aStacks[PVE_CARD_STATIC_SHIELD])
					AddBarrier(From, 10);
			}
			else
			{
				Multiplier += Run.m_aStacks[PVE_CARD_FURY_METER] * 0.20f;
				if(Run.m_aStacks[PVE_CARD_AVATAR_OF_WAR])
					Run.m_AvatarEndTick = m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 6;
			}
		}
		else if(ResourceEnabled && pResource)
			*pResource = min(10, *pResource + Gain);

		Run.m_DirectHits++;
		if(Run.m_aStacks[PVE_CARD_APEX_EXECUTION] && Run.m_DirectHits % 10 == 0)
		{
			Multiplier *= 1.75f;
			CTargetStatus *pStatus = TargetStatus(pOutgoingTarget, false);
			if(pStatus && pStatus->m_VulnerablePercent > 0)
				pStatus->m_VulnerableEndTick =
					m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 4;
		}
		if(Specialization == PVE_SPECIALIZATION_ELECTRIC && Run.m_aStacks[PVE_CARD_CAPACITOR])
		{
			Run.m_CapacitorHits++;
			if(Run.m_CapacitorHits % 5 == 0)
				Multiplier *= 2.0f;
		}
		if(Run.m_KillChainEndTick >= m_pGameServer->Server()->Tick())
			Multiplier += Run.m_KillChainStacks * 0.04f;
		if(Run.m_ReaperEndTick >= m_pGameServer->Server()->Tick())
			Multiplier += 0.15f;
		if(Run.m_AvatarEndTick >= m_pGameServer->Server()->Tick() && Specialization == PVE_SPECIALIZATION_MELEE)
			Multiplier += 0.40f;
		if(Run.m_aStacks[PVE_CARD_GLASS_EDGE])
			Multiplier += 0.35f;
		if(m_Mode == PVE_MODE_INVASION)
		{
			Multiplier += min(0.40f, Run.m_InvasionFloorsCompleted * Run.m_aStacks[PVE_CARD_ADAPTATION] * 0.04f);
			Multiplier += min(0.20f, Run.m_DeathlessFloors * Run.m_aStacks[PVE_CARD_FLOOR_MEMORY] * 0.04f);
		}
		if(m_Mode == PVE_MODE_HORDE && Run.m_aStacks[PVE_CARD_SIEGE_MASTER] &&
		   Source.m_Kind == EAttackSourceKind::Building)
			Multiplier += 0.30f;
		if(m_Mode == PVE_MODE_HORDE && Run.m_aStacks[PVE_CARD_ENDLESS_ENGINE])
			Multiplier += m_DeathlessHordeWaves * 0.05f;
		if(m_Mode == PVE_MODE_EXTRACTION && Run.m_aStacks[PVE_CARD_FINAL_DEPARTURE])
		{
			const CGameControllerExtract *pExtract =
				dynamic_cast<const CGameControllerExtract *>(m_pGameServer->m_pController);
			if(pExtract && pExtract->Evacuating())
				Multiplier += 0.30f;
		}
		if(Run.m_aStacks[PVE_CARD_LIQUID_ASSETS] && m_pGameServer->m_apPlayers[From])
			Multiplier += min(0.20f, (m_pGameServer->m_apPlayers[From]->GetGold() / 50) * 0.02f);
		if(ActiveContract() == PVE_CONTRACT_GLASS_CANNON)
			Multiplier *= 1.25f;
	}

	if(IsEligiblePlayer(To) && (From < 0 || m_pGameServer->IsBot(From)))
	{
		CPlayerRun &Run = m_aPlayers[To];
		CCharacter *pTarget = m_pGameServer->GetPlayerChar(To);
		float Reduction = Run.m_aStacks[PVE_CARD_DAMAGE_DAMPENER] * 0.08f;
		if(pTarget)
			for(int Ally = 0; Ally < MAX_CLIENTS; Ally++)
			{
				if(IsEligiblePlayer(Ally) && Ally != To && m_aPlayers[Ally].m_aStacks[PVE_CARD_GUARDIAN])
				{
					CCharacter *pAlly = m_pGameServer->GetPlayerChar(Ally);
					if(pAlly && distance(pAlly->m_Pos, pTarget->m_Pos) <= 320.0f)
					{
						Reduction += 0.12f;
						break;
					}
				}
			}
		if(Run.m_AvatarEndTick >= m_pGameServer->Server()->Tick())
			Reduction += 0.20f;
		if(Run.m_aStacks[PVE_CARD_HOLD_THE_LINE] && InHordeDefenseArea(To))
			Reduction += 0.15f;
		if(m_Mode == PVE_MODE_EXTRACTION && pTarget && Run.m_aStacks[PVE_CARD_COURIER] && pTarget->IsBombCarrier())
			Reduction += 0.10f;
		float GuardianReduction = 0.0f;
		for(int Ally = 0; Ally < MAX_CLIENTS; Ally++)
			if(pTarget && IsEligiblePlayer(Ally) && m_aPlayers[Ally].m_DroneModule == PVE_DRONE_GUARDIAN &&
			   m_aPlayers[Ally].m_pDrone && m_aPlayers[Ally].m_pDrone->Active() &&
			   distance(m_aPlayers[Ally].m_pDrone->m_Pos, pTarget->m_Pos) <= 280.0f)
				GuardianReduction = max(GuardianReduction, min(0.25f, 0.10f * DroneEfficiency(Ally)));
		Reduction += GuardianReduction;
		if(Run.m_aStacks[PVE_CARD_GLASS_EDGE])
			Multiplier *= 1.20f;
		if(ActiveContract() == PVE_CONTRACT_GLASS_CANNON)
			Multiplier *= 1.35f;
		if(ActiveContract() == PVE_CONTRACT_OVERCLOCKED_HOSTILES)
			Multiplier *= 1.25f;
		Multiplier *= 1.0f - min(0.50f, Reduction);
	}
	if(IsEligiblePlayer(To) && From == To && Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
	   WeaponSpecialization(Source.m_Weapon) == PVE_SPECIALIZATION_EXPLOSIVE &&
	   m_aPlayers[To].m_aStacks[PVE_CARD_CONTROLLED_FUSE])
		Multiplier *= 0.5f;

	int Result = max(1, (int)(Damage * clamp(Multiplier, 0.0f, 2.5f) + 0.5f));
	if(IsEligiblePlayer(To) && (From < 0 || m_pGameServer->IsBot(From)))
	{
		CPlayerRun &Run = m_aPlayers[To];
		Run.m_LastDamageTick = m_pGameServer->Server()->Tick();
		if(Run.m_Barrier > 0)
		{
			const int Absorbed = min(Result, Run.m_Barrier);
			Run.m_Barrier -= Absorbed;
			Result -= Absorbed;
			if(Run.m_Barrier == 0 && Run.m_aStacks[PVE_CARD_AEGIS_LOOP] && !Run.m_AegisLoopUsed)
			{
				Run.m_AegisLoopUsed = true;
				CCharacter *pTarget = m_pGameServer->GetPlayerChar(To);
				if(pTarget)
					pTarget->IncreaseArmor(15);
			}
			SendBuildState(To, true);
		}
	}
	if(pOutgoingTarget)
	{
		CPlayerRun &Run = m_aPlayers[From];
		ProcessHit(From, pOutgoingTarget, Result, true);
		if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_FIREARM && Run.m_aStacks[PVE_CARD_PINPOINT_BURST])
			ApplyVulnerable(pOutgoingTarget, 3, 5);
		if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_EXPLOSIVE)
		{
			if(Run.m_aStacks[PVE_CARD_BREACH_CHARGE])
				ApplyVulnerable(pOutgoingTarget, 3, 5);
			if(Run.m_aStacks[PVE_CARD_SHRAPNEL])
				ApplyBleed(pOutgoingTarget, Run.m_aStacks[PVE_CARD_SHRAPNEL], Source);
			if(Run.m_aStacks[PVE_CARD_CATACLYSM] && Run.m_EmpoweredBlasts % 3 == 0)
				ScheduleSecondaryBlast(Source, pOutgoingTarget->m_Pos, Result);
		}
		if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_ELECTRIC)
		{
			if(Run.m_aStacks[PVE_CARD_CONDUCTIVE])
			{
				CTargetStatus *pStatus = TargetStatus(pOutgoingTarget, true);
				if(pStatus)
					pStatus->m_ConductiveEndTick =
						m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 5;
			}
			if(Run.m_aStacks[PVE_CARD_THUNDERHEAD] && Run.m_FullVoltageReleases % 3 == 0)
				ApplyThunderhead(Source, pOutgoingTarget, Result);
		}
		if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_MELEE && Run.m_aStacks[PVE_CARD_OPEN_WOUND])
			ApplyBleed(pOutgoingTarget, 3, Source);
		if(Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
		   WeaponSpecialization(Source.m_Weapon) == PVE_SPECIALIZATION_MELEE && Run.m_aStacks[PVE_CARD_GUARD_BREAKER] &&
		   IsHeavyWeaponAttack(Source))
			ApplyVulnerable(pOutgoingTarget, 3, min(6, Run.m_aStacks[PVE_CARD_GUARD_BREAKER] * 2));
	}
	if(pOutgoingTarget && Source.m_Kind == EAttackSourceKind::PlayerWeapon &&
	   WeaponSpecialization(Source.m_Weapon) == PVE_SPECIALIZATION_ELECTRIC)
		ApplyArcConductor(Source, pOutgoingTarget, pOutgoingTarget->m_Pos, Result);
	return Result;
}

int CPveDirector::ModifyDroidDamage(const CAttackSource &Source, int Damage, bool Boss, CDroid *pTarget)
{
	const int From = Source.m_Owner;
	if(!Enabled() || !IsEligiblePlayer(From) || Damage <= 0 || m_ApplyingSecondaryEffect)
		return Damage;
	int Result = ModifyDamage(Source, -2, Damage);
	CPlayerRun &Run = m_aPlayers[From];
	const int Specialization = Source.m_Kind == EAttackSourceKind::PlayerWeapon ? WeaponSpecialization(Source.m_Weapon)
																				: PVE_SPECIALIZATION_NONE;
	if(pTarget && pTarget->m_MaxHealth > 0 && pTarget->m_Health * 100 <= pTarget->m_MaxHealth * 30)
		Result += (int)(Damage * Run.m_aStacks[PVE_CARD_FINISHER] * 0.20f + 0.5f);
	if(Boss)
		Result += (int)(Damage * Run.m_aStacks[PVE_CARD_BOSS_HUNTER] * 0.25f + 0.5f);
	const int Vulnerable = VulnerablePercent(pTarget);
	Result += (int)(Damage * Vulnerable / 100.0f + 0.5f);
	if(Vulnerable > 0 && Run.m_aStacks[PVE_CARD_PREDATOR])
		Result += (int)(Damage * 0.15f + 0.5f);
	if(Vulnerable > 0 && Specialization == PVE_SPECIALIZATION_FIREARM)
		Result += (int)(Damage * Run.m_aStacks[PVE_CARD_ARMOR_PIERCER] * 0.08f + 0.5f);
	if(Boss && Specialization == PVE_SPECIALIZATION_EXPLOSIVE && Run.m_aStacks[PVE_CARD_SIEGE_PAYLOAD])
		Result += (int)(Damage * 0.30f + 0.5f);
	if(m_Mode == PVE_MODE_INVASION)
	{
		CGameControllerInvasion *pInvasion = dynamic_cast<CGameControllerInvasion *>(m_pGameServer->m_pController);
		if(pInvasion && pInvasion->IsObjectiveTarget(Boss))
		{
			Result += (int)(Damage * Run.m_aStacks[PVE_CARD_OBJECTIVE_SPECIALIST] * 0.20f + 0.5f);
			if(Run.m_aStacks[PVE_CARD_DEEP_SOVEREIGN] && pInvasion->IsFinalObjective())
				Result += (int)(Damage * 0.50f + 0.5f);
			if(Specialization == PVE_SPECIALIZATION_EXPLOSIVE && Run.m_aStacks[PVE_CARD_SIEGE_PAYLOAD])
				Result += (int)(Damage * 0.30f + 0.5f);
		}
	}
	Result = clamp(Result, max(1, (int)(Damage * 0.5f)), max(1, (int)(Damage * 2.5f)));
	ProcessHit(From, pTarget, Result, true);
	if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_FIREARM && Run.m_aStacks[PVE_CARD_PINPOINT_BURST])
		ApplyVulnerable(pTarget, 3, 5);
	if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_EXPLOSIVE)
	{
		if(Run.m_aStacks[PVE_CARD_BREACH_CHARGE])
			ApplyVulnerable(pTarget, 3, 5);
		if(Run.m_aStacks[PVE_CARD_SHRAPNEL])
			ApplyBleed(pTarget, Run.m_aStacks[PVE_CARD_SHRAPNEL], Source);
		if(Run.m_aStacks[PVE_CARD_CATACLYSM] && Run.m_EmpoweredBlasts % 3 == 0)
			ScheduleSecondaryBlast(Source, pTarget->m_Pos + pTarget->m_Center, Result);
	}
	if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_ELECTRIC)
	{
		if(Run.m_aStacks[PVE_CARD_CONDUCTIVE])
		{
			CTargetStatus *pStatus = TargetStatus(pTarget, true);
			if(pStatus)
				pStatus->m_ConductiveEndTick =
					m_pGameServer->Server()->Tick() + m_pGameServer->Server()->TickSpeed() * 5;
		}
		if(Run.m_aStacks[PVE_CARD_THUNDERHEAD] && Run.m_FullVoltageReleases % 3 == 0)
			ApplyThunderhead(Source, pTarget, Result);
	}
	if(Run.m_LastEmpoweredSpecialization == PVE_SPECIALIZATION_MELEE && Run.m_aStacks[PVE_CARD_OPEN_WOUND])
		ApplyBleed(pTarget, 3, Source);
	if(Specialization == PVE_SPECIALIZATION_MELEE && Run.m_aStacks[PVE_CARD_GUARD_BREAKER] &&
	   IsHeavyWeaponAttack(Source))
		ApplyVulnerable(pTarget, 3, min(6, Run.m_aStacks[PVE_CARD_GUARD_BREAKER] * 2));
	if(pTarget && Specialization == PVE_SPECIALIZATION_ELECTRIC)
		ApplyArcConductor(Source, pTarget, pTarget->m_Pos + pTarget->m_Center, Result);
	return Result;
}

int CPveDirector::ModifyGold(int ClientID, int Amount) const
{
	if(Amount <= 0 || !IsEligiblePlayer(ClientID))
		return Amount;
	float Multiplier = 1.0f;
	if(Enabled())
		Multiplier *= 1.0f + m_aPlayers[ClientID].m_aStacks[PVE_CARD_SCAVENGER] * 0.12f;
	if(Enabled() && ActiveContract() == PVE_CONTRACT_RESOURCE_DROUGHT)
		Multiplier *= 0.5f;
	return max(1, (int)(Amount * Multiplier + 0.5f));
}

int CPveDirector::ModifyShopCost(int ClientID, int Cost) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return Cost;
	float Multiplier = 1.0f - min(0.40f, m_aPlayers[ClientID].m_aStacks[PVE_CARD_QUARTERMASTER] * 0.10f);
	if(ActiveContract() == PVE_CONTRACT_RESOURCE_DROUGHT)
		Multiplier *= 1.5f;
	return max(1, (int)(Cost * Multiplier + 0.5f));
}

int CPveDirector::ModifyBuildingCost(int ClientID, int Cost) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return Cost;
	float Multiplier = m_aPlayers[ClientID].m_aStacks[PVE_CARD_ENGINEER] ? 0.80f : 1.0f;
	if(ActiveContract() == PVE_CONTRACT_FORTIFICATION_TAX)
		Multiplier *= 1.75f;
	return max(1, (int)(Cost * Multiplier + 0.99f));
}

int CPveDirector::ModifyBuildingRepair(int ClientID, int Amount) const
{
	if(!IsEligiblePlayer(ClientID) || Amount <= 0)
		return Amount;
	float Multiplier = 1.0f;
	if(Enabled() && m_aPlayers[ClientID].m_aStacks[PVE_CARD_ENGINEER])
		Multiplier *= 1.25f;
	return max(1, (int)(Amount * Multiplier + 0.5f));
}

void CPveDirector::RefundBuilding(int ClientID, int KitCost) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID) || KitCost <= 0 || !m_aPlayers[ClientID].m_aStacks[PVE_CARD_RECYCLER])
		return;
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientID);
	const int Refund = max(1, (KitCost * 40 + 99) / 100);
	if(pChr)
		pChr->AddKits(Refund);
	else if(m_pGameServer->m_apPlayers[ClientID])
	{
		CPlayerData *pData =
			m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
		if(pData)
			pData->m_Kits = min(99, pData->m_Kits + Refund);
	}
}

float CPveDirector::ModifyExplosionRadius(int Owner, float Radius) const
{
	if(!Enabled() || !IsEligiblePlayer(Owner))
		return Radius;
	float Multiplier = m_aPlayers[Owner].m_aStacks[PVE_CARD_WIDE_BLAST] ? 1.25f : 1.0f;
	const CPlayerRun &Run = m_aPlayers[Owner];
	const int Threshold = max(3, 5 - Run.m_aStacks[PVE_CARD_PACKED_CHARGE]);
	if(Run.m_aStacks[PVE_CARD_CONTROLLED_FUSE] && Run.m_aWeaponResources[PVE_SPECIALIZATION_EXPLOSIVE - 1] >= Threshold)
		Multiplier += 0.20f;
	return Radius * Multiplier;
}

float CPveDirector::CooldownReduction(int ClientID, const CWeaponSpec &Weapon) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return 0.0f;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	float Reduction = Run.m_aStacks[PVE_CARD_QUICK_HANDS] * 0.08f;
	CResolvedWeaponProfile Profile;
	if(!CWeaponCatalog::TryResolve(Weapon, &Profile))
		return 0.0f;
	const int RenderType = Profile.m_Visual.m_RenderType;
	const bool Firearm = !Profile.m_Combat.m_ExplosiveProjectile && Profile.m_Combat.m_ElectroAmount <= 0.0f &&
						 !Profile.m_Combat.m_LaserWeapon && Profile.m_Combat.m_FiringType != WFT_MELEE &&
						 RenderType != WRT_MELEE && RenderType != WRT_MELEESMALL && RenderType != WRT_SPIN;
	if(Firearm && Run.m_aStacks[PVE_CARD_GUNSLINGER])
		Reduction += 0.20f;
	return min(0.30f, Reduction);
}

float CPveDirector::MovementMultiplier(int ClientID) const
{
	if(Enabled() && ClientID >= 0 && ClientID < MAX_CLIENTS && m_pGameServer->IsBot(ClientID))
		return EnemySpeedMultiplier();
	if(!Enabled() || !IsEligiblePlayer(ClientID) || m_Mode != PVE_MODE_EXTRACTION)
		return 1.0f;
	const CGameControllerExtract *pExtract = dynamic_cast<const CGameControllerExtract *>(m_pGameServer->m_pController);
	CCharacter *pCharacter = m_pGameServer->GetPlayerChar(ClientID);
	float Result = 1.0f;
	if(pCharacter && pCharacter->IsBombCarrier())
	{
		if(m_aPlayers[ClientID].m_aStacks[PVE_CARD_COURIER])
			Result += 0.10f;
		if(ActiveContract() == PVE_CONTRACT_HEAVY_CARGO && pCharacter->IsBombCarrier())
			Result -= 0.20f;
	}
	if(pExtract && pExtract->Evacuating())
	{
		if(m_aPlayers[ClientID].m_aStacks[PVE_CARD_ESCAPE_ARTIST])
			Result += 0.15f;
		if(m_aPlayers[ClientID].m_aStacks[PVE_CARD_FINAL_DEPARTURE])
			Result += 0.15f;
	}
	return Result;
}

float CPveDirector::InteractionSpeedBonus(int ClientID) const
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return 1.0f;
	const CPlayerRun &Run = m_aPlayers[ClientID];
	float Bonus = Run.m_aStacks[PVE_CARD_SABOTEUR] * 0.25f + Run.m_aStacks[PVE_CARD_SIGNAL_HACKER] * 0.10f;
	if(Run.m_aStacks[PVE_CARD_CARTOGRAPHER])
		Bonus += 0.15f;
	return 1.0f + min(0.60f, Bonus);
}

float CPveDirector::EnemyCountMultiplier() const
{
	float Multiplier = 1.0f;
	if(ActiveContract() == PVE_CONTRACT_PURGE_PROTOCOL)
		Multiplier = 1.35f;
	else if(ActiveContract() == PVE_CONTRACT_DOUBLE_WAVE)
		Multiplier = 1.50f;
	else if(ActiveContract() == PVE_CONTRACT_RISING_TIDE)
	{
		const float aMultipliers[4] = {1.15f, 1.30f, 1.45f, 1.60f};
		Multiplier = aMultipliers[clamp(m_ContractProgress, 0, 3)];
	}
	return Multiplier;
}

float CPveDirector::DeadlineMultiplier() const
{
	const float ContractMultiplier = ActiveContract() == PVE_CONTRACT_TIGHT_DEADLINE ? 0.70f : 1.0f;
	return ContractMultiplier;
}

float CPveDirector::ReinforcementMultiplier() const
{
	const float ContractMultiplier = ActiveContract() == PVE_CONTRACT_OVERRUN ? 2.0f : 1.0f;
	return ContractMultiplier;
}

float CPveDirector::EnemySpeedMultiplier() const
{
	const float ContractMultiplier = ActiveContract() == PVE_CONTRACT_OVERCLOCKED_HOSTILES ? 1.15f : 1.0f;
	return ContractMultiplier;
}

float CPveDirector::EnemyHealthMultiplier() const
{
	const float ContractMultiplier =
		ActiveContract() == PVE_CONTRACT_RISING_TIDE && m_ContractProgress >= 3 ? 1.25f : 1.0f;
	return ContractMultiplier;
}

bool CPveDirector::ShopsAllowed() const
{
	return ActiveContract() != PVE_CONTRACT_SEALED_SUPPLIES;
}

bool CPveDirector::RespawnAllowed() const
{
	return ActiveContract() != PVE_CONTRACT_NO_RESPAWN;
}

bool CPveDirector::UseLastStand(int ClientID)
{
	if(!Enabled() || !IsEligiblePlayer(ClientID))
		return false;
	CPlayerRun &Run = m_aPlayers[ClientID];
	if(!Run.m_aStacks[PVE_CARD_LAST_STAND] || Run.m_LastStandUsed)
		return false;
	Run.m_LastStandUsed = true;
	CPlayerData *pData =
		m_pGameServer->Server()->GetPlayerData(ClientID, m_pGameServer->m_apPlayers[ClientID]->GetColorID());
	if(pData)
		pData->m_PveLastStandUsed = true;
	return true;
}

int CPveDirector::TeamCheckpoint() const
{
	int Checkpoint = 9999;
	bool Found = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsEligiblePlayer(i))
			continue;
		if(!m_aPlayers[i].m_ProgressSynced)
			return 1;
		Checkpoint = min(Checkpoint, m_aPlayers[i].m_PreferredCheckpoint);
		Found = true;
	}
	return Found ? Checkpoint : 1;
}

bool CPveDirector::ProgressReady() const
{
	if(!Enabled() || EligiblePlayerCount() <= 0)
		return false;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsEligiblePlayer(i) && !m_aPlayers[i].m_ProgressSynced)
			return false;
	return true;
}

void CPveDirector::ClearRun()
{
	if(InIntermission())
		m_pGameServer->m_World.m_Paused = m_WasWorldPaused;
	if(m_pBlackBoxRadar)
	{
		m_pGameServer->m_World.DestroyEntity(m_pBlackBoxRadar);
		m_pBlackBoxRadar = 0;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayerRun &Run = m_aPlayers[i];
		DestroyDrone(i);
		Run.m_Choices = 0;
		Run.m_ChoicePending = false;
		Run.m_LastStandUsed = false;
		Run.m_KillChainStacks = 0;
		Run.m_KillChainEndTick = 0;
		Run.m_SustainedHits = 0;
		Run.m_SustainedEndTick = 0;
		Run.m_CapacitorHits = 0;
		Run.m_ReaperKills = 0;
		Run.m_ReaperChainEndTick = 0;
		Run.m_ReaperEndTick = 0;
		Run.m_ShockwaveEndTick = 0;
		Run.m_InvasionFloorsCompleted = 0;
		Run.m_StageSuppliesApplied = false;
		Run.m_EmergencyPlatingUsed = false;
		Run.m_PendingArmor = 0;
		Run.m_PendingKits = 0;
		Run.m_PendingAmmo = false;
		Run.m_LegendaryCard = -1;
		for(int Resource = 0; Resource < 4; Resource++)
			Run.m_aWeaponResources[Resource] = 0;
		Run.m_Barrier = 0;
		Run.m_DroneModule = PVE_DRONE_NONE;
		Run.m_DroneSwitchReadyTick = 0;
		Run.m_DeathlessFloors = 0;
		for(int CardID = 0; CardID < NUM_PVE_CARDS; CardID++)
		{
			if(Run.m_aStacks[CardID] > 0 && m_pGameServer->m_apPlayers[i] && !m_pGameServer->m_apPlayers[i]->m_IsBot)
			{
				CNetMsg_Sv_PvePerk Msg;
				Msg.m_ClientID = i;
				Msg.m_Card = CardID;
				Msg.m_Stacks = 0;
				Msg.m_Choices = 0;
				m_pGameServer->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
			}
			Run.m_aStacks[CardID] = 0;
		}
		if(m_pGameServer->m_apPlayers[i] && !m_pGameServer->m_apPlayers[i]->m_IsBot)
		{
			CPlayerData *pData = m_pGameServer->Server()->GetPlayerData(i, m_pGameServer->m_apPlayers[i]->GetColorID());
			if(pData)
				pData->Reset();
		}
	}
	m_UsedContracts = 0;
	m_IntermissionState = PVE_INTERMISSION_NONE;
	m_EndTick = 0;
	m_LastIntermissionSyncTick = 0;
	m_PerkTargetChoices = 0;
	m_PerkAfterContract = false;
	m_ActiveContract = -1;
	m_ContractState = PVE_CONTRACT_STATE_NONE;
	m_ContractStartTick = 0;
	m_ContractEndTick = 0;
	m_ContractProgress = 0;
	m_ContractTarget = 0;
	m_ContractParticipants = 0;
	m_pEliteContractBoss = 0;
	mem_zero(m_apEliteContractGuards, sizeof(m_apEliteContractGuards));
	m_NumEliteContractGuards = 0;
	mem_zero(m_aTargetStatus, sizeof(m_aTargetStatus));
	m_TargetStatusCount = 0;
	m_TargetSummaryTick = -1;
	m_VulnerableTargetCount = 0;
	m_BleedingTargetCount = 0;
	m_DeathlessHordeWaves = 0;
	m_AnyStageDeath = false;
	SendContractStatus();
}
