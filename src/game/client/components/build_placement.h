#ifndef GAME_CLIENT_COMPONENTS_BUILD_PLACEMENT_H
#define GAME_CLIENT_COMPONENTS_BUILD_PLACEMENT_H

#include <game/client/component.h>

#include "build_placement_logic.h"

class CBuildPlacement : public CComponent
{
	BuildPlacementLogic::CStateMachine m_State;
	BuildPlacementLogic::CPlacementTrigger m_Trigger;
	BuildPlacementLogic::CBuildPlacementResult m_Result;
	vec2 m_WheelCursor;
	bool m_ConfirmPressed;
	int m_DebugWheel;
	int m_DebugBuilding;
	int m_DebugValidity;

	static void ConKeyBuildMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugWheel(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugPlacement(IConsole::IResult *pResult, void *pUserData);

	bool CanOpenWheel() const;
	int SelectedBuilding() const;
	int BuildingPrice(int Building) const;
	bool CanAfford(int Building) const;
	void OpenWheel();
	void ReleaseWheel();
	void Cancel();
	void EvaluatePlacement();
	void SendPlacement();
	void RenderWheel();
	void RenderPlacement();
	void MapGameGroup();
	const char *InvalidReasonText(BuildPlacementLogic::EInvalidReason Reason) const;

public:
	CBuildPlacement();
	void OnReset() override;
	void OnConsoleInit() override;
	void OnRender() override;
	bool OnMouseMove(float x, float y) override;
	bool OnInput(IInput::CEvent Event) override;

	bool WheelActive() const { return m_State.State() == BuildPlacementLogic::STATE_WHEEL || m_DebugWheel != 0; }
	bool PlacementActive() const { return m_State.State() == BuildPlacementLogic::STATE_PLACEMENT || m_DebugBuilding >= 0; }
	bool Active() const { return WheelActive() || PlacementActive(); }
};

#endif
