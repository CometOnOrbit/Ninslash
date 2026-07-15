#ifndef GAME_SERVER_PVE_DIRECTOR_H
#define GAME_SERVER_PVE_DIRECTOR_H

#include <base/vmath.h>
#include <game/pve_roguelite.h>

class CGameContext;
class CCharacter;
class CDroid;
class CEntity;
class CPveDrone;

class CPveDirector
{
	struct CPlayerRun
	{
		bool m_Connected;
		bool m_ProgressSynced;
		bool m_ChoicePending;
		bool m_LastStandUsed;
		int m_ResearchPoints;
		CPveResearchMask m_ResearchMask;
		int m_HighestInvasion;
		int m_PreferredCheckpoint;
		int m_Choices;
		int m_ChoiceNonce;
		int m_LastChoiceNonce;
		int m_aOffered[3];
		int m_aStacks[NUM_PVE_CARDS];
		int m_ContractVote;
		int m_LastContractNonce;
		int m_OperationVote;
		int m_LastOperationNonce;
		int m_LastResearchNonce;
		int m_LegendaryCard;
		int m_aWeaponResources[4];
		int m_Barrier;
		int m_StageKills;
		int m_SecondWindTriggers;
		int m_SalvageKits;
		int m_DirectHits;
		int m_EmpoweredBlasts;
		int m_FullVoltageReleases;
		int m_LastEmpoweredSpecialization;
		int m_PerfectSequenceShots;
		bool m_PerfectSequencePending;
		int m_TacticalReloadShots;
		int m_DeathlessFloors;
		int m_AvatarEndTick;
		int m_LastDamageTick;
		int m_LastBarrierRefitTick;
		bool m_AegisLoopUsed;
		bool m_ObjectiveCacheUsed;
		bool m_CleanExitUsed;
		bool m_DiedThisStage;
		CPveDrone *m_pDrone;
		CEntity *m_pDroneTarget;
		int m_DroneModule;
		int m_DroneSwitchReadyTick;
		int m_DroneActionTick;
		int m_LastDroneNonce;
		int m_LastBuildStateTick;
		int m_aLastBuildState[11];
		int m_KillChainStacks;
		int m_KillChainEndTick;
		int m_SustainedHits;
		int m_SustainedEndTick;
		int m_CapacitorHits;
		int m_ReaperKills;
		int m_ReaperChainEndTick;
		int m_ReaperEndTick;
		int m_ShockwaveEndTick;
		int m_InvasionFloorsCompleted;
		bool m_StageSuppliesApplied;
		bool m_EmergencyPlatingUsed;
		int m_PendingArmor;
		int m_PendingKits;
		bool m_PendingAmmo;

		void Reset();
	};

	struct CTargetStatus
	{
		CEntity *m_pTarget;
		int m_aMarkingHits[MAX_CLIENTS];
		int m_VulnerablePercent;
		int m_VulnerableEndTick;
		int m_BleedStacks;
		int m_BleedEndTick;
		int m_BleedNextTick;
		int m_BleedOwner;
		int m_BleedWeapon;
		int m_ConductiveEndTick;
	};

	struct CPendingBlast
	{
		vec2 m_Pos;
		int m_Owner;
		int m_Weapon;
		int m_Damage;
		int m_Tick;
	};

	CGameContext *m_pGameServer;
	CPlayerRun m_aPlayers[MAX_CLIENTS];
	int m_Mode;
	int m_IntermissionState;
	int m_EndTick;
	int m_LastIntermissionSyncTick;
	int m_PerkTargetChoices;
	int m_NextNonce;
	bool m_WasWorldPaused;
	bool m_PerkAfterContract;
	bool m_PendingPerkChoice;
	bool m_PendingContractVote;
	int m_aContractOptions[2];
	int m_ContractNonce;
	int m_aOperationOptions[2];
	int m_OperationNonce;
	int m_UsedContracts;
	unsigned m_UsedOperations;
	int m_ActiveOperation;
	int m_OperationState;
	int m_ActiveContract;
	int m_ContractState;
	int m_ContractStartTick;
	int m_ContractEndTick;
	int m_ContractProgress;
	int m_ContractTarget;
	unsigned long long m_ContractParticipants;
	int m_OperationEndTick;
	CDroid *m_pEliteContractBoss;
	CDroid *m_apEliteContractGuards[8];
	int m_NumEliteContractGuards;
	class CRadar *m_pBlackBoxRadar;
	vec2 m_BlackBoxPos;
	int m_BlackBoxHoldTicks;
	bool m_ApplyingSecondaryEffect;
	CTargetStatus m_aTargetStatus[256];
	int m_TargetSummaryTick;
	int m_VulnerableTargetCount;
	int m_BleedingTargetCount;
	int m_DeathlessHordeWaves;
	bool m_AnyStageDeath;
	CPendingBlast m_aPendingBlasts[32];

	bool IsEligiblePlayer(int ClientID) const;
	bool OperationsEnabled() const;
	int EligiblePlayerCount() const;
	int CurrentWeaponSpecialization(int ClientID) const;
	int WeaponSpecialization(int Weapon) const;
	bool CardEligible(int ClientID, int CardID) const;
	int DrawCard(int ClientID, const bool *pExcluded, int RequiredSpecialization, bool CommonOnly) const;
	void GenerateChoices(int ClientID);
	void BeginOperationVote(bool ContractVote, bool PerkChoice);
	void BeginContractVote(bool PerkAfterContract);
	void BeginPerkChoice();
	void FinishOperationVote();
	void FinishContractVote();
	void FinishIntermission();
	void ApplyChoice(int ClientID, int CardID, bool Catchup = false);
	void SendChoice(int ClientID);
	void SendOperationVote(int ClientID = -1);
	void SendOperationState(int ClientID = -1);
	void SendContractVote(int ClientID = -1);
	void SendContractStatus(int ClientID = -1);
	void SendProgress(int ClientID);
	void SendValidation(int ClientID, int Code);
	bool AllChoicesComplete() const;
	bool AllOperationVotesComplete() const;
	bool AllContractVotesComplete() const;
	void GrantCatchup(int ClientID);
	void TickBlackBox();
	void ApplyStageSupplies(int ClientID);
	void ApplyArcConductor(int ClientID, class CEntity *pOriginalTarget, vec2 Origin, int Weapon, int Damage);
	CTargetStatus *TargetStatus(CEntity *pTarget, bool Create);
	void ClearTargetStatus(CEntity *pTarget);
	void ApplyVulnerable(CEntity *pTarget, int Percent, int Seconds);
	void ApplyBleed(CEntity *pTarget, int Stacks, int ClientID, int Weapon);
	int VulnerablePercent(CEntity *pTarget);
	void ProcessHit(int ClientID, CEntity *pTarget, int Weapon, int Damage, bool Direct);
	void TickTargetStatuses();
	void UpdateTargetSummary();
	void ScheduleSecondaryBlast(int ClientID, int Weapon, vec2 Pos, int Damage);
	void TickPendingBlasts();
	void ApplyThunderhead(int ClientID, CEntity *pTarget, int Weapon, int Damage);
	void TickPlayerState(int ClientID);
	void TickDrone(int ClientID);
	float DroneEfficiency(int ClientID) const;
	bool InHordeDefenseArea(int ClientID) const;
	void SendBuildState(int ClientID, bool Force = false);
	void DestroyDrone(int ClientID);

public:
	explicit CPveDirector(CGameContext *pGameServer);
	~CPveDirector();

	bool Enabled() const;
	bool InIntermission() const { return m_IntermissionState != PVE_INTERMISSION_NONE; }
	int Mode() const { return m_Mode; }
	int ActiveOperation() const { return m_OperationState == PVE_OPERATION_STATE_ACTIVE ? m_ActiveOperation : -1; }
	int ActiveContract() const { return Enabled() && m_ContractState == PVE_CONTRACT_STATE_ACTIVE ? m_ActiveContract : -1; }
	int ContractState() const { return m_ContractState; }

	void Tick();
	void OnClientEnter(int ClientID);
	void OnClientDrop(int ClientID);
	void OnProgress(int ClientID, int Version, int Points, int Mask0, int Mask1, int Mask2, int Mask3, int HighestInvasion, int PreferredCheckpoint);
	void OnResearchBuy(int ClientID, int Nonce, int CardID);
	void OnChoice(int ClientID, int Nonce, int CardID);
	void OnOperationVote(int ClientID, int Nonce, int OperationID);
	void OnContractVote(int ClientID, int Nonce, int ContractID);
	void OnDroneModule(int ClientID, int Nonce, int Module);

	void StartIntermission(bool ContractVote, bool PerkChoice);
	void OnStageStart();
	void OnPlayerSpawn(int ClientID);
	void OnStageComplete(bool Success = true);
	void OnPlayerDeath(int ClientID);
	void OnBossKilled(bool ContractBoss = false);
	void OnEnemyKilled(int ClientID, int Weapon, vec2 Pos, CEntity *pTarget = 0);
	void OnDroidKilled(CDroid *pDroid, int ClientID, int Weapon);
	void OnMeleeAttack(int ClientID, int Weapon, vec2 Pos, int Damage);
	void OnSwitchTriggered();
	void OnObjectiveComplete();
	void OnGoldSpent(int ClientID, int Amount);
	void OnFullReload(int ClientID);
	void OnEvacuationStarted();
	void OnCargoDelivered();
	void RegisterEliteContractBoss(CDroid *pBoss);
	void CompleteContract(bool Success);
	void RewardResearch(int Amount, int Reason, int HighestInvasion = 0);

	int ModifyDamage(int From, int To, int Weapon, int Damage);
	int ModifyDroidDamage(int From, int Weapon, int Damage, bool Boss, CDroid *pTarget);
	int ModifyGold(int ClientID, int Amount) const;
	int ModifyShopCost(int ClientID, int Cost) const;
	int ModifyBuildingCost(int ClientID, int Cost) const;
	int ModifyBuildingRepair(int ClientID, int Amount) const;
	int ModifyRecovery(int ClientID, int Amount, bool Health) const;
	int AddBarrier(int ClientID, int Amount);
	void RefundBuilding(int ClientID, int KitCost) const;
	float ModifyExplosionRadius(int Owner, float Radius) const;
	float CooldownReduction(int ClientID, int Weapon) const;
	float MovementMultiplier(int ClientID) const;
	float EnemyCountMultiplier() const;
	float DeadlineMultiplier() const;
	float ReinforcementMultiplier() const;
	float EnemySpeedMultiplier() const;
	float EnemyHealthMultiplier() const;
	float OperationRepairMultiplier() const;
	float OperationGoldMultiplier() const;
	bool RespawnAllowed() const;
	bool ShopsAllowed() const;
	bool UseLastStand(int ClientID);
	int PerkStacks(int ClientID, int CardID) const;
	int DroneModule(int ClientID) const;
	int TeamCheckpoint() const;
	bool ProgressReady() const;
	float OperationEnemyCountMultiplier() const;
	float OperationDeadlineMultiplier() const;
	float OperationReinforcementMultiplier() const;
	float OperationEnemySpeedMultiplier() const;
	float OperationEnemyHealthMultiplier() const;
	void ClearRun();
};

#endif
