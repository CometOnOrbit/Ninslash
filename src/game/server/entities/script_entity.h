#ifndef GAME_SERVER_ENTITIES_SCRIPT_ENTITY_H
#define GAME_SERVER_ENTITIES_SCRIPT_ENTITY_H

#include <game/server/entity.h>
#include <game/weapons/weapon_catalog.h>

// The only networked entity shape exposed to Workshop weapon scripts.  It is
// deliberately small: Lua controls decisions through the runtime, while C++
// owns collision, damage, lifetime and snapshots.
enum EScriptEntityKind
{
	SCRIPT_ENTITY_PROJECTILE,
	SCRIPT_ENTITY_RAY,
	SCRIPT_ENTITY_AREA,
	SCRIPT_ENTITY_SUMMON,
};

struct CScriptEntitySpec
{
	int m_Kind;
	vec2 m_Pos;
	vec2 m_Velocity;
	vec2 m_From;
	int m_LifeTicks;
	int m_Radius;
	int m_Damage;
	int m_Bounces;
	int m_Gravity;
	int m_State[8];
};

class CScriptEntity : public CEntity
{
  public:
	CScriptEntity(CGameWorld *pGameWorld, const CAttackSource &Source, const CScriptEntitySpec &Spec);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	int State(int Index) const { return Index >= 0 && Index < 8 ? m_aState[Index] : 0; }
	void SetState(int Index, int Value)
	{
		if(Index >= 0 && Index < 8)
			m_aState[Index] = Value;
	}
	int Owner() const { return m_Source.m_Owner; }

  private:
	bool HitCharacters(vec2 From, vec2 To);
	void DestroySelf();

	CAttackSource m_Source;
	vec2 m_Velocity;
	vec2 m_From;
	int m_Kind;
	int m_LifeTicks;
	int m_Damage;
	int m_Bounces;
	int m_Gravity;
	int m_aState[8];
};

#endif
