#ifndef ENGINE_SHARED_PLATFORM_EVENT_QUEUE_H
#define ENGINE_SHARED_PLATFORM_EVENT_QUEUE_H

class CPlatformEventQueue
{
public:
	struct CEntry
	{
		int m_Event;
		int m_Value;
		bool m_Eligible;
	};
	enum { MAX_ENTRIES = 64 };

private:
	CEntry m_aEntries[MAX_ENTRIES];
	int m_Count;

public:
	CPlatformEventQueue();
	void Clear();
	int Count() const { return m_Count; }
	const CEntry *First() const { return m_Count ? &m_aEntries[0] : 0; }
	bool Add(int Event, int Value, bool Eligible);
	void RemoveFirst();
	bool ReadText(const char *pText);
	int WriteText(char *pBuffer, int BufferSize) const;
};

#endif
