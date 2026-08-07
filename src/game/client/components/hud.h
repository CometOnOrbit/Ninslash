

#ifndef GAME_CLIENT_COMPONENTS_HUD_H
#define GAME_CLIENT_COMPONENTS_HUD_H
#include <game/client/component.h>

class CHud : public CComponent
{
	enum
	{
		DEBUG_STATUS_PAUSED = 1 << 0,
		DEBUG_STATUS_CONNECTION = 1 << 1,
		DEBUG_STATUS_WARMUP = 1 << 2,
		DEBUG_STATUS_READY = 1 << 3,
	};
	enum
	{
		STATUS_STACK_PAUSED = 0,
		STATUS_STACK_READY,
		STATUS_STACK_CONNECTION,
		STATUS_STACK_VOTE,
	};

	float m_Width, m_Height;
	float m_AverageFPS;
	int m_DebugStatusMask;
	int64 m_DebugStatusUntil;
	int m_DebugStatusScreenshotFrames;
	int m_LastObjectiveSignature;
	int64 m_ObjectiveTransitionStart;
	int64 m_ObjectiveNoticeUntil;
	int64 m_LastHitEvent;
	int64 m_LastHitSound;
	int64 m_HitMarkerUntil;
	int m_HitDamage;
	int m_HitTargetType;
	bool m_HitKilled;
	float m_aStatusAppear[4];
	int64 m_LastAnimationTime;

	static void ConDebugStatus(IConsole::IResult *pResult, void *pUserData);
	bool DebugStatusActive(int Flag) const;
	bool WarmupActive() const;
	bool PausedNoticeActive() const;
	bool ReadyNoticeActive() const;
	bool ConnectionNoticeActive() const;
	float StatusStackY(int BeforeFlag) const;

	void RenderCursor();

	void DrawCircular(float x, float y, float r, int Segments, int FillAmount, int Max, bool Flip = false);

	void RenderFps();
	void RenderConnectionWarning();
	void RenderStartCountdown();
	void RenderReadyUpNotification();
	void RenderTeambalanceWarning();
	void RenderVoting();
	void RenderLowHealthVignette(const CNetObj_Character *pCharacter);
	void RenderHealthAndAmmo(const CNetObj_Character *pCharacter);
	void RenderGameTimer();
	void RenderRaceTimer();
	void RenderPauseNotification();
	void RenderSuddenDeath();
	void RenderScoreHud();
	void RenderSpectatorHud();
	void RenderMovementInformation();
	void UpdateAnimations();
	void RenderStatusNotice(const char *pText, float Y, vec4 AccentColor, float Amount);

	// Shared HUD layout anchors (screen space: height=300).
	float ScoreHudTop() const;
	float BottomReservedHeight() const;

	void MapscreenToGroup(float CenterX, float CenterY, struct CMapItemGroup *PGroup);

  public:
	CHud();
	void RenderObjective();
	void RenderPveEnvironment();
	void OnHitConfirm(vec2 Pos, int Damage, int TargetType, bool Killed);

	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
};

#endif
