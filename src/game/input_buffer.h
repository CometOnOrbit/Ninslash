#ifndef GAME_INPUT_BUFFER_H
#define GAME_INPUT_BUFFER_H

inline int QueueInputUntil(int CurrentTick, int RemainingTicks, int WindowTicks)
{
	return RemainingTicks > 0 && RemainingTicks <= WindowTicks ? CurrentTick + WindowTicks : 0;
}

enum EInputBufferState
{
	INPUT_BUFFER_WAITING,
	INPUT_BUFFER_READY,
	INPUT_BUFFER_EXPIRED,
};

inline EInputBufferState InputBufferState(int CurrentTick, int EndTick, bool ActionReady)
{
	if(!EndTick || CurrentTick > EndTick)
		return INPUT_BUFFER_EXPIRED;
	return ActionReady ? INPUT_BUFFER_READY : INPUT_BUFFER_WAITING;
}

class CDiscreteInputPulse
{
	int m_QueuedValue = 0;
	bool m_NeedsRelease = false;

  public:
	void Reset()
	{
		m_QueuedValue = 0;
		m_NeedsRelease = false;
	}

	void Queue(int Value)
	{
		if(Value > 0)
			m_QueuedValue = Value;
	}

	void CancelQueued() { m_QueuedValue = 0; }
	bool NeedsRelease() const { return m_NeedsRelease; }

	// Returns -1 when the caller should preserve its current value.
	int Prepare()
	{
		if(m_NeedsRelease)
			return 0;
		if(m_QueuedValue <= 0)
			return -1;
		const int Value = m_QueuedValue;
		m_QueuedValue = 0;
		return Value;
	}

	void OnSent(int Value)
	{
		if(Value > 0)
			m_NeedsRelease = true;
		else if(m_NeedsRelease)
			m_NeedsRelease = false;
	}
};

inline int SwitchInputBufferTicks(int ReloadTicks, int TickSpeed)
{
	int Ticks = ReloadTicks + 3;
	if(Ticks < 3)
		Ticks = 3;
	if(Ticks > TickSpeed)
		Ticks = TickSpeed;
	return Ticks;
}

#endif
