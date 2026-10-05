#ifndef GAME_INDUSTRIAL_BOSS_H
#define GAME_INDUSTRIAL_BOSS_H
#include <base/vmath.h>
#include <game/foundry_warden.h>
#include <generated/protocol.h>

enum
{
	INDUSTRIAL_RAIL,
	INDUSTRIAL_BULKHEAD,
	INDUSTRIAL_ARC,
	INDUSTRIAL_VAULT,
	NUM_INDUSTRIAL_BOSSES
};
enum
{
	IB_IDLE,
	IB_MOVE,
	IB_TURN,
	IB_DASH,
	IB_MELEE,
	IB_RANGED,
	IB_STAGGER,
	IB_ENRAGE,
	IB_DEATH,
	IB_ARRIVAL,
	IB_LEAP,
	IB_SPECIAL,
	NUM_IB_ACTS
};
struct CIndustrialProfile
{
	const char *m_pName;
	int m_Health;
	float m_Scale, m_Run, m_Dash;
	vec2 m_Box;
	const char *m_apParts[4];
};
inline bool IsIndustrialBoss(int Type)
{
	return Type >= DROIDTYPE_BOSSRAIL && Type <= DROIDTYPE_BOSSVAULT;
}
// The RAIL slot is now the Skitter Matriarch (CSkitterMatriarch); only these three still run on CIndustrialBoss.
inline bool IsLegacyIndustrialBoss(int Type)
{
	return Type >= DROIDTYPE_BOSSBULKHEAD && Type <= DROIDTYPE_BOSSVAULT;
}
inline int IndustrialKind(int Type)
{
	return clamp(Type - DROIDTYPE_BOSSRAIL, 0, NUM_INDUSTRIAL_BOSSES - 1);
}
inline const CIndustrialProfile &IndustrialProfile(int Kind)
{
	static const CIndustrialProfile P[] = {
		{"Rail Reaper", 3800, .70f, 9.5f, 27.0f, vec2(156, 128), {"Core", "Cutter", "Traction", "Launcher"}},
		{"Bulkhead Colossus", 5400, .74f, 7.5f, 22.0f, vec2(128, 160), {"Core", "Shields", "Hydraulics", "Loader"}},
		{"Arc Conductor", 5200, .72f, 10.0f, 24.0f, vec2(128, 152), {"Core", "Coils", "Stabilizers", "Distributor"}},
		{"Vault Overseer", 6800, .70f, 8.5f, 23.0f, vec2(160, 160), {"Core", "Armor", "Thrusters", "Controllers"}}};
	return P[clamp(Kind, 0, NUM_INDUSTRIAL_BOSSES - 1)];
}
inline const CWardenAct &IndustrialAct(int Act)
{
	static const CWardenAct A[] = {{"idle", DROIDANIM_IDLE, 2.4f, true},
								   {"move", DROIDANIM_MOVE, 1.f, true},
								   {"turn", DROIDANIM_TURN, .32f, false},
								   {"charge", DROIDANIM_CHARGE, 2.6f, false},
								   {"sweep", DROIDANIM_ATTACK, 1.8f, false},
								   {"scrap_burst", DROIDANIM_MORTAR, 2.2f, false},
								   {"stagger", DROIDANIM_STAGGER, .8f, false},
								   {"enrage", DROIDANIM_ENRAGE, 1.8f, false},
								   {"death", DROIDANIM_IDLE, 2.8f, false},
								   {"arrival", DROIDANIM_GRAB, 1.6f, false},
								   {"leap", DROIDANIM_JUMPATTACK, 1.4f, false},
								   {"special", DROIDANIM_VENT, 2.4f, false}};
	return A[clamp(Act, 0, NUM_IB_ACTS - 1)];
}
inline int IndustrialTurnSwitchTick()
{
	return WardenTicks(.16f);
}
inline int IndustrialTurnEndTick()
{
	return WardenTicks(IndustrialAct(IB_TURN).m_Duration);
}
inline bool IndustrialTurnFacingSwitched(int Tick)
{
	return Tick >= IndustrialTurnSwitchTick();
}
inline bool IndustrialTurnLocked(int Act, int Tick)
{
	return Act == IB_TURN && Tick < IndustrialTurnEndTick();
}
inline int IndustrialActFromSnap(int Anim, int Status)
{
	if(Status == DROIDSTATUS_TERMINATED)
		return IB_DEATH;
	for(int i = 0; i < NUM_IB_ACTS; i++)
		if(IndustrialAct(i).m_Anim == Anim)
			return i;
	return IB_IDLE;
}
inline bool IndustrialActive(int Act, int Tick)
{
	return (Act == IB_DASH && Tick >= 40 && Tick < 65) || (Act == IB_MELEE && Tick >= 36 && Tick < 50);
}
inline bool IndustrialExposed(int Act, int Tick)
{
	return Act == IB_STAGGER || Act == IB_ENRAGE || (Act == IB_DASH && Tick >= 82) || (Act == IB_MELEE && Tick >= 56) ||
		   (Act == IB_RANGED && Tick >= 76) || (Act == IB_SPECIAL && Tick >= 84);
}
// Index is the actual map-template band, not a modulo of a possibly custom
// floor.
inline int MilestoneBossForMapBand(int Band)
{
	static const int Types[] = {DROIDTYPE_BOSSWARDEN,
								DROIDTYPE_BOSSRAIL,
								DROIDTYPE_BOSSANGLER,
								DROIDTYPE_BOSSBULKHEAD,
								DROIDTYPE_BOSSARC,
								DROIDTYPE_BOSSVAULT,
								DROIDTYPE_BOSSSTAR};
	return Band >= 0 && Band < 7 ? Types[Band] : DROIDTYPE_BOSSWARDEN;
}
#endif
