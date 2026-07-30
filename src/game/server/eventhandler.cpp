

#include <base/system.h>
#include "eventhandler.h"
#include "gamecontext.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = 0;
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, int64 Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return 0;
	if(m_CurrentOffset + Size >= MAX_DATASIZE)
		return 0;

	void *p = &m_aData[m_CurrentOffset];
	m_aEvents[m_NumEvents].m_Offset = m_CurrentOffset;
	m_aEvents[m_NumEvents].m_Type = Type;
	m_aEvents[m_NumEvents].m_Size = Size;
	m_aEvents[m_NumEvents].m_ClientMask = Mask;
	m_CurrentOffset += Size;
	m_NumEvents++;
	return p;
}

void CEventHandler::Clear()
{
	m_NumEvents = 0;
	m_CurrentOffset = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aEvents[i].m_ClientMask, SnappingClient))
		{
			CNetEvent_Common *ev = (CNetEvent_Common *)&m_aData[m_aEvents[i].m_Offset];
			bool Visible = SnappingClient == -1;
			if(!Visible)
			{
				const vec2 Delta = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos - vec2(ev->m_X, ev->m_Y);
				Visible = dot(Delta, Delta) < 1500.0f * 1500.0f;
			}
			if(Visible)
			{
				void *d = GameServer()->Server()->SnapNewItem(m_aEvents[i].m_Type, i, m_aEvents[i].m_Size);
				if(d)
					mem_copy(d, &m_aData[m_aEvents[i].m_Offset], m_aEvents[i].m_Size);
			}
		}
	}
}
