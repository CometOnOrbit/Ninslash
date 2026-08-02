#ifndef GAME_SERVER_ENTITIES_EXTRACTION_OBJECT_H
#define GAME_SERVER_ENTITIES_EXTRACTION_OBJECT_H

#include <game/server/entity.h>

class CGameControllerExtract;

class CExtractionObject : public CEntity
{
	CGameControllerExtract *m_pController;
	int m_Type;
	int m_Value;
	int m_State;
	int m_Progress;
	int m_Owner;

public:
	enum
	{
		TYPE_LOOT,
		TYPE_OUTPOST,
		TYPE_EVAC,
		TYPE_REVIVE,
	};

	CExtractionObject(CGameWorld *pWorld, CGameControllerExtract *pController, vec2 Pos, int Type, int Value = 0, int Owner = -1);
	void Snap(int SnappingClient) override;
	int ObjectType() const { return m_Type; }
	int Value() const { return m_Value; }
	int Owner() const { return m_Owner; }
	int State() const { return m_State; }
	void SetState(int State) { m_State = State; }
	void SetProgress(int Progress) { m_Progress = clamp(Progress, 0, 100); }
};

#endif
