#include <base/system.h>
#include <engine/platform_events.h>

#include "platform_event_queue.h"

#include <stdio.h>

CPlatformEventQueue::CPlatformEventQueue()
{
	Clear();
}

void CPlatformEventQueue::Clear()
{
	m_Count = 0;
}

bool CPlatformEventQueue::Add(int Event, int Value, bool Eligible)
{
	if(Event < 0 || Event >= NUM_PLATFORM_SERVER_EVENTS)
		return false;
	for(int i = 0; i < m_Count; i++)
	{
		if(m_aEntries[i].m_Event != Event)
			continue;
		m_aEntries[i].m_Eligible = m_aEntries[i].m_Eligible || Eligible;
		if(Event < 12 || Event == PLATFORM_EVENT_LB_INVASION_FLOOR)
			m_aEntries[i].m_Value = m_aEntries[i].m_Value > Value ? m_aEntries[i].m_Value : Value;
		else if(Event == PLATFORM_EVENT_LB_FIXED_SEED_TIME_MS)
			m_aEntries[i].m_Value =
				m_aEntries[i].m_Value <= 0 || Value < m_aEntries[i].m_Value ? Value : m_aEntries[i].m_Value;
		else if(Event == PLATFORM_EVENT_STAT_COOP_COMPLETIONS)
			m_aEntries[i].m_Value += Value > 1 ? Value : 1;
		return true;
	}
	if(m_Count >= MAX_ENTRIES)
		return false;
	m_aEntries[m_Count].m_Event = Event;
	m_aEntries[m_Count].m_Value = Value;
	m_aEntries[m_Count].m_Eligible = Eligible;
	m_Count++;
	return true;
}

void CPlatformEventQueue::RemoveFirst()
{
	if(!m_Count)
		return;
	for(int i = 1; i < m_Count; i++)
		m_aEntries[i - 1] = m_aEntries[i];
	m_Count--;
}

bool CPlatformEventQueue::ReadText(const char *pText)
{
	Clear();
	if(!pText)
		return false;
	const char *pLine = pText;
	while(*pLine)
	{
		int Event, Value, Eligible, Consumed = 0;
		if(sscanf(pLine, "%d %d %d%n", &Event, &Value, &Eligible, &Consumed) != 3 || Consumed <= 0 ||
		   (Eligible != 0 && Eligible != 1) || !Add(Event, Value, Eligible != 0))
		{
			Clear();
			return false;
		}
		pLine += Consumed;
		if(*pLine == '\r')
			pLine++;
		if(*pLine == '\n')
			pLine++;
		else if(*pLine)
		{
			Clear();
			return false;
		}
	}
	return true;
}

int CPlatformEventQueue::WriteText(char *pBuffer, int BufferSize) const
{
	if(!pBuffer || BufferSize <= 0)
		return -1;
	int Offset = 0;
	for(int i = 0; i < m_Count; i++)
	{
		if(BufferSize - Offset < 32)
			return -1;
		str_format(pBuffer + Offset,
				   BufferSize - Offset,
				   "%d %d %d\n",
				   m_aEntries[i].m_Event,
				   m_aEntries[i].m_Value,
				   m_aEntries[i].m_Eligible ? 1 : 0);
		const int Written = str_length(pBuffer + Offset);
		Offset += Written;
	}
	return Offset;
}
