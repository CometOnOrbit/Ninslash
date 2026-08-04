#ifndef GAME_CLIENT_GAMECLIENT_H
#define GAME_CLIENT_GAMECLIENT_H

#include <base/vmath.h>
#include <engine/client.h>
#include <engine/console.h>
#include <engine/shared/config.h>
#include <game/layers.h>
#include <game/gamecore.h>
#include <game/weapon_catalog.h>
#include <game/challenge_variant.h>
#include <game/challenge_script_runtime.h>
#include "render.h"

struct CNetObj_WeaponRuntime;
struct CNetObj_RaceInfo;
struct CNetObj_RacePlayer;
struct CNetObj_ChallengeRuntime;
struct CNetMsg_Sv_ChallengeInfo;
struct CNetObj_PveEnvironmentStatus;

class CGameClient : public IGameClient
{
	class CStack
	{
	  public:
		enum
		{
			MAX_COMPONENTS = 128,
		};

		CStack();
		void Add(class CComponent *pComponent);

		class CComponent *m_paComponents[MAX_COMPONENTS];
		int m_Num;
	};

	CStack m_All;
	CStack m_Input;
	CNetObjHandler m_NetObjHandler;

	class IEngine *m_pEngine;
	class IInput *m_pInput;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class IClient *m_pClient;
	class ISound *m_pSound;
	class IGamepad *m_pGamepad;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IDemoPlayer *m_pDemoPlayer;
	class IDemoRecorder *m_pDemoRecorder;
	class IServerBrowser *m_pServerBrowser;
	class IEditor *m_pEditor;
	class IFriends *m_pFriends;

	class CCustomStuff *m_pCustomStuff;
	class CSkelebank *m_pSkelebank;

	CLayers m_Layers;
	class CCollision m_Collision;
	CUI m_UI;

	void DispatchInput();
	void ProcessEvents();
	void UpdatePositions();

	int m_PredictedTick;
	int m_LastNewPredictedTick;

	int64 m_LastSendInfo;

	static void ConTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConKill(IConsole::IResult *pResult, void *pUserData);
	static void ConReadyChange(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponReloadClient(IConsole::IResult *pResult, void *pUserData);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult,
										  void *pUserData,
										  IConsole::FCommandCallback pfnCallback,
										  void *pCallbackUserData);

  public:
	struct CPredictedScriptEntity
	{
		bool m_Active;
		CWeaponSpec m_Weapon;
		int m_Kind;
		vec2 m_Pos;
		vec2 m_From;
		vec2 m_Velocity;
		int m_Radius;
		int m_Life;
	};
	CPredictedScriptEntity m_aPredictedScriptEntities[64];
	int m_NumPredictedScriptEntities;

	// Community challenge Lua is optional. The server snapshot remains the
	// authority and overwrites this state whenever a correction arrives.
	CChallengeScriptRuntime m_ChallengeScript;
	bool m_ChallengeScriptLoaded;
	char m_aChallengeContentHash[65];
	int m_ChallengeVariantMask;
	int m_ChallengeSeed;
	bool m_ChallengeInfoReceived;
	bool m_TextureBuffersCreated;
	bool m_DarkVisionForcedMultiBuffering;
	bool m_DarkVisionPreviousMultiBuffering;

	struct SVisionStatus
	{
		int m_FlashStartTick;
		int m_FlashEndTick;
		int m_BlindEndTick;
		int m_FlashAlpha;
		int m_LightingBrightness;
	};
	SVisionStatus m_aVisionStatus[MAX_CLIENTS];
	int m_LocalVisionOwner = -1;
	int m_LocalFlashStartTick = 0;
	int m_LocalFlashEndTick = 0;
	int m_LocalBlindEndTick = 0;
	int m_LocalFlashAlpha = 0;
	int m_LocalLightingTarget = 255;
	float m_LocalLightingBrightness = 1.0f;
	int m_PveEnvironmentBiome = 0;
	int m_PveEnvironmentPhase = 0;
	int m_PveEnvironmentBossPhase = 0;
	int m_PveEnvironmentLevel = 0;
	int m_PveEnvironmentPhaseEndTick = 0;

	IKernel *Kernel() { return IInterface::Kernel(); }
	IEngine *Engine() const { return m_pEngine; }
	class IGraphics *Graphics() const { return m_pGraphics; }
	class IClient *Client() const { return m_pClient; }
	// Returns centiseconds. While a racer is active this is the live elapsed
	// time; after finishing it is the frozen server result.
	int RaceTime(int ClientID) const;
	static void FormatRaceTime(int Time, char *pBuf, int BufSize);
	class CUI *UI() { return &m_UI; }
	class ISound *Sound() const { return m_pSound; }
	class IGamepad *Gamepad() const { return m_pGamepad; }
	class IInput *Input() const { return m_pInput; }
	class IStorage *Storage() const { return m_pStorage; }
	class IConsole *Console() { return m_pConsole; }
	bool ReloadWeaponPackages(char *pError, int ErrorSize);
	bool GameplayInputCaptured() const;
	bool GameplayInputFullyCaptured() const;
	class ITextRender *TextRender() const { return m_pTextRender; }
	class IDemoPlayer *DemoPlayer() const { return m_pDemoPlayer; }
	class IDemoRecorder *DemoRecorder() const { return m_pDemoRecorder; }
	class IServerBrowser *ServerBrowser() const { return m_pServerBrowser; }
	class CRenderTools *RenderTools() { return &m_RenderTools; }
	class CLayers *Layers() { return &m_Layers; };
	class CCollision *Collision() { return &m_Collision; };
	class IEditor *Editor() { return m_pEditor; }

	class IFriends *Friends() { return m_pFriends; }
	class CCustomStuff *CustomStuff() { return m_pCustomStuff; }
	// Challenge rules are server-authoritative. Before the server's challenge
	// handshake arrives, expose no variants so a stale local preset cannot
	// affect a remote game. cl_challenge_variants is only used when starting a
	// local listen server (the menu copies it to sv_challenge_variants).
	int ChallengeVariantMask() const { return m_ChallengeInfoReceived ? m_ChallengeVariantMask : 0; }
	bool DarkVisionEnabled() const
	{
		const bool ChallengeDark = m_ChallengeInfoReceived &&
			ChallengeVariantEnabled(m_ChallengeVariantMask, CHALLENGE_DARK);
		const bool ServerDark = m_LocalLightingTarget < 255 || m_LocalLightingBrightness < 0.999f;
		return ChallengeDark || ServerDark;
	}
	// The value is server-authored and interpolated locally. It is also used for
	// the ambient clear color, so the whole lighting pass fades coherently.
	float LightingBrightness() const
	{
		const bool ChallengeDark = m_ChallengeInfoReceived &&
			ChallengeVariantEnabled(m_ChallengeVariantMask, CHALLENGE_DARK);
		return ChallengeDark ? 0.0f : clamp(m_LocalLightingBrightness, 0.0f, 1.0f);
	}
	bool LocalZoomAllowed() const;
	int ServerZoom() const;
	int PveEnvironmentBiome() const { return m_PveEnvironmentBiome; }
	int PveEnvironmentPhase() const { return m_PveEnvironmentPhase; }
	int PveEnvironmentBossPhase() const { return m_PveEnvironmentBossPhase; }
	int PveEnvironmentLevel() const { return m_PveEnvironmentLevel; }
	int PveEnvironmentPhaseEndTick() const { return m_PveEnvironmentPhaseEndTick; }
	float FlashOverlayAlpha() const;
	class CSkelebank *Skelebank() { return m_pSkelebank; }

	int NetobjNumCorrections() { return m_NetObjHandler.NumObjCorrections(); }
	const char *NetobjCorrectedOn() { return m_NetObjHandler.CorrectedObjOn(); }

	bool m_SuppressEvents;
	bool m_NewTick;
	bool m_NewPredictedTick;
	int m_FlagDropTick[2];

	// TODO: move this
	CTuningParams m_Tuning;

	enum
	{
		SERVERMODE_PURE = 0,
		SERVERMODE_MOD,
		SERVERMODE_PUREMOD,
	};
	int m_ServerMode;

	int m_DemoSpecID;

	vec2 m_LocalCharacterPos;

	// for building checks
	bool BuildingNear(vec2 Pos, float Range);

	bool IsLocalUndead();
	bool BuildingEnabled();
	bool Survival();
	bool SurvivalAcid();
	bool IsCoop();

	void AddFluidForce(vec2 Pos, vec2 Vel);

	vec4 GetPlayerColor(int ClientID);
	vec4 GetBloodColor(int ClientID);

	const char *GetPlayerLabel(int ClientID, char *pBuf, int BufSize) const;
	void SanitizeSocialString(const char *pSrc, char *pDst, int DstSize) const;
	int EffectiveFilterChat() const;

	// predicted players
	CCharacterCore m_PredictedPrevChar;
	CCharacterCore m_PredictedChar;

	CBallCore m_PredictedPrevBall;
	CBallCore m_PredictedBall;

	// snap pointers
	struct CSnapState
	{
		const CNetObj_Character *m_pLocalCharacter;
		const CNetObj_Character *m_pLocalPrevCharacter;
		const CNetObj_PlayerInfo *m_pLocalInfo;
		const CNetObj_SpectatorInfo *m_pSpectatorInfo;
		const CNetObj_SpectatorInfo *m_pPrevSpectatorInfo;
		const CNetObj_Flag *m_paFlags[2];
		const CNetObj_GameInfo *m_pGameInfoObj;
		const CNetObj_GameData *m_pGameDataObj;
		const CNetObj_RaceInfo *m_pRaceInfo;
		const CNetObj_RacePlayer *m_apRacePlayers[MAX_CLIENTS];
		int m_GameDataSnapID;

		const CNetObj_PlayerInfo *m_paPlayerInfos[MAX_CLIENTS];
		const CNetObj_WeaponRuntime *m_apWeaponRuntimes[MAX_CLIENTS];
		const CNetObj_ChallengeRuntime *m_apChallengeRuntimes[MAX_CLIENTS];
		const CNetObj_PlayerInfo *m_paInfoByScore[MAX_CLIENTS];
		const CNetObj_PlayerInfo *m_paInfoByTeam[MAX_CLIENTS];

		int m_LocalClientID;
		int m_NumPlayers;
		int m_aTeamSize[2];

		// jumppad positions
		int m_ImpactCount;
		vec4 m_aImpactPos[MAX_IMPACTS];

		// spectate data
		struct CSpectateInfo
		{
			bool m_Active;
			int m_SpectatorID;
			bool m_UsePosition;
			vec2 m_Position;
		} m_SpecInfo;

		//
		struct CCharacterInfo
		{
			bool m_Active;

			// snapshots
			CNetObj_Character m_Prev;
			CNetObj_Character m_Cur;

			// interpolated position
			vec2 m_Position;
		};

		CCharacterInfo m_aCharacters[MAX_CLIENTS];

		//
		struct CBallInfo
		{
			bool m_Active;

			// snapshots
			CNetObj_Ball m_Prev;
			CNetObj_Ball m_Cur;

			// interpolated position
			vec2 m_Position;
		};

		CBallInfo m_Ball;
	};

	CSnapState m_Snap;

	// client data
	struct CClientData
	{
		int m_ColorBody;
		int m_ColorFeet;
		int m_ColorTopper;
		int m_ColorSkin;

		bool m_IsBot;
		int m_BloodColor;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		char m_aTopperName[64];
		char m_aEyeName[64];
		char m_aHeadName[64];
		char m_aBodyName[64];
		char m_aFootName[64];
		char m_aHandName[64];
		int m_TopperID;
		int m_EyeID;
		int m_HeadID;
		int m_BodyID;
		int m_HandID;
		int m_FootID;
		int m_Team;
		int m_Emoticon;
		int m_EmoticonStart;
		CCharacterCore m_Predicted;

		CTeeRenderInfo m_SkinInfo;	 // this is what the server reports
		CTeeRenderInfo m_RenderInfo; // this is what we use

		float m_Angle;
		bool m_Active;
		bool m_ChatIgnore;
		bool m_Friend;
		unsigned long long m_PlatformUserID;
		bool m_PlatformIdentityVerified;
		bool m_PlatformPlayedWithReported;

		void UpdateRenderInfo();
		void Reset();
	};

	CClientData m_aClients[MAX_CLIENTS];

	CRenderTools m_RenderTools;

	void AddPlayerSplatter(vec2 Pos, vec4 Color);
	bool LoadChallengeScript(const CNetMsg_Sv_ChallengeInfo &Info);
	void EnsureDarkVisionRenderBuffers();
	void RestoreDarkVisionRenderBuffers();
	void UpdateLocalVisionStatus();
	void UpdateVisionLighting();
	void RenderVisionOverlay();

	void OnReset();

	// hooks
	virtual void OnConnected();
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnMessage(int MsgId, CUnpacker *pUnpacker);
	virtual void OnPlatformPlayerIdentity(int ClientID, bool Present, unsigned long long UserID);
	virtual void OnNewSnapshot();
	virtual void OnPredict();
	virtual void OnActivateEditor();
	virtual int OnSnapInput(int *pData);
	virtual void OnShutdown();
	virtual void OnEnterGame();
	virtual void OnRconLine(const char *pLine);
	virtual void OnGameOver();
	virtual void OnStartGame();

	virtual const char *GetItemName(int Type);
	virtual const char *Version();
	virtual const char *NetVersion();

	// actions
	// TODO: move these
	void SendSwitchTeam(int Team);
	void SendInfo(bool Start);
	void SendKill(int ClientID);

	// pointers to all systems
	class CGameConsole *m_pGameConsole;
	class CBinds *m_pBinds;
	class CBlocks *m_pBlocks;
	class CParticles *m_pParticles;
	class CBlood *m_pBlood;
	class CGuts *m_pGuts;
	class CBrains *m_pBrains;
	class CTracer *m_pTracers;
	class CSplatter *m_pSplatter;
	class CSpark *m_pSpark;
	class CFluid *m_pFluid;
	class CCBelt *m_pCBelt;
	class CLight *m_pLight;
	class CMenus *m_pMenus;
	class CInventory *m_pInventory;
	class CBuildPlacement *m_pBuildPlacement;
	class CSkins *m_pSkins;
	class CCountryFlags *m_pCountryFlags;
	class CFlow *m_pFlow;
	class CChat *m_pChat;
	class CDamageInd *m_pDamageind;
	class CCamera *m_pCamera;
	class CControls *m_pControls;
	class CEffects *m_pEffects;
	class CSounds *m_pSounds;
	class CMotd *m_pMotd;
	class CGameVoteDisplay *m_pGameVoteDisplay;
	class CPveRoguelite *m_pPveRoguelite;
	class CMapImages *m_pMapimages;
	class CVoting *m_pVoting;
	class CScoreboard *m_pScoreboard;
	class CHud *m_pHud;
	class CItems *m_pItems;
	class CWeapons *m_pWeapons;
	class CDroids *m_pDroids;
	class CRadar *m_pRadar;
	class CBuildings *m_pBuildings;
	class CBuildings2 *m_pBuildings2;
	class CMapLayers *m_pMapLayersBackGround;
	class CMapLayers *m_pMapLayersForeGround;
};

inline float HueToRgb(float v1, float v2, float h)
{
	if(h < 0.0f)
		h += 1;
	if(h > 1.0f)
		h -= 1;
	if((6.0f * h) < 1.0f)
		return v1 + (v2 - v1) * 6.0f * h;
	if((2.0f * h) < 1.0f)
		return v2;
	if((3.0f * h) < 2.0f)
		return v1 + (v2 - v1) * ((2.0f / 3.0f) - h) * 6.0f;
	return v1;
}

inline vec3 HslToRgb(vec3 HSL)
{
	if(HSL.s == 0.0f)
		return vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l + HSL.s) - (HSL.s * HSL.l);
		float v1 = 2.0f * HSL.l - v2;

		return vec3(
			HueToRgb(v1, v2, HSL.h + (1.0f / 3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f / 3.0f)));
	}
}

extern const char *Localize(const char *Str);

#endif
