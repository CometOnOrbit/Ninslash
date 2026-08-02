#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/extract.h>

#include "extraction_object.h"

CExtractionObject::CExtractionObject(CGameWorld *pWorld, CGameControllerExtract *pController, vec2 Pos, int Type, int Value, int Owner) :
	CEntity(pWorld, CGameWorld::ENTTYPE_SCRIPTED),
	m_pController(pController),
	m_Type(Type),
	m_Value(Value),
	m_State(0),
	m_Progress(0),
	m_Owner(Owner)
{
	m_Pos = Pos;
	m_ProximityRadius = 24.0f;
	GameWorld()->InsertEntity(this);
}

void CExtractionObject::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_State == 3)
		return;
	CNetObj_ExtractionObject *pObj = static_cast<CNetObj_ExtractionObject *>(
		Server()->SnapNewItem(NETOBJTYPE_EXTRACTIONOBJECT, m_ID, sizeof(CNetObj_ExtractionObject)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_Type = m_Type;
	pObj->m_Value = m_Value;
	pObj->m_State = m_State;
	pObj->m_Progress = m_Progress;
}
