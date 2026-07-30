from datatypes import *
import weapon_types

Emotes = ["NORMAL", "PAIN", "HAPPY", "SURPRISE", "ANGRY", "BLINK"]
PlayerFlags = ["PLAYING", "IN_MENU", "CHATTING", "SCOREBOARD", "READY"]
GameFlags = ["TEAMS", "INFECTION", "COOP", "SURVIVAL", "BUILD", "FLAGS", "ACID"]
GameStateFlags = ["GAMEOVER", "SUDDENDEATH", "PAUSED"]

Emoticons = ["OOP", "EXCLAMATION", "HEARTS", "DROP", "DOTDOT", "MUSIC", "SORRY", "GHOST", "SUSHI", "SPLATTEE", "DEVILTEE", "ZOMG", "ZZZ", "WTF", "EYES", "QUESTION"]

Powerups = ["HEALTH", "AMMO", "WEAPON", "ARMOR", "COIN", "KIT"]

WEAPON_DEFINITION_ID_MAX = 65535
WEAPON_LEVEL_MAX = weapon_types.LEVEL_MAX
ATTACK_SOURCE_KIND_MAX = 4

def WeaponSpecFields(prefix):
	return [NetIntRange(prefix + "DefinitionId", 0, WEAPON_DEFINITION_ID_MAX), NetIntRange(prefix + "Level", 0, WEAPON_LEVEL_MAX)]

def AttackSourceFields():
	return [
		NetIntRange("m_SourceKind", 0, ATTACK_SOURCE_KIND_MAX),
		NetIntAny("m_SourceType"),
		*WeaponSpecFields("m_Weapon"),
	]

# keep masks at the end
Statuses = ["SPAWNING", "AFLAME", "SLOWED", "ELECTRIC", "DEATHRAY", "SHIELD", "DASH", "INVISIBILITY", "SLOWMOVING", "BOMBCARRIER", "MASK1", "MASK2", "MASK3"]

BallStatuses = ["STATIONARY", "SUPER"]

Damagetypes = ["NORMAL", "FLAME", "ELECTRIC", "FLUID"]

Droidstatus = ["IDLE", "HURT", "ELECTRIC", "TERMINATED"]
Droidtype = ["WALKER", "STAR", "CRAWLER", "BOSSCRAWLER", "FLY", "BOSSSTAR", "BOSSWALKER", "BOSSSPLITTER",
	"BULWARK", "ASSEMBLER", "SABOTEUR", "RAILGUNNER", "SIEGE_ENGINE", "OVERSEER_CORE"]
Buildingtype = ["NONE", "SAWBLADE", "MINE1", "MINE2", "BARREL", "BARREL2", "BARREL3", "TURRET", "LAZER", "POWERUPPER",
	"BASE", "STAND", "FLAMETRAP", "JUMPPAD", "SWITCH", "DOOR1", "GENERATOR", "POWERBARREL", "POWERBARREL2",
	"LIGHTNINGWALL", "LIGHTNINGWALL2", "REACTOR", "REACTOR_DESTROYED", "TESLACOIL", "SCREEN", "SHOP", "PVE_SHIELD_NODE"]
BuildingEnum = "\n".join("\tBUILDING_%s%s," % (name, "=1" if index == 1 else "") for index, name in enumerate(Buildingtype) if index)
Droidanim = ["IDLE", "MOVE", "ATTACK", "JUMPATTACK"]

CoreAction = ["IDLE", "JUMP", "WALLJUMP", "ROLL", "SLIDE", "SLIDEKICK", "FALL", "JUMPPAD", "HANG"]

InventoryAction = ["SWAP", "COMBINE", "TAKEPART", "DROP", "SHOP", "ROLL"]

ForgeOperation = ["REPLACE_PART2", "SPIN", "UPGRADE", "MOD_RECIPE", "AUTO"]
ForgeResult = ["SUCCESS", "DISABLED", "TOO_FAR", "NOT_ENOUGH_GOLD", "BUSY", "INVALID_SLOT", "INVALID_RECIPE", "NO_CHANGE"]

Radar = ["CHARACTER", "HUMAN", "ENEMY", "DOOR", "REACTOR", "BOMB"]

RawHeader = '''

#include <engine/message.h>

enum
{
	INPUT_STATE_MASK=0x3f
};

enum
{
	TEAM_NEUTRAL=-1,
	TEAM_SPECTATORS=-1,
	TEAM_RED,
	TEAM_BLUE,

	FLAG_MISSING=-3,
	FLAG_ATSTAND,
	FLAG_TAKEN,

''' + BuildingEnum + '''
	
	BSTATUS_REPAIR=1,
	BSTATUS_NOPE,
	BSTATUS_MIRROR,
	BSTATUS_FIRE,
	BSTATUS_ON,
	BSTATUS_EVENT,
	NUM_BSTATUS,
	
	FX_EXPLOSION1=1,
	FX_EXPLOSION2,
	FX_SMALLELECTRIC,
	FX_ELECTRIC,
	FX_SUPERELECTRIC,
	FX_GREEN_EXPLOSION,
	FX_ELECTROHIT,
	FX_ELECTROMINE,
	FX_SHIELDHIT,
	FX_MINE,
	FX_BARREL,
	FX_LAZERLOAD,
	FX_BLOOD1,
	FX_BLOOD2,
	FX_BLOOD3,
	FX_MONSTERDEATH,
	FX_MONSTERSPAWN,
	FX_TAKEOFF,
	FX_FLAME1,
	FX_ROLLDASH,
	NUMFX,
	
	EFFECT_ELECTRODAMAGE=1,
	EFFECT_DEATHRAY,
	EFFECT_SPAWNING,
	EFFECT_DAMAGE,
	EFFECT_INVISIBILITY,
	EFFECT_HP,
	EFFECT_DASH,
	EFFECT_FUEL,
	NUM_EFFECTS,

	HIT_TARGET_FLESH=0,
	HIT_TARGET_METAL,
	HIT_TARGET_SHIELD,
	NUM_HIT_TARGETS,
	
	DEATHTYPE_EMPTY=0,
	DEATHTYPE_SWORD,
	DEATHTYPE_CHAINSAW,
	DEATHTYPE_PISTOL,
	DEATHTYPE_SHOTGUN,
	DEATHTYPE_GRENADELAUNCHER,
	DEATHTYPE_RIFLE,
	DEATHTYPE_ELECTROLAUNCHER,
	DEATHTYPE_LASER,
	DEATHTYPE_FLAMER,
	DEATHTYPE_SAWBLADE,
	DEATHTYPE_SPIKE,
	DEATHTYPE_LANDMINE,
	DEATHTYPE_ELECTROMINE,
	DEATHTYPE_BARREL,

	CHATMODE_ALL=0,
	CHATMODE_TEAM,
	CHATMODE_WHISPER,
	DEATHTYPE_DEATHRAY,
	DEATHTYPE_DROID_WALKER,
	DEATHTYPE_FLAMETRAP,
	DEATHTYPE_TURRETADDITION,
	DEATHTYPE_POWERBARREL,
	DEATHTYPE_LIGHTNINGWALL,
	DEATHTYPE_TESLACOIL,
	DEATHTYPE_DROID_STAR,
	NUM_DEATHTYPES,
	
	NUM_SLOTS=12,
	NUM_BODIES=7,
	MAX_PLAYERITEMS=2,
	
	SPEC_FREEVIEW=-1,
};
'''

RawSource = '''
#include <engine/message.h>
#include "protocol.h"
'''

Enums = [
	Enum("EMOTE", Emotes),
	Enum("POWERUP", Powerups),
	Enum("EMOTICON", Emoticons),
	Enum("STATUS", Statuses),
	Enum("BALLSTATUS", BallStatuses),
	Enum("DAMAGETYPE", Damagetypes),
	Enum("DROIDSTATUS", Droidstatus),
	Enum("DROIDTYPE", Droidtype),
	Enum("DROIDANIM", Droidanim),
	Enum("COREACTION", CoreAction),
	Enum("INVENTORYACTION", InventoryAction),
	Enum("FORGEOP", ForgeOperation),
	Enum("FORGERESULT", ForgeResult),
	Enum("RADAR", Radar)
]

Flags = [
	Flags("PLAYERFLAG", PlayerFlags),
	Flags("GAMEFLAG", GameFlags),
	Flags("GAMESTATEFLAG", GameStateFlags)
]

Objects = [

	NetObject("PlayerInput", [
		NetIntAny("m_Direction"),
		NetIntAny("m_TargetX"),
		NetIntAny("m_TargetY"),

		NetIntAny("m_Jump"),
		NetIntAny("m_Fire"),
		NetIntAny("m_Hook"),
		NetIntAny("m_Charge"),
		NetIntAny("m_Down"),

		NetIntRange("m_PlayerFlags", 0, 256),

		NetIntAny("m_WantedWeapon"),
		NetIntAny("m_NextWeapon"),
		NetIntAny("m_PrevWeapon"),
	]),

	NetObject("Projectile", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntAny("m_Vel2X"),
		NetIntAny("m_Vel2Y"),

		*AttackSourceFields(),
		NetTick("m_StartTick"),
	]),

	# A bounded, script-owned combat entity. Its simulation remains authoritative
	# on the server, while the fixed state slots allow clients to deterministically
	# reconstruct and predict the same entity without arbitrary Lua object
	# serialization.
	NetObject("ScriptEntity", [
		NetIntRange("m_Kind", 0, 3),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntAny("m_FromX"),
		NetIntAny("m_FromY"),
		NetIntRange("m_Radius", 0, 4096),
		NetIntRange("m_Life", 0, 65535),
		*AttackSourceFields(),
		NetIntAny("m_State0"),
		NetIntAny("m_State1"),
		NetIntAny("m_State2"),
		NetIntAny("m_State3"),
		NetIntAny("m_State4"),
		NetIntAny("m_State5"),
		NetIntAny("m_State6"),
		NetIntAny("m_State7"),
	]),

	# Explicit, bounded state for a held scripted weapon. It is separate from
	# Character so older character prediction can stay focused on movement.
	NetObject("WeaponRuntime", [
		NetIntRange("m_Owner", 0, 'MAX_CLIENTS-1'),
		*WeaponSpecFields("m_Weapon"),
		NetIntAny("m_RandomState"),
		NetIntAny("m_State0"), NetIntAny("m_State1"), NetIntAny("m_State2"), NetIntAny("m_State3"),
		NetIntAny("m_State4"), NetIntAny("m_State5"), NetIntAny("m_State6"), NetIntAny("m_State7"),
	]),

	NetObject("Laser", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_FromX"),
		NetIntAny("m_FromY"),
		NetIntAny("m_Charge"),

		NetTick("m_StartTick"),
	]),
	
	NetObject("LaserFail", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_FromX"),
		NetIntAny("m_FromY"),
		NetIntRange("m_PowerLevel", 0, 100),

		NetTick("m_StartTick"),
	]),

	NetObject("Pickup", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetBool("m_Mirror"),
		NetIntAny("m_Angle"),
		NetIntRange("m_Type", 0, 'max_int'),
		NetIntAny("m_Subtype"),
		*WeaponSpecFields("m_Weapon"),
	]),
	
	NetObject("Weapon", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		*WeaponSpecFields("m_Weapon"),
		NetIntAny("m_AttackTick"),
		NetIntAny("m_Angle")
	]),
	
	NetObject("Droid", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_Angle"),
		NetIntRange("m_AttackTick", 0, 'max_int'),

		NetIntRange("m_Type", 0, 16),
		NetIntRange("m_Status", 0, 16),
		NetIntRange("m_Anim", 0, 8),
		NetIntRange("m_Dir", -1, 1),
	]),

	NetObject("Building", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntRange("m_Status", 0, 'max_int'),
		NetIntRange("m_Type", 0, 'max_int'),
		NetIntAny("m_Team")
	]),
	
	NetObject("Block", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_Type")
	]),
	
	NetObject("Turret:Building", [
		NetIntAny("m_Angle"),
		*WeaponSpecFields("m_Weapon"),
		NetIntRange("m_AttackTick", 0, 'max_int')
	]),
	
	NetObject("Powerupper:Building", [
		NetIntRange("m_Item", -1, 9)
	]),
	
	NetObject("Shop:Building", [
		*WeaponSpecFields("m_Item1"),
		*WeaponSpecFields("m_Item2"),
		*WeaponSpecFields("m_Item3"),
		*WeaponSpecFields("m_Item4"),
		*WeaponSpecFields("m_Item5")
	]),

	NetObject("Flag", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),

		NetIntRange("m_Team", 'TEAM_RED', 'TEAM_BLUE')
	]),
	
	NetObject("Radar", [
		NetIntAny("m_TargetX"),
		NetIntAny("m_TargetY"),
		NetIntAny("m_Type")
	]),

	NetObject("GameInfo", [
		NetIntRange("m_GameFlags", 0, 256),
		NetIntRange("m_GameStateFlags", 0, 256),
		NetTick("m_RoundStartTick"),
		NetIntRange("m_WarmupTimer", 0, 'max_int'),

		NetIntRange("m_ScoreLimit", 0, 'max_int'),
		NetIntRange("m_TimeLimit", 0, 'max_int'),

		NetIntRange("m_RoundNum", 0, 'max_int'),
		NetIntRange("m_RoundCurrent", 0, 'max_int'),

		NetIntRange("m_ForgeMode", 0, 2),
		NetIntAny("m_ForgeBaseCost"),
		NetIntAny("m_ForgeLevelCost"),
	]),

	NetObject("GameData", [
		NetIntAny("m_TeamscoreRed"),
		NetIntAny("m_TeamscoreBlue"),

		NetIntRange("m_FlagCarrierRed", 'FLAG_MISSING', 'MAX_CLIENTS-1'),
		NetIntRange("m_FlagCarrierBlue", 'FLAG_MISSING', 'MAX_CLIENTS-1'),
	]),

	NetObject("BallCore", [
		NetIntAny("m_Tick"),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntAny("m_Angle"),
		NetIntAny("m_AngleForce"),
		NetIntAny("m_Status"),
	]),
	
	NetObject("Ball:BallCore", [
	]),
	
	NetObject("CharacterCore", [
		NetIntAny("m_Tick"),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntRange("m_MoveSpeedMultiplier", 10, 300),
		NetIntAny("m_Movement1"),
		
		NetIntRange("m_Health", 0, 100),
		NetIntRange("m_HookedPlayer", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_HookState", -1, 6),
		NetTick("m_HookTick"),

		NetIntAny("m_HookX"),
		NetIntAny("m_HookY"),
		NetIntAny("m_HookDx"),
		NetIntAny("m_HookDy"),

		NetIntAny("m_Angle"),
		NetIntRange("m_Direction", -1, 1),
		NetIntRange("m_Down", 0, 1),
		NetIntAny("m_Anim"),
		NetIntAny("m_LockDirection"),
		NetIntRange("m_HandJetpack", 0, 1),
		NetIntRange("m_Jetpack", 0, 1),
		NetIntRange("m_JetpackPower", 0, 200),
		NetIntRange("m_Wallrun", -100, 100),
		NetIntAny("m_Roll"),
		NetIntRange("m_Slide", -10, 32),
		
		NetIntRange("m_JumpTimer", -10, 10),

		NetIntRange("m_Charge", 0, 1),
		NetIntRange("m_ChargeLevel", -50, 100),
		
		NetIntAny("m_Status"),
		NetIntAny("m_DamageTick"),
		
		NetIntRange("m_Action", 0, 64),
		NetIntAny("m_ActionState"),
		
		NetIntRange("m_Jumped", 0, 3),
		NetIntAny("m_CoyoteTime"),
		NetIntAny("m_JumpBufferTime"),
		NetBool("m_PrevJumpInput"),
		
		NetBool("m_Sliding"),
		NetBool("m_Grounded"),
		NetIntRange("m_Slope", -1, 1),
	]),

	NetObject("Character:CharacterCore", [
		NetIntRange("m_PlayerFlags", 0, 256),
		NetIntRange("m_Armor", 0, 100),
		NetIntRange("m_AmmoCount", 0, 30),
		*WeaponSpecFields("m_Weapon"),
		NetIntRange("m_Emote", 0, len(Emotes)),
		NetIntRange("m_AttackTick", 0, 'max_int'),
		NetIntAny("m_Movement"),
	]),

	NetObject("PlayerInfo", [
		NetIntRange("m_Local", 0, 1),
		NetIntRange("m_ClientID", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Team", 'TEAM_SPECTATORS', 'TEAM_BLUE'),
		NetIntRange("m_Spectating", 0, 1),

		NetIntAny("m_Score"),
		NetIntAny("m_Latency"),
		
		NetIntRange("m_WeaponSlot", 0, 3),
		*WeaponSpecFields("m_Weapon1"),
		*WeaponSpecFields("m_Weapon2"),
		*WeaponSpecFields("m_Weapon3"),
		*WeaponSpecFields("m_Weapon4"),
		
		NetIntRange("m_Kits", 0, 99),
	]),

	NetObject("ClientInfo", [
		# 4*4 = 16 charachters
		NetIntAny("m_Name0"), NetIntAny("m_Name1"), NetIntAny("m_Name2"),
		NetIntAny("m_Name3"),

		# 4*3 = 12 charachters
		NetIntAny("m_Clan0"), NetIntAny("m_Clan1"), NetIntAny("m_Clan2"),

		NetIntAny("m_Country"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Topper0"), NetIntAny("m_Topper1"), NetIntAny("m_Topper2"),
		NetIntAny("m_Topper3"), NetIntAny("m_Topper4"), NetIntAny("m_Topper5"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Eye0"), NetIntAny("m_Eye1"), NetIntAny("m_Eye2"),
		NetIntAny("m_Eye3"), NetIntAny("m_Eye4"), NetIntAny("m_Eye5"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Head0"), NetIntAny("m_Head1"), NetIntAny("m_Head2"),
		NetIntAny("m_Head3"), NetIntAny("m_Head4"), NetIntAny("m_Head5"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Body0"), NetIntAny("m_Body1"), NetIntAny("m_Body2"),
		NetIntAny("m_Body3"), NetIntAny("m_Body4"), NetIntAny("m_Body5"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Hand0"), NetIntAny("m_Hand1"), NetIntAny("m_Hand2"),
		NetIntAny("m_Hand3"), NetIntAny("m_Hand4"), NetIntAny("m_Hand5"),
		
		# 4*6 = 24 charachters
		NetIntAny("m_Foot0"), NetIntAny("m_Foot1"), NetIntAny("m_Foot2"),
		NetIntAny("m_Foot3"), NetIntAny("m_Foot4"), NetIntAny("m_Foot5"),

		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
		NetIntAny("m_ColorTopper"),
		NetIntAny("m_ColorSkin"),
		
		NetIntRange("m_IsBot", 0, 1),
		NetIntRange("m_BloodColor", 0, 3),
	]),

	NetObject("SpectatorInfo", [
		NetIntRange("m_SpectatorID", 'SPEC_FREEVIEW', 'MAX_CLIENTS-1'),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),

	## Events

	NetEvent("Common", [
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),

	# for buildings
	NetEvent("AmmoFill:Common", [
		NetIntRange("m_Weapon", 0, 'NUM_WEAPONS-1'),
	]),
	NetEvent("Repair:Common", []),
	
	NetEvent("BuildingHit:Common", []),
	NetEvent("FlameHit:Common", []),
	
	NetEvent("Explosion:Common", [
		*AttackSourceFields(),
	]),

	NetEvent("FlameExplosion:Common", []),
	NetEvent("Spawn:Common", []),
	NetEvent("HammerHit:Common", []),
	
	NetEvent("FX:Common", [
		NetIntRange("m_FX", '1', 'NUMFX-1'),
	]),
	
	NetEvent("Lazer:Common", [
		NetIntAny("m_Height"),
	]),
	
	NetEvent("Swordtracer:Common", [
		NetIntAny("m_Angle"),
	]),

	NetEvent("Death:Common", [
		NetIntRange("m_ClientID", 0, 'MAX_CLIENTS-1'),
	]),

	NetEvent("SoundGlobal:Common", [ #TODO 0.7: remove me
		NetIntRange("m_SoundID", 0, 'NUM_SOUNDS-1'),
	]),

	NetEvent("SoundWorld:Common", [
		NetIntRange("m_SoundID", 0, 'NUM_SOUNDS-1'),
	]),

	NetEvent("Block:Common", [
		NetIntAny("m_Type")
	]),
	
	NetEvent("DamageInd:Common", [
		NetIntAny("m_Angle"),
		NetIntAny("m_Damage"),
		NetIntRange("m_ClientID", -1, 'MAX_CLIENTS-1'),
	]),
	
	NetEvent("Effect:Common", [
		NetIntRange("m_EffectID", 0, 'NUM_EFFECTS-1'),
		NetIntRange("m_ClientID", 0, 'MAX_CLIENTS-1'),
	]),

	# Appended for protocol v10. Keep all legacy object/event IDs stable.
	NetObject("PveDrone", [
		NetIntRange("m_Owner", 0, 'MAX_CLIENTS-1'),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
		NetIntAny("m_VelX"),
		NetIntAny("m_VelY"),
		NetIntRange("m_Module", 0, 3),
		NetIntRange("m_State", 0, 6),
		NetIntRange("m_Health", 0, 40),
		NetIntAny("m_TargetX"),
		NetIntAny("m_TargetY"),
		NetIntAny("m_ActionTick"),
		NetIntAny("m_SwitchReadyTick"),
	]),

	# Appended for protocol v12. This is sent only to the attacking player and followers.
	NetEvent("HitConfirm:Common", [
		NetIntAny("m_Damage"),
		NetIntRange("m_TargetType", 0, 'NUM_HIT_TARGETS-1'),
		NetIntRange("m_Killed", 0, 1),
		*AttackSourceFields(),
	]),

	# Appended for protocol v15; custom weapon resources use the runtime ID
	# already negotiated by the matching Mod collection.
	NetEvent("WeaponSound:Common", [
		NetIntRange("m_WeaponDefinitionId", 0, WEAPON_DEFINITION_ID_MAX),
		NetIntRange("m_WeaponLevel", 0, WEAPON_LEVEL_MAX),
		NetIntRange("m_Slot", 0, 2),
	]),
]

# todo: remove unnecessary ones
Messages = [

	### Server messages
	#NetMessage("Sv_Motd", [
	#	NetString("m_pMessage"),
	#]),

	NetMessage("Sv_Broadcast", [
		NetString("m_pMessage"),
	]),
	
	NetMessage("Sv_GameVote", [
		NetString("m_pName"),
		NetString("m_pDescription"),
		NetString("m_pImage"),
		NetString("m_pPlayers"),
		NetIntAny("m_Index"),
		NetIntAny("m_TimeLeft"),
	]),

	NetMessage("Sv_GameVoteStatus", [
		NetIntRange("m_Index", 0, 98),
		NetIntRange("m_Votes", 0, 'MAX_CLIENTS'),
	]),

	NetMessage("Sv_Chat", [
		NetIntRange("m_Mode", 0, 3),
		NetIntRange("m_ClientID", -1, 'MAX_CLIENTS-1'),
		NetIntRange("m_TargetID", -1, 'MAX_CLIENTS-1'),
		NetStringStrict("m_pMessage"),
	]),

	NetMessage("Sv_KillMsg", [
		NetIntRange("m_Killer", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Victim", 0, 'MAX_CLIENTS-1'),
		*AttackSourceFields(),
		NetIntAny("m_ModeSpecial"),
	]),

	NetMessage("Sv_SoundGlobal", [
		NetIntRange("m_SoundID", 0, 'NUM_SOUNDS-1'),
	]),

	NetMessage("Sv_TuneParams", []),
	NetMessage("Sv_ExtraProjectile", []),
	NetMessage("Sv_ReadyToEnter", [
		NetStringStrict("m_pWeaponContentHash"),
	]),

	NetMessage("Sv_Emoticon", [
		NetIntRange("m_ClientID", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Emoticon", 0, 'NUM_EMOTICONS-1'),
	]),

	NetMessage("Sv_VoteClearOptions", [
	]),

	NetMessage("Sv_VoteOptionListAdd", [
		NetIntRange("m_NumOptions", 1, 15),
		NetStringStrict("m_pDescription0"), NetStringStrict("m_pDescription1"),	NetStringStrict("m_pDescription2"),
		NetStringStrict("m_pDescription3"),	NetStringStrict("m_pDescription4"),	NetStringStrict("m_pDescription5"),
		NetStringStrict("m_pDescription6"), NetStringStrict("m_pDescription7"), NetStringStrict("m_pDescription8"),
		NetStringStrict("m_pDescription9"), NetStringStrict("m_pDescription10"), NetStringStrict("m_pDescription11"),
		NetStringStrict("m_pDescription12"), NetStringStrict("m_pDescription13"), NetStringStrict("m_pDescription14"),
	]),

	NetMessage("Sv_VoteOptionAdd", [
		NetStringStrict("m_pDescription"),
	]),

	NetMessage("Sv_VoteOptionRemove", [
		NetStringStrict("m_pDescription"),
	]),

	NetMessage("Sv_VoteSet", [
		NetIntRange("m_Timeout", 0, 60),
		NetStringStrict("m_pDescription"),
		NetStringStrict("m_pReason"),
	]),

	NetMessage("Sv_VoteStatus", [
		NetIntRange("m_Type", 0, 1),
		NetIntRange("m_Yes", 0, 'MAX_CLIENTS'),
		NetIntRange("m_No", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Pass", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Total", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Option5", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Option6", 0, 'MAX_CLIENTS'),
	]),
	
	NetMessage("Sv_Inventory", [
		*WeaponSpecFields("m_Item1"),
		*WeaponSpecFields("m_Item2"),
		*WeaponSpecFields("m_Item3"),
		*WeaponSpecFields("m_Item4"),
		*WeaponSpecFields("m_Item5"),
		*WeaponSpecFields("m_Item6"),
		*WeaponSpecFields("m_Item7"),
		*WeaponSpecFields("m_Item8"),
		*WeaponSpecFields("m_Item9"),
		*WeaponSpecFields("m_Item10"),
		*WeaponSpecFields("m_Item11"),
		*WeaponSpecFields("m_Item12"),
		NetIntRange("m_Gold", 0, 999),
		NetIntRange("m_Item1Ammo", 0, 'max_int'),
		NetIntRange("m_Item2Ammo", 0, 'max_int'),
		NetIntRange("m_Item3Ammo", 0, 'max_int'),
		NetIntRange("m_Item4Ammo", 0, 'max_int'),
		NetIntRange("m_Item5Ammo", 0, 'max_int'),
		NetIntRange("m_Item6Ammo", 0, 'max_int'),
		NetIntRange("m_Item7Ammo", 0, 'max_int'),
		NetIntRange("m_Item8Ammo", 0, 'max_int'),
		NetIntRange("m_Item9Ammo", 0, 'max_int'),
		NetIntRange("m_Item10Ammo", 0, 'max_int'),
		NetIntRange("m_Item11Ammo", 0, 'max_int'),
		NetIntRange("m_Item12Ammo", 0, 'max_int'),
	]),

	### Client messages / 14
	NetMessage("Cl_Say", [
		NetIntRange("m_Mode", 0, 2),
		NetIntRange("m_Target", -1, 'MAX_CLIENTS-1'),
		NetStringStrict("m_pMessage"),
	]),

	NetMessage("Cl_SetTeam", [
		NetIntRange("m_Team", 'TEAM_SPECTATORS', 'TEAM_BLUE'),
	]),

	NetMessage("Cl_SetSpectatorMode", [
		NetIntRange("m_SpectatorID", 'SPEC_FREEVIEW', 'MAX_CLIENTS-1'),
	]),

	NetMessage("Cl_StartInfo", [
		NetStringStrict("m_pName"),
		NetStringStrict("m_pClan"),
		NetIntAny("m_Country"),
		NetStringStrict("m_pTopper"),
		NetStringStrict("m_pEye"),
		NetStringStrict("m_pHead"),
		NetStringStrict("m_pBody"),
		NetStringStrict("m_pHand"),
		NetStringStrict("m_pFoot"),
		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
		NetIntAny("m_ColorTopper"),
		NetIntAny("m_ColorSkin"),
		NetIntAny("m_BloodColor"),
		NetIntRange("m_IsBot", 0, 1),
		NetIntRange("m_Language", 0, 999),
	]),

	NetMessage("Cl_ChangeInfo", [
		NetStringStrict("m_pName"),
		NetStringStrict("m_pClan"),
		NetIntAny("m_Country"),
		NetStringStrict("m_pTopper"),
		NetStringStrict("m_pEye"),
		NetStringStrict("m_pHead"),
		NetStringStrict("m_pBody"),
		NetStringStrict("m_pHand"),
		NetStringStrict("m_pFoot"),
		NetIntAny("m_ColorBody"),
		NetIntAny("m_ColorFeet"),
		NetIntAny("m_ColorTopper"),
		NetIntAny("m_ColorSkin"),
		NetIntAny("m_BloodColor"),
		NetIntRange("m_IsBot", 0, 1),
        NetIntRange("m_Language", 0, 999),
	]),

	NetMessage("Cl_Kill", []),

	NetMessage("Cl_Emoticon", [
		NetIntRange("m_Emoticon", 0, 'NUM_EMOTICONS-1'),
	]),
	
	
	NetMessage("Cl_DropWeapon", []),
	
	#NetMessage("Cl_SwitchGroup", []),
	
	NetMessage("Cl_SelectItem", [
		NetIntRange("m_Item", 0, 99),
	]),
	
	NetMessage("Cl_UseKit", [
		NetIntRange("m_Kit", 0, 99),
		NetIntAny("m_X"),
		NetIntAny("m_Y"),
	]),
	
	NetMessage("Cl_Vote", [
		NetIntRange("m_Vote", -1, 1),
	]),
	
	NetMessage("Cl_VoteGameMode", [
		NetIntRange("m_Vote", 0, 98),
	]),

	NetMessage("Cl_CallVote", [
		NetStringStrict("m_Type"),
		NetStringStrict("m_Value"),
		NetStringStrict("m_Reason"),
	]),
	
	NetMessage("Cl_InventoryAction", [
		NetIntRange("m_Type", 0, 6),
		NetIntAny("m_Item1"),
		NetIntAny("m_Item2"),
		NetIntAny("m_Slot"),
	]),

	# Protocol extensions must stay below the original message set. Inserting a
	# message above this point changes legacy client message IDs and silently
	# breaks voting, mode selection and inventory actions on mixed builds.
	# Persistent values are split into 32-bit fields for portable generation.
	NetMessage("Sv_PveProgress", [
		NetIntAny("m_Version"),
		NetIntRange("m_ResearchPoints", 0, 999),
		NetIntAny("m_ResearchMask0"),
		NetIntAny("m_ResearchMask1"),
		NetIntAny("m_ResearchMask2"),
		NetIntAny("m_ResearchMask3"),
		NetIntRange("m_HighestInvasion", 0, 9999),
		NetIntRange("m_PreferredCheckpoint", 1, 9999),
	]),

	NetMessage("Sv_PveChoice", [
		NetIntAny("m_Nonce"),
		NetIntAny("m_EndTick"),
		NetIntRange("m_ChoiceSequence", 1, 99),
		NetIntRange("m_Card0", 0, 102),
		NetIntRange("m_Card1", 0, 102),
		NetIntRange("m_Card2", 0, 102),
		NetIntRange("m_Stack0", 0, 3),
		NetIntRange("m_Stack1", 0, 3),
		NetIntRange("m_Stack2", 0, 3),
	]),

	NetMessage("Sv_PvePerk", [
		NetIntRange("m_ClientID", 0, 'MAX_CLIENTS-1'),
		NetIntRange("m_Card", 0, 102),
		NetIntRange("m_Stacks", 0, 3),
		NetIntRange("m_Choices", 0, 99),
	]),

	NetMessage("Sv_PveContractVote", [
		NetIntAny("m_Nonce"),
		NetIntAny("m_EndTick"),
		NetIntRange("m_Contract0", 0, 19),
		NetIntRange("m_Contract1", 0, 19),
		NetIntRange("m_Votes0", 0, 'MAX_CLIENTS'),
		NetIntRange("m_Votes1", 0, 'MAX_CLIENTS'),
	]),

	NetMessage("Sv_PveContractStatus", [
		NetIntRange("m_Contract", -1, 19),
		NetIntRange("m_State", 0, 3),
		NetIntAny("m_Progress"),
		NetIntAny("m_Target"),
		NetIntAny("m_EndTick"),
	]),

	NetMessage("Sv_PveResearchReward", [
		NetIntRange("m_Amount", 0, 99),
		NetIntRange("m_Reason", 0, 3),
		NetIntRange("m_HighestInvasion", 0, 9999),
		NetIntRange("m_UnlockedCheckpoint", 1, 9999),
	]),

	NetMessage("Sv_PveValidation", [
		NetIntRange("m_Code", 1, 7),
	]),

	NetMessage("Sv_PveBuildState", [
		NetIntRange("m_Focus", 0, 10),
		NetIntRange("m_BlastCharge", 0, 10),
		NetIntRange("m_Voltage", 0, 10),
		NetIntRange("m_Fury", 0, 10),
		NetIntRange("m_Barrier", 0, 30),
		NetIntRange("m_VulnerableTargets", 0, 99),
		NetIntRange("m_BleedingTargets", 0, 99),
		NetIntRange("m_LegendaryCard", -1, 99),
		NetIntRange("m_DroneModule", 0, 3),
		NetIntAny("m_DroneSwitchReadyTick"),
	]),

	NetMessage("Cl_PveProgress", [
		NetIntAny("m_Version"),
		NetIntRange("m_ResearchPoints", 0, 999),
		NetIntAny("m_ResearchMask0"),
		NetIntAny("m_ResearchMask1"),
		NetIntAny("m_ResearchMask2"),
		NetIntAny("m_ResearchMask3"),
		NetIntRange("m_HighestInvasion", 0, 9999),
		NetIntRange("m_PreferredCheckpoint", 1, 9999),
	]),

	NetMessage("Cl_PveChoice", [
		NetIntAny("m_Nonce"),
		NetIntRange("m_Card", 0, 102),
	]),

	NetMessage("Cl_PveContractVote", [
		NetIntAny("m_Nonce"),
		NetIntRange("m_Contract", 0, 19),
	]),

	NetMessage("Cl_PveResearchBuy", [
		NetIntAny("m_Nonce"),
		NetIntRange("m_Card", 7, 99),
	]),

	NetMessage("Cl_PveDroneModule", [
		NetIntAny("m_Nonce"),
		NetIntRange("m_Module", 1, 3),
	]),

	NetMessage("Sv_PveInvasionRetryVote", [
		NetIntAny("m_Nonce"),
		NetIntAny("m_EndTick"),
		NetIntRange("m_CurrentFloor", 1, 9999),
		NetIntRange("m_RetryVotes", 0, 'MAX_CLIENTS'),
		NetIntRange("m_ResetVotes", 0, 'MAX_CLIENTS'),
	]),

	NetMessage("Sv_PveInvasionRetryResult", [
		NetIntRange("m_Result", 0, 2),
		NetIntAny("m_EndTick"),
		NetStringStrict("m_pPlayerName"),
	]),

	NetMessage("Cl_PveInvasionRetryVote", [
		NetIntAny("m_Nonce"),
		NetIntRange("m_Choice", 0, 1),
	]),

	# Appended for protocol v14. Keep this below every existing extension so
	# all pre-v14 message IDs remain stable.
	NetMessage("Sv_ForgeResult", [
		NetIntRange("m_Result", 0, 'NUM_FORGERESULTS-1'),
		NetIntAny("m_Operation"),
		NetIntAny("m_TargetSlot"),
		NetIntAny("m_MaterialSlot"),
		NetIntRange("m_Cost", 0, 999),
		*WeaponSpecFields("m_Product"),
		NetIntRange("m_ProductAmmo", 0, 'max_int'),
		NetIntRange("m_ProductMaxAmmo", 0, 'max_int'),
	]),

	NetMessage("Sv_TutorialState", [
		NetIntRange("m_Chapter", 1, 6),
		NetIntRange("m_Step", 0, 9),
		NetIntRange("m_Progress", 0, 999),
		NetIntRange("m_Target", 0, 999),
		NetIntAny("m_Nonce"),
		NetIntRange("m_CompletedMask", 0, 63),
		NetIntRange("m_Flags", 0, 15),
	]),

	NetMessage("Cl_TutorialAction", [
		NetIntRange("m_Action", 0, 4),
		NetIntAny("m_Nonce"),
		NetIntAny("m_Value"),
	]),
]
