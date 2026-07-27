#ifndef GAME_CLIENT_COMPONENTS_PVE_ROGUELITE_H
#define GAME_CLIENT_COMPONENTS_PVE_ROGUELITE_H

#include <game/client/component.h>
#include <game/pve_roguelite.h>

class CPveRoguelite : public CComponent
{
	bool m_ChoiceActive;
	bool m_ContractVoteActive;
	bool m_InvasionRetryVoteActive;
	bool m_InvasionRetryResultActive;
	bool m_ResearchVisible;
	bool m_ProgressSent;
	bool m_ProgressStorageWritable;
	bool m_MouseTrigger;
	int m_ChoiceNonce;
	int m_ChoiceSequence;
	int m_ContractNonce;
	int m_ChoiceEndTick;
	int m_ContractEndTick;
	int m_InvasionRetryNonce;
	int m_InvasionRetryEndTick;
	int m_InvasionRetryFloor;
	int m_aInvasionRetryVotes[2];
	int m_SelectedInvasionRetry;
	int m_InvasionRetryResult;
	int m_InvasionRetryResultEndTick;
	char m_aInvasionRetryPlayerName[64];
	int m_aChoiceCards[3];
	int m_aChoiceStacks[3];
	int m_aContractOptions[2];
	int m_aContractVotes[2];
	int m_FocusedChoice;
	int m_SelectedContract;
	int m_ActiveContract;
	int m_ContractState;
	int m_ContractProgress;
	int m_ContractTarget;
	int m_ContractStatusEndTick;
	int m_aWeaponResources[4];
	int m_Barrier;
	int m_VulnerableTargets;
	int m_BleedingTargets;
	int m_LegendaryCard;
	int m_DroneModule;
	int m_DroneSwitchReadyTick;
	int m_DroneNonce;
	int m_DroneHealth;
	int m_DroneState;
	int m_DroneActionTick;
	bool m_DroneWheelActive;
	bool m_DroneTutorialSeen;
	int m_DroneWheelSelected;
	vec2 m_DroneWheelMouse;
	int m_ValidationCode;
	int64 m_ValidationUntil;
	int m_SelectedResearch;
	int m_ResearchTab;
	int m_ResearchBranch;
	int m_ResearchRoute;
	int m_ResearchNonce;
	int m_DebugChoiceScreenshotFrames;
	int m_DebugResearchScreenshotFrames;
	int m_DebugBuildScreenshotFrames;
	int m_DebugGameScreenshotFrames;
	int64 m_DebugGameScreenshotEarliestTime;
	int m_DebugScreenshotPage;
	bool m_DebugBuildPreview;
	int m_aRunPerks[NUM_PVE_CARDS];
	int m_aNodeButtonIDs[NUM_PVE_CARDS];
	int m_aTabButtonIDs[3];
	int m_aBranchButtonIDs[4];
	int m_aRouteButtonIDs[3];
	int m_BuyButtonID;
	int m_CheckpointButtonID;
	vec2 m_SelectorMouse;
	float m_AppearAmount;
	float m_ResearchAppearAmount;
	float m_aBranchExpand[4];
	float m_aRouteExpand[3];
	float m_ResearchProgressDisplay;
	float m_SelectionPulse;
	float m_aCardFocus[3];
	int m_ResearchAnimTab;
	int m_TutorialMoveMask;
	int m_TutorialFireCount;
	int m_TutorialKillCount;
	int m_TutorialObjectiveSignature;
	bool m_TutorialPerkChosen;
	int m_TutorialNonce;
	int m_TutorialProgress;
	int m_TutorialTarget;
	int m_TutorialFlags;

	CPveResearchMask ParseResearchMask() const;
	void StoreResearchMask(CPveResearchMask Mask);
	void LoadProgress();
	void SaveProgress();
	void SendChoice(int Slot);
	void SendContractVote(int Slot);
	void SendInvasionRetryVote(int Choice);
	void SendDroneModule(int Module);
	void DrawSelectionOverlay(bool ContractVote);
	void DrawInvasionRetryVote();
	void DrawInvasionRetryResult();
	void DrawContractHud();
	void DrawBuildHud();
	void DrawDrones();
	void DrawTutorialHud();
	void AdvanceTutorial();
	void TickTutorial();
	void DrawDroneWheel();
	void DrawText(float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth = -1.0f, int Align = -1);
	void DrawWrappedText(float X, float Y, float Size, const char *pText, vec4 Color, float MaxWidth, int MaxLines);
	void DrawPanel(const CUIRect &Rect, vec4 Color, float Rounding = 8.0f);
	void DrawIcon(int Image, int Sprite, float X, float Y, float Size, vec4 Color);
	bool CanBuyResearch(int CardID, const CPveResearchMask &Mask) const;
	bool TutorialResearchActive() const;
	void BuySelectedResearch();
	void CycleCheckpoint();
	static void ConDebugChoice(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugContract(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugInvasionRetry(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugResearch(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugBuild(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugScreenshot(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugGameScreenshot(IConsole::IResult *pResult, void *pUserData);
	static void ConDroneModule(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyDroneWheel(IConsole::IResult *pResult, void *pUserData);

public:
	void SendTutorialAction(int Action, int Value = 0);
	CPveRoguelite();
	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnInit();
	virtual void OnRelease();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnInput(IInput::CEvent Event);
	virtual bool OnMouseMove(float x, float y);

	void SyncProgress();
	void RenderResearch(CUIRect MainView);
	void RenderMenuDebugOverlay();
	void RenderBuildDebug();
	int ShopCost(int BaseCost) const;
	int BuildingCost(int BaseCost) const;
	bool ChoiceActive() const { return m_ChoiceActive || m_ContractVoteActive || m_InvasionRetryVoteActive || m_InvasionRetryResultActive; }
	bool DroneWheelActive() const { return m_DroneWheelActive; }
	void OnGameOver();

	// World drones must paint with players/droids (before the light pass), not with HUD overlays.
	class CRenderWorld : public CComponent
	{
	public:
		CPveRoguelite *m_pRoguelite;
		virtual void OnRender() { if(m_pRoguelite) m_pRoguelite->DrawDrones(); }
	};
	CRenderWorld m_RenderWorld;
};

#endif
